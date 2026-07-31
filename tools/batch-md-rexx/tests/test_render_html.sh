#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-html-test"
RENDERER="${THE_BATCH_RENDERER_RXBIN:-${BUILD_DIR}/release/batch-md-rexx/render-html.rxbin}"
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
  echo "Skipping batch Markdown REXX HTML test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX HTML test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX HTML test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown REXX HTML test; THE was built without CREXX" >&2
  exit 77
fi

if [[ "${RENDERER}" != *.rxbin || ! -f "${RENDERER}" ]]; then
  echo "Batch Markdown REXX HTML test requires the precompiled renderer: ${RENDERER}" >&2
  exit 1
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
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

assert_empty_stderr() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.err" ]] || fail "${name}: expected stderr to be empty"
}

cat > "${WORK_DIR}/html.md" <<'EOF_HTML'
# HTML

```rexx id=escape-source run=false output=text
options levelb
say "<tag & value>"
/* comment */
```

```text
plain <fence> & markdown
```
EOF_HTML

run_crexx_capture html \
  "${RENDERER_UNDER_TEST}" \
  -args \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${TEMPLATE_DIR}" \
  "${WORK_DIR}/html.md"

assert_rc html 0
assert_empty_stderr html

rg '<!doctype html>' "${WORK_DIR}/html.out" >/dev/null
rg '<style>' "${WORK_DIR}/html.out" >/dev/null
rg '\.syn-keyword' "${WORK_DIR}/html.out" >/dev/null
rg '\.syn-comment' "${WORK_DIR}/html.out" >/dev/null
rg '<figure class="the-example the-example-rexx" id="escape-source">' "${WORK_DIR}/html.out" >/dev/null
rg '<span class="syn-preprocessor">options</span>' "${WORK_DIR}/html.out" >/dev/null
rg '<span class="syn-keyword">say</span>' "${WORK_DIR}/html.out" >/dev/null
rg '<span class="syn-string">&quot;&lt;tag &amp; value&gt;&quot;</span>' "${WORK_DIR}/html.out" >/dev/null
rg '<span class="syn-comment">/\* comment \*/</span>' "${WORK_DIR}/html.out" >/dev/null
rg 'plain &lt;fence&gt; &amp; markdown' "${WORK_DIR}/html.out" >/dev/null

if rg '<tag & value>' "${WORK_DIR}/html.out" >/dev/null; then
  fail "html: source string was not escaped"
fi

CUSTOM_TEMPLATE_DIR="${WORK_DIR}/custom-template"
cp -R "${TEMPLATE_DIR}" "${CUSTOM_TEMPLATE_DIR}"
cat > "${CUSTOM_TEMPLATE_DIR}/example-open.tpl" <<'EOF_CUSTOM_EXAMPLE'
<figure class="custom-example the-example-{{language_attr}}" id="{{id_attr}}" data-template="custom">
<figcaption>{{id}}</figcaption>
EOF_CUSTOM_EXAMPLE
cat > "${CUSTOM_TEMPLATE_DIR}/style-token.tpl" <<'EOF_CUSTOM_TOKEN'
<b data-style="{{style_class}}">{{text}}</b>
EOF_CUSTOM_TOKEN

run_crexx_capture custom-template \
  "${RENDERER_UNDER_TEST}" \
  -args \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${CUSTOM_TEMPLATE_DIR}" \
  "${WORK_DIR}/html.md"

assert_rc custom-template 0
assert_empty_stderr custom-template
rg '<figure class="custom-example the-example-rexx" id="escape-source" data-template="custom">' "${WORK_DIR}/custom-template.out" >/dev/null
rg '<b data-style="syn-keyword">say</b>' "${WORK_DIR}/custom-template.out" >/dev/null

cat > "${WORK_DIR}/diagnostic.md" <<'EOF_DIAG'
```rexx id=bad-source
options levelb
this is invalid
```
EOF_DIAG

run_crexx_capture diagnostic \
  "${RENDERER_UNDER_TEST}" \
  -args \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${TEMPLATE_DIR}" \
  "${WORK_DIR}/diagnostic.md"

assert_rc diagnostic 1
rg 'parser diagnostics present in example bad-source' "${WORK_DIR}/diagnostic.err" >/dev/null
rg 'SYNTAX_ERROR' "${WORK_DIR}/diagnostic.err" >/dev/null

echo "Batch Markdown REXX HTML render test passed."
