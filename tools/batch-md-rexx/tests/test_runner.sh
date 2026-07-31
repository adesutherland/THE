#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-runner-test"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
THE_HOME="${THE_HOME_DIR:-${BUILD_DIR}/release}"
RUNNER="${THE_BATCH_RUNNER:-${THE_HOME}/the-batch-md-rexx}"
BATCH_DRIVER="${THE_BATCH_DRIVER_RXBIN:-${THE_HOME}/batch-md-rexx/batch-md-rexx.rxbin}"
RENDERER="${THE_BATCH_RENDERER_RXBIN:-${THE_HOME}/batch-md-rexx/render-html.rxbin}"
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
  if [[ -n "${CREXX}" && -x "$(dirname "${CREXX}")/rxc" ]]; then
    RXC="$(dirname "${CREXX}")/rxc"
  elif command -v rxc >/dev/null 2>&1; then
    RXC="$(command -v rxc)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc" ]]; then
    RXC="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc"
  fi
fi

if [[ -z "${CREXX}" || ! -x "${CREXX}" ]]; then
  echo "Skipping batch Markdown REXX runner test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX runner test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX runner test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown REXX runner test; THE was built without CREXX" >&2
  exit 77
fi

if [[ ! -x "${RUNNER}" || "${BATCH_DRIVER}" != *.rxbin || ! -f "${BATCH_DRIVER}" ||
      "${RENDERER}" != *.rxbin || ! -f "${RENDERER}" ]]; then
  echo "Batch Markdown REXX runner test requires the packaged launcher and RXBIN artifacts" >&2
  exit 1
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
CREXX_SPY="${WORK_DIR}/crexx-spy"
CREXX_INVOCATION_LOG="${WORK_DIR}/crexx-invocations.log"
cat > "${CREXX_SPY}" <<'EOF_CREXX_SPY'
#!/usr/bin/env bash
set -euo pipefail

printf '%s\n' "$*" >> "${THE_CREXX_INVOCATION_LOG}"
exec "${THE_REAL_CREXX}" "$@"
EOF_CREXX_SPY
chmod +x "${CREXX_SPY}"

fail() {
  echo "$*" >&2
  exit 1
}

run_capture() {
  local name="$1"
  shift

  set +e
  "$@" > "${WORK_DIR}/${name}.out" 2> "${WORK_DIR}/${name}.err"
  local status=$?
  set -e

  printf '%s\n' "${status}" > "${WORK_DIR}/${name}.rc"
}

assert_rc() {
  local name="$1"
  local expected="$2"
  local actual

  actual="$(cat "${WORK_DIR}/${name}.rc")"
  if [[ "${actual}" != "${expected}" ]]; then
    echo "${name}: expected rc ${expected}, got ${actual}" >&2
    echo "--- stderr ---" >&2
    sed -n '1,160p' "${WORK_DIR}/${name}.err" >&2
    exit 1
  fi
}

assert_empty_stdout() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.out" ]] || fail "${name}: expected stdout to be empty"
}

env_args=(
  "CREXX=${CREXX_SPY}"
  "THE_REAL_CREXX=${CREXX}"
  "THE_CREXX_INVOCATION_LOG=${CREXX_INVOCATION_LOG}"
  "THE_BIN=${THE_BIN}"
  "THE_HOME_DIR=${THE_HOME}"
  "THE_CREXX_RXC=${RXC}"
)

cat > "${WORK_DIR}/runner.md" <<'EOF_RUNNER'
# Runner

```rexx id=runner-hello run=true kind=standalone output=text expect=inline:runner-ok
options levelb
say "runner-ok"
```
EOF_RUNNER

run_capture direct env "${env_args[@]}" "${RUNNER}" "${WORK_DIR}/runner.md" "${WORK_DIR}/runner.html"
assert_rc direct 0
assert_empty_stdout direct
rg 'batch-md-rexx: rendering ' "${WORK_DIR}/direct.err" >/dev/null
rg 'batch-md-rexx: wrote ' "${WORK_DIR}/direct.err" >/dev/null
rg 'runner-ok' "${WORK_DIR}/runner.html" >/dev/null

run_capture validate env "${env_args[@]}" "${RUNNER}" --validate "${WORK_DIR}/runner.md"
assert_rc validate 0
assert_empty_stdout validate
rg 'batch-md-rexx: validating ' "${WORK_DIR}/validate.err" >/dev/null
rg 'batch-md-rexx: validation passed' "${WORK_DIR}/validate.err" >/dev/null

rg -- '-nocompile .*batch-md-rexx\.rxbin' "${CREXX_INVOCATION_LOG}" >/dev/null
rg -- '-nocompile .*render-html\.rxbin' "${CREXX_INVOCATION_LOG}" >/dev/null
if rg 'batch-md-rexx\.crexx|render-html\.crexx' "${CREXX_INVOCATION_LOG}" >/dev/null; then
  fail "runner: production driver or renderer was compiled from source"
fi

echo "Batch Markdown REXX runner test passed."
