#!/usr/bin/env python3
"""Generate an HTML report of unsupported VerihoggFormat constructs."""

from __future__ import annotations

import argparse
import html
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from site_layout import document

DEFAULT_EXTENSIONS = {".sv", ".svh"}
SKIP_DIRS = {".git", "build", "out", "venv", ".venv", "node_modules"}
DEFAULT_TIMEOUT_SECONDS = 120.0
PROJECTS_DIR = Path(__file__).resolve().parent / "projects"

SUMMARY_ENTRY_RE = re.compile(
    r"^  - (.+?)\s{2,}(\d+) occurrences?,\s+first at (.+)$", re.MULTILINE
)
TOKEN_PREFIX = "unsupported token kind "
INCOMPATIBLE_LABEL = "incompatible conditional compilation branches"

TOKEN_SYMBOLS = {
    "DoublePlus": "++",
    "DoubleMinus": "--",
    "PlusColon": "+:",
    "MinusColon": "-:",
    "DoubleColon": "::",
    "Dollar": "$",
    "MinusArrow": "->",
    "OrEqualsArrow": "=>",
}
KEYWORD_SUFFIX = "Keyword"

KEYWORD_LEXEMES = {
    "AcceptOnKeyword": "accept_on",
    "AlwaysCombKeyword": "always_comb",
    "AlwaysFFKeyword": "always_ff",
    "AlwaysLatchKeyword": "always_latch",
    "FirstMatchKeyword": "first_match",
    "IgnoreBinsKeyword": "ignore_bins",
    "IllegalBinsKeyword": "illegal_bins",
    "JoinAnyKeyword": "join_any",
    "JoinNoneKeyword": "join_none",
    "PulseStyleOnDetectKeyword": "pulsestyle_ondetect",
    "PulseStyleOnEventKeyword": "pulsestyle_onevent",
    "RejectOnKeyword": "reject_on",
    "SAlwaysKeyword": "s_always",
    "SEventuallyKeyword": "s_eventually",
    "SNextTimeKeyword": "s_nexttime",
    "SUntilKeyword": "s_until",
    "SUntilWithKeyword": "s_until_with",
    "SyncAcceptOnKeyword": "sync_accept_on",
    "SyncRejectOnKeyword": "sync_reject_on",
    "UntilWithKeyword": "until_with",
    "WaitOrderKeyword": "wait_order",
}

ANNOTATION_REASONS = {
    "unknown lexical token",
    "unmatched closing bracket",
    "mismatched closing bracket",
    "based literal wildcard is not supported",
    "conditional operator has no matching colon",
    "unclosed bracket before statement end",
}
ANNOTATION_HINTS = ("bracket", "wildcard", "no matching colon")

CSS = """
:root {
  --bg: #0f172a;
  --panel: #111c34;
  --panel-2: #162240;
  --text: #e6edf7;
  --muted: #8ca0bf;
  --line: #24345a;
  --accent: #7c9cff;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font: 14px/1.5 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
}
header {
  padding: 28px 32px 20px;
  border-bottom: 1px solid var(--line);
  background: linear-gradient(180deg, #131f3b, #0f172a);
}
header h2 { margin: 0 0 6px; font-size: 26px; }
header .meta { margin: 0; color: var(--muted); }
main { padding: 24px 32px 48px; max-width: 1500px; margin: 0 auto; }
.cards {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  gap: 12px;
  margin-bottom: 24px;
}
.card {
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 12px;
  padding: 14px 16px;
}
.card .value { font-size: 26px; font-weight: 650; }
.card .label { color: var(--muted); font-size: 12px; margin-top: 2px; }
h3 { font-size: 18px; margin: 0 0 10px; }
.lead { color: var(--muted); margin: 0 0 12px; }
table {
  width: 100%;
  border-collapse: collapse;
  background: var(--panel);
  border: 1px solid var(--line);
  border-radius: 10px;
  overflow: hidden;
}
th, td {
  padding: 9px 11px;
  border-bottom: 1px solid var(--line);
  text-align: left;
  vertical-align: top;
}
thead th {
  background: var(--panel-2);
  color: var(--muted);
  font-size: 12px;
  text-transform: uppercase;
  letter-spacing: .03em;
}
tbody tr:last-child td { border-bottom: none; }
.num { text-align: right; font-variant-numeric: tabular-nums; white-space: nowrap; }
.muted { color: var(--muted); }
code {
  background: #0b1428;
  border: 1px solid var(--line);
  border-radius: 5px;
  padding: 1px 5px;
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  font-size: 12.5px;
}
ul { margin: 0; padding-left: 20px; }
li { margin: 2px 0; }
"""


@dataclass
class FileOccurrence:
    count: int = 0
    first_line: int = 0


@dataclass
class ConstructStats:
    count: int = 0
    files: dict[str, FileOccurrence] = field(default_factory=dict)


def die(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(2)


def find_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(name for name in dirnames if name not in SKIP_DIRS)
        for filename in sorted(filenames):
            if Path(filename).suffix.lower() in DEFAULT_EXTENSIONS:
                files.append(Path(dirpath) / filename)
    return sorted(files)


def classify(label: str) -> tuple[str, str]:
    if label.startswith(TOKEN_PREFIX):
        return "token", label[len(TOKEN_PREFIX) :].strip()
    if label == INCOMPATIBLE_LABEL:
        return "incompatible", label
    lowered = label.lower()
    if label in ANNOTATION_REASONS or any(hint in lowered for hint in ANNOTATION_HINTS):
        return "annotation", label
    return "construct", label


def type_label(label: str) -> str:
    category, _ = classify(label)
    if category in {"token", "annotation"}:
        return "invalid annotation"
    if category == "incompatible":
        return "incompatible conditional branches"
    if category == "construct":
        return "unsupported construct"
    return "other"


def token_lexeme(kind: str) -> str | None:
    if kind in KEYWORD_LEXEMES:
        return KEYWORD_LEXEMES[kind]
    if kind.endswith(KEYWORD_SUFFIX) and len(kind) > len(KEYWORD_SUFFIX):
        return kind[: -len(KEYWORD_SUFFIX)].lower()
    return TOKEN_SYMBOLS.get(kind)


def display_html(label: str) -> str:
    category, value = classify(label)
    escaped = html.escape(value)
    if category == "token":
        lexeme = token_lexeme(value)
        if lexeme:
            return (
                f"<code>{html.escape(lexeme)}</code> "
                f'<span class="muted">{escaped}</span>'
            )
    return f"<code>{escaped}</code>"


def parse_line(location: str) -> int:
    _, separator, tail = location.rpartition(":")
    if separator and tail.isdigit():
        return int(tail)
    return 0


def parse_summary(stderr: str) -> list[tuple[str, int, int]]:
    entries: list[tuple[str, int, int]] = []
    for match in SUMMARY_ENTRY_RE.finditer(stderr):
        label = match.group(1).strip()
        count = int(match.group(2))
        line = parse_line(match.group(3).strip())
        entries.append((label, count, line))
    return entries


def scan_file(formatter: str, path: Path, timeout: float) -> dict:
    try:
        completed = subprocess.run(
            [formatter, "--no-links", str(path)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired:
        return {"file": str(path), "error": f"timeout after {timeout:g}s"}
    except OSError as exc:
        return {"file": str(path), "error": str(exc)}

    entries = parse_summary(completed.stderr)
    reported_error = "Error:" in completed.stderr or "cannot read" in completed.stderr.lower()
    if completed.returncode != 0 or (not entries and reported_error):
        message = completed.stderr.strip() or f"exit code {completed.returncode}"
        return {"file": str(path), "error": message}

    return {"file": str(path), "entries": entries}


def scan_project(
    files: list[Path], formatter: str, jobs: int, timeout: float
) -> tuple[dict[str, ConstructStats], list[dict]]:
    results: list[dict] = []
    errors: list[dict] = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {
            pool.submit(scan_file, formatter, path, timeout): path for path in files
        }
        for future in as_completed(futures):
            try:
                result = future.result()
            except Exception as exc:
                result = {"file": str(futures[future]), "error": str(exc)}
            if "error" in result:
                errors.append(result)
            else:
                results.append(result)

    stats: dict[str, ConstructStats] = {}
    for result in results:
        file_path = result["file"]
        for label, count, line in result["entries"]:
            entry = stats.setdefault(label, ConstructStats())
            entry.count += count
            occurrence = entry.files.setdefault(file_path, FileOccurrence())
            occurrence.count += count
            if line and (occurrence.first_line == 0 or line < occurrence.first_line):
                occurrence.first_line = line

    return stats, errors


def relative_path(path: str, root: Path) -> str:
    try:
        return os.path.relpath(path, root)
    except ValueError:
        return path


def render_html(
    project: str,
    root: Path,
    files: list[Path],
    stats: dict[str, ConstructStats],
    errors: list[dict],
    formatter: str,
    *,
    site: bool = False,
) -> str:
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    total_occurrences = sum(item.count for item in stats.values())
    files_with_warnings = len({path for item in stats.values() for path in item.files})

    rows: list[str] = []
    for label, item in sorted(
        stats.items(), key=lambda pair: (-pair[1].count, pair[0])
    ):
        file_parts: list[str] = []
        for file_path, occurrence in sorted(
            item.files.items(), key=lambda pair: relative_path(pair[0], root)
        ):
            location = html.escape(relative_path(file_path, root))
            if site:
                location = location.replace("/", "/<wbr>")
            if occurrence.first_line:
                location = f"{location}:{occurrence.first_line}"
            file_parts.append(
                f"<code>{location}</code> "
                f'<span class="muted">({occurrence.count})</span>'
            )
        rows.append(
            "<tr>"
            f'<td data-label="{html.escape(label)}">{display_html(label)}</td>'
            f'<td class="muted">{html.escape(type_label(label))}</td>'
            f'<td class="num">{item.count}</td>'
            f"<td>{'<br>'.join(file_parts)}</td>"
            "</tr>"
        )

    if rows:
        body = "".join(rows)
    else:
        body = (
            '<tr><td colspan="4" class="muted">'
            "No unsupported constructs found.</td></tr>"
        )

    error_section = ""
    if errors:
        items = "".join(
            "<li>"
            f"<code>{html.escape(relative_path(item['file'], root))}</code>: "
            f"{html.escape(item['error'])}"
            "</li>"
            for item in sorted(
                errors, key=lambda item: relative_path(item["file"], root)
            )
        )
        error_section = f"<section><h3>Skipped files</h3><ul>{items}</ul></section>"

    cards = [
        (len(files) - len(errors) if site else len(files), "Files scanned"),
        (files_with_warnings, "Files with warnings"),
        (len(stats), "Unique constructs"),
        (total_occurrences, "Occurrences"),
        (len(errors), "Skipped files"),
    ]
    table = (
        "<table>"
        "<thead><tr><th>Construct</th><th>Error type</th>"
        '<th class="num">Count</th><th>Files</th></tr></thead>'
        f"<tbody>{body}</tbody></table>"
    )
    if site:
        stats_html = "".join(
            f"<div><dt>{label}</dt><dd>{value}</dd></div>" for value, label in cards
        )
        return document(
            project,
            f'<p class="meta">Generated {generated}</p>'
            f'<dl class="project-stats">{stats_html}</dl>'
            '<section><h3>Constructs</h3>'
            '<p class="lead">Locations show the first occurrence per file.</p>'
            f'<div class="table-scroll" tabindex="0" role="region" aria-label="Constructs table">{table}</div>'
            f'</section>{error_section}',
            detail=True,
        )

    card_html = "".join(
        '<div class="card">'
        f'<div class="value">{value}</div>'
        f'<div class="label">{html.escape(label)}</div>'
        "</div>"
        for value, label in cards
    )

    return (
        "<!doctype html>\n"
        '<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        f"<title>{html.escape(project)} — "
        "VerihoggFormat unsupported constructs</title>\n"
        f"<style>{CSS}</style>\n"
        "</head>\n<body>\n"
        "<header>"
        f"<h2>{html.escape(project)}</h2>"
        '<p class="meta">VerihoggFormat unsupported constructs · '
        f"generated {generated}</p>"
        "</header>\n<main>\n"
        f'<section class="cards">{card_html}</section>\n'
        "<section>"
        "<h3>Constructs</h3>"
        '<p class="lead">Grouped from the formatter end-of-run summary. '
        "Counts are occurrences; locations point to the first occurrence "
        "in each file.</p>"
        f"{table}"
        "</section>\n"
        f"{error_section}\n"
        f'<p class="muted">Formatter: <code>{html.escape(Path(formatter).name)}</code></p>\n'
        "</main>\n</body>\n</html>\n"
    )


def find_formatter(requested: str | None) -> str:
    if requested:
        expanded = os.path.expanduser(requested)
        found = shutil.which(expanded)
        if found is None:
            candidate = Path(expanded)
            if candidate.is_file() and os.access(candidate, os.X_OK):
                found = str(candidate)
        if found is None:
            die(f"verihogg-format not found: {requested}")
        return str(Path(found).resolve())

    found = shutil.which("verihogg-format")
    if found is not None:
        return str(Path(found).resolve())

    repo_root = Path(__file__).resolve().parents[2]
    candidates = [
        repo_root / "result" / "bin" / "verihogg-format",
        repo_root / "build" / "bin" / "verihogg-format",
    ]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate.resolve())

    tried = ", ".join(str(candidate) for candidate in candidates)
    die(
        "verihogg-format not found. Pass --formatter PATH or install it in "
        f"PATH. Also checked: {tried}"
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate an HTML report of unsupported VerihoggFormat "
        "constructs for one SystemVerilog project."
    )
    parser.add_argument(
        "project",
        type=Path,
        help="root directory of the SystemVerilog project",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="output HTML path (default: "
        "tools/unsupported-report/projects/unsupported-<project>.html)",
    )
    parser.add_argument(
        "--formatter",
        help="path to verihogg-format (default: PATH, then repo result/bin "
        "and build/bin)",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        help="parallel formatter processes (default: CPU count)",
    )
    parser.add_argument("--site", action="store_true", help="render for the combined Pages site")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = args.project.expanduser().resolve()
    if not root.is_dir():
        die(f"Project directory does not exist: {root}")

    if args.jobs is not None and args.jobs < 1:
        die("--jobs must be at least 1")

    formatter = find_formatter(args.formatter)

    jobs = args.jobs or max(1, os.cpu_count() or 1)
    project = root.name or "project"
    output = args.output or (PROJECTS_DIR / f"unsupported-{project}.html")
    output = output.expanduser()
    output.parent.mkdir(parents=True, exist_ok=True)

    files = find_files(root)
    if not files:
        die(f"No .sv or .svh files found under {root}")

    print(f"Scanning {len(files)} files in {root}", file=sys.stderr)
    stats, errors = scan_project(files, formatter, jobs, DEFAULT_TIMEOUT_SECONDS)
    for error in errors:
        print(
            f"Skipped {relative_path(error['file'], root)}: {error['error']}",
            file=sys.stderr,
        )
    if args.site and len(errors) == len(files):
        die(f"Could not scan any files in {root}")

    page = render_html(project, root, files, stats, errors, formatter, site=args.site)
    output.write_text(page, encoding="utf-8")

    print(
        f"Report: {output} ({len(stats)} constructs, {len(errors)} skipped files)",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
