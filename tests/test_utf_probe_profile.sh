#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
PROBE_BIN="${PROBE_BIN:-${BUILD_DIR}/utf_terminal_probe}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/utf-probe-profile-test"
PROFILE="${WORK_DIR}/system-profile.the"
THE_HOME="${WORK_DIR}/home"
SAMPLE="${WORK_DIR}/sample.txt"

case "$(uname -s)" in
  Darwin) SYSTEM_PROFILE_NAME="system-osx.the" ;;
  Linux) SYSTEM_PROFILE_NAME="system-linux.the" ;;
  MINGW*|MSYS*|CYGWIN*) SYSTEM_PROFILE_NAME="system-windows.the" ;;
  *) SYSTEM_PROFILE_NAME="system-unix.the" ;;
esac

if [[ ! -x "${PROBE_BIN}" || ! -x "${THE_BIN}" ]]; then
  echo "Skipping UTF probe profile test; build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping UTF probe profile test; THE was built without CREXX support" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${THE_HOME}"
printf 'probe profile smoke\n' > "${SAMPLE}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
address the
'SET UTF TERMINAL CLASS combining CURSORSTRATEGY suffix'
'SET UTF TERMINAL CLASS combining REPLACESTRATEGY prev'
'SET UTF TERMINAL CLASS keycap LAYOUT 2 CURSOR 2'
'SET UTF TERMINAL CLASS keycap CURSORSTRATEGY whole'
'SET UTF TERMINAL CLASS keycap REPLACESTRATEGY whole'
'SET UTF TERMINAL CLASS short-zwj DISPLAY components CURSORSTRATEGY suffix'
'SET UTF TERMINAL CLASS short-zwj DISPLAY components REPLACESTRATEGY prev'
PROFILE_EOF

TERM=xterm-256color TERM_PROGRAM=Apple_Terminal \
  "${PROBE_BIN}" calibrate all --no-visual --write-profile \
  --profile "${PROFILE}" > "${WORK_DIR}/probe.out" 2> "${WORK_DIR}/probe.err"

if grep -Eq "STRATEGY[[:space:]]*'$" "${PROFILE}"; then
  echo "Probe wrote a strategy command without a strategy value" >&2
  exit 1
fi

grep -q "'SET UTF TERMINAL CLASS combining CURSORSTRATEGY suffix'" "${PROFILE}"
grep -q "'SET UTF TERMINAL CLASS combining REPLACESTRATEGY prev'" "${PROFILE}"
grep -q "'SET UTF TERMINAL CLASS keycap CURSORSTRATEGY whole'" "${PROFILE}"
grep -q "'SET UTF TERMINAL CLASS keycap REPLACESTRATEGY whole'" "${PROFILE}"
grep -q "'SET UTF TERMINAL CLASS short-zwj DISPLAY components CURSORSTRATEGY suffix'" "${PROFILE}"
grep -q "'SET UTF TERMINAL CLASS short-zwj DISPLAY components REPLACESTRATEGY prev'" "${PROFILE}"

cp "${PROFILE}" "${THE_HOME}/${SYSTEM_PROFILE_NAME}"
env THE_HOME_DIR="${THE_HOME}" \
  "${THE_BIN}" -b -q -n "${SAMPLE}" \
  > "${WORK_DIR}/the.out" 2> "${WORK_DIR}/the.err"

if grep -q "Invalid operand" "${WORK_DIR}/the.err"; then
  cat "${WORK_DIR}/the.err" >&2
  exit 1
fi
