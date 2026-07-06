#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-tex-test"
RENDERER="${TOOL_DIR}/render-html.crexx"
RUNNER="${TOOL_DIR}/the-batch-md-rexx"
TEMPLATE_DIR="${TOOL_DIR}/templates/tex/default"
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
  if [[ -n "${CREXX}" && -x "$(dirname "${CREXX}")/rxc" ]]; then
    RXC="$(dirname "${CREXX}")/rxc"
  elif command -v rxc >/dev/null 2>&1; then
    RXC="$(command -v rxc)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc" ]]; then
    RXC="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/rxc"
  fi
fi

if [[ -z "${CREXX}" || ! -x "${CREXX}" ]]; then
  echo "Skipping batch Markdown REXX TeX test; crexx is missing" >&2
  exit 77
fi

if [[ -z "${RXC}" || ! -x "${RXC}" ]]; then
  echo "Skipping batch Markdown REXX TeX test; rxc is missing" >&2
  exit 77
fi

if [[ ! -x "${THE_BIN}" || ! -d "${THE_HOME}" ]]; then
  echo "Skipping batch Markdown REXX TeX test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping batch Markdown REXX TeX test; THE was built without CREXX" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}/fakebin"

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
    sed -n '1,80p' "${WORK_DIR}/${name}.out" >&2
    exit 1
  fi
}

cat > "${WORK_DIR}/tex.md" <<'EOF_TEX_MD'
# TeX Smoke & Escaping

Ordinary markdown with 100%, {braces}, and path\to\thing.
continued on the next source line.

```rexx id=tex-demo run=true kind=standalone output=markdown
options levelb
say "Result & status"
say "- value: a_b & 100"
```
EOF_TEX_MD

run_capture direct "${CREXX}" -nokeep "${RENDERER}" -args \
  --format tex \
  --the "${THE_BIN}" \
  --home "${THE_HOME}" \
  --crexx "${CREXX}" \
  --parser rxc \
  --parser-command "${RXC}" \
  --parser-arg --syntaxhighlight \
  --template-dir "${TEMPLATE_DIR}" \
  "${WORK_DIR}/tex.md"
assert_rc direct 0
cp "${WORK_DIR}/direct.out" "${WORK_DIR}/direct.tex"
rg '\\documentclass' "${WORK_DIR}/direct.tex" >/dev/null
rg '\\TheMarkdownHOne\{TeX Smoke \\& Escaping\}' "${WORK_DIR}/direct.tex" >/dev/null
rg -F '\TheMarkdownParagraph{Ordinary markdown with 100\%, \{braces\}, and path\textbackslash{}to\textbackslash{}thing. continued on the next source line.}' "${WORK_DIR}/direct.tex" >/dev/null
rg '\\begin\{TheExample\}\{tex-demo\}\{rexx\}' "${WORK_DIR}/direct.tex" >/dev/null
rg '\\TheRunStatus\{0\}' "${WORK_DIR}/direct.tex" >/dev/null
rg '\\begin\{itemize\}' "${WORK_DIR}/direct.tex" >/dev/null
rg '100\\%' "${WORK_DIR}/direct.tex" >/dev/null
rg 'a\\_b \\& 100' "${WORK_DIR}/direct.tex" >/dev/null
rg '\\textbackslash\{\}' "${WORK_DIR}/direct.tex" >/dev/null

env_args=(
  "CREXX=${CREXX}"
  "THE_BIN=${THE_BIN}"
  "THE_HOME_DIR=${THE_HOME}"
  "THE_CREXX_RXC=${RXC}"
)

run_capture runner env "${env_args[@]}" "${RUNNER}" --format tex "${WORK_DIR}/tex.md" "${WORK_DIR}/runner.tex"
assert_rc runner 0
[[ ! -s "${WORK_DIR}/runner.out" ]] || fail "runner: expected stdout to be empty"
rg 'batch-md-rexx: rendering tex ' "${WORK_DIR}/runner.err" >/dev/null
rg '\\documentclass' "${WORK_DIR}/runner.tex" >/dev/null

cat > "${WORK_DIR}/fakebin/tectonic" <<'EOF_TECTONIC'
#!/usr/bin/env bash
set -euo pipefail

outdir=""
input=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --outdir)
      outdir="$2"
      shift 2
      ;;
    *)
      input="$1"
      shift
      ;;
  esac
done

mkdir -p "${outdir}"
printf 'fake pdf for %s\n' "${input}" > "${outdir}/$(basename "${input}" .tex).pdf"
EOF_TECTONIC
chmod +x "${WORK_DIR}/fakebin/tectonic"

fakebin_path="${WORK_DIR}/fakebin"
if command -v cygpath >/dev/null 2>&1; then
  fakebin_path="$(cygpath -u "${fakebin_path}")"
fi

run_capture pdf env "PATH=${fakebin_path}:${PATH}" "${env_args[@]}" "${RUNNER}" --pdf --pdf-engine auto "${WORK_DIR}/tex.md" "${WORK_DIR}/runner.pdf"
assert_rc pdf 0
[[ ! -s "${WORK_DIR}/pdf.out" ]] || fail "pdf: expected stdout to be empty"
rg 'batch-md-rexx: compiling PDF with tectonic' "${WORK_DIR}/pdf.err" >/dev/null
rg 'fake pdf for ' "${WORK_DIR}/runner.pdf" >/dev/null

echo "Batch Markdown REXX TeX/PDF test passed."
