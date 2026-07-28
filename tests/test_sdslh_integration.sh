#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/sdslh-integration-test"
PROFILE="${WORK_DIR}/test_profile.the"
SAMPLE="${WORK_DIR}/test.toy"

find_tp() {
  local candidate

  for candidate in \
    "${THE_SDSLH_TP:-}" \
    "${HOME}/.local/bin/tp" \
    "${BUILD_DIR}/sdslh/toyparser/tp" \
    "${ROOT_DIR}/../DSL-Syntax-Highlighter/cmake-build-debug/toyparser/tp"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping SDSLH integration test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping SDSLH integration test; THE was built without CREXX support" >&2
  exit 77
fi

if ! TP_BIN="$(find_tp)"; then
  echo "Skipping SDSLH integration test; tp parser executable is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
printf 'int x = 1\n' > "${SAMPLE}"
cat > "${PROFILE}" <<PROFILE_EOF
options levelb
address the
'set sdslh tp ${TP_BIN} -d';
'set autocolor *.toy tp';
'set coloring on auto';
'sdslhwait 5000';
pmsgs = .string[]
address the "extract /pmsgs/" expose pmsgs[]
if pmsgs[0] = "1" then 'emsg SDSLH_PMSGS_COUNT_OK'
if pmsgs[1] = "2 1 ERROR - Expected semicolon after statement" then 'emsg SDSLH_PMSGS_ENTRY_OK';
'input // delayed delta comment';
'sdslhwait 5000';
'qquit';
PROFILE_EOF

(
  cd "${WORK_DIR}"
  rm -f parser.log editor.log editor_stderr.log the.out
  THE_SDSLH_LOG="${WORK_DIR}/editor.log" "${THE_BIN}" -b -p "${PROFILE}" "${SAMPLE}" > the.out 2> editor_stderr.log
)

PARSE_COUNT="$(grep -c "base_parse_buffer: starting parse" "${WORK_DIR}/parser.log" || true)"

if grep -q "base_load_initial_content" "${WORK_DIR}/parser.log" \
  && [[ "${PARSE_COUNT}" -ge 2 ]] \
  && grep -q "base_parse_buffer: starting parse, lines=3" "${WORK_DIR}/parser.log" \
  && grep -q "SDSLH_PMSGS_COUNT_OK" "${WORK_DIR}/editor_stderr.log" \
  && grep -q "SDSLH_PMSGS_ENTRY_OK" "${WORK_DIR}/editor_stderr.log"; then
  echo "Integration Test Passed: Handshake, diagnostics extract, initial load, and delayed delta parse successful."
  exit 0
fi

echo "Integration Test Failed: SDSLH initial load, diagnostics extract, or delayed delta parse not observed." >&2
echo "--- editor stderr ---" >&2
cat "${WORK_DIR}/editor_stderr.log" >&2 || true
echo "--- editor log ---" >&2
if [[ -f "${WORK_DIR}/editor.log" ]]; then
  cat "${WORK_DIR}/editor.log" >&2
else
  echo "No editor.log found" >&2
fi
echo "--- parser log ---" >&2
cat "${WORK_DIR}/parser.log" >&2 || echo "No parser.log found" >&2
exit 1
