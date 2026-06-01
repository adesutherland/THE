#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the_llm_headless" >&2
  exit 2
fi

exe=$1
work_dir=${TMPDIR:-/tmp}/the-llm-headless-mini-$$
mkdir -p "$work_dir"
trap 'rm -rf "$work_dir"' EXIT

sample=$work_dir/sample.txt
out=$work_dir/out.txt

printf 'alpha beta gamma\n' > "$sample"

"$exe" --mini-session "$sample" > "$out"

rg 'the_llm_headless mini-session status: file saved' "$out" >/dev/null
rg 'alpha delta gamma' "$out" >/dev/null
rg 'edited by the_llm_headless' "$out" >/dev/null
rg '"buffer":\{"path":"' "$out" >/dev/null
rg '"dirty":0' "$out" >/dev/null
rg 'alpha delta gamma' "$sample" >/dev/null
rg 'edited by the_llm_headless' "$sample" >/dev/null
