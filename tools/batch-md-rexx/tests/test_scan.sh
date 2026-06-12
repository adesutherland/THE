#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${TOOL_DIR}/../.." && pwd)"
BUILD_DIR="${THE_BUILD_DIR:-${ROOT_DIR}/cmake-build-debug}"
WORK_DIR="${BUILD_DIR}/batch-md-rexx-scan-test"
RENDERER="${TOOL_DIR}/render-html.crexx"
CREXX="${CREXX:-${THE_CREXX:-}}"

if [[ -z "${CREXX}" ]]; then
  if command -v crexx >/dev/null 2>&1; then
    CREXX="$(command -v crexx)"
  elif [[ -x "${ROOT_DIR}/../CREXX/cmake-build-debug/bin/crexx" ]]; then
    CREXX="${ROOT_DIR}/../CREXX/cmake-build-debug/bin/crexx"
  fi
fi

if [[ -z "${CREXX}" || ! -x "${CREXX}" ]]; then
  echo "Skipping batch Markdown REXX scanner test; crexx is missing" >&2
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

assert_empty_stdout() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.out" ]] || fail "${name}: expected stdout to be empty"
}

assert_empty_stderr() {
  local name="$1"
  [[ ! -s "${WORK_DIR}/${name}.err" ]] || fail "${name}: expected stderr to be empty"
}

cat > "${WORK_DIR}/scan-valid.md" <<'EOF_VALID'
# Scanner

Intro before examples.

```text
not an example id=ignored
```

  ~~~~rexx id=tilde.one run=false output=text
say "tilde"
  ~~~~~

```crexx id=quoted.one run=false kind=standalone output="text" fail-on-diagnostics=false timeout=25
say "<hello & goodbye>"
```

    ```rexx id=not-a-fence
    four-space code block is markdown
    ```
EOF_VALID

run_crexx_capture scan-valid "${RENDERER_UNDER_TEST}" -args --scan "${WORK_DIR}/scan-valid.md"
assert_rc scan-valid 0
assert_empty_stderr scan-valid
rg '^markdown start=1 end=8$' "${WORK_DIR}/scan-valid.out" >/dev/null
rg '^example id=tilde\.one language=rexx opening=9 body-start=10 body-end=10 closing=11 run=false kind=standalone output=text fail-on-diagnostics=true$' "${WORK_DIR}/scan-valid.out" >/dev/null
rg '^markdown start=12 end=12$' "${WORK_DIR}/scan-valid.out" >/dev/null
rg '^example id=quoted\.one language=crexx opening=13 body-start=14 body-end=14 closing=15 run=false kind=standalone output=text fail-on-diagnostics=false timeout=25$' "${WORK_DIR}/scan-valid.out" >/dev/null
rg '^markdown start=16 end=19$' "${WORK_DIR}/scan-valid.out" >/dev/null

cat > "${WORK_DIR}/unterminated.md" <<'EOF_UNTERMINATED'
# Unterminated

```text
still markdown, but the fence must close
EOF_UNTERMINATED

run_crexx_capture unterminated "${RENDERER_UNDER_TEST}" -args --scan "${WORK_DIR}/unterminated.md"
assert_rc unterminated 1
assert_empty_stdout unterminated
rg 'unterminated\.md:3: error: unterminated fenced code block' "${WORK_DIR}/unterminated.err" >/dev/null

cat > "${WORK_DIR}/malformed.md" <<'EOF_MALFORMED'
# Malformed

```rexx id=bad id=again unknown=x run=yes output='bad/name' timeout=0 loose-token
say "bad"
```
EOF_MALFORMED

run_crexx_capture malformed "${RENDERER_UNDER_TEST}" -args --scan "${WORK_DIR}/malformed.md"
assert_rc malformed 1
assert_empty_stdout malformed
rg "duplicate attribute 'id'" "${WORK_DIR}/malformed.err" >/dev/null
rg "unknown attribute 'unknown'" "${WORK_DIR}/malformed.err" >/dev/null
rg "expected key=value attribute, got 'loose-token'" "${WORK_DIR}/malformed.err" >/dev/null
rg "run must be true or false, got 'yes'" "${WORK_DIR}/malformed.err" >/dev/null
rg "invalid output name 'bad/name'" "${WORK_DIR}/malformed.err" >/dev/null
rg "timeout must be a positive integer, got '0'" "${WORK_DIR}/malformed.err" >/dev/null

cat > "${WORK_DIR}/duplicate-id.md" <<'EOF_DUPLICATE'
```rexx id=dup
say "one"
```

~~~the id=dup
say "two"
~~~
EOF_DUPLICATE

run_crexx_capture duplicate-id "${RENDERER_UNDER_TEST}" -args --scan "${WORK_DIR}/duplicate-id.md"
assert_rc duplicate-id 1
assert_empty_stdout duplicate-id
rg "duplicate example id 'dup' first defined on line 1" "${WORK_DIR}/duplicate-id.err" >/dev/null

cat > "${WORK_DIR}/quoted-error.md" <<'EOF_QUOTED'
```rexx id="still-open
say "bad"
```
EOF_QUOTED

run_crexx_capture quoted-error "${RENDERER_UNDER_TEST}" -args --scan "${WORK_DIR}/quoted-error.md"
assert_rc quoted-error 1
assert_empty_stdout quoted-error
rg 'invalid fence attributes: unterminated quote' "${WORK_DIR}/quoted-error.err" >/dev/null

echo "Batch Markdown REXX scanner test passed."
