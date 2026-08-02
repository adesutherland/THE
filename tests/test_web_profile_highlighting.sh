#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 /path/to/the /path/to/web-profile.the" >&2
  exit 2
fi

the_bin=$1
web_profile=$2
parser=${HOME}/.local/bin/rxc

if [[ ! -x "$parser" ]]; then
  echo "rxc syntax highlighter is not installed; skipping" >&2
  exit 77
fi

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
sample="$work_dir/sample.crexx"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"

printf '/* This is a test */\nsay "hello"\n' > "$sample"
printf '%s\n' \
  'command sdslhwait 5000' \
  'look filearea compact max=120 prefix=0 command=0 status=0' \
  'quit' |
  TERM= THE_HOME_DIR="$(dirname "$the_bin")/release" \
    "$the_bin" --driver llm -p "$web_profile" "$sample" \
      >"$out" 2>"$err"

rg '"s":\[\[0,20,"comment"\]\]' "$out" >/dev/null
rg '"s":\[\[0,3,"keyword"\],\[4,7,"string"\]\]' "$out" >/dev/null
