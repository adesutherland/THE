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

if ! TP_BIN="$(find_tp)"; then
  echo "Skipping SDSLH integration test; tp parser executable is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
printf '// test comment\n' > "${SAMPLE}"
cat > "${PROFILE}" <<PROFILE_EOF
options levelb
address the
'set sdslh tp ${TP_BIN} -d'
'set autocolor *.toy tp'
'set coloring on auto'
'input // test comment'
'qquit'
PROFILE_EOF

(
  cd "${WORK_DIR}"
  rm -f parser.log editor.log editor_stderr.log the.out
  "${THE_BIN}" -b -p "${PROFILE}" "${SAMPLE}" > the.out 2> editor_stderr.log
)

if grep -q "base_load_initial_content" "${WORK_DIR}/parser.log"; then
  echo "Integration Test Passed: Handshake and initial load successful."
  exit 0
fi

echo "Integration Test Failed: Handshake not found in parser log." >&2
echo "--- editor stderr ---" >&2
cat "${WORK_DIR}/editor_stderr.log" >&2 || true
echo "--- parser log ---" >&2
cat "${WORK_DIR}/parser.log" >&2 || echo "No parser.log found" >&2
exit 1
