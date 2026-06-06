#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/system-profile-test"
THE_HOME="${WORK_DIR}/home"
USER_PROFILE="${WORK_DIR}/user_profile.the"
SAMPLE="${WORK_DIR}/sample.txt"

case "$(uname -s)" in
  Darwin) SYSTEM_PROFILE_NAME="system-osx.the" ;;
  Linux) SYSTEM_PROFILE_NAME="system-linux.the" ;;
  MINGW*|MSYS*|CYGWIN*) SYSTEM_PROFILE_NAME="system-windows.the" ;;
  *) SYSTEM_PROFILE_NAME="system-unix.the" ;;
esac

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping system profile test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping system profile test; THE was built without CREXX support" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${THE_HOME}"
printf 'system profile smoke\n' > "${SAMPLE}"

cat > "${THE_HOME}/${SYSTEM_PROFILE_NAME}" <<'PROFILE_EOF'
options levelb
address the
'set utf display normal class keycap output sanitize keycap metrics output width 1 advance 1 cursor 1 repaint 1'
'set utf display decomposed'
utf = .string[]
address the "extract /utf/" expose utf[]
if utf[1] = "ON" then 'emsg QUERY_UTF_ON'
if utf[2] = "DISPLAY DECOMPOSED" then 'emsg QUERY_UTF_MODE'
if utf[3] > 0 then 'emsg QUERY_UTF_RULES'
if utf[31] = "SET UTF DISPLAY NORMAL CLASS keycap OUTPUT SANITIZE METRICS OUTPUT MARK NONE WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1 CURSORSTRATEGY FIRST REPLACESTRATEGY WHOLE" then 'emsg QUERY_UTF_RULE'
'emsg SYSTEM_PROFILE_RAN'
PROFILE_EOF

cat > "${USER_PROFILE}" <<'PROFILE_EOF'
options levelb
address the
'emsg USER_PROFILE_RAN'
'file'
PROFILE_EOF

env THE_HOME_DIR="${THE_HOME}" \
  "${THE_BIN}" -b -q -p "${USER_PROFILE}" "${SAMPLE}" \
  > "${WORK_DIR}/with-user.out" 2> "${WORK_DIR}/with-user.err"

grep -q "SYSTEM_PROFILE_RAN" "${WORK_DIR}/with-user.err"
grep -q "QUERY_UTF_ON" "${WORK_DIR}/with-user.err"
grep -q "QUERY_UTF_MODE" "${WORK_DIR}/with-user.err"
grep -q "QUERY_UTF_RULES" "${WORK_DIR}/with-user.err"
grep -q "QUERY_UTF_RULE" "${WORK_DIR}/with-user.err"
grep -q "USER_PROFILE_RAN" "${WORK_DIR}/with-user.err"

system_line="$(grep -n "SYSTEM_PROFILE_RAN" "${WORK_DIR}/with-user.err" | head -n 1 | cut -d: -f1)"
user_line="$(grep -n "USER_PROFILE_RAN" "${WORK_DIR}/with-user.err" | head -n 1 | cut -d: -f1)"
if [[ "${system_line}" -ge "${user_line}" ]]; then
  echo "System profile did not run before user profile" >&2
  exit 1
fi

env THE_HOME_DIR="${THE_HOME}" \
  "${THE_BIN}" -b -q -n -p "${USER_PROFILE}" "${SAMPLE}" \
  > "${WORK_DIR}/no-user.out" 2> "${WORK_DIR}/no-user.err"

grep -q "SYSTEM_PROFILE_RAN" "${WORK_DIR}/no-user.err"
grep -q "QUERY_UTF_ON" "${WORK_DIR}/no-user.err"
grep -q "QUERY_UTF_MODE" "${WORK_DIR}/no-user.err"
grep -q "QUERY_UTF_RULES" "${WORK_DIR}/no-user.err"
grep -q "QUERY_UTF_RULE" "${WORK_DIR}/no-user.err"
if grep -q "USER_PROFILE_RAN" "${WORK_DIR}/no-user.err"; then
  echo "-n should skip the user profile but keep the system profile" >&2
  exit 1
fi
