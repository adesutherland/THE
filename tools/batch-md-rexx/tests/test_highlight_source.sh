#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-highlight-source-test"
RENDERER="${TOOL_DIR}/render-html.crexx"
WRAPPER="${TOOL_DIR}/the-highlight-source"
TEMPLATE_DIR="${TOOL_DIR}/templates/tex/default"
REXX_SAMPLE="${SCRIPT_DIR}/fixtures/crexx-doc-hello.crexx"
RXAS_SAMPLE="${SCRIPT_DIR}/fixtures/crexx-doc-hello.rxas"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
THE_HOME="${THE_HOME_DIR:-${BUILD_DIR}/release}"
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

run_capture rexx-style "${CREXX}" -nokeep "${RENDERER}" -args \
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

run_capture rxas-fragment "${CREXX}" -nokeep "${RENDERER}" -args \
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

run_capture body-only "${CREXX}" -nokeep "${RENDERER}" -args \
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

echo "Batch Markdown source fragment highlight test passed."
