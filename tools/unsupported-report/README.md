# Unsupported constructs report

Generates one self-contained HTML page for one SystemVerilog project. The
script runs `verihogg-format` on every `.sv` and `.svh` file and parses the
formatter's grouped end-of-run summary.

## Requirements

- Python 3.10 or newer
- `verihogg-format` in `PATH`, or pass `--formatter`

## Usage

```bash
python3 tools/unsupported-report/unsupported_report.py /path/to/project
```

By default the page is written to
`tools/unsupported-report/projects/unsupported-<project>.html`.

Write the page to a specific path:

```bash
python3 tools/unsupported-report/unsupported_report.py /path/to/project \
  -o gh-pages/projects/my-project.html
```

Use a specific formatter binary and more parallel jobs:

```bash
python3 tools/unsupported-report/unsupported_report.py /path/to/project \
  --formatter ./build/bin/verihogg-format \
  --jobs 8
```

## Options

- `-o, --output` — output HTML path. Default:
  `tools/unsupported-report/projects/unsupported-<project>.html`.
- `--formatter` — path to `verihogg-format`. Default: found in `PATH`.
- `--jobs` — number of parallel formatter processes. Default: CPU count.

## Notes

- The project directory is scanned recursively for `.sv` and `.svh` files.
- `.git`, `build`, `out`, `venv`, `.venv`, and `node_modules` are skipped.
- One formatter process is started per file because the summary aggregates
  counts across all files passed in one invocation.
- The report shows the first occurrence line for each construct in each file.
- All constructs from the summary are included.
- Files that fail or time out are listed on the page and reported on stderr.

## Index page

Build the combined page from all project pages:

```bash
python3 tools/unsupported-report/build_index.py
```

By default it reads `tools/unsupported-report/projects` and writes
`tools/unsupported-report/index.html`.

Use custom directories:

```bash
python3 tools/unsupported-report/build_index.py \
  --projects-dir /path/to/projects \
  -o /path/to/index.html
```

## GitHub Pages

Generate one page per project into a `gh-pages` checkout:

```bash
python3 tools/unsupported-report/unsupported_report.py ../my-project \
  -o ../gh-pages/projects/my-project.html
```
