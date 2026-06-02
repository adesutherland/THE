#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/the" >&2
  exit 2
fi

the_bin=$1
repo_root=$(cd "$(dirname "$0")/.." && pwd)
release_dir=$(cd "$(dirname "$the_bin")" && pwd)/release
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

sample="$work_dir/sample.c"
second="$work_dir/second.txt"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"

printf 'alpha beta gamma\nint main(void) { return 0; }\n' > "$sample"
printf 'second buffer\n' > "$second"

"$the_bin" -h > "$work_dir/default-help.txt"
"$the_bin" --driver curses -h > "$work_dir/curses-help.txt"
rg -- '--driver curses\|llm' "$work_dir/default-help.txt" >/dev/null
rg -- '--driver curses\|llm' "$work_dir/curses-help.txt" >/dev/null

printf '%s\n' \
  'capabilities' \
  'look full compact max=120' \
  'command c/beta/OMEGA/' \
  'look filearea compact max=120' \
  'command colouring on auto' \
  'look filearea compact max=120' \
  'debug dump-driver-ops' \
  'transient readv seed' \
  'transient text X' \
  'transient key enter' \
  'transient result' \
  'transient dialog modal' \
  'transient key tab' \
  'transient key enter' \
  'transient result' \
  'transient popup' \
  'transient key down' \
  'transient hit 2 8' \
  'transient result' \
  'transient close' \
  'command set pending on d' \
  'look prefix compact max=120' \
  'command mark stream 1 1 2 10' \
  'look full compact max=120' \
  "command edit $second" \
  'look full compact max=120' \
  'command readv cmdline cmdseed' \
  'transient look' \
  'transient text Y' \
  'transient key enter' \
  'transient result' \
  'command dialog /Prompt/ editfield /seed/ title /LLM/ okcancel' \
  'transient look' \
  'transient text Z' \
  'transient key tab' \
  'transient key enter' \
  'transient result' \
  'command popup center initial 2 /One/Two/---/Three/' \
  'transient look' \
  'transient key down' \
  'transient key enter' \
  'transient result' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$sample" \
    >"$out" 2>"$err"

rg '"surface":"the"' "$out" >/dev/null
rg '"driver":"llm"' "$out" >/dev/null
rg '"curses":false' "$out" >/dev/null
rg '"command_dispatcher":"full-the"' "$out" >/dev/null
rg '"full_the_dispatcher":true' "$out" >/dev/null
rg '"real_buffers":true' "$out" >/dev/null
rg '"syntax_style_spans":true' "$out" >/dev/null
rg '"parser_diagnostics":"first-class-snapshot-array"|"parser_diagnostics":"unavailable-in-this-build"' "$out" >/dev/null
rg '"transient_ui":"shared-transientui-protocol-adapter"' "$out" >/dev/null
rg '"crexx_macros":(true|false)' "$out" >/dev/null
if grep -aq 'CREXX unavailable' "$the_bin"; then
  rg '"crexx_macros":false' "$out" >/dev/null
else
  rg '"crexx_macros":true' "$out" >/dev/null
fi

rg '"role":"file"' "$out" >/dev/null
rg '"line":1' "$out" >/dev/null
rg 'alpha beta gamma' "$out" >/dev/null
rg 'alpha OMEGA gamma' "$out" >/dev/null
rg '"dirty":1' "$out" >/dev/null
rg '"buffers":\[' "$out" >/dev/null
rg '"path":"[^"]*sample\.c","dirty":1,"lines":2,"current":0' "$out" >/dev/null
rg '"path":"[^"]*second\.txt","dirty":0,"lines":1,"current":1' "$out" >/dev/null
rg 'second buffer' "$out" >/dev/null
rg '"p":"000001"' "$out" >/dev/null
rg '"p":"d"' "$out" >/dev/null
rg '"selection":\{"active":1,"start_line":1,"start_cell":1,"end_line":2,"end_cell":10' "$out" >/dev/null
rg '"s":\[' "$out" >/dev/null
rg '"debug":"dump-driver-ops"' "$out" >/dev/null
rg '"kind":"readv"' "$out" >/dev/null
rg '"text":"seedX"' "$out" >/dev/null
rg '"kind":"dialog"' "$out" >/dev/null
rg '"action":"accept"' "$out" >/dev/null
rg '"kind":"popup"' "$out" >/dev/null
rg '"selected_item":1' "$out" >/dev/null
rg '"status":"transient cleared"' "$out" >/dev/null
rg '"kind":"readv","source":"command-readv","action":"accept","committed":1,"text":"cmdseedY"' "$out" >/dev/null
rg '"kind":"dialog","source":"command-dialog","action":"accept","committed":1,"edit_text":"seedZ","selected_button":0' "$out" >/dev/null
rg '"kind":"popup","source":"command-popup","action":"accept","committed":1,"selected_item":3' "$out" >/dev/null
rg '"title":" LLM "' "$out" >/dev/null
rg '"text":"Two"' "$out" >/dev/null

if rg -q 'Error opening terminal|setupterm|initscr' "$out" "$err"; then
  echo "llm driver appeared to initialize curses" >&2
  cat "$out" >&2
  cat "$err" >&2
  exit 1
fi

if rg -q 'Unable to update CREXX variable' "$out" "$err"; then
  echo "llm command modal continuation tried to write Rexx variables without an active macro" >&2
  cat "$out" >&2
  cat "$err" >&2
  exit 1
fi

exit 0
