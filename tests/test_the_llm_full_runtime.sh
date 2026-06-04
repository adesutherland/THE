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
utf_box="$work_dir/utf-box.txt"
utf_fill="$work_dir/utf-fill.txt"
utf_copy="$work_dir/utf-copy.txt"
utf_move="$work_dir/utf-move.txt"
out="$work_dir/out.jsonl"
utf_out="$work_dir/utf-box.jsonl"
utf_fill_out="$work_dir/utf-fill.jsonl"
utf_copy_out="$work_dir/utf-copy.jsonl"
utf_move_out="$work_dir/utf-move.jsonl"
err="$work_dir/err.log"

printf 'alpha beta gamma\nint main(void) { return 0; }\n' > "$sample"
printf 'second buffer\n' > "$second"
printf 'A\344\270\255B\n' > "$utf_box"
printf 'A\344\270\255B\n' > "$utf_fill"
printf 'A\344\270\255B\n----\n' > "$utf_copy"
printf 'A\344\270\255B\n----\n' > "$utf_move"

"$the_bin" -h > "$work_dir/default-help.txt"
"$the_bin" --driver curses -h > "$work_dir/curses-help.txt"
rg -- '--driver curses\|llm' "$work_dir/default-help.txt" >/dev/null
rg -- '--driver curses\|llm' "$work_dir/curses-help.txt" >/dev/null

printf '%s\n' \
  'capabilities' \
  'look full compact max=120' \
  'delta filearea compact max=120' \
  'focus command' \
  'text proto' \
  'type check' \
  'key left' \
  'hit command 0 4 2' \
  'focus filearea' \
  'hit filearea 1 1 0' \
  'key right' \
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
rg '"inputs":\["look","delta","capabilities","focus","hit","key","text","type","command","debug","transient","quit"\]' "$out" >/dev/null
rg '"crexx_macros":(true|false)' "$out" >/dev/null
if grep -aq 'CREXX unavailable' "$the_bin"; then
  rg '"crexx_macros":false' "$out" >/dev/null
else
  rg '"crexx_macros":true' "$out" >/dev/null
fi

rg '"role":"file"' "$out" >/dev/null
rg '"mode":"delta"' "$out" >/dev/null
rg '"status":"focus changed"' "$out" >/dev/null
rg '"status":"text applied"' "$out" >/dev/null
rg '"status":"key applied"' "$out" >/dev/null
rg '"status":"hit applied"' "$out" >/dev/null
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
rg '"status":"bye"' "$out" >/dev/null

printf '%s\n' \
  'command mark box 1 3 1 3' \
  'look full compact max=80 prefix=0 command=0 status=0 utf=all' \
  'command delete block' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_box" \
    >"$utf_out" 2>>"$err"

rg '"selection":\{"active":1,"start_line":1,"start_cell":3,"end_line":1,"end_cell":3' "$utf_out" >/dev/null
rg '\[1,2,2,2,2,2,"wide","native","none",0,0\]' "$utf_out" >/dev/null
rg '"t":"AB"' "$utf_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_out"; then
  echo "UTF box delete split a cluster" >&2
  cat "$utf_out" >&2
  exit 1
fi

printf '%s\n' \
  'command mark box 1 3 1 3' \
  'command fill X' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_fill" \
    >"$utf_fill_out" 2>>"$err"

rg '"t":"AXXB"' "$utf_fill_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_fill_out"; then
  echo "UTF box fill split a cluster" >&2
  cat "$utf_fill_out" >&2
  exit 1
fi

printf '%s\n' \
  'command mark box 1 3 1 3' \
  'hit filearea 2 1 1' \
  'command copy block' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_copy" \
    >"$utf_copy_out" 2>>"$err"

rg '"line":2,"cur":1,"t":"-中---"' "$utf_copy_out" >/dev/null
rg '\[1,2,2,2,2,2,"wide","native","none",0,0\]' "$utf_copy_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_copy_out"; then
  echo "UTF box copy split a cluster" >&2
  cat "$utf_copy_out" >&2
  exit 1
fi

printf '%s\n' \
  'command mark box 1 3 1 3' \
  'hit filearea 2 1 1' \
  'command move block' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_move" \
    >"$utf_move_out" 2>>"$err"

rg '"line":1,"cur":0,"t":"AB"' "$utf_move_out" >/dev/null
rg '"line":2,"cur":1,"t":"-中---"' "$utf_move_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_move_out"; then
  echo "UTF box move split a cluster" >&2
  cat "$utf_move_out" >&2
  exit 1
fi

if rg -q 'Error opening terminal|setupterm|initscr' \
     "$out" "$utf_out" "$utf_fill_out" "$utf_copy_out" "$utf_move_out" "$err"; then
  echo "llm driver appeared to initialize curses" >&2
  cat "$out" >&2
  cat "$utf_out" >&2
  cat "$utf_fill_out" >&2
  cat "$utf_copy_out" >&2
  cat "$utf_move_out" >&2
  cat "$err" >&2
  exit 1
fi

if rg -q 'Unable to update CREXX variable' \
     "$out" "$utf_out" "$utf_fill_out" "$utf_copy_out" "$utf_move_out" "$err"; then
  echo "llm command modal continuation tried to write Rexx variables without an active macro" >&2
  cat "$out" >&2
  cat "$utf_out" >&2
  cat "$utf_fill_out" >&2
  cat "$utf_copy_out" >&2
  cat "$utf_move_out" >&2
  cat "$err" >&2
  exit 1
fi

exit 0
