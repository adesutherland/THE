#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the" >&2
  exit 2
fi

the_bin=$1
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
sample="$work_dir/sample.txt"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"

printf 'alpha\nbeta\ngamma\n' > "$sample"
printf '%s\n' \
  'hit prefix 2 11 0' \
  'text d' \
  'look prefix compact max=120' \
  'key enter' \
  'look full compact max=120' \
  'quit' |
  TERM= THE_HOME_DIR="$(dirname "$the_bin")/release" \
    "$the_bin" --driver llm -n "$sample" >"$out" 2>"$err"

rg '"status":"hit applied".*"zone":"prefix","line":2,"row":11,"cell":0' "$out" >/dev/null
rg '"status":"text applied".*"zone":"prefix","line":2,"row":11,"cell":1' "$out" >/dev/null
rg '"mode":"prefix".*"line":2,"cur":1,"p":"d","t":"beta"' "$out" >/dev/null
rg '"status":"key applied".*"dirty":1,"lines":2' "$out" >/dev/null
rg '"mode":"full".*"t":"alpha".*"t":"gamma"' "$out" >/dev/null
if rg '"mode":"full".*"t":"beta"' "$out" >/dev/null; then
  echo "prefix delete did not remove the selected line" >&2
  exit 1
fi
