#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the" >&2
  exit 2
fi

the_bin=$1
repo_root=$(cd "$(dirname "$0")/.." && pwd)
release_dir=$(cd "$(dirname "$the_bin")" && pwd)/release
crexx_bin_dir="${CREXX_BIN_DIR:-${repo_root}/../CREXX/cmake-build-debug/bin}"
rxc="${THE_CREXX_RXC:-${crexx_bin_dir}/rxc}"
rxas="${THE_CREXX_RXAS:-${crexx_bin_dir}/rxas}"
library_rxbin="${THE_CREXX_LIBRARY_RXBIN:-${crexx_bin_dir}/library.rxbin}"

if [[ ! -x "$the_bin" ]]; then
  echo "Skipping LLM CREXX profile test; THE build output is missing" >&2
  exit 77
fi

if grep -aq "CREXX unavailable" "$the_bin"; then
  echo "Skipping LLM CREXX profile test; THE was built without CREXX" >&2
  exit 77
fi

if [[ ! -x "$rxc" || ! -x "$rxas" || ! -f "$library_rxbin" ]]; then
  echo "Skipping LLM CREXX profile test; CREXX tools or import library are missing" >&2
  exit 77
fi

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

profile="$work_dir/profile.the"
sample="$work_dir/sample.txt"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"
cache_dir="$work_dir/cache"

cat > "$profile" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the
'input LLM_PROFILE_INSERTED'
'emsg LLM_PROFILE_OK'
PROFILE_EOF

printf 'alpha\n' > "$sample"

printf '%s\n' capabilities 'look filearea compact max=80' quit |
  env \
    TERM= \
    THE_HOME_DIR="$release_dir" \
    THE_CREXX_RXC="$rxc" \
    THE_CREXX_RXAS="$rxas" \
    THE_CREXX_IMPORT_DIR="$crexx_bin_dir" \
    THE_CREXX_LOCATION="$crexx_bin_dir" \
    THE_CREXX_LIBRARY_RXBIN="$library_rxbin" \
    CREXXSAA_CACHE_DIR="$cache_dir" \
    "$the_bin" --driver llm -p "$profile" "$sample" \
    >"$out" 2>"$err"

rg '"crexx_macros":true' "$out" >/dev/null
rg 'LLM_PROFILE_OK' "$out" "$err" >/dev/null
rg 'LLM_PROFILE_INSERTED' "$out" >/dev/null
rg '"dirty":1' "$out" >/dev/null

if rg -q 'Error opening terminal|setupterm|initscr' "$out" "$err"; then
  echo "llm CREXX profile test appeared to initialize curses" >&2
  cat "$out" "$err" >&2
  exit 1
fi
