#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the_agent" >&2
  exit 2
fi

exe=$1
if [[ ! -x "$exe" ]]; then
  echo "not executable: $exe" >&2
  exit 1
fi

work_dir=${TMPDIR:-/tmp}/the-agent-capabilities-$$
mkdir -p "$work_dir"
trap 'rm -rf "$work_dir"' EXIT

sample=$work_dir/sample.txt
out=$work_dir/out.jsonl

printf 'alpha\nbeta\n' > "$sample"

"$exe" --rows 6 --cols 80 "$sample" > "$out" <<'CMDS'
capabilities
command sos topedge
command sos delchar
command sos delword
command sos makecurr
debug capabilities
quit
CMDS

rg '"surface":"the_agent"' "$out" >/dev/null
rg '"command_dispatcher":"agent-subset"' "$out" >/dev/null
rg '"full_the_dispatcher":false' "$out" >/dev/null
rg '"sos_commands":"navigation-and-edit-subset"' "$out" >/dev/null
rg '"mouse":"logical-hit-subset"' "$out" >/dev/null
rg '"prefix_commands":"agent-editing-subset"' "$out" >/dev/null
rg '"selection":true' "$out" >/dev/null
rg '"undo_redo":true' "$out" >/dev/null
rg '"buffers":true' "$out" >/dev/null
rg '"project_files":true' "$out" >/dev/null
rg '"delta_views":true' "$out" >/dev/null
rg '"transient_ui":true' "$out" >/dev/null
rg '"build_test_hooks":"external-shell-or-ctest"' "$out" >/dev/null
rg '"supported_sos_commands":\["topedge","bottomedge","leftedge","rightedge","firstcol","lastcol","endchar","firstchar","delchar","cuadelchar","delback","cuadelback","delend","delword","prefix","tabfieldf","tabfieldb","qcmnd","execute"\]' "$out" >/dev/null
rg '"find TEXT"' "$out" >/dev/null
rg '"replace /OLD/NEW/"' "$out" >/dev/null
rg '"appendline TEXT"' "$out" >/dev/null
rg '"open PATH"' "$out" >/dev/null
rg '"supported_prefix_commands":\["d\|del\|delete","dup\|copy","r TEXT","i TEXT","a TEXT"\]' "$out" >/dev/null
rg '"transient_commands":\["transient readv \[TEXT\]"' "$out" >/dev/null
rg '"outside_llm_headless_target":\[' "$out" >/dev/null
rg '"name":"full THE command dispatcher"' "$out" >/dev/null
rg '"name":"CREXX macros"' "$out" >/dev/null
rg '"name":"build and test execution"' "$out" >/dev/null
rg '"ok":1,"status":"cursor moved"' "$out" >/dev/null
rg '"ok":1,"status":"deleted"' "$out" >/dev/null
rg '"ok":1,"status":"deleted word"' "$out" >/dev/null
rg '"ok":0,"status":"unsupported command"' "$out" >/dev/null
rg '"unsupported":\{"kind":"command","input":"sos makecurr"' "$out" >/dev/null
rg '"capabilities_hint":"capabilities"' "$out" >/dev/null
rg '"ok":1,"status":"bye"' "$out" >/dev/null
