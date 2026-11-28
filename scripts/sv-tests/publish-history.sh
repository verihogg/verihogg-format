#!/usr/bin/env bash
set -euo pipefail

formatter=${1:?Pass the formatter checkout}
report=${2:?Pass the generated report directory}
snapshot=${3:?Pass the current snapshot file}
remote=${HISTORY_REMOTE:-}
ref=refs/heads/formatter-history

archive=$(mktemp -d)
trap 'rm -rf -- "$archive"' EXIT

if [[ -n "$remote" ]]; then
  git -C "$archive" init --quiet --initial-branch=formatter-history
  if git -C "$archive" ls-remote --exit-code "$remote" "$ref" >/dev/null; then
    git -C "$archive" fetch --quiet --no-tags --depth=1 "$remote" "$ref"
    git -C "$archive" reset --quiet --hard FETCH_HEAD
  else
    status=$?
    # A new archive is expected to have no branch. Other errors must stop the build.
    if [[ "$status" != 2 ]]; then
      exit "$status"
    fi
  fi
fi

args=(--archive "$archive" --report "$report" --snapshot "$snapshot"
      --repository verihogg/verihogg-format)
if [[ "${4:-}" == --publish ]]; then
  : "${remote:?Set HISTORY_REMOTE when publishing}"
  args+=(--record-from "$formatter")
fi
python3 "$formatter/scripts/sv-tests/history_page.py" "${args[@]}"

if [[ "${4:-}" == --publish ]]; then
  git -C "$archive" add -- history.json snapshots
  if ! git -C "$archive" diff --cached --quiet; then
    git -C "$archive" -c user.name='github-actions[bot]' \
      -c user.email='41898282+github-actions[bot]@users.noreply.github.com' \
      commit --quiet -m "Record formatter support for $(git -C "$formatter" rev-parse --short HEAD)"
    git -C "$archive" push "$remote" "HEAD:$ref"
  fi
fi
