#!/usr/bin/env python3
"""Add available project reports and their navigation to the Pages artifact."""

import argparse
from pathlib import Path
import shutil

from build_index import main as build_index

NAV = '<nav class="report-nav" aria-label="Construct reports">'
PROJECT_LINK = '<a href="projects/index.html">In projects</a>'


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    parser.add_argument("artifacts", type=Path)
    args = parser.parse_args()

    if not args.artifacts.is_dir():
        parser.error(f"Missing project artifact directory: {args.artifacts}")

    pages = {}
    for name in ("unsupported.html", "constructs.html"):
        path = args.site / name
        page = path.read_text(encoding="utf-8")
        before, start, after = page.partition(NAV)
        links, end, rest = after.partition("</nav>")
        if page.count(NAV) != 1 or not end:
            parser.error(f"Expected one construct navigation block in {path}")
        pages[path] = before + start + links + PROJECT_LINK + end + rest

    index = args.site / "projects" / "index.html"
    details = index.parent / "details"
    details.mkdir(parents=True)
    for page in args.artifacts.glob("unsupported-*.html"):
        shutil.copyfile(page, details / page.name)

    build_index(["--site", "--projects-dir", str(details), "--output", str(index)])
    for path, page in pages.items():
        path.write_text(page, encoding="utf-8")


if __name__ == "__main__":
    main()
