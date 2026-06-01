#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the_llm_harness" >&2
  exit 2
fi

exe=$1
work_dir=${TMPDIR:-/tmp}/the-llm-harness-script-$$
mkdir -p "$work_dir"
trap 'rm -rf "$work_dir"' EXIT

sample=$work_dir/sample.txt
other=$work_dir/other.txt
out=$work_dir/out.jsonl

cat > "$sample" <<'TEXT'
alpha beta gamma
A1️⃣B
omega
TEXT

cat > "$other" <<'TEXT'
one beta
two
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
command sos prefix
look focus compact prefix=0
command sos tabfieldf
look focus compact prefix=0
command sos tabfieldb
look focus compact prefix=0
command sos leftedge
command sos delchar
look filearea compact max=40
command sos delword
look filearea compact max=40
hit status 0 5 0
hit tabline 0 0 0
hit divider 0 3 0
hit window 0 2 0 0 0
quit
CMDS

"$exe" --rows 6 --cols 80 "$sample" >> "$out" <<CMDS
command open $other
look filearea compact max=40
command find beta
command replace-all beta theta
command insertline zero
command deleteline
command appendline harness appended
command save
look full compact max=40
quit
CMDS

"$exe" --rows 8 --cols 100 "$sample" >> "$out" <<CMDS
look full compact max=80
delta compact max=80
command prefix 2 r prefix row
look full compact max=80
command prefix-execute
look full compact max=80
command undo
look full compact max=80
command redo
look full compact max=80
command select 1 0 1 5
command selection-copy
look full compact max=80
command selection-replace ALPHA
look full compact max=80
command undo
look full compact max=80
command buffer-open $other
command appendline buffer two edit
command buffer-switch 0
look full compact max=80
command buffer-switch 1
command save
command project-list $work_dir
look full compact max=80
delta compact max=80
transient readv seed
transient look
transient text X
transient key enter
transient result
transient dialog modal
transient key tab
transient key enter
transient result
transient popup
transient key down
transient hit 2 8
transient result
quit
CMDS

rg '"mode":"filearea"' "$out" >/dev/null
rg '"mode":"focus"' "$out" >/dev/null
rg '"mode":"delta"' "$out" >/dev/null
rg '"baseline":1' "$out" >/dev/null
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
rg 'harness appended' "$out" >/dev/null
rg '"status":"file saved"' "$out" >/dev/null
rg 'one theta' "$out" >/dev/null
rg '"buffer":\{"path":"' "$out" >/dev/null
rg '"dirty":0' "$out" >/dev/null
rg 'BZ' "$out" >/dev/null
rg '"t":"alpha beta gamm"' "$out" >/dev/null
rg '"t":"lpha beta gamm"' "$out" >/dev/null
rg '"t":"beta gamm"' "$out" >/dev/null
rg '"status":"status hit"' "$out" >/dev/null
rg '"status":"tabline hit"' "$out" >/dev/null
rg '"status":"divider hit"' "$out" >/dev/null
rg '"status":"window selected"' "$out" >/dev/null
rg '"pc":"r prefix row"' "$out" >/dev/null
rg '"t":"prefix row"' "$out" >/dev/null
rg '"history":\{"undo":1,"redo":0\}' "$out" >/dev/null
rg '"history":\{"undo":0,"redo":1\}' "$out" >/dev/null
rg '"selection":\{"active":1' "$out" >/dev/null
rg '"clipboard":"alpha"' "$out" >/dev/null
rg 'ALPHA beta gamma' "$out" >/dev/null
rg '"buffers":\[' "$out" >/dev/null
rg '"current":1' "$out" >/dev/null
rg "\"project\":\\{\"root\":\"$work_dir\"" "$out" >/dev/null
rg '"kind":"readv"' "$out" >/dev/null
rg '"text":"seedX"' "$out" >/dev/null
rg '"kind":"dialog"' "$out" >/dev/null
rg '"action":"accept"' "$out" >/dev/null
rg '"kind":"popup"' "$out" >/dev/null
rg '"ok":1' "$out" >/dev/null
rg 'one theta' "$other" >/dev/null
rg 'harness appended' "$other" >/dev/null
rg 'buffer two edit' "$other" >/dev/null
