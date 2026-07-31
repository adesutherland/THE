#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-highlight-source-test"
RENDERER_SOURCE="${TOOL_DIR}/render-html.crexx"
TEMPLATE_DIR="${TOOL_DIR}/templates/tex/default"
HTML_TEMPLATE_DIR="${TOOL_DIR}/templates/html/default"
REXX_SAMPLE="${SCRIPT_DIR}/fixtures/crexx-doc-hello.crexx"
RXAS_SAMPLE="${SCRIPT_DIR}/fixtures/crexx-doc-hello.rxas"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
THE_HOME="${THE_HOME_DIR:-${BUILD_DIR}/release}"
RENDERER="${THE_BATCH_RENDERER_RXBIN:-${THE_HOME}/batch-md-rexx/render-html.rxbin}"
WRAPPER="${THE_HIGHLIGHT_SOURCE:-${THE_HOME}/the-highlight-source}"
CREXX="${CREXX:-${THE_CREXX:-}}"
RXC="${THE_CREXX_RXC:-}"
RXAS="${THE_CREXX_RXAS:-}"

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

if [[ -z "${RXAS}" ]]; then
  if [[ -n "${RXC}" && -x "$(dirname "${RXC}")/rxas" ]]; then
    RXAS="$(dirname "${RXC}")/rxas"
  elif command -v rxas >/dev/null 2>&1; then
    RXAS="$(command -v rxas)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxas" ]]; then
    RXAS="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxas"
  fi
fi

if [[ -z "${CREXX}" || ! -x "${CREXX}" ]]; then
  echo "Skipping batch Markdown source fragment highlight test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown source fragment highlight test; rxc is missing" >&2
  exit 77
fi

if [[ -z "${RXAS}" || ! -x "${RXAS}" ]]; then
  echo "Skipping batch Markdown source fragment highlight test; rxas is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown source fragment highlight test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown source fragment highlight test; THE was built without CREXX" >&2
  exit 77
fi

if [[ "${RENDERER}" != *.rxbin || ! -f "${RENDERER}" || ! -x "${WRAPPER}" ]]; then
  echo "Batch Markdown source fragment test requires the packaged launcher and renderer RXBIN" >&2
  exit 1
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

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
    sed -n '1,200p' "${WORK_DIR}/${name}.err" >&2
    echo "--- stdout ---" >&2
    sed -n '1,120p' "${WORK_DIR}/${name}.out" >&2
    exit 1
  fi
}

run_capture rexx-style "${CREXX}" -nocompile "${RENDERER}" -args \
  --highlight-source \
  --format tex-fragment \
  --language rexx \
  --include-style true \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${TEMPLATE_DIR}" \
  "${REXX_SAMPLE}"
assert_rc rexx-style 0
rg '\\definecolor\{ThePaper\}' "${WORK_DIR}/rexx-style.out" >/dev/null
rg '\\begin\{TheCodeBlock\}' "${WORK_DIR}/rexx-style.out" >/dev/null
rg '\\end\{TheCodeBlock\}' "${WORK_DIR}/rexx-style.out" >/dev/null
rg 'hello cRexx world' "${WORK_DIR}/rexx-style.out" >/dev/null
rg '\\TheSyn' "${WORK_DIR}/rexx-style.out" >/dev/null

run_capture rxas-fragment "${CREXX}" -nocompile "${RENDERER}" -args \
  --highlight-source \
  --format tex-fragment \
  --language rxas \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --rxas-parser-command "${RXAS}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${TEMPLATE_DIR}" \
  "${RXAS_SAMPLE}"
assert_rc rxas-fragment 0
! rg '\\definecolor\{ThePaper\}' "${WORK_DIR}/rxas-fragment.out" >/dev/null || fail "rxas-fragment: style definitions should be opt-in"
rg '\\begin\{TheCodeBlock\}' "${WORK_DIR}/rxas-fragment.out" >/dev/null
rg 'setnumdgts' "${WORK_DIR}/rxas-fragment.out" >/dev/null
rg 'hello cRexx world' "${WORK_DIR}/rxas-fragment.out" >/dev/null
rg '\\TheSyn' "${WORK_DIR}/rxas-fragment.out" >/dev/null

run_capture body-only "${CREXX}" -nocompile "${RENDERER}" -args \
  --highlight-source \
  --format tex-fragment \
  --language rexx \
  --include-wrapper false \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${TEMPLATE_DIR}" \
  "${REXX_SAMPLE}"
assert_rc body-only 0
! rg '\\begin\{TheCodeBlock\}' "${WORK_DIR}/body-only.out" >/dev/null || fail "body-only: wrapper should be opt-in"
rg 'hello cRexx world' "${WORK_DIR}/body-only.out" >/dev/null

run_capture html-style "${CREXX}" -nocompile "${RENDERER}" -args \
  --highlight-source \
  --format html-fragment \
  --language crexx \
  --include-style true \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${HTML_TEMPLATE_DIR}" \
  "${REXX_SAMPLE}"
assert_rc html-style 0
rg '^<style>$' "${WORK_DIR}/html-style.out" >/dev/null
rg '^</style>$' "${WORK_DIR}/html-style.out" >/dev/null
rg '\.the-example-source \.syn-keyword' "${WORK_DIR}/html-style.out" >/dev/null
rg '<pre class="the-example-source"><code>' "${WORK_DIR}/html-style.out" >/dev/null
rg '<span class="syn-preprocessor">options</span>' "${WORK_DIR}/html-style.out" >/dev/null
rg '</code></pre>' "${WORK_DIR}/html-style.out" >/dev/null

env_args=(
  "CREXX=${CREXX}"
  "THE_BIN=${THE_BIN}"
  "THE_HOME_DIR=${THE_HOME}"
  "THE_CREXX_RXC=${RXC}"
  "THE_CREXX_RXAS=${RXAS}"
)

run_capture wrapper-inferred env "${env_args[@]}" "${WRAPPER}" "${RXAS_SAMPLE}"
assert_rc wrapper-inferred 0
rg '\\begin\{TheCodeBlock\}' "${WORK_DIR}/wrapper-inferred.out" >/dev/null
rg 'setnumdgts' "${WORK_DIR}/wrapper-inferred.out" >/dev/null
rg '\\TheSyn' "${WORK_DIR}/wrapper-inferred.out" >/dev/null

run_capture wrapper-html env "${env_args[@]}" "${WRAPPER}" \
  --format html-fragment \
  --include-style false \
  "${REXX_SAMPLE}"
assert_rc wrapper-html 0
! rg '^<style>$' "${WORK_DIR}/wrapper-html.out" >/dev/null || fail "wrapper-html: style definitions should be opt-in"
rg '<pre class="the-example-source"><code>' "${WORK_DIR}/wrapper-html.out" >/dev/null
rg '<span class="syn-preprocessor">options</span>' "${WORK_DIR}/wrapper-html.out" >/dev/null
rg '</code></pre>' "${WORK_DIR}/wrapper-html.out" >/dev/null

run_capture timeout-fails-closed env \
  "THE_BATCH_HIGHLIGHT_PROFILE=${TOOL_DIR}/highlight-source-profile.the" \
  "${CREXX}" -nocompile "${RENDERER}" -args \
  --highlight-source \
  --format html-fragment \
  --language crexx \
  --timeout 1 \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${HTML_TEMPLATE_DIR}" \
  "${RENDERER_SOURCE}"
assert_rc timeout-fails-closed 2
[[ ! -s "${WORK_DIR}/timeout-fails-closed.out" ]] || fail "timeout-fails-closed: partial highlighted output must not be emitted"
rg 'SDSLHWAIT failed while highlighting' "${WORK_DIR}/timeout-fails-closed.err" >/dev/null

PRECOMPILED_WRAPPER="${WRAPPER}"
[[ -x "${PRECOMPILED_WRAPPER}" ]] || fail "precompiled-wrapper: release launcher is missing"
[[ -f "${THE_HOME}/batch-md-rexx/render-html.rxbin" ]] || fail "precompiled-wrapper: render-html.rxbin is missing"
[[ -f "${THE_HOME}/batch-md-rexx/highlight-source-profile.the" ]] || fail "precompiled-wrapper: stable profile is missing"
run_capture precompiled-wrapper env "${env_args[@]}" "${PRECOMPILED_WRAPPER}" \
  --format html-fragment \
  "${REXX_SAMPLE}"
assert_rc precompiled-wrapper 0
rg '<span class="syn-preprocessor">options</span>' "${WORK_DIR}/precompiled-wrapper.out" >/dev/null

echo "Batch Markdown source fragment highlight test passed."
