#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/normal-area-query-test"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping normal area query test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping normal area query test; THE was built without CREXX" >&2
  exit 77
fi

if ! command -v script >/dev/null 2>&1; then
  echo "Skipping normal area query test; script(1) is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
printf 'one\ntwo\n' > "${SAMPLE}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the

'cursor cmdline 1'
'text abc'
field = .string[]
address the "extract /field/" expose field[]
say "CMD_FIELD=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'cursor cmdline 2'
'text Z'
field = .string[]
address the "extract /field/" expose field[]
say "CMD_FIELD2=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

'set prefix on'
'cursor file 1 1'
'sos prefix'
'text ab'
field = .string[]
address the "extract /field/" expose field[]
say "PREFIX_FIELD=" || field[1] || ":" || field[2] || ":" || field[3] || ":" || field[4]

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

grep -q "CMD_FIELD=abc::4:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "CMD_FIELD2=aZc:c:3:COMMAND" "${WORK_DIR}/combined.txt"
grep -q "PREFIX_FIELD=ab::3:PREFIX" "${WORK_DIR}/combined.txt"

echo "Normal area query test passed."
