#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/sos-logical-edit-query-test"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping SOS logical edit query test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping SOS logical edit query test; THE was built without CREXX" >&2
  exit 77
fi

if ! command -v script >/dev/null 2>&1; then
  echo "Skipping SOS logical edit query test; script(1) is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
cat > "${SAMPLE}" <<'SAMPLE_EOF'
alpha beta gamma
delta epsilon
SAMPLE_EOF

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the

'set tabs 1 5 9';
'cursor cmdline 1';
'text abcde';
'cursor cmdline 3';
'sos delchar';
field = .string[]
address the "extract /field/" expose field[]
say "CMD_DELCHAR=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'sos delback';
field = .string[]
address the "extract /field/" expose field[]
say "CMD_DELBACK=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'sos delend';
field = .string[]
address the "extract /field/" expose field[]
say "CMD_DELEND=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'sos qcmnd';
'text tabtest';
'cursor cmdline 2';
'sos tabf';
field = .string[]
address the "extract /field/" expose field[]
say "CMD_TABF=" || field[3] || ":" || field[4]

'sos tabb';
field = .string[]
address the "extract /field/" expose field[]
say "CMD_TABB=" || field[3] || ":" || field[4]

'cursor file 1 1';
'sos tabwordf';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_TABWORDF=" || field[2] || ":" || field[3] || ":" || field[4]

'sos tabwordf';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_TABWORDF2=" || field[2] || ":" || field[3] || ":" || field[4]

'sos tabwordb';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_TABWORDB=" || field[2] || ":" || field[3] || ":" || field[4]

'sos delword';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_DELWORD=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'cursor file 2 4';
'sos cursoradj';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_CURSORADJ=" || field[2] || ":" || field[3] || ":" || field[4]

'cursor file 2 2';
'sos cursorshift';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_CURSORSHIFT=" || field[2] || ":" || field[3] || ":" || field[4]

'cursor file 2 2';
'sos firstcol';
'sos tabf';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_SETTAB_TABF=" || field[3] || ":" || field[4]

'cursor file 2 2';
'sos instab';
field = .string[]
address the "extract /field/" expose field[]
say "FILE_INSTAB=" || field[3] || ":" || field[4]

'set prefix on';
'cursor file 2 1';
'sos prefix';
'text abc';
'sos firstcol';
'sos delchar';
field = .string[]
address the "extract /field/" expose field[]
say "PREFIX_DELCHAR=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'sos endchar';
'sos delback';
field = .string[]
address the "extract /field/" expose field[]
say "PREFIX_DELBACK=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'qquit';
PROFILE_EOF

run_with_pty() {
  local transcript="${WORK_DIR}/typescript"
  local stdout="${WORK_DIR}/stdout.txt"
  local stderr="${WORK_DIR}/stderr.txt"

  if script -q "${WORK_DIR}/probe" true >/dev/null 2>&1; then
    TERM="${TERM:-xterm}" script -q "${transcript}" "$@" >"${stdout}" 2>"${stderr}"
  else
    local quoted=""
    printf -v quoted "%q " "$@"
    TERM="${TERM:-xterm}" script -q -c "${quoted}" "${transcript}" >"${stdout}" 2>"${stderr}"
  fi
  cat "${stdout}" "${stderr}" "${transcript}" > "${WORK_DIR}/combined.txt"
}

run_with_pty "${THE_BIN}" -p "${PROFILE}" "${SAMPLE}"

grep -q "CMD_DELCHAR=abde:d:3:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "CMD_DELBACK=ade:d:2:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "CMD_DELEND=a::2:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "CMD_TABF=5:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "CMD_TABB=1:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "FILE_TABWORDF=b:7:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_TABWORDF2=g:12:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_TABWORDB=b:7:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_DELWORD=alpha gamma:g:7:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_CURSORADJ=d:4:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_CURSORSHIFT=d:2:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_SETTAB_TABF=5:TEXT" "${WORK_DIR}/combined.txt"
grep -q "FILE_INSTAB=5:TEXT" "${WORK_DIR}/combined.txt"
grep -q "PREFIX_DELCHAR=bc:b:1:PREFIX" "${WORK_DIR}/combined.txt"
grep -q "PREFIX_DELBACK=b::2:PREFIX" "${WORK_DIR}/combined.txt"

echo "SOS logical edit query test passed."
