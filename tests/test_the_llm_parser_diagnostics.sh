#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the" >&2
  exit 2
fi

the_bin=$1
repo_root=$(cd "$(dirname "$0")/.." && pwd)
release_dir=$(cd "$(dirname "$the_bin")" && pwd)/release

find_tp() {
  local candidate

  for candidate in \
    "${THE_SDSLH_TP:-}" \
    "${HOME}/.local/bin/tp" \
    "${repo_root}/../DSL-Syntax-Highlighter/cmake-build-debug/toyparser/tp"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

if [[ ! -x "$the_bin" ]]; then
  echo "Skipping LLM parser diagnostics test; THE build output is missing" >&2
  exit 77
fi

if ! tp_bin=$(find_tp); then
  echo "Skipping LLM parser diagnostics test; toy parser is missing" >&2
  exit 77
fi

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

sample="$work_dir/sample.toy"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"

printf 'int x = 1\n' > "$sample"

printf '%s\n' \
  "command set sdslh tp ${tp_bin} -d" \
  'command set autocolor *.toy tp' \
  'command colouring on auto' \
  'command sdslhwait 5000' \
  'look full compact max=120' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$sample" \
    >"$out" 2>"$err"

rg '"diagnostics":\[' "$out" >/dev/null
rg '"line":2' "$out" >/dev/null
rg '"column":1' "$out" >/dev/null
rg '"severity":"ERROR"' "$out" >/dev/null
rg '"code":"-"' "$out" >/dev/null
rg '"message":"Expected semicolon after statement"' "$out" >/dev/null

if rg -q 'Error opening terminal|setupterm|initscr' "$out" "$err"; then
  echo "llm parser diagnostics test appeared to initialize curses" >&2
  cat "$out" "$err" >&2
  exit 1
fi
