#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-output-test"
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
  echo "Skipping batch Markdown REXX output render test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX output render test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX output render test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown REXX output render test; THE was built without CREXX" >&2
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

cat > "${WORK_DIR}/output.md" <<'EOF_OUTPUT'
# Output

```rexx id=markdown-output run=true kind=standalone output=markdown
options levelb
say "# Run Report"
say ""
say "| name | value |"
say "| --- | --- |"
say "| answer | 42 |"
say ""
say "- safe"
say "- escaped"
say ""
say "<script>alert(1)</script>"
say "inline `code`"
```

```rexx id=parser-output run=true kind=standalone output=rxc
options levelb
say 'say "from parser"'
```

```rexx id=html-safe run=true kind=standalone output=html
options levelb
say "<b>not bold</b>"
```
EOF_OUTPUT

run_crexx_capture output "${RENDERER_UNDER_TEST}" "${renderer_args[@]}" "${WORK_DIR}/output.md"
assert_rc output 0
assert_empty_stderr output

rg '<div class="the-output the-output-stdout the-output-markdown">' "${WORK_DIR}/output.out" >/dev/null
rg '<h3>Run Report</h3>' "${WORK_DIR}/output.out" >/dev/null
rg '<table>' "${WORK_DIR}/output.out" >/dev/null
rg '<th>name</th>' "${WORK_DIR}/output.out" >/dev/null
rg '<td>answer</td>' "${WORK_DIR}/output.out" >/dev/null
rg '<li>safe</li>' "${WORK_DIR}/output.out" >/dev/null
rg '&lt;script&gt;alert\(1\)&lt;/script&gt;' "${WORK_DIR}/output.out" >/dev/null
rg 'inline <code>code</code>' "${WORK_DIR}/output.out" >/dev/null
rg '<figure class="the-example the-example-rexx" id="parser-output">' "${WORK_DIR}/output.out" >/dev/null
rg '<span class="syn-keyword">say</span> <span class="syn-string">&quot;from parser&quot;</span>' "${WORK_DIR}/output.out" >/dev/null
rg '&lt;b&gt;not bold&lt;/b&gt;' "${WORK_DIR}/output.out" >/dev/null

if rg '<script>alert\(1\)</script>' "${WORK_DIR}/output.out" >/dev/null; then
  fail "output: markdown output emitted raw script HTML"
fi

if rg '<b>not bold</b>' "${WORK_DIR}/output.out" >/dev/null; then
  fail "output: html output was not escaped"
fi

echo "Batch Markdown REXX output render test passed."
