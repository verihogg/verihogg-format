#!/usr/bin/env python3
"""Render the formatter support timeline in the sv-tests report."""

import argparse
import html
from collections import defaultdict
from itertools import groupby
from pathlib import Path

from history import commit_distance, load_history, read_history, read_json, record


def timeline(history):
    by_sha = defaultdict(list)
    for run in history["runs"]:
        by_sha[run["sha"]].append(run)

    entries = []
    for commit in history.get("backfill", []):
        measured = by_sha.pop(commit["sha"], [])
        if not measured:
            entries.append(commit)
            continue
        entries.extend(measured[:-1])
        entries.append({**commit, **measured[-1]})
    for run in history["runs"]:
        if run["sha"] in by_sha:
            entries.append(run)
    return entries


def commit_link(run, repository):
    sha = html.escape(run["sha"], quote=True)
    subject = html.escape(run["subject"] or sha)
    return f'<a href="{repository}/commit/{sha}">{subject} <code>{sha[:8]}</code></a>'


def render_history(archive, repository, current):
    runs = timeline(load_history(archive))
    repository = "https://github.com/" + html.escape(repository, quote=True)

    def render_commit(run):
        date = html.escape(run["date"][:10])
        return f'<time datetime="{date}">{date}</time> {commit_link(run, repository)}'

    def group_kind(run):
        if run.get("before_diagnostics"):
            return "early"
        if "baseline" not in run:
            return "missing"
        if not (run["baseline"] or run["added"] or run["lost"]
                or run.get("diagnostics_removed") or run.get("diagnostics_added")):
            return "unchanged"
        return "entry"

    count = len(current["constructs"])
    noun = "construct" if count == 1 else "constructs"
    latest = (
        '<section class="latest-results"><h3>Latest results</h3>'
        f'<p>{count} unsupported {noun}. '
        '<a href="unsupported.html">View full report</a></p></section>'
    )
    entries = []
    for kind, group in groupby(reversed(runs), group_kind):
        if kind != "entry":
            items = list(group)
            count = len(items)
            noun = "commit" if count == 1 else "commits"
            label = {
                "early": f"Before unsupported-construct diagnostics ({count} {noun})",
                "missing": f"No measured results ({count} {noun})",
                "unchanged": f"{count} tested {noun} with no confirmed support changes",
            }[kind]
            lines = []
            for run in items:
                summary = render_commit(run)
                if (run.get("commit_count") or 0) > 1:
                    summary += render_range(run, repository)
                lines.append(f"<li>{summary}</li>")
            group_class = "no-changes unchanged" if kind == "unchanged" else "no-changes"
            entries.append(
                f'<li class="{group_class}"><details><summary>{label}</summary>'
                f'<ul>{"".join(lines)}</ul></details></li>'
            )
            continue

        for run in group:
            changes = render_changes(run)
            if run["baseline"]:
                message = ("Test configuration changed. Tracking starts again here."
                           if run["base_sha"] else "First recorded run.")
                changes.insert(0, f'<p class="baseline">{message}</p>')
            commit = f'<h3>{render_commit(run)}</h3>'
            if (run.get("commit_count") or 0) > 1:
                commit += render_range(run, repository)
            entries.append(
                f'<li class="history-entry"><div class="commit-meta">{commit}</div>'
                f'<div class="entry-changes">{"".join(changes)}</div></li>'
            )
    if entries:
        entries.insert(0, f'<li class="history-latest">{latest}</li>')
        return '<ol class="timeline" role="list">' + "\n".join(entries) + "</ol>"
    return latest + '<p class="history-empty">History will appear after the first recorded run on main.</p>'


def render_changes(run):
    lines = []
    for key, label, marker in (
        ("added", "newly supported", "+"),
        ("lost", "regressed", "−"),
    ):
        names = run[key]
        if names:
            noun = "construct" if len(names) == 1 else "constructs"
            items = "".join(
                f'<li><span aria-hidden="true">{marker}</span><code>{html.escape(name)}</code></li>'
                for name in names
            )
            lines.append(f'<ul class="support-change support-{key}" role="list" '
                         f'aria-label="{len(names)} {label} {noun}">{items}</ul>')
    for key, label in (
        ("diagnostics_removed", "{} no longer observed"),
        ("diagnostics_added", "new {}"),
    ):
        names = run.get(key)
        if names:
            title = label.format("diagnostic" if len(names) == 1 else "diagnostics")
            items = "".join(f"<li>{html.escape(name)}</li>" for name in names)
            lines.append(f'<details class="diagnostic-change"><summary>{len(names)} {title}</summary>'
                         f"<ul>{items}</ul></details>")
    return lines


def render_range(run, repository):
    base = html.escape(run["base_sha"], quote=True)
    sha = html.escape(run["sha"], quote=True)
    count = run["commit_count"]
    return f'<p><a href="{repository}/compare/{base}...{sha}">{count} commits since the previous run</a></p>'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--record-from", type=Path, help="Formatter checkout with main history")
    args = parser.parse_args()

    current = read_json(args.snapshot)
    if args.record_from:
        runs = read_history(args.archive)
        try:
            distance = commit_distance(args.record_from, runs[-1]["sha"], current["sha"]) if runs else None
        except ValueError as error:
            parser.exit(1, f"{error}. Skipping publication of this run.\n")
        record(args.archive, current, distance)

    template = (Path(__file__).parent / "templates" / "history.html").read_text(encoding="utf-8")
    content = render_history(args.archive, args.repository, current)
    (args.report / "history.html").write_text(
        template.replace("<!-- SUPPORT_HISTORY -->", content), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
