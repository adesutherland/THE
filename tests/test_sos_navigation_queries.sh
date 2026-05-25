#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/sos-navigation-query-test"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping SOS navigation query test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping SOS navigation query test; THE was built without CREXX" >&2
  exit 77
fi

if ! command -v script >/dev/null 2>&1; then
  echo "Skipping SOS navigation query test; script(1) is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
for i in $(seq 1 40); do
  printf 'line %02d text\n' "${i}"
done > "${SAMPLE}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the

'cursor cmdline 5'
'sos topedge'
field = .string[]
address the "extract /field/" expose field[]
say "CMD_TOP_FIELD=" || field[3] || ":" || field[4]

'cursor cmdline 5'
'set prefix on right 6 0'
'sos topedge'
field = .string[]
address the "extract /field/" expose field[]
say "CMD_TOP_RIGHT_FIELD=" || field[3] || ":" || field[4]

'set prefix on left 6 0'
'cursor cmdline 5'
'sos bottomedge'
field = .string[]
address the "extract /field/" expose field[]
say "BOTTOM_FIELD=" || field[3] || ":" || field[4]

'sos topedge'
field = .string[]
address the "extract /field/" expose field[]
say "TOP_FIELD=" || field[3] || ":" || field[4]

'set prefix on'
'cursor file 2 3'
'sos prefix'
'text zz'
field = .string[]
address the "extract /field/" expose field[]
say "PREFIX_TEXT_FIELD=" || field[1] || ":" || field[3] || ":" || field[4]

'sos bottomedge'
field = .string[]
address the "extract /field/" expose field[]
say "PREFIX_BOTTOM_FIELD=" || field[3] || ":" || field[4]

'sos topedge'
field = .string[]
address the "extract /field/" expose field[]
say "PREFIX_TOP_FIELD=" || field[3] || ":" || field[4]

'sos leftedge'
field = .string[]
address the "extract /field/" expose field[]
say "LEFTEDGE_FIELD=" || field[3] || ":" || field[4]

'cursor file 6 4'
'sos makecurr'
field = .string[]
address the "extract /field/" expose field[]
say "MAKECURR_FIELD=" || field[2] || ":" || field[3] || ":" || field[4]

'qquit'
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

grep -q "CMD_TOP_FIELD=5:TEXT" "${WORK_DIR}/combined.txt"
grep -q "CMD_TOP_RIGHT_FIELD=11:TEXT" "${WORK_DIR}/combined.txt"
grep -q "BOTTOM_FIELD=5:TEXT" "${WORK_DIR}/combined.txt"
grep -q "TOP_FIELD=5:TEXT" "${WORK_DIR}/combined.txt"
grep -q "PREFIX_TEXT_FIELD=zz:3:PREFIX" "${WORK_DIR}/combined.txt"
grep -q "PREFIX_BOTTOM_FIELD=3:PREFIX" "${WORK_DIR}/combined.txt"
grep -q "PREFIX_TOP_FIELD=3:PREFIX" "${WORK_DIR}/combined.txt"
grep -q "LEFTEDGE_FIELD=1:TEXT" "${WORK_DIR}/combined.txt"
grep -q "MAKECURR_FIELD=e:4:TEXT" "${WORK_DIR}/combined.txt"

echo "SOS navigation query test passed."
