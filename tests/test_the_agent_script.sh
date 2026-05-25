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
hit prefix 2 1 2
look focus compact
hit filearea 2 1 1
focus command
text goto 2
look focus compact prefix=0
key left
look focus compact prefix=0
key right
look focus compact prefix=0
hit command 0 4 6
key enter
look focus compact prefix=0
command end
type Z
look filearea compact max=40
command sos qcmnd
look focus compact prefix=0
key esc
command goto 2
command sos rightedge
look focus compact prefix=0
command sos leftedge
look focus compact prefix=0
command sos topedge
look focus compact prefix=0
command sos rightedge
command sos delback
look filearea compact max=40
command sos leftedge
command sos delchar
look filearea compact max=40
hit status 0 5 0
hit tabline 0 0 0
hit divider 0 3 0
hit window 0 2 0 0 0
quit
CMDS

rg '"mode":"filearea"' "$out" >/dev/null
rg '"mode":"focus"' "$out" >/dev/null
rg '"cell":1' "$out" >/dev/null
rg '"zone":"prefix"' "$out" >/dev/null
rg '"zone":"command"' "$out" >/dev/null
rg '"role":"command"' "$out" >/dev/null
rg '"line":1' "$out" >/dev/null
rg '"line":2' "$out" >/dev/null
rg '"cell":5' "$out" >/dev/null
rg '"cell":6' "$out" >/dev/null
rg '"cell":0' "$out" >/dev/null
rg '"cell":4' "$out" >/dev/null
rg 'A1' "$out" >/dev/null
rg 'BZ' "$out" >/dev/null
rg '"t":"alph"' "$out" >/dev/null
rg '"t":"lph"' "$out" >/dev/null
rg '"status":"status hit"' "$out" >/dev/null
rg '"status":"tabline hit"' "$out" >/dev/null
rg '"status":"divider hit"' "$out" >/dev/null
rg '"status":"window selected"' "$out" >/dev/null
rg '"ok":1' "$out" >/dev/null
