#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${1:-${THE_BIN:-${BUILD_DIR}/the}}"
WORK_DIR="${BUILD_DIR}/curses-text-entry-test"
PROFILE="${WORK_DIR}/profile.the"
SAMPLE="${WORK_DIR}/sample.txt"
EXPECTED="${WORK_DIR}/expected.txt"
NEW_SAMPLE="${WORK_DIR}/new-file.crexx"
NEW_EXPECTED="${WORK_DIR}/new-file-expected.txt"

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping curses text entry test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping curses text entry test; THE was built without CREXX" >&2
  exit 77
fi

if ! command -v script >/dev/null 2>&1; then
  echo "Skipping curses text entry test; script(1) is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
printf 'seed\n' > "${SAMPLE}"
printf 'abcd\n' > "${EXPECTED}"
printf 'abc\n' > "${NEW_EXPECTED}"

cat > "${PROFILE}" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the
'set linend on #';
'define C-F cursor file 0 1';
'define C-G file';
'cursor file 1 1';
PROFILE_EOF

run_with_pty() {
  local name="$1"
  local target="$2"
  local input_prefix="${3:-}"
  local transcript="${WORK_DIR}/${name}.typescript"
  local stdout="${WORK_DIR}/${name}.stdout.txt"
  local stderr="${WORK_DIR}/${name}.stderr.txt"
  local quoted=""
  local the_home

  the_home="$(dirname "${THE_BIN}")/release"
  if [[ ! -d "${the_home}" ]]; then
    the_home="$(dirname "${THE_BIN}")"
  fi
  printf -v quoted "%q " "${THE_BIN}" -p "${PROFILE}" "${target}"

  if script -q -e -c "true" "${WORK_DIR}/probe" >/dev/null 2>&1; then
    (
      sleep 0.5
      printf '%babc\007' "${input_prefix}"
    ) | TERM="${TERM:-xterm-256color}" THE_HOME_DIR="${the_home}" \
      script -q -e -c "${quoted}" "${transcript}" >"${stdout}" 2>"${stderr}"
  elif script -q "${WORK_DIR}/probe" true >/dev/null 2>&1; then
    (
      sleep 0.5
      printf '%babc\007' "${input_prefix}"
    ) | TERM="${TERM:-xterm-256color}" THE_HOME_DIR="${the_home}" \
      script -q "${transcript}" "${THE_BIN}" -p "${PROFILE}" "${target}" \
        >"${stdout}" 2>"${stderr}"
  else
    echo "Skipping curses text entry test; unsupported script(1) interface" >&2
    exit 77
  fi
}

run_with_pty existing "${SAMPLE}"

if ! cmp "${EXPECTED}" "${SAMPLE}"; then
  echo "Curses text entry saved unexpected bytes:" >&2
  od -An -tx1 "${SAMPLE}" >&2
  cat "${WORK_DIR}/existing.stdout.txt" "${WORK_DIR}/existing.stderr.txt" \
      "${WORK_DIR}/existing.typescript" >&2
  exit 1
fi

run_with_pty new-file "${NEW_SAMPLE}" '\006'

if ! cmp "${NEW_EXPECTED}" "${NEW_SAMPLE}"; then
  echo "Curses new-file text entry saved unexpected bytes:" >&2
  od -An -tx1 "${NEW_SAMPLE}" >&2 || true
  cat "${WORK_DIR}/new-file.stdout.txt" "${WORK_DIR}/new-file.stderr.txt" \
      "${WORK_DIR}/new-file.typescript" >&2
  exit 1
fi

echo "Curses text entry test passed."
