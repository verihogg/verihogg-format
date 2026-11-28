#!/usr/bin/env python3
"""Verify the actual formatter invocation used by the sv-tests runner."""

import os
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path('tools').resolve()))
from runners.VerihoggFormat import VerihoggFormat


def main():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        source = root / 'input.sv'
        source.write_text('module m; initial assert (1); endmodule\n')
        runner = VerihoggFormat()
        repo, sha = os.environ["FORMATTER_REPOSITORY"], os.environ["FORMATTER_SHA"]
        expected_url = f"https://github.com/{repo}/tree/{sha}"
        if runner.url != expected_url:
            raise SystemExit(f'Runner labels the report with the wrong formatter: {runner.url}')
        params = {
            'files': [str(source)], 'incdirs': [str(root)], 'defines': ['TEST=1'],
            'timeout': '30',
        }
        result = runner.run(str(root / 'work'), params)
        log, code = result[:2]
        if code != 0 or runner.warning_count == 0:
            raise SystemExit(f'Runner did not format and compare a warning test:\n{log}')
        formatted = root / 'work' / 'formatted' / source.name
        if not formatted.is_file() or formatted.read_text() == source.read_text():
            raise SystemExit('Formatter did not modify the test input')
        print('Formatter runner smoke test passed')


if __name__ == '__main__':
    main()
