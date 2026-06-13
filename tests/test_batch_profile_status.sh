#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
CREXX_BIN_DIR="${CREXX_BIN_DIR:-${ROOT_DIR}/../CREXX/cmake-build-debug/bin}"
RXC="${THE_CREXX_RXC:-${CREXX_BIN_DIR}/rxc}"
RXAS="${THE_CREXX_RXAS:-${CREXX_BIN_DIR}/rxas}"
CREXX_LIBRARY_RXBIN="${THE_CREXX_LIBRARY_RXBIN:-${CREXX_BIN_DIR}/library.rxbin}"
WORK_DIR="${BUILD_DIR}/batch-profile-status-test"
THE_HOME="${WORK_DIR}/home"
SAMPLE="${WORK_DIR}/sample.txt"

if [[ ! -x "${RXC}" ]]; then
  if command -v rxc >/dev/null 2>&1; then
    RXC="$(command -v rxc)"
    CREXX_BIN_DIR="$(dirname "${RXC}")"
  fi
fi

if [[ ! -x "${RXAS}" ]]; then
  if [[ -x "${CREXX_BIN_DIR}/rxas" ]]; then
    RXAS="${CREXX_BIN_DIR}/rxas"
  elif command -v rxas >/dev/null 2>&1; then
    RXAS="$(command -v rxas)"
  fi
fi

if [[ ! -f "${CREXX_LIBRARY_RXBIN}" ]]; then
  if [[ -f "${CREXX_BIN_DIR}/library.rxbin" ]]; then
    CREXX_LIBRARY_RXBIN="${CREXX_BIN_DIR}/library.rxbin"
  fi
fi

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping batch profile status test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch profile status test; THE was built without CREXX support" >&2
  exit 77
fi

if [[ ! -x "${RXC}" ]]; then
  echo "Skipping batch profile status test; CREXX compiler is missing: ${RXC}" >&2
  exit 77
fi

if [[ ! -x "${RXAS}" ]]; then
  echo "Skipping batch profile status test; CREXX assembler is missing: ${RXAS}" >&2
  exit 77
fi

if [[ ! -f "${CREXX_LIBRARY_RXBIN}" ]]; then
  echo "Skipping batch profile status test; CREXX import library is missing: ${CREXX_LIBRARY_RXBIN}" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${THE_HOME}"
printf 'batch profile status\n' > "${SAMPLE}"

run_profile() {
  local name="$1"
  local expected_rc="$2"
  local profile="${WORK_DIR}/${name}.the"
  local actual_rc

  set +e
  env \
    THE_HOME_DIR="${THE_HOME}" \
    THE_CREXX_RXC="${RXC}" \
    THE_CREXX_RXAS="${RXAS}" \
    THE_CREXX_IMPORT_DIR="${CREXX_BIN_DIR}" \
    THE_CREXX_LOCATION="${CREXX_BIN_DIR}" \
    THE_CREXX_LIBRARY_RXBIN="${CREXX_LIBRARY_RXBIN}" \
    "${THE_BIN}" -b -q -p "${profile}" "${SAMPLE}" \
      > "${WORK_DIR}/${name}.out" 2> "${WORK_DIR}/${name}.err"
  actual_rc=$?
  set -e

  if [[ "${actual_rc}" != "${expected_rc}" ]]; then
    echo "${name}: expected rc ${expected_rc}, got ${actual_rc}" >&2
    echo "--- stderr ---" >&2
    sed -n '1,120p' "${WORK_DIR}/${name}.err" >&2
    exit 1
  fi
}

cat > "${WORK_DIR}/ok-close.the" <<'PROFILE_EOF'
options levelb
import rxfnsb

address the 'emsg BATCH_PROFILE_OK'
address the 'qquit'
exit 0
PROFILE_EOF

run_profile ok-close 0
grep -q "BATCH_PROFILE_OK" "${WORK_DIR}/ok-close.err"
if grep -q "Error 0077" "${WORK_DIR}/ok-close.err"; then
  echo "ok-close: did not expect files-open batch error" >&2
  exit 1
fi

cat > "${WORK_DIR}/exit7-close.the" <<'PROFILE_EOF'
options levelb
import rxfnsb

address the 'qquit'
call lineout "stderr", "BATCH_PROFILE_EXIT7_CLOSE"
exit 7
PROFILE_EOF

run_profile exit7-close 7
grep -q "BATCH_PROFILE_EXIT7_CLOSE" "${WORK_DIR}/exit7-close.err"
if grep -q "Error 0077" "${WORK_DIR}/exit7-close.err"; then
  echo "exit7-close: did not expect files-open batch error" >&2
  exit 1
fi

cat > "${WORK_DIR}/open-file.the" <<'PROFILE_EOF'
options levelb
import rxfnsb

call lineout "stderr", "BATCH_PROFILE_OPEN_FILE"
exit 0
PROFILE_EOF

run_profile open-file 77
grep -q "BATCH_PROFILE_OPEN_FILE" "${WORK_DIR}/open-file.err"
grep -q "Error 0077: Files still open in batch: 1" "${WORK_DIR}/open-file.err"

cat > "${WORK_DIR}/exit7-open.the" <<'PROFILE_EOF'
options levelb
import rxfnsb

call lineout "stderr", "BATCH_PROFILE_EXIT7_OPEN"
exit 7
PROFILE_EOF

run_profile exit7-open 7
grep -q "BATCH_PROFILE_EXIT7_OPEN" "${WORK_DIR}/exit7-open.err"
grep -q "Error 0077: Files still open in batch: 1" "${WORK_DIR}/exit7-open.err"

echo "Batch profile status test passed."
