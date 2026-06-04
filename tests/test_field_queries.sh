#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/field-query-test"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"
MODE="${1:-ascii}"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping field query test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping field query test; THE was built without CREXX" >&2
  exit 77
fi

if ! command -v script >/dev/null 2>&1; then
  echo "Skipping field query test; script(1) is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

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

if [[ "${MODE}" == "--utf" ]]; then
  printf 'A👩‍💻B beta\n' > "${SAMPLE}"
  cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the

'cursor file 1 2'
field = .string[]
address the "extract /field/" expose field[]
say "UTF_FIELD=" || field[2] || ":" || field[3] || ":" || field[4]

'qquit'
PROFILE_EOF

  run_with_pty "${THE_BIN}" -p "${PROFILE}" "${SAMPLE}"
  grep -q "UTF_FIELD=👩‍💻:2:TEXT" "${WORK_DIR}/combined.txt"
else
  printf 'alpha beta gamma\n' > "${SAMPLE}"
  cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the

'cursor file 1 8'
field = .string[]
address the "extract /field/" expose field[]
say "ASCII_FIELD=" || field[2] || ":" || field[3] || ":" || field[4]

fieldword = .string[]
address the "extract /fieldword/" expose fieldword[]
say "ASCII_FIELDWORD=" || fieldword[1] || ":" || fieldword[2] || ":" || fieldword[3]

hexdisplay = .string[]
address the "extract /hexdisplay/" expose hexdisplay[]
say "HEXDISPLAY_DEFAULT=" || hexdisplay[0] || ":" || hexdisplay[1] || ":" || hexdisplay[2]

address the "set hexdisplay chars"
address the "extract /hexdisplay/" expose hexdisplay[]
say "HEXDISPLAY_CHARS=" || hexdisplay[0] || ":" || hexdisplay[1] || ":" || hexdisplay[2]

address the "set hexdisplay codes"
address the "extract /hexdisplay/" expose hexdisplay[]
say "HEXDISPLAY_CODES=" || hexdisplay[0] || ":" || hexdisplay[1] || ":" || hexdisplay[2]

address the "set hexdisplay off"
address the "extract /hexdisplay/" expose hexdisplay[]
say "HEXDISPLAY_OFF=" || hexdisplay[0] || ":" || hexdisplay[1] || ":" || hexdisplay[2]

address the "set hexdisplay on"
address the "extract /hexdisplay/" expose hexdisplay[]
say "HEXDISPLAY_ON=" || hexdisplay[0] || ":" || hexdisplay[1] || ":" || hexdisplay[2]

'qquit'
PROFILE_EOF

  run_with_pty "${THE_BIN}" -p "${PROFILE}" "${SAMPLE}"
  grep -q "ASCII_FIELD=e:8:TEXT" "${WORK_DIR}/combined.txt"
  grep -q "ASCII_FIELDWORD=beta:beta:7" "${WORK_DIR}/combined.txt"
  grep -q "HEXDISPLAY_DEFAULT=2:ON:BOTH" "${WORK_DIR}/combined.txt"
  grep -q "HEXDISPLAY_CHARS=2:ON:CHARS" "${WORK_DIR}/combined.txt"
  grep -q "HEXDISPLAY_CODES=2:ON:CODES" "${WORK_DIR}/combined.txt"
  grep -q "HEXDISPLAY_OFF=2:OFF:CODES" "${WORK_DIR}/combined.txt"
  grep -q "HEXDISPLAY_ON=2:ON:BOTH" "${WORK_DIR}/combined.txt"
fi

echo "Field query test passed (${MODE})."
