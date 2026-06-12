#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/stylespans-extract-test"
THE_HOME="${WORK_DIR}/home"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/stylespans.rexx"
RXC="${THE_CREXX_RXC:-}"

if [[ -z "${RXC}" ]]; then
  if command -v rxc >/dev/null 2>&1; then
    RXC="$(command -v rxc)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc" ]]; then
    RXC="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc"
  fi
fi

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping STYLESPANS extract test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping STYLESPANS extract test; THE was built without CREXX" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping STYLESPANS extract test; rxc is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${THE_HOME}"
cp "${SCRIPT_DIR}/fixtures/stylespans.rexx" "${SAMPLE}"

cat > "${PROFILE}" <<PROFILE_EOF
options levelb
import rxfnsb
address the

'set sdslh rxc ${RXC} --syntaxhighlight'
'set autocolor *.rexx rxc'
'set coloring on auto'
'sdslhwait 5000'

stylespans = .string[]
address the "extract /stylespans/" expose stylespans[]
span_count = stylespans[0]
'emsg STYLESPANS_COUNT=' || span_count
do i = 1 to span_count
  span_record = stylespans[i]
  'emsg STYLESPAN=' || span_record
end

address the "extract /stylespans 3 3/" expose stylespans[]
span_count = stylespans[0]
'emsg STYLESPANS_RANGE_COUNT=' || span_count
do i = 1 to span_count
  span_record = stylespans[i]
  'emsg STYLESPAN_RANGE=' || span_record
end

'set coloring off'
address the "extract /stylespans/" expose stylespans[]
span_count = stylespans[0]
'emsg STYLESPANS_OFF_COUNT=' || span_count

'qquit'
PROFILE_EOF

env THE_HOME_DIR="${THE_HOME}" \
  "${THE_BIN}" -b -q -p "${PROFILE}" "${SAMPLE}" \
  > "${WORK_DIR}/the.out" 2> "${WORK_DIR}/the.err"

rg 'STYLESPANS_COUNT=[1-9][0-9]*' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=1 0 7 preprocessor' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=1 8 6 identifier' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=2 0 3 keyword' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=2 4 [0-9]+ string' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=3 0 13 comment' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=4 0 7 keyword' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN=4 16 9 string' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPANS_RANGE_COUNT=1' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPAN_RANGE=3 0 13 comment' "${WORK_DIR}/the.err" >/dev/null
rg 'STYLESPANS_OFF_COUNT=0' "${WORK_DIR}/the.err" >/dev/null

echo "STYLESPANS extract test passed."
