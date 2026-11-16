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
    for path in (index, root / 'constructs.html', root / 'unsupported.html'):
        if not path.is_file():
            parser.error(f'Missing report file: {path}')
    if table.exists():
        parser.error(f'Refusing to overwrite existing file: {table}')
    template = Path(__file__).resolve().parent / 'sv-tests-home.html'
    if not template.is_file():
        parser.error(f'Missing landing page template: {template}')
    index.rename(table)
    shutil.copyfile(template, index)


if __name__ == '__main__':
    main()
