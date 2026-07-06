#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-highlight-test"
HIGHLIGHTER="${TOOL_DIR}/highlight.crexx"
VALID_SAMPLE="${SCRIPT_DIR}/fixtures/highlight-valid.rexx"
INVALID_SAMPLE="${SCRIPT_DIR}/fixtures/highlight-invalid.rexx"
THE_BIN="${THE_BIN:-${BUILD_DIR}/the}"
THE_HOME="${THE_HOME_DIR:-${BUILD_DIR}/release}"
CREXX="${CREXX:-${THE_CREXX:-}}"
RXC="${THE_CREXX_RXC:-}"

if [[ -z "${CREXX}" ]]; then
  if command -v crexx >/dev/null 2>&1; then
    CREXX="$(command -v crexx)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/crexx" ]]; then
    CREXX="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/crexx"
  fi
fi

if [[ -z "${RXC}" ]]; then
  if command -v rxc >/dev/null 2>&1; then
    RXC="$(command -v rxc)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc" ]]; then
    RXC="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc"
  fi
fi

if [[ -z "${CREXX}" || ! -x "${CREXX}" ]]; then
  echo "Skipping batch Markdown REXX highlight test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX highlight test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX highlight test; THE build output is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
# Expected diagnostic failures can leave CREXX intermediates, so run a
# disposable copy of the prototype.
HIGHLIGHTER_UNDER_TEST="${WORK_DIR}/highlight.crexx"
cp "${HIGHLIGHTER}" "${HIGHLIGHTER_UNDER_TEST}"

fail() {
  echo "$*" >&2
  exit 1
}

run_crexx_capture() {
  local name="$1"
  shift

  set +e
  "${CREXX}" -nokeep "$@" > "${WORK_DIR}/${name}.out" 2> "${WORK_DIR}/${name}.err"
  local status=$?
  set -e

  printf '%s\n' "${status}" > "${WORK_DIR}/${name}.rc"
}

assert_rc() {
  local name="$1"
  local expected="$2"
  local actual

  actual="$(cat "${WORK_DIR}/${name}.rc")"
  [[ "${actual}" == "${expected}" ]] || fail "${name}: expected rc ${expected}, got ${actual}"
}

assert_empty_stdout() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.out" ]] || fail "${name}: expected stdout to be empty"
}

assert_empty_stderr() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.err" ]] || fail "${name}: expected stderr to be empty"
}

assert_rg() {
  local pattern="$1"
  local file="$2"

  sed 's/\r$//' "${file}" | rg "${pattern}" >/dev/null
}

run_crexx_capture valid \
  "${HIGHLIGHTER_UNDER_TEST}" \
  -args \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  "${VALID_SAMPLE}"

assert_rc valid 0
assert_empty_stderr valid
assert_rg 'highlight source=.*highlight-valid\.rexx parser=rxc sdslhwait=5000 diagnostics=0' "${WORK_DIR}/valid.out"
assert_rg 'span line=1 start=0 len=7 style=preprocessor' "${WORK_DIR}/valid.out"
assert_rg 'span line=1 start=8 len=6 style=identifier' "${WORK_DIR}/valid.out"
assert_rg 'span line=2 start=0 len=3 style=keyword' "${WORK_DIR}/valid.out"
assert_rg 'span line=2 start=4 len=7 style=string' "${WORK_DIR}/valid.out"
assert_rg 'span line=3 start=0 len=13 style=comment' "${WORK_DIR}/valid.out"
assert_rg 'span line=4 start=0 len=7 style=keyword' "${WORK_DIR}/valid.out"
assert_rg 'span line=4 start=16 len=9 style=string' "${WORK_DIR}/valid.out"

run_crexx_capture invalid \
  "${HIGHLIGHTER_UNDER_TEST}" \
  -args \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  "${INVALID_SAMPLE}"

assert_rc invalid 1
assert_empty_stdout invalid
assert_rg 'highlight\.crexx: parser diagnostics present' "${WORK_DIR}/invalid.err"
assert_rg 'SYNTAX_ERROR' "${WORK_DIR}/invalid.err"

run_crexx_capture invalid-allowed \
  "${HIGHLIGHTER_UNDER_TEST}" \
  -args \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --fail-on-diagnostics false \
  "${INVALID_SAMPLE}"

assert_rc invalid-allowed 0
assert_empty_stderr invalid-allowed
assert_rg 'highlight source=.*highlight-invalid\.rexx parser=rxc sdslhwait=5000 diagnostics=1' "${WORK_DIR}/invalid-allowed.out"
assert_rg 'span line=1 start=0 len=7 style=preprocessor' "${WORK_DIR}/invalid-allowed.out"

assert_rg 'command sdslhwait ' "${HIGHLIGHTER}"

echo "Batch Markdown REXX highlight prototype test passed."
