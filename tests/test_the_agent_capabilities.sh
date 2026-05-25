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
debug capabilities
quit
CMDS

rg '"surface":"the_agent"' "$out" >/dev/null
rg '"command_dispatcher":"agent-subset"' "$out" >/dev/null
rg '"full_the_dispatcher":false' "$out" >/dev/null
rg '"sos_commands":false' "$out" >/dev/null
rg '"use_crexx_for":\["full THE command execution","SOS command behavior","macro/profile integration"\]' "$out" >/dev/null
rg '"ok":0,"status":"unsupported command"' "$out" >/dev/null
rg '"unsupported":\{"kind":"command","input":"sos topedge"' "$out" >/dev/null
rg '"capabilities_hint":"capabilities"' "$out" >/dev/null
rg '"ok":1,"status":"bye"' "$out" >/dev/null
