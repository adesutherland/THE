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
WORK_DIR="${BUILD_DIR}/crexx-profile-test"
CACHE_DIR="${WORK_DIR}/cache"
PROFILE="${WORK_DIR}/crexx_profile.the"
SAMPLE="${WORK_DIR}/sample.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping CREXX profile test; THE build output is missing: ${THE_BIN}" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping CREXX profile test; THE was built without CREXX support" >&2
  exit 77
fi

if [[ ! -x "${RXC}" ]]; then
  echo "Skipping CREXX profile test; CREXX compiler is missing: ${RXC}" >&2
  exit 77
fi

if [[ ! -x "${RXAS}" ]]; then
  echo "Skipping CREXX profile test; CREXX assembler is missing: ${RXAS}" >&2
  exit 77
fi

if [[ ! -f "${CREXX_LIBRARY_RXBIN}" ]]; then
  echo "Skipping CREXX profile test; CREXX import library is missing: ${CREXX_LIBRARY_RXBIN}" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
/bin/cp "${SCRIPT_DIR}/crexx_profile.the" "${PROFILE}"
printf 'profile smoke\n' > "${SAMPLE}"

run_the() {
  local name="$1"
  shift

  env "$@" \
    THE_CREXX_RXC="${RXC}" \
    THE_CREXX_RXAS="${RXAS}" \
    THE_CREXX_IMPORT_DIR="${CREXX_BIN_DIR}" \
    THE_CREXX_LOCATION="${CREXX_BIN_DIR}" \
    THE_CREXX_LIBRARY_RXBIN="${CREXX_LIBRARY_RXBIN}" \
    CREXXSAA_CACHE_DIR="${CACHE_DIR}" \
    CREXXSAA_CACHE_TRACE=1 \
    "${THE_BIN}" -b -q -p "${PROFILE}" "${SAMPLE}" \
      > "${WORK_DIR}/${name}.out" 2> "${WORK_DIR}/${name}.err"
}

run_the first
grep -q "CREXX_PROFILE_HOSTED" "${WORK_DIR}/first.err"
grep -q "CREXX_EXPOSE_FILENAME" "${WORK_DIR}/first.err"
grep -q "CREXX_EXPOSE_SCALAR_STEM_ALIAS" "${WORK_DIR}/first.err"
grep -q "CREXX_EDITV_EXPOSE_ROUNDTRIP" "${WORK_DIR}/first.err"
grep -q "CREXX_FILECTLCHAR_ON" "${WORK_DIR}/first.err"
grep -q "CREXX_EDITV_SCALAR_STEM_ALIAS" "${WORK_DIR}/first.err"
grep -q "CREXX_VALIDTARGET_BASIC" "${WORK_DIR}/first.err"
grep -q "CREXX_VALIDTARGET_SPARE" "${WORK_DIR}/first.err"
grep -q "CREXX_VALIDTARGET_NOTFOUND" "${WORK_DIR}/first.err"
grep -q "CREXX_SANDBOX_FILENAME" "${WORK_DIR}/first.err"
grep -q "CREXX_EDITV_SANDBOX_ROUNDTRIP" "${WORK_DIR}/first.err"
grep -q "CREXX_INPUTSTEM_ROUNDTRIP" "${WORK_DIR}/first.err"
grep -q "CREXXSAA cache miss:" "${WORK_DIR}/first.err"
printf 'alpha\n  beta # raw\ngamma\n' > "${WORK_DIR}/expected.txt"
cmp "${WORK_DIR}/expected.txt" "${SAMPLE}"

if ! find "${CACHE_DIR}" -name '*.rxbin' | grep -q .; then
  echo "Expected CREXXSAA cache to contain a compiled rxbin" >&2
  exit 1
fi

run_the second
grep -q "CREXX_PROFILE_HOSTED" "${WORK_DIR}/second.err"
grep -q "CREXX_EXPOSE_SCALAR_STEM_ALIAS" "${WORK_DIR}/second.err"
grep -q "CREXX_EDITV_EXPOSE_ROUNDTRIP" "${WORK_DIR}/second.err"
grep -q "CREXX_FILECTLCHAR_ON" "${WORK_DIR}/second.err"
grep -q "CREXX_EDITV_SCALAR_STEM_ALIAS" "${WORK_DIR}/second.err"
grep -q "CREXX_VALIDTARGET_BASIC" "${WORK_DIR}/second.err"
grep -q "CREXX_VALIDTARGET_SPARE" "${WORK_DIR}/second.err"
grep -q "CREXX_VALIDTARGET_NOTFOUND" "${WORK_DIR}/second.err"
grep -q "CREXX_EDITV_SANDBOX_ROUNDTRIP" "${WORK_DIR}/second.err"
grep -q "CREXX_INPUTSTEM_ROUNDTRIP" "${WORK_DIR}/second.err"
grep -q "CREXXSAA cache hit:" "${WORK_DIR}/second.err"
cmp "${WORK_DIR}/expected.txt" "${SAMPLE}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
address the
'emsg CREXX_PROFILE_CHANGED'
'file'
PROFILE_EOF

run_the changed
grep -q "CREXX_PROFILE_CHANGED" "${WORK_DIR}/changed.err"
grep -q "CREXXSAA cache stale:" "${WORK_DIR}/changed.err"

run_the refresh CREXXSAA_CACHE_REFRESH=1
grep -q "CREXX_PROFILE_CHANGED" "${WORK_DIR}/refresh.err"
grep -q "CREXXSAA cache refresh:" "${WORK_DIR}/refresh.err"
