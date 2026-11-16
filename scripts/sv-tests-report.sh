#!/usr/bin/env bash
# Run from the root of the pinned sv-tests checkout.
set -euo pipefail

generators_dir=${1:?Pass the directory containing the two report generators}
: "${FORMATTER_SHA:?Set FORMATTER_SHA to the tested formatter commit}"
: "${FORMATTER_REPOSITORY:?Set FORMATTER_REPOSITORY to owner/repository}"

command -v verihogg-format
command -v slang-driver
slang-driver --version

make info

# Match the sv-tests CI order; fusesoc and black-parrot use more resources.
generators=$(make list-generators)
for generator in $generators; do
  case "$generator" in
    fusesoc|black-parrot) continue ;;
  esac
  make "generate-$generator"
  make
done
for generator in $generators; do
  case "$generator" in
    fusesoc|black-parrot)
      make "generate-$generator"
      make
      ;;
  esac
done

test -s out/logs/VerihoggFormat/version
test -s out/logs/VerihoggFormat/url
make report

test -n "$(find out/report/results/verihoggformat -name '*.config.js' -print -quit)"
test -d out/report/logs
test -d out/report/tests
python3 "$generators_dir/sv_tests_unsupported_report.py" --report out/report
PYTHONPATH="$generators_dir" python3 - "$generators_dir" <<'PY'
from pathlib import Path
import subprocess
import sys
from sv_tests_unsupported_report import load_tests
from sv_tests_warning_constructs import render_page

root = Path('out/report').resolve()
tests = load_tests(root)
if not tests:
    raise SystemExit('No VerihoggFormat tests in report')
output = root / 'constructs.html'
if any(test.status == 'warning' for test in tests):
    subprocess.run([sys.executable, str(Path(sys.argv[1]) /
                   'sv_tests_warning_constructs.py'), '--report', str(root)],
                   check=True)
else:
    output.write_text(render_page({}, root, output), encoding='utf-8')
    print('Report: no warning constructs')
PY
test -s out/report/index.html
test -s out/report/unsupported.html
test -s out/report/constructs.html

FORMATTER_SHA="$FORMATTER_SHA" FORMATTER_REPOSITORY="$FORMATTER_REPOSITORY" python3 - <<'PY'
import html
import os
import subprocess
from datetime import datetime, timezone
from pathlib import Path

index = Path('out/report/index.html')
page = index.read_text(encoding='utf-8')
if '</body>' not in page:
    raise SystemExit('Cannot add report links: missing </body>')
repository = html.escape(os.environ['FORMATTER_REPOSITORY'], quote=True)
sha = html.escape(os.environ['FORMATTER_SHA'], quote=True)
sv_tests_sha = html.escape(subprocess.check_output(
    ['git', 'rev-parse', 'HEAD'], text=True).strip())
slang_version = html.escape(subprocess.check_output(
    ['slang-driver', '--version'], text=True).strip())
generated = datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M UTC')
links = (
    '<p style="padding: 1em">'
    '<a href="index.html">All reports</a> · '
    f'<a href="https://github.com/{repository}/commit/{sha}">Formatter {sha}</a><br>'
    f'sv-tests {sv_tests_sha} · {slang_version} · {generated}'
    '</p>\n'
)
index.write_text(page.replace('</body>', links + '</body>', 1), encoding='utf-8')
PY

# Keep the full sv-tests table alongside the new report selection page.
python3 "$GITHUB_WORKSPACE/formatter/scripts/prepare-sv-tests-home.py" out/report
