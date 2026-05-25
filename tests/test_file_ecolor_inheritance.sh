#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/file-ecolor-inheritance-test"
PROFILE="${WORK_DIR}/profile.the"
FIRST="${WORK_DIR}/first.txt"
SECOND="${WORK_DIR}/second.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping file ECOLOR inheritance test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping file ECOLOR inheritance test; THE was built without CREXX support" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
printf 'first\n' > "${FIRST}"
printf 'second\n' > "${SECOND}"

cat > "${PROFILE}" <<PROFILE_EOF
options levelb
import rxfnsb
address the

ecolor = .string[]
address the "set ecolor A bold on"
address the "extract /ecolor A/" expose ecolor[]
say "first=" || ecolor[1]
address the "edit ${SECOND}"
address the "extract /ecolor A/" expose ecolor[]
say "second=" || ecolor[1]
address the "cancel"
PROFILE_EOF

"${THE_BIN}" -b -q -p "${PROFILE}" "${FIRST}" > "${WORK_DIR}/out.txt" 2> "${WORK_DIR}/err.txt"

grep -q "first=A bold" "${WORK_DIR}/out.txt"
grep -q "second=A bold" "${WORK_DIR}/out.txt"
