#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
THE_BIN="${THE_BIN:-${BUILD_DIR}/release/the}"
WORK_DIR="${BUILD_DIR}/sdslh-multifile-lifecycle-test"
PROFILE="${WORK_DIR}/test_profile.the"
REXX_SAMPLE="${WORK_DIR}/sample.rexx"
MD_SAMPLE="${WORK_DIR}/sample.md"
OUT_SAMPLE="${WORK_DIR}/sample.out"

find_tp() {
  local candidate

  for candidate in \
    "${THE_SDSLH_TP:-}" \
    "${HOME}/.local/bin/tp" \
    "${BUILD_DIR}/sdslh/toyparser/tp" \
    "${ROOT_DIR}/../DSL-Syntax-Highlighter/cmake-build-debug/toyparser/tp"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

find_mdp() {
  local candidate

  for candidate in \
    "${THE_SDSLH_MDP:-}" \
    "${HOME}/.local/bin/mdp" \
    "${ROOT_DIR}/../DSL-Syntax-Highlighter/cmake-build-debug/parsers/markdown/mdp"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

if [[ ! -x "${THE_BIN}" ]]; then
  echo "Skipping SDSLH multifile lifecycle test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "${THE_BIN}"; then
  echo "Skipping SDSLH multifile lifecycle test; THE was built without CREXX support" >&2
  exit 77
fi

if ! TP_BIN="$(find_tp)"; then
  echo "Skipping SDSLH multifile lifecycle test; tp parser executable is missing" >&2
  exit 77
fi

if ! MDP_BIN="$(find_mdp)"; then
  echo "Skipping SDSLH multifile lifecycle test; mdp parser executable is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"

cat > "${REXX_SAMPLE}" <<'REXX_EOF'
say "hello"
REXX_EOF

cat > "${MD_SAMPLE}" <<'MD_EOF'
# Title

Some *markdown* text.
MD_EOF

cat > "${OUT_SAMPLE}" <<'OUT_EOF'
!Rred output!N
OUT_EOF

cat > "${PROFILE}" <<PROFILE_EOF
options levelb
address the

'set sdslh rxc ${TP_BIN} -d'
'set autocolor *.rexx rxc'
'set sdslh mdp ${MDP_BIN} --syntaxhighlight'
'set autocolor *.md mdp'

coloring = .string[]

'set coloring on auto'
address the "extract /coloring/" expose coloring[]
if coloring[1] = "ON" then do
   if coloring[3] = "rxc" then 'emsg SDSLH_MULTI_REXX_RCX'
end

'edit ${MD_SAMPLE}'
'set coloring on auto'
address the "extract /coloring/" expose coloring[]
if coloring[1] = "ON" then do
   if coloring[3] = "mdp" then 'emsg SDSLH_MULTI_MD_MDP'
end

'edit ${OUT_SAMPLE}'
'set coloring on auto'
address the "extract /coloring/" expose coloring[]
if coloring[1] = "ON" then do
   if coloring[3] = "NULL" then 'emsg SDSLH_MULTI_OUT_NULL'
end
'set coloring off'
address the "extract /coloring/" expose coloring[]
if coloring[1] = "OFF" then 'emsg SDSLH_MULTI_OUT_OFF'

'edit ${MD_SAMPLE}'
'set coloring on auto'
address the "extract /coloring/" expose coloring[]
if coloring[1] = "ON" then do
   if coloring[3] = "mdp" then 'emsg SDSLH_MULTI_MD_AFTER_OUT'
end

'edit ${REXX_SAMPLE}'
'set coloring on auto'
address the "extract /coloring/" expose coloring[]
if coloring[1] = "ON" then do
   if coloring[3] = "rxc" then 'emsg SDSLH_MULTI_REXX_AGAIN'
end

'qquit'
PROFILE_EOF

(
  cd "${WORK_DIR}"
  "${THE_BIN}" -b -q -p "${PROFILE}" "${REXX_SAMPLE}" > the.out 2> the.err
)

grep -q "SDSLH_MULTI_REXX_RCX" "${WORK_DIR}/the.err"
grep -q "SDSLH_MULTI_MD_MDP" "${WORK_DIR}/the.err"
grep -q "SDSLH_MULTI_OUT_NULL" "${WORK_DIR}/the.err"
grep -q "SDSLH_MULTI_OUT_OFF" "${WORK_DIR}/the.err"
grep -q "SDSLH_MULTI_MD_AFTER_OUT" "${WORK_DIR}/the.err"
grep -q "SDSLH_MULTI_REXX_AGAIN" "${WORK_DIR}/the.err"

echo "SDSLH multifile lifecycle test passed."
