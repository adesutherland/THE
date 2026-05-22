#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the_agent" >&2
  exit 2
fi

exe=$1
work_dir=${TMPDIR:-/tmp}/the-agent-script-$$
mkdir -p "$work_dir"
trap 'rm -rf "$work_dir"' EXIT

sample=$work_dir/sample.txt
out=$work_dir/out.jsonl

cat > "$sample" <<'TEXT'
alpha
A1️⃣B
omega
TEXT

"$exe" --rows 6 --cols 80 "$sample" > "$out" <<'CMDS'
look filearea compact max=40
key right
look focus compact prefix=0
command goto 2
command end
type Z
look filearea compact max=40
quit
CMDS

rg '"mode":"filearea"' "$out" >/dev/null
rg '"mode":"focus"' "$out" >/dev/null
rg '"cell":1' "$out" >/dev/null
rg 'A1' "$out" >/dev/null
rg 'BZ' "$out" >/dev/null
rg '"ok":1' "$out" >/dev/null
