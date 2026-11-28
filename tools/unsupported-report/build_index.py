#!/usr/bin/env python3
"""Build an index page for VerihoggFormat unsupported-construct reports."""

from __future__ import annotations

import argparse
import html
import os
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import quote

from site_layout import document
from unsupported_report import CSS, display_html

DEFAULT_PROJECTS_DIR = Path(__file__).resolve().parent / "projects"
DEFAULT_OUTPUT = Path(__file__).resolve().parent / "index.html"

INDEX_CSS = """
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
ul.projects { columns: 2; column-gap: 32px; }
"""


@dataclass
class ProjectPage:
    name: str
    path: Path
    rows: list[list[tuple[str, str]]]
    labels: dict[str, str] = field(default_factory=dict)


@dataclass
class Aggregate:
    construct: str
    error_type: str
    label: str
    count: int = 0
    projects: set[str] = field(default_factory=set)


class PageParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.in_header = False
        self.title_tag = ""
        self.title_parts: list[str] = []
        self.in_row = False
        self.in_cell = False
        self.cell_tag = ""
        self.cell_label = ""
        self.cell_parts: list[str] = []
        self.row: list[tuple[str, str]] = []
        self.rows: list[list[tuple[str, str]]] = []
        self.labels: dict[str, str] = {}

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag == "header":
            self.in_header = True
        elif tag == "h1" or (tag == "h2" and self.in_header):
            self.title_tag = tag
        elif tag == "tr":
            self.in_row = True
            self.row = []
        elif tag in {"td", "th"} and self.in_row:
            self.in_cell = True
            self.cell_tag = tag
            self.cell_label = dict(attrs).get("data-label") or ""
            self.cell_parts = []
        elif tag == "br" and self.in_cell:
            self.cell_parts.append(" ")

    def handle_endtag(self, tag: str) -> None:
        if tag == "header":
            self.in_header = False
        elif tag == self.title_tag:
            self.title_tag = ""
        elif tag in {"td", "th"} and self.in_cell and tag == self.cell_tag:
            text = " ".join("".join(self.cell_parts).split())
            self.row.append((self.cell_tag, text))
            if self.cell_tag == "td" and len(self.row) == 1 and self.cell_label:
                self.labels[text] = self.cell_label
            self.in_cell = False
            self.cell_tag = ""
            self.cell_label = ""
        elif tag == "tr" and self.in_row:
            if self.row and all(kind == "td" for kind, _ in self.row):
                self.rows.append(self.row)
            self.in_row = False

    def handle_data(self, data: str) -> None:
        if self.title_tag:
            self.title_parts.append(data)
        if self.in_cell:
            self.cell_parts.append(data)

    @property
    def project_name(self) -> str:
        return " ".join("".join(self.title_parts).split())


def die(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(2)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build an index page for VerihoggFormat unsupported-construct "
        "reports."
    )
    parser.add_argument(
        "--projects-dir",
        type=Path,
        default=DEFAULT_PROJECTS_DIR,
        help="directory containing project HTML pages "
        "(default: tools/unsupported-report/projects)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="output HTML path (default: tools/unsupported-report/index.html)",
    )
    parser.add_argument("--site", action="store_true", help="render for the combined Pages site")
    return parser.parse_args(argv)


def read_page(path: Path) -> ProjectPage:
    parser = PageParser()
    parser.feed(path.read_text(encoding="utf-8"))
    name = parser.project_name or path.stem.removeprefix("unsupported-")
    return ProjectPage(name=name, path=path, rows=parser.rows, labels=parser.labels)


def find_pages(projects_dir: Path, output: Path) -> list[ProjectPage]:
    pages: list[ProjectPage] = []
    for path in sorted(projects_dir.glob("*.html")):
        if path.name == "index.html" or path.resolve() == output:
            continue
        try:
            pages.append(read_page(path))
        except (OSError, UnicodeError) as exc:
            print(f"Skipped {path.name}: {exc}", file=sys.stderr)
    return sorted(pages, key=lambda page: (page.name.casefold(), page.path.name))


def aggregate(pages: list[ProjectPage]) -> dict[tuple[str, str], Aggregate]:
    result: dict[tuple[str, str], Aggregate] = {}
    for page in pages:
        for row in page.rows:
            if len(row) < 4:
                continue
            construct = row[0][1]
            error_type = row[1][1]
            try:
                count = int(row[2][1].replace(",", ""))
            except ValueError:
                continue
            if not construct:
                continue
            key = (construct, error_type)
            label = page.labels.get(construct, construct)
            item = result.get(key)
            if item is None:
                item = Aggregate(construct, error_type, label)
                result[key] = item
            elif item.label == item.construct and label != construct:
                item.label = label
            item.count += count
            item.projects.add(page.name)
    return result


def href_for(page: ProjectPage, output: Path) -> str:
    relative = os.path.relpath(page.path, output.parent)
    return quote(Path(relative).as_posix(), safe="/")


def render_html(
    pages: list[ProjectPage],
    aggregates: dict[tuple[str, str], Aggregate],
    output: Path,
    *,
    site: bool = False,
) -> str:
    generated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    hrefs = {page.name: href_for(page, output) for page in pages}

    if pages:
        links = "".join(
            f'<li><a href="{html.escape(hrefs[page.name])}">'
            f"{html.escape(page.name)}</a></li>"
            for page in pages
        )
        list_class = "project-list" if site else "projects"
        projects = f'<ul class="{list_class}">{links}</ul>'
    else:
        projects = '<p class="muted">No project pages found.</p>'

    if site and not pages:
        return document(
            "Unsupported constructs in projects",
            '<p class="muted">No project reports are available for this run.</p>',
        )

    rows: list[str] = []
    for item in sorted(
        aggregates.values(),
        key=lambda entry: (
            -entry.count,
            entry.construct.casefold(),
            entry.error_type.casefold(),
        ),
    ):
        project_links = (", " if site else "<br>").join(
            f'<a href="{html.escape(hrefs[name])}">{html.escape(name)}</a>'
            for name in sorted(item.projects, key=str.casefold)
        )
        rows.append(
            "<tr>"
            f"<td>{display_html(item.label)}</td>"
            f'<td class="muted">{html.escape(item.error_type)}</td>'
            f'<td class="num">{item.count}</td>'
            f"<td>{project_links}</td>"
            "</tr>"
        )

    if rows:
        body = "".join(rows)
    else:
        body = (
            '<tr><td colspan="4" class="muted">'
            "No unsupported constructs found.</td></tr>"
        )

    table = (
        "<table>"
        "<thead><tr><th>Construct</th><th>Error type</th>"
        '<th class="num">Count</th><th>Projects</th></tr></thead>'
        f"<tbody>{body}</tbody></table>"
    )
    if site:
        return document(
            "Unsupported constructs in projects",
            f'<p class="meta">Generated {generated}</p>'
            f'<section><h3>Projects scanned ({len(pages)})</h3>{projects}</section>'
            '<section><h3>Constructs</h3>'
            '<p class="lead">Occurrences across the projects listed above.</p>'
            f'<div class="table-scroll" tabindex="0" role="region" aria-label="Constructs table">{table}</div>'
            '</section>',
        )

    return (
        "<!doctype html>\n"
        '<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1">\n'
        "<title>VerihoggFormat unsupported constructs</title>\n"
        f"<style>{CSS}{INDEX_CSS}</style>\n"
        "</head>\n<body>\n"
        "<header>"
        "<h2>VerihoggFormat unsupported constructs</h2>"
        '<p class="meta">'
        f"{len(pages)} projects · generated {generated}</p>"
        "</header>\n<main>\n"
        "<section><h3>Projects</h3>"
        f"{projects}</section>\n"
        "<section><h3>Constructs</h3>"
        '<p class="lead">Aggregated by construct and error type. '
        "Counts are occurrences across all projects.</p>"
        f"{table}"
        "</section>\n"
        "</main>\n</body>\n</html>\n"
    )


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    projects_dir = args.projects_dir.expanduser().resolve()
    if not projects_dir.is_dir():
        die(f"Projects directory does not exist: {projects_dir}")

    output = args.output.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    pages = find_pages(projects_dir, output)
    aggregates = aggregate(pages)
    page = render_html(pages, aggregates, output, site=args.site)
    output.write_text(page, encoding="utf-8")

    print(
        f"Index: {output} ({len(pages)} projects, {len(aggregates)} constructs)",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
