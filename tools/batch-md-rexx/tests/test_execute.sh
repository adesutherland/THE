#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-execute-test"
RENDERER="${TOOL_DIR}/render-html.crexx"
TEMPLATE_DIR="${TOOL_DIR}/templates/html/default"
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
  echo "Skipping batch Markdown REXX execute test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX execute test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX execute test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown REXX execute test; THE was built without CREXX" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
RENDERER_UNDER_TEST="${WORK_DIR}/render-html.crexx"
cp "${RENDERER}" "${RENDERER_UNDER_TEST}"

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

assert_empty_stderr() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.err" ]] || fail "${name}: expected stderr to be empty"
}

renderer_args=(
  -args
  --the "${THE_BIN}"
  --home "${THE_HOME}"
  --crexx "${CREXX}"
  --parser rxc
  --parser-command "${RXC}"
  --parser-arg --syntaxhighlight
  --template-dir "${TEMPLATE_DIR}"
)

cat > "${WORK_DIR}/execute.md" <<'EOF_EXECUTE'
# Execute

```rexx id=standalone-run run=true kind=standalone output=text
options levelb
import rxfnsb
say "hello stdout"
call lineout "stderr", "hello stderr"
```

```rexx id=highlighted-output run=true kind=standalone output=rexx
options levelb
say 'say "from output"'
```

```the id=macro-run run=true kind=the-macro output=text
address the
'emsg MACRO_MESSAGE'
```

```the id=address-run run=true kind=address-the output=text fail-on-diagnostics=false
emsg ADDRESS_THE_MESSAGE
```
EOF_EXECUTE

run_crexx_capture execute "${RENDERER_UNDER_TEST}" "${renderer_args[@]}" "${WORK_DIR}/execute.md"
assert_rc execute 0
assert_empty_stderr execute

rg '<figure class="the-example the-example-rexx" id="standalone-run">' "${WORK_DIR}/execute.out" >/dev/null
rg '<p class="the-run-status">rc=0</p>' "${WORK_DIR}/execute.out" >/dev/null
rg 'hello stdout' "${WORK_DIR}/execute.out" >/dev/null
rg 'hello stderr' "${WORK_DIR}/execute.out" >/dev/null
rg '<figure class="the-example the-example-rexx" id="highlighted-output">' "${WORK_DIR}/execute.out" >/dev/null
rg '<span class="syn-keyword">say</span> <span class="syn-string">&quot;from output&quot;</span>' "${WORK_DIR}/execute.out" >/dev/null
rg 'MACRO_MESSAGE' "${WORK_DIR}/execute.out" >/dev/null
rg 'ADDRESS_THE_MESSAGE' "${WORK_DIR}/execute.out" >/dev/null

cat > "${WORK_DIR}/nonzero.md" <<'EOF_NONZERO'
```rexx id=bad-rc run=true kind=standalone output=text
options levelb
exit 7
```
EOF_NONZERO

run_crexx_capture nonzero "${RENDERER_UNDER_TEST}" "${renderer_args[@]}" "${WORK_DIR}/nonzero.md"
assert_rc nonzero 1
rg 'example bad-rc exited with rc 7' "${WORK_DIR}/nonzero.err" >/dev/null

cat > "${WORK_DIR}/allow-rc.md" <<'EOF_ALLOW'
```rexx id=allowed-rc run=true kind=standalone output=text allow-rc=7
options levelb
exit 7
```
EOF_ALLOW

run_crexx_capture allow-rc "${RENDERER_UNDER_TEST}" "${renderer_args[@]}" "${WORK_DIR}/allow-rc.md"
assert_rc allow-rc 0
assert_empty_stderr allow-rc
rg '<p class="the-run-status">rc=7</p>' "${WORK_DIR}/allow-rc.out" >/dev/null

echo "Batch Markdown REXX execute test passed."
