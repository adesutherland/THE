#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/message-history-test"
THE_HOME="${WORK_DIR}/home"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping message history test; THE build output is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${THE_HOME}"
printf 'message history smoke\n' > "${SAMPLE}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
address the

'msg MESSAGE_HISTORY_ONE'
'emsg MESSAGE_HISTORY_TWO'

messages = .string[]
address the "extract /messages/" expose messages[]
all_count = messages[0]
all_first = messages[1]
all_second = messages[2]

address the "extract /messages 1/" expose messages[]

if all_count = "2" then 'emsg MESSAGE_HISTORY_COUNT_OK'
if all_first = "MESSAGE_HISTORY_ONE" then 'emsg MESSAGE_HISTORY_FIRST_OK'
if all_second = "MESSAGE_HISTORY_TWO" then 'emsg MESSAGE_HISTORY_SECOND_OK'
if messages[0] = "1" then 'emsg MESSAGE_HISTORY_RECENT_COUNT_OK'
if messages[1] = "MESSAGE_HISTORY_TWO" then 'emsg MESSAGE_HISTORY_RECENT_OK'

'qquit'
PROFILE_EOF

env THE_HOME_DIR="${THE_HOME}" \
  "${THE_BIN}" -b -q -p "${PROFILE}" "${SAMPLE}" \
  > "${WORK_DIR}/the.out" 2> "${WORK_DIR}/the.err"

grep -q "MESSAGE_HISTORY_COUNT_OK" "${WORK_DIR}/the.err"
grep -q "MESSAGE_HISTORY_FIRST_OK" "${WORK_DIR}/the.err"
grep -q "MESSAGE_HISTORY_SECOND_OK" "${WORK_DIR}/the.err"
grep -q "MESSAGE_HISTORY_RECENT_COUNT_OK" "${WORK_DIR}/the.err"
grep -q "MESSAGE_HISTORY_RECENT_OK" "${WORK_DIR}/the.err"

echo "Message history extract test passed."
