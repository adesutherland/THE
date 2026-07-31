#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-validate-test"
RENDERER="${THE_BATCH_RENDERER_RXBIN:-${BUILD_DIR}/release/batch-md-rexx/render-html.rxbin}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
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
  echo "Skipping batch Markdown REXX validate test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX validate test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX validate test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown REXX validate test; THE was built without CREXX" >&2
  exit 77
fi

if [[ "${RENDERER}" != *.rxbin || ! -f "${RENDERER}" ]]; then
  echo "Batch Markdown REXX validate test requires the precompiled renderer: ${RENDERER}" >&2
  exit 1
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}/expected"
RENDERER_UNDER_TEST="${RENDERER}"

fail() {
  echo "$*" >&2
  exit 1
}

run_crexx_capture() {
  local name="$1"
  shift

  set +e
  "${CREXX}" -nocompile "$@" > "${WORK_DIR}/${name}.out" 2> "${WORK_DIR}/${name}.err"
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

renderer_args=(
  -args
  --validate
  --the "${THE_BIN}"
  --home "${THE_HOME}"
  --crexx "${CREXX}"
  --parser rxc
  --parser-command "${RXC}"
  --parser-arg --syntaxhighlight
)

cat > "${WORK_DIR}/expected/two-lines.txt" <<'EOF_EXPECTED'
alpha
beta
EOF_EXPECTED
expected_before="$(cksum "${WORK_DIR}/expected/two-lines.txt")"

cat > "${WORK_DIR}/valid.md" <<'EOF_VALID'
```rexx id=inline-ok run=true kind=standalone output=text expect=inline:hello
options levelb
say "hello"
```

```rexx id=file-ok run=true kind=standalone output=text expect=expected/two-lines.txt
options levelb
say "alpha"
say "beta"
```
EOF_VALID

run_crexx_capture valid "${RENDERER_UNDER_TEST}" "${renderer_args[@]}" "${WORK_DIR}/valid.md"
assert_rc valid 0
assert_empty_stdout valid
assert_empty_stderr valid
expected_after="$(cksum "${WORK_DIR}/expected/two-lines.txt")"
[[ "${expected_before}" == "${expected_after}" ]] || fail "validate: expected fixture changed"

cat > "${WORK_DIR}/mismatch.md" <<'EOF_MISMATCH'
```rexx id=bad-output run=true kind=standalone output=text expect=inline:expected
options levelb
say "actual"
```
EOF_MISMATCH

run_crexx_capture mismatch "${RENDERER_UNDER_TEST}" "${renderer_args[@]}" "${WORK_DIR}/mismatch.md"
assert_rc mismatch 1
assert_empty_stdout mismatch
rg 'expected output mismatch in example bad-output' "${WORK_DIR}/mismatch.err" >/dev/null
rg 'line 1 expected: expected' "${WORK_DIR}/mismatch.err" >/dev/null
rg 'line 1 actual: actual' "${WORK_DIR}/mismatch.err" >/dev/null

echo "Batch Markdown REXX validate test passed."
