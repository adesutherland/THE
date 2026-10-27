#!/usr/bin/env bash
set -euo pipefail

the_bin=${1:?usage: test_pending_runtime.sh /path/to/the CREXX|REGINA}
engine=${2:?missing interpreter}
export THE_REXX_INTERPRETER=$engine
case "$engine" in CREXX|REGINA) ;; *) exit 77 ;; esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
release=$(cd "$(dirname "$the_bin")/release" && pwd)
the_bin=$(cd "$(dirname "$the_bin")" && pwd)/$(basename "$the_bin")
mkdir "$work/home"

macro() {
  local name=$1
  if [[ $engine == CREXX ]]; then
    printf 'options levelb\naddress the\n' > "$work/$name.the"
  else
    printf 'address THE\n' > "$work/$name.the"
  fi
  # cREXX permits adjacent string literals; terminate host commands explicitly.
  sed "/^'/s/\$/;/" >> "$work/$name.the"
}

run() {
  local name=$1
  printf 'alpha\nbeta\ngamma\n' > "$work/$name.txt"
  {
    printf '%s\n' 'command set compat xedit xedit xedit' 'command set prefix on left 8 0'
    cat
    printf '%s\n' quit
  } | TERM= THE_HOME_DIR="$work/home" THE_MACRO_PATH="$work" \
      THE_DRIVER_PATH="$release/drivers" CREXXSAA_CACHE_DIR="$work/cache" \
      "$the_bin" --driver llm -n "$work/$name.txt" \
      > "$work/$name.out" 2> "$work/$name.err"
  if rg -q 'Invalid operand: THE_KEY_TAB|REXX interpreter returned an error|Unable to run CREXX macro' "$work/$name.err"; then
    cat "$work/$name.err" >&2
    exit 1
  fi
}

require() { rg -q -- "$2" "$work/$1.out" "$work/$1.err" || { cat "$work/$1.out" "$work/$1.err" >&2; exit 1; }; }
reject() { if rg -q -- "$2" "$work/$1.out" "$work/$1.err"; then cat "$work/$1.out" "$work/$1.err" >&2; exit 1; fi; }
before() {
  local first second
  first=$(rg -n "$2" "$work/$1.err" | head -1 | cut -d: -f1)
  second=$(rg -n "$3" "$work/$1.err" | head -1 | cut -d: -f1)
  [[ $first -lt $second ]] || { cat "$work/$1.err" >&2; exit 1; }
}

if [[ $engine == REGINA ]]; then
  macro query <<'MACRO'
'extract /pending oldname SHARED/'
if pending.0 = 7 then 'emsg FOUND' pending.1 pending.2 pending.3
else 'emsg EMPTY'
MACRO
else
  macro query <<'MACRO'
pool = .standardaddresssandbox()
address the 'extract /pending oldname SHARED/' sandbox pool
count = pool.get('pending.0')
line = pool.get('pending.1')
name = pool.get('pending.2')
original = pool.get('pending.3')
if count = '7' then 'emsg FOUND '||line||' '||name||' '||original
else 'emsg EMPTY'
MACRO
fi

run synonyms <<'INPUT'
command set prefix synonym AA SHARED
command set prefix synonym BB SHARED
command :2
command set pending on BB
command macro query
command set pending off
command macro query
look prefix compact
INPUT
require synonyms 'FOUND 2 BB SHARED'
require synonyms 'EMPTY'
require synonyms '"pending_prefix":\{"count":0,"commands":\[\]\}'
reject synonyms '"p":"BB"'

macro child <<'MACRO'
'emsg CHILD_RAN'
exit 0
MACRO
macro parent <<'MACRO'
'command :1 input inserted'
'command :2 set pending on CHILD'
'emsg PARENT_RAN'
exit 0
MACRO
run defer <<'INPUT'
command set prefix synonym PARENT parent
command set prefix synonym CHILD child
command :1
command set pending on PARENT
command sos doprefix
look prefix compact
command emsg FIRST_PASS_END
command sos doprefix
look prefix compact
INPUT
require defer 'PARENT_RAN'
require defer 'CHILD_RAN'
before defer FIRST_PASS_END CHILD_RAN
require defer '"command":"CHILD"'
require defer '"pending_prefix":\{"count":0,"commands":\[\]\}'

macro replace <<'MACRO'
'command :2 set pending on CHILD'
'emsg REPLACED'
exit 0
MACRO
macro stale <<'MACRO'
'emsg STALE_RAN'
exit 0
MACRO
run replace <<'INPUT'
command set prefix synonym PARENT replace
command set prefix synonym CHILD child
command set prefix synonym STALE stale
command :1
command set pending on PARENT
command :2
command set pending on STALE
command sos doprefix
command emsg FIRST_PASS_END
command sos doprefix
INPUT
reject replace STALE_RAN
before replace FIRST_PASS_END CHILD_RAN

macro resetter <<'MACRO'
'reset prefix'
'command :2 set pending on CHILD'
'sos doprefix'
'emsg RESET_RETURN'
exit 0
MACRO
run reset <<'INPUT'
command set prefix synonym PARENT resetter
command set prefix synonym CHILD child
command :1
command set pending on PARENT
command sos doprefix
command emsg FIRST_PASS_END
command sos doprefix
INPUT
require reset RESET_RETURN
before reset FIRST_PASS_END CHILD_RAN

macro self <<'MACRO'
'command :1 set pending on CHILD'
'emsg REQUEUED'
exit 0
MACRO
run self <<'INPUT'
command set prefix synonym PARENT self
command set prefix synonym CHILD child
command :1
command set pending on PARENT
command sos doprefix
command emsg FIRST_PASS_END
command sos doprefix
INPUT
before self FIRST_PASS_END CHILD_RAN

macro deleter <<'MACRO'
'command :1 delete 1'
'command :1 set pending on CHILD'
'emsg DELETE_RETURN'
exit 0
MACRO
run delete <<'INPUT'
command set prefix synonym PARENT deleter
command set prefix synonym CHILD child
command :1
command set pending on PARENT
command sos doprefix
command emsg FIRST_PASS_END
command sos doprefix
look filearea compact
INPUT
before delete FIRST_PASS_END CHILD_RAN
reject delete '"t":"alpha"'
require delete '"lines":2'

macro stopper <<'MACRO'
'emsg STOP_RETURN'
exit 7
MACRO
run stop <<'INPUT'
command set prefix synonym PARENT stopper
command set prefix synonym CHILD child
command :1
command set pending on PARENT
command :2
command set pending on CHILD
command sos doprefix
command emsg FIRST_PASS_END
command sos doprefix
INPUT
before stop FIRST_PASS_END CHILD_RAN

# A failed macro must also preserve a later command entered by typing, not
# just entries created with SET PENDING (which have a separate retention flag).
run stop_typed <<'INPUT'
command set prefix synonym AA stopper
command set prefix synonym BB child
command cursor goto 1 1
command cursor prefix
text AA
command cursor goto 2 1
command cursor prefix
text BB
key enter
command emsg FIRST_PASS_END
look prefix compact
command sos doprefix
INPUT
before stop_typed FIRST_PASS_END CHILD_RAN
require stop_typed '"original":"BB"'

run error <<'INPUT'
command :1
command set pending error XX20
look prefix compact
command macro query
command sos doprefix
look prefix compact
command :2
command set pending on d
command set pending off
command sos doprefix
look filearea compact
INPUT
require error '"p":"\?XX20"'
require error EMPTY
require error '"pending_prefix":\{"count":0,"commands":\[\]\}'
require error '"t":"alpha"'
require error '"t":"beta"'
if [[ $(rg -c '"p":"\?XX20"' "$work/error.out") != 1 ]]; then
  echo 'Error prefix did not clear on the following pass' >&2; exit 1
fi

run block <<'INPUT'
command :1
command set pending block DD
command :2
command set pending block DD
command sos doprefix
look filearea compact
INPUT
require block '"lines":1'
require block '"t":"gamma"'
reject block '"t":"alpha"|"t":"beta"'

run native <<'INPUT'
command set compat the the the
command set prefix synonym PARENT parent
command set prefix synonym CHILD child
command :1
command set pending on PARENT
command sos doprefix
command emsg FIRST_PASS_END
INPUT
before native CHILD_RAN FIRST_PASS_END
echo "Pending runtime checks passed ($engine)"
