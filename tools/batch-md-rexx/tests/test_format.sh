#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-format-test"
VALIDATOR="${TOOL_DIR}/validate.crexx"
SAMPLE="${SCRIPT_DIR}/fixtures/valid.md"
CREXX="${CREXX:-${THE_CREXX:-}}"

if [[ -z "${CREXX}" ]]; then
  if command -v crexx >/dev/null 2>&1; then
    CREXX="$(command -v crexx)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/crexx" ]]; then
    CREXX="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/crexx"
  fi
fi

if [[ -z "${CREXX}" || ! -x "${CREXX}" ]]; then
  echo "Skipping batch Markdown REXX format test; crexx is missing" >&2
  exit 77
fi

rm -rf "${WORK_DIR}"
mkdir -p "${WORK_DIR}"
# Expected-failure validator cases can leave CREXX intermediates, so compile a
# disposable copy instead of writing byproducts beside the source script.
VALIDATOR_UNDER_TEST="${WORK_DIR}/validate.crexx"
cp "${VALIDATOR}" "${VALIDATOR_UNDER_TEST}"

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

assert_nonzero_rc() {
  local name="$1"
  local actual

  actual="$(cat "${WORK_DIR}/${name}.rc")"
  [[ "${actual}" != "0" ]] || fail "${name}: expected nonzero rc"
}

assert_empty_stdout() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.out" ]] || fail "${name}: expected stdout to be empty"
}

assert_empty_stderr() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.err" ]] || fail "${name}: expected stderr to be empty"
}

cat > "${WORK_DIR}/crexx-exit7.crexx" <<'EOF_EXIT7'
options levelb
exit 7
EOF_EXIT7
run_crexx_capture crexx-exit7 "${WORK_DIR}/crexx-exit7.crexx"
assert_rc crexx-exit7 7

cat > "${WORK_DIR}/crexx-main9.crexx" <<'EOF_MAIN9'
options levelb
main: procedure = .int
  return 9
EOF_MAIN9
run_crexx_capture crexx-main9 "${WORK_DIR}/crexx-main9.crexx"
assert_rc crexx-main9 9

cat > "${WORK_DIR}/crexx-bad.crexx" <<'EOF_BAD'
options levelb
this is invalid
EOF_BAD
run_crexx_capture crexx-bad "${WORK_DIR}/crexx-bad.crexx"
assert_nonzero_rc crexx-bad
assert_empty_stdout crexx-bad
rg '#SYNTAX_ERROR' "${WORK_DIR}/crexx-bad.err" >/dev/null

cat > "${WORK_DIR}/crexx-address-rc.crexx" <<'EOF_ADDRESS_RC'
options levelb
import rxfnsb

rexx_version = .string
assembler rxvers rexx_version
platform = translate(word(rexx_version, 1))

if platform = "WINDOWS" then address command "cmd /c exit 3"
else address command "sh -c 'exit 3'"

if rc <> 3 then do
  say "ADDRESS COMMAND rc expected 3, got" rc
  return 1
end
return 0
EOF_ADDRESS_RC
run_crexx_capture crexx-address-rc "${WORK_DIR}/crexx-address-rc.crexx"
assert_rc crexx-address-rc 0
assert_empty_stderr crexx-address-rc

printf 'one\ntwo\n' > "${WORK_DIR}/crexx-lines-final-newline.txt"
cat > "${WORK_DIR}/crexx-lines.crexx" <<'EOF_LINES'
options levelb
import rxfnsb
arg args = .string[]

path = args[1]
count = 0
do forever
  available = lines(path)
  if available < 0 then return 1
  if available = 0 then leave
  text = linein(path)
  count = count + 1
  if count = 3 then do
    say "unexpected third line <"text">"
    return 1
  end
end

if count <> 2 then do
  say "expected 2 lines, got" count
  return 1
end
return 0
EOF_LINES
run_crexx_capture crexx-lines "${WORK_DIR}/crexx-lines.crexx" -args "${WORK_DIR}/crexx-lines-final-newline.txt"
assert_rc crexx-lines 0
assert_empty_stderr crexx-lines

cat > "${WORK_DIR}/crexx-missing-lines.crexx" <<'EOF_MISSING_LINES'
options levelb
import rxfnsb
arg args = .string[]

if lines(args[1]) <> -1 then return 1
return 0
EOF_MISSING_LINES
run_crexx_capture crexx-missing-lines "${WORK_DIR}/crexx-missing-lines.crexx" -args "${WORK_DIR}/does-not-exist.txt"
assert_rc crexx-missing-lines 0
assert_empty_stdout crexx-missing-lines
rg 'ERROR lines\(\) file .* could not open' "${WORK_DIR}/crexx-missing-lines.err" >/dev/null

run_crexx_capture manifest "${VALIDATOR_UNDER_TEST}" -args "${SAMPLE}"
assert_rc manifest 0
assert_empty_stderr manifest
cp "${WORK_DIR}/manifest.out" "${WORK_DIR}/manifest.txt"
rg 'example id=source-only .*run=false' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=source-only .*output=text' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=hello-run .*run=true' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=hello-run .*output=text' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=hello-run .*timeout=5000' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=hello-run .*expect=hello-output' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=markdown-output .*run=true' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=markdown-output .*output=markdown' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=buffer-message .*kind=the-macro' "${WORK_DIR}/manifest.txt" >/dev/null
rg 'example id=buffer-message .*output=text' "${WORK_DIR}/manifest.txt" >/dev/null

run_crexx_capture roundtrip "${VALIDATOR_UNDER_TEST}" -args --round-trip "${SAMPLE}"
assert_rc roundtrip 0
assert_empty_stderr roundtrip
cmp "${SAMPLE}" "${WORK_DIR}/roundtrip.out"

cat > "${WORK_DIR}/duplicate-id.md" <<'EOF_DUP'
# Duplicate id

```rexx id=dup run=false
say "one"
```

```rexx id=dup run=true
say "two"
```
EOF_DUP

run_crexx_capture duplicate "${VALIDATOR_UNDER_TEST}" -args "${WORK_DIR}/duplicate-id.md"
assert_rc duplicate 1
assert_empty_stdout duplicate
rg "duplicate example id 'dup' first defined on line 3" "${WORK_DIR}/duplicate.err" >/dev/null

cat > "${WORK_DIR}/invalid-id.md" <<'EOF_INVALID_ID'
# Invalid id

```rexx id=123bad run=false
say "bad"
```
EOF_INVALID_ID

run_crexx_capture invalid-id "${VALIDATOR_UNDER_TEST}" -args "${WORK_DIR}/invalid-id.md"
assert_rc invalid-id 1
assert_empty_stdout invalid-id
rg "invalid example id '123bad'" "${WORK_DIR}/invalid-id.err" >/dev/null

cat > "${WORK_DIR}/invalid-attributes.md" <<'EOF_INVALID_ATTR'
# Invalid attributes

```rexx id=bad-run run=yes output='bad/name'
say "bad"
```
EOF_INVALID_ATTR

run_crexx_capture invalid-attributes "${VALIDATOR_UNDER_TEST}" -args "${WORK_DIR}/invalid-attributes.md"
assert_rc invalid-attributes 1
assert_empty_stdout invalid-attributes
rg "run must be true or false, got 'yes'" "${WORK_DIR}/invalid-attributes.err" >/dev/null
rg "invalid output name 'bad/name'" "${WORK_DIR}/invalid-attributes.err" >/dev/null

run_crexx_capture missing-markdown "${VALIDATOR_UNDER_TEST}" -args "${WORK_DIR}/missing.md"
assert_rc missing-markdown 2
assert_empty_stdout missing-markdown
rg "validate.crexx: cannot read markdown file: ${WORK_DIR}/missing.md" "${WORK_DIR}/missing-markdown.err" >/dev/null

echo "Batch Markdown REXX format test passed."
