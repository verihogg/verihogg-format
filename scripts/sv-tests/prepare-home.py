#!/usr/bin/env python3
"""Add a landing page to a complete VerihoggFormat sv-tests report."""

import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path, help='Generated report directory')
    args = parser.parse_args()
    root = args.report
    index = root / 'index.html'
    table = root / 'report.html'
    construct_reports = (root / 'constructs.html', root / 'unsupported.html')
    for path in (index, *construct_reports):
        if not path.is_file():
            parser.error(f'Missing report file: {path}')
    if table.exists():
        parser.error(f'Refusing to overwrite existing file: {table}')
    templates = Path(__file__).resolve().parent / 'templates'
    template = templates / 'home.html'
    stylesheet = templates / 'sv-tests-reports.css'
    for asset in (template, stylesheet):
        if not asset.is_file():
            parser.error(f'Missing landing page asset: {asset}')

    updated_reports = {}
    stylesheet_link = f'<link rel="stylesheet" href="{stylesheet.name}">\n'
    for path in construct_reports:
        head, style, rest = path.read_text(encoding='utf-8').partition('<style>')
        if not style:
            parser.error(f'Missing inline stylesheet in {path}')
        updated_reports[path] = head + stylesheet_link + style + rest

    index.rename(table)
    shutil.copyfile(template, index)
    shutil.copyfile(stylesheet, root / stylesheet.name)
    for path, page in updated_reports.items():
        path.write_text(page, encoding='utf-8')


if __name__ == '__main__':
    main()
