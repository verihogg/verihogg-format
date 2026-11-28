#!/usr/bin/env bash
# Import the shared formatter build into a report job.
set -euo pipefail

closure=${1:?Pass the formatter closure directory}
test -s "$closure/formatter-root.txt"
test -s "$closure/formatter.closure.zst"
zstd -dc "$closure/formatter.closure.zst" | nix-store --import >/dev/null
root=$(cat "$closure/formatter-root.txt")
test -x "$root/bin/verihogg-format"
printf '%s\n' "$root/bin" >> "$GITHUB_PATH"
