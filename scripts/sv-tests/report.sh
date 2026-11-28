#!/usr/bin/env bash
# Run from the root of the pinned sv-tests checkout.
set -euo pipefail

generators_dir=${1:?Pass the directory containing the two report generators}
: "${FORMATTER_SHA:?Set FORMATTER_SHA to the tested formatter commit}"
: "${FORMATTER_REPOSITORY:?Set FORMATTER_REPOSITORY to owner/repository}"
formatter_dir="$GITHUB_WORKSPACE/formatter"
snapshot="${RUNNER_TEMP:-out}/formatter-history-snapshot.json"

command -v verihogg-format
command -v slang-driver
slang-driver --version

# The runner embeds its URL into every test log. Its default is the gitlink
# recorded by sv-tests, not necessarily the formatter binary being tested.
python3 - <<'PY'
from pathlib import Path

runner = Path('tools/runners/VerihoggFormat.py')
source = runner.read_text(encoding='utf-8')
old = 'self.url = f"https://github.com/verihogg/verihogg-format/tree/{self.get_commit()}"'
new = 'self.url = "https://github.com/{}/tree/{}".format(os.environ["FORMATTER_REPOSITORY"], os.environ["FORMATTER_SHA"])'
if source.count(old) != 1:
    raise SystemExit('Unexpected VerihoggFormat runner URL; review the pinned runner')
source = source.replace(old, new)
# The formatter lexes each file directly and does not accept -I or -D.
# Keep include paths and defines for the separate Slang CST comparison.
old_args = '''        fmt_cmd += include_args(formatted_dir)
        fmt_cmd += [f"-D{d}" for d in params['defines']]
'''
if source.count(old_args) != 1:
    raise SystemExit('Unexpected VerihoggFormat arguments; review the pinned runner')
source = source.replace(old_args, '')
# Newer formatter versions print an aggregated summary instead of individual
# Warning lines. Keep both formats so warning statuses remain meaningful.
old_warnings = '''        warning_count = len(re.findall(r"^Warning(?: in\\b|:)", fmt_log,
                                       re.MULTILINE))'''
new_warnings = '''        warning_count = len(re.findall(r"^Warning(?: in\\b|:)", fmt_log,
                                       re.MULTILINE))
        if "The formatter does not support these constructs yet" in fmt_log:
            warning_count += sum(int(count) for count in re.findall(
                r"^  - .+?\\s{2,}(\\d+) occurrences?,\\s+first at ",
                fmt_log, re.MULTILINE))'''
if source.count(old_warnings) != 1:
    raise SystemExit('Unexpected VerihoggFormat warning handling; review the pinned runner')
runner.write_text(source.replace(old_warnings, new_warnings), encoding='utf-8')
PY
make info

# A successful exit code alone is insufficient: the CLI can return zero when
# rejecting arguments. Exercise the real runner and verify changed output.
python3 "$GITHUB_WORKSPACE/formatter/scripts/sv-tests-runner-smoke.py"

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
PYTHONPATH="$generators_dir:$formatter_dir/scripts/sv-tests" python3 - "$generators_dir" "$formatter_dir" "$snapshot" <<'PY'
import hashlib
import os
from pathlib import Path
import subprocess
import sys
from sv_tests_unsupported_report import load_tests
from sv_tests_warning_constructs import render_page
from history import make_snapshot, write_json

root = Path('out/report').resolve()
tests = load_tests(root)
if not tests:
    raise SystemExit('No VerihoggFormat tests in report')
generators, formatter, snapshot_path = map(Path, sys.argv[1:])
sha = os.environ['FORMATTER_SHA']
subject, date = subprocess.check_output(
    ['git', '-C', formatter, 'show', '-s', '--format=%s%n%cI', sha], text=True
).splitlines()
config = {
    'sv_tests': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'slang': subprocess.check_output(['slang-driver', '--version'], text=True).strip(),
    'diagnostics': hashlib.sha256((generators / 'sv_tests_unsupported_report.py').read_bytes()).hexdigest(),
}
write_json(snapshot_path, make_snapshot(tests, sha, subject, date, config))

output = root / 'constructs.html'
if any(test.status == 'warning' for test in tests):
    subprocess.run([sys.executable, str(generators / 'sv_tests_warning_constructs.py'),
                    '--report', str(root)],
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
python3 "$formatter_dir/scripts/sv-tests/prepare-home.py" out/report
HISTORY_REMOTE= bash "$formatter_dir/scripts/sv-tests/publish-history.sh" "$formatter_dir" out/report "$snapshot"
