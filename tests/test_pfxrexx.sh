#!/usr/bin/env bash
set -euo pipefail
export THE_REXX_INTERPRETER=REGINA
the_bin=${1:?usage: test_pfxrexx.sh /path/to/the}
the_bin=$(cd "$(dirname "$the_bin")" && pwd)/$(basename "$the_bin")
release=$(dirname "$the_bin")/release
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

run() {
  local name=$1
  printf '%s\n' "${seed:-seed}" tail > "$work/$name.rexx"
  {
    printf '%s\n' 'command macro pfxsetup' 'command :1'
    cat
    printf '%s\n' quit
  } | TERM= THE_HOME_DIR="$release" THE_MACRO_PATH="$release" \
      THE_DRIVER_PATH="$release/drivers" \
      "$the_bin" --driver llm -n "$work/$name.rexx" \
      > "$work/$name.out" 2> "$work/$name.err"
  if rg -q 'REXX interpreter returned an error|Invalid SET command|Invalid prefix subcommand|Invalid operand|Func=' "$work/$name.err"; then
    cat "$work/$name.err" >&2; exit 1
  fi
}
require() { rg -q -- "$2" "$work/$1.out" "$work/$1.err" || { cat "$work/$1.out" "$work/$1.err" >&2; exit 1; }; }

# Real text entry, two attention events, and a saved result, not just command ACKs.
run do <<'INPUT'
command cursor goto 1 1
command cursor prefix
text DO
key enter
look filearea compact
text say 'hello'
key enter
look filearea compact
key enter
look filearea compact
command save
INPUT
require do '"focus":\{"zone":"filearea","line":3,"row":[0-9]+,"cell":3\}'
require do '"t":"   \."'
require do '"command":"RIC"'
require do '"pending_prefix":\{"count":0,"commands":\[\]\}'
printf "seed\ndo\n   say 'hello'\nend\ntail\n" > "$work/expected"
cmp "$work/expected" "$work/do.rexx"

run if <<'INPUT'
command set pending on IF
command sos doprefix
look filearea compact
INPUT
require if '"t":"if \. then do"'
require if '"t":"else do"'
require if '"lines":8'

run select <<'INPUT'
command set pending on SEL
command sos doprefix
look filearea compact
command sos doprefix
look filearea compact
INPUT
require select '"t":"select"'
require select '"t":" when \. then do"'
require select '"t":" otherwise do"'
require select '"lines":10'
require select '"pending_prefix":\{"count":0,"commands":\[\]\}'

seed=select run when <<'INPUT'
command set pending on WHEN
command sos doprefix
look filearea compact
INPUT
require when '"t":" when \. then do"'
require when '"focus":\{"zone":"filearea","line":2,"row":[0-9]+,"cell":6\}'

run count <<'INPUT'
command set pending on 3RI
command sos doprefix
look filearea compact
command sos doprefix
look filearea compact
INPUT
require count '"original":"RIC3"'
require count '"lines":5'
require count '"lines":2'
require count '"pending_prefix":\{"count":0,"commands":\[\]\}'

run config <<'INPUT'
command editv set pfxrexx_indent_do 5
command set pending on do
command sos doprefix
look filearea compact
INPUT
require config '"t":"     \."'
require config '"focus":\{"zone":"filearea","line":3,"row":[0-9]+,"cell":5\}'

run invalid <<'INPUT'
command set pending on DO0
command sos doprefix
look prefix compact
command sos doprefix
look prefix compact
INPUT
require invalid 'Expected a repetition count from 1 to 99'
require invalid '"p":"\?DO"'
require invalid '"pending_prefix":\{"count":0,"commands":\[\]\}'

# Installed Classic profile and generated terminal profile must both be usable.
printf '%s\n' capabilities quit | TERM= THE_HOME_DIR="$release" \
  THE_DRIVER_PATH="$release/drivers" "$the_bin" --driver llm \
  -p "$release/profile_regina.the" "$work/do.rexx" > "$work/profile.out" 2> "$work/profile.err"
rg -q '"macro_interpreter":"REGINA"' "$work/profile.out"
if rg -q 'Error |OPTIONS|Unable to' "$work/profile.err"; then cat "$work/profile.err" >&2; exit 1; fi
echo 'PFXREXX workflow checks passed (Regina)'
