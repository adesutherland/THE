#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/the" >&2
  exit 2
fi

the_bin=$1
release_dir=$(cd "$(dirname "$the_bin")" && pwd)/release
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

sample="$work_dir/rgb.txt"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"

printf 'rgb\n' > "$sample"

printf '%s\n' \
  'command set color filearea #123456 on #654321' \
  'command query color filearea' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$sample" \
    >"$out" 2>"$err"

rg 'color FILEAREA #123456 on #654321' "$out" >/dev/null
if rg -q 'Error opening terminal|setupterm|initscr' "$out" "$err"; then
  echo "llm driver appeared to initialize curses" >&2
  cat "$out" >&2
  cat "$err" >&2
  exit 1
fi
