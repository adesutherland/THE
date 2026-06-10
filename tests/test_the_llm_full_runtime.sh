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
utf_shift="$work_dir/utf-shift.txt"
utf_prefix_shift="$work_dir/utf-prefix-shift.txt"
utf_prefix_bounds_left="$work_dir/utf-prefix-bounds-left.txt"
utf_prefix_bounds_right="$work_dir/utf-prefix-bounds-right.txt"
utf_prefix_case="$work_dir/utf-prefix-case.txt"
utf_cua_overlay="$work_dir/utf-cua-overlay.txt"
utf_keycap="$work_dir/utf-keycap.txt"
utf_flag="$work_dir/utf-flag.txt"
utf_replay="$work_dir/utf-replay.txt"
utf_entry="$work_dir/utf-entry.txt"
llm_usability="$work_dir/llm-usability.txt"
out="$work_dir/out.jsonl"
utf_out="$work_dir/utf-box.jsonl"
utf_fill_out="$work_dir/utf-fill.jsonl"
utf_copy_out="$work_dir/utf-copy.jsonl"
utf_move_out="$work_dir/utf-move.jsonl"
utf_shift_out="$work_dir/utf-shift.jsonl"
utf_prefix_shift_out="$work_dir/utf-prefix-shift.jsonl"
utf_prefix_bounds_left_out="$work_dir/utf-prefix-bounds-left.jsonl"
utf_prefix_bounds_right_out="$work_dir/utf-prefix-bounds-right.jsonl"
utf_prefix_case_out="$work_dir/utf-prefix-case.jsonl"
utf_cua_overlay_out="$work_dir/utf-cua-overlay.jsonl"
utf_keycap_out="$work_dir/utf-keycap.jsonl"
utf_flag_out="$work_dir/utf-flag.jsonl"
utf_replay_seed_out="$work_dir/utf-replay-seed.jsonl"
utf_replay_out="$work_dir/utf-replay.jsonl"
utf_entry_out="$work_dir/utf-entry.jsonl"
llm_usability_out="$work_dir/llm-usability.jsonl"
utf_replay_rules="$work_dir/utf-replay.rules"
utf_replay_seed_debug="$work_dir/utf-replay-seed.debug"
utf_replay_debug="$work_dir/utf-replay.debug"
err="$work_dir/err.log"

printf 'alpha beta gamma\nint main(void) { return 0; }\n' > "$sample"
printf 'second buffer\n' > "$second"
printf 'A\344\270\255B\n' > "$utf_box"
printf 'A\344\270\255B\n' > "$utf_fill"
printf 'A\344\270\255B\n----\n' > "$utf_copy"
printf 'A\344\270\255B\n----\n' > "$utf_move"
printf 'A\344\270\255B\n' > "$utf_shift"
printf 'A\344\270\255B\nC\344\270\255D\n' > "$utf_prefix_shift"
printf 'A\344\270\255B\nC\344\270\255D\n' > "$utf_prefix_bounds_left"
printf 'A\344\270\255B\nC\344\270\255D\n' > "$utf_prefix_bounds_right"
printf 'a\344\270\255b\nc\344\270\255d\n' > "$utf_prefix_case"
printf 'A\344\270\255B\nC\344\270\255D\nX\344\270\255Y\n' > "$utf_cua_overlay"
printf 'A1\357\270\217\342\203\243B\n' > "$utf_keycap"
printf 'A\360\237\207\272\360\237\207\270B\n' > "$utf_flag"
printf 'A1\357\270\217\342\203\243B\nA\360\237\207\272\360\237\207\270B\n' > "$utf_replay"
printf '\n' > "$utf_entry"
printf 'TODO alpha\nbravo lower\ncharlie lower\n' > "$llm_usability"

"$the_bin" -h > "$work_dir/default-help.txt"
"$the_bin" --driver curses -h > "$work_dir/curses-help.txt"
rg -- '--driver curses\|llm' "$work_dir/default-help.txt" >/dev/null
rg -- '--driver curses\|llm' "$work_dir/curses-help.txt" >/dev/null

printf '%s\n' \
  'capabilities' \
  'debug utf-display' \
  'command set utf display decomposed' \
  'debug utf-display' \
  'command set utf display normal' \
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
rg '"utf":true' "$out" >/dev/null
rg '"utf_display_mode":"normal"' "$out" >/dev/null
rg '"inputs":\["look","delta","capabilities","focus","hit","key","text","type","text-utf","insert","insert-utf","command","debug","transient","quit"\]' "$out" >/dev/null
rg '"debug_commands":\[[^]]*"utf-display"' "$out" >/dev/null
rg '"debug":"utf-display","supported":true,"active_mode":"normal"' "$out" >/dev/null
rg '"debug":"utf-display","supported":true,"active_mode":"decomposed"' "$out" >/dev/null
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
  'command c/TODO/DONE/' \
  'focus filearea' \
  'insert after 1 inserted by llm protocol' \
  'command set zone 1 20' \
  'hit filearea 3 1 0' \
  'command set pending block ucc' \
  'hit filearea 4 1 0' \
  'command set pending block ucc' \
  'command sos doprefix' \
  'look filearea compact max=120 prefix=1 command=0 status=0' \
  'command save' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$llm_usability" \
    >"$llm_usability_out" 2>>"$err"

rg '"status":"command dispatched","message_changed":1,"last_message":"1 occurrence\(s\) changed' "$llm_usability_out" >/dev/null
rg '"status":"focus changed","message_changed":0' "$llm_usability_out" >/dev/null
if awk '/"status":"focus changed"/ && /"last_message"/ {found=1} END {exit found ? 0 : 1}' "$llm_usability_out"; then
  echo "llm ack repeated a stale last_message" >&2
  cat "$llm_usability_out" >&2
  exit 1
fi
rg '"status":"insert applied","message_changed":0.*"buffer":\{"path":"[^"]*llm-usability\.txt","dirty":1,"lines":4\}' "$llm_usability_out" >/dev/null
rg '"pending_prefix":\{"count":1,"commands":\[\{"line":3,"command":"ucc","original":"ucc","cmd_idx":[0-9-]+,"block":1' "$llm_usability_out" >/dev/null
rg '"pending_prefix":\{"count":2,"commands":\[\{"line":3,"command":"ucc","original":"ucc","cmd_idx":[0-9-]+,"block":1[^]]*\{"line":4,"command":"ucc","original":"ucc","cmd_idx":[0-9-]+,"block":1' "$llm_usability_out" >/dev/null
rg '"line":2,"cur":0,"p":"000002","t":"inserted by llm protocol"' "$llm_usability_out" >/dev/null
rg '"line":3,"cur":0,"p":"000003","t":"BRAVO LOWER"' "$llm_usability_out" >/dev/null
rg '"line":4,"cur":1,"p":"000004","t":"CHARLIE LOWER"' "$llm_usability_out" >/dev/null
printf 'DONE alpha\ninserted by llm protocol\nBRAVO LOWER\nCHARLIE LOWER\n' > "$work_dir/llm-usability.expected"
cmp "$work_dir/llm-usability.expected" "$llm_usability"

printf '%s\n' \
  'command set insertmode on' \
  'focus filearea' \
  'hit filearea 1 1 0' \
  'command utftext U+0041 U+4E2D U+0042' \
  'text X中Y' \
  'text-utf U+1F1FA+1F1F8' \
  'insert-utf after 1 U+0043 U+4E2D U+0044' \
  'look filearea compact max=120 prefix=0 command=0 status=0 utf=all' \
  'focus command' \
  'command utftext U+0045 U+0046' \
  'look full compact max=120 prefix=0 command=1 status=0 cursor=1' \
  'command save' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_entry" \
    >"$utf_entry_out" 2>>"$err"

rg '"status":"text applied"' "$utf_entry_out" >/dev/null
rg '"status":"insert applied"' "$utf_entry_out" >/dev/null
rg '"line":1,"cur":0,"t":"A中BX中Y🇺🇸"' "$utf_entry_out" >/dev/null
rg '"line":2,"cur":1,"t":"C中D"' "$utf_entry_out" >/dev/null
rg '"command":"EF"' "$utf_entry_out" >/dev/null
printf 'A\344\270\255BX\344\270\255Y\360\237\207\272\360\237\207\270\nC\344\270\255D\n' > "$work_dir/utf-entry.expected"
cmp "$work_dir/utf-entry.expected" "$utf_entry"

printf '%s\n' \
  'command set utf display normal class keycap output sanitize keycap metrics output mark compressed width 1 advance 1 cursor 1 repaint 1 cursorstrategy cells replacestrategy cells' \
  'debug utf-display' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'command set utf display decomposed' \
  'debug utf-display' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'command set utf display single' \
  'debug utf-display' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_keycap" \
    >"$utf_keycap_out" 2>>"$err"

rg '"debug":"utf-display","supported":true,"active_mode":"normal"' "$utf_keycap_out" >/dev/null
rg '"debug":"utf-display","supported":true,"active_mode":"decomposed"' "$utf_keycap_out" >/dev/null
rg '"debug":"utf-display","supported":true,"active_mode":"single"' "$utf_keycap_out" >/dev/null
rg 'SET UTF DISPLAY NORMAL CLASS keycap OUTPUT SANITIZE METRICS OUTPUT MARK COMPRESSED WIDTH 1 ADVANCE 1 CURSOR 1 REPAINT 1 DISPLAYSTRATEGY INLINE CURSORSTRATEGY CELLS REPLACESTRATEGY CELLS' "$utf_keycap_out" >/dev/null
rg '\[1,1,1,1,1,1,"keycap","sanitize","compressed",1,0,"normal","keycap","base","output","1"\]' "$utf_keycap_out" >/dev/null
rg '\[1,1,3,3,3,3,"keycap","components","none",0,0,"decomposed","keycap","components","components","[^"]+"\]' "$utf_keycap_out" >/dev/null
rg '\[1,1,1,1,1,1,"keycap","base","none",0,0,"single","keycap","base","profile","1"\]' "$utf_keycap_out" >/dev/null

printf '%s\n' \
  'command set utf display decomposed class regional-flag output components metrics components displaystrategy isolate' \
  'command set utf display decomposed' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'debug dump-driver-ops' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_flag" \
    >"$utf_flag_out" 2>>"$err"

rg '\[1,2,5,5,5,5,"regional-flag","components","none",0,0,"decomposed","regional-flag","components","components","[^"]+"\]' "$utf_flag_out" >/dev/null
rg 'render-cluster:window:[0-9]+:[0-9]+:1:1:1:2:2:2:2' "$utf_flag_out" >/dev/null
rg 'render-cluster:window:[0-9]+:[0-9]+:3:1:1:1:1:1:1' "$utf_flag_out" >/dev/null
rg 'render-cluster:window:[0-9]+:[0-9]+:4:1:1:2:2:2:2' "$utf_flag_out" >/dev/null
rg 'overlay-attrs:window:[0-9]+:[0-9]+:1:5' "$utf_flag_out" >/dev/null

printf '%s\n' \
  'command set utf display normal class keycap output sanitize keycap metrics output mark compressed width 1 advance 1 cursor 1 repaint 1 cursorstrategy cells replacestrategy cells' \
  'command set utf display normal class modifier output native metrics profile mark none width 2 advance 4 cursor 4 repaint 4 cursorstrategy cells replacestrategy line' \
  'command set utf display normal class regional-flag output native metrics profile mark none width 2 advance 3 cursor 3 repaint 3 cursorstrategy cells replacestrategy cells' \
  'command set utf display decomposed class regional-flag output components metrics components displaystrategy isolate cursorstrategy cells replacestrategy cells' \
  'command set utf display normal class short-zwj output substitute U+25A1 metrics output mark substituted width 1 advance 1 cursor 1 repaint 1 cursorstrategy cells replacestrategy cells' \
  'command set utf display single' \
  'debug utf-display' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_replay" \
    >"$utf_replay_seed_out" 2>>"$err"

rg '"debug":"utf-display","supported":true,"active_mode":"single"' "$utf_replay_seed_out" | tail -n 1 > "$utf_replay_seed_debug"
active_mode="$(sed -E 's/.*"active_mode":"([^"]+)".*/\1/' "$utf_replay_seed_debug")"
{
  printf 'command set utf display %s\n' "$active_mode"
  rg -o '"SET UTF DISPLAY [^"]+"' "$utf_replay_seed_debug" |
    sed 's/^"/command /;s/"$//'
  printf 'debug utf-display\n'
  printf 'quit\n'
} > "$utf_replay_rules"

TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_replay" \
  <"$utf_replay_rules" >"$utf_replay_out" 2>>"$err"
rg '"debug":"utf-display","supported":true,"active_mode":"single"' "$utf_replay_out" | tail -n 1 > "$utf_replay_debug"
cmp "$utf_replay_seed_debug" "$utf_replay_debug"

printf '%s\n' \
  'command mark box 1 3 1 3' \
  'look full compact max=80 prefix=0 command=0 status=0 utf=all' \
  'command delete block' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_box" \
    >"$utf_out" 2>>"$err"

rg '"selection":\{"active":1,"start_line":1,"start_cell":3,"end_line":1,"end_cell":3' "$utf_out" >/dev/null
rg '\[1,2,2,2,2,2,"wide","native","none",0,0,"normal","wide","native","profile",""\]' "$utf_out" >/dev/null
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
rg '\[1,2,2,2,2,2,"wide","native","none",0,0,"normal","wide","native","profile",""\]' "$utf_copy_out" >/dev/null
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

printf '%s\n' \
  'command shift left 2 1' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_shift" \
    >"$utf_shift_out" 2>>"$err"

rg '"line":1,"cur":1,"t":"B"' "$utf_shift_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_shift_out"; then
  echo "UTF shift split a cluster" >&2
  cat "$utf_shift_out" >&2
  exit 1
fi

printf '%s\n' \
  'hit filearea 1 1 0' \
  'command set pending block 2<<' \
  'hit filearea 2 1 0' \
  'command set pending block 2<<' \
  'command sos doprefix' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_prefix_shift" \
    >"$utf_prefix_shift_out" 2>>"$err"

rg '"line":1,"cur":0,"t":"B"' "$utf_prefix_shift_out" >/dev/null
rg '"line":2,"cur":1,"t":"D"' "$utf_prefix_shift_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_prefix_shift_out"; then
  echo "UTF prefix block shift split a cluster" >&2
  cat "$utf_prefix_shift_out" >&2
  exit 1
fi

printf '%s\n' \
  'command set zone 2 4' \
  'hit filearea 1 1 0' \
  'command set pending block ((' \
  'hit filearea 2 1 0' \
  'command set pending block ((' \
  'command sos doprefix' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_prefix_bounds_left" \
    >"$utf_prefix_bounds_left_out" 2>>"$err"

rg '"line":1,"cur":0,"t":"AB  "' "$utf_prefix_bounds_left_out" >/dev/null
rg '"line":2,"cur":1,"t":"CD  "' "$utf_prefix_bounds_left_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_prefix_bounds_left_out"; then
  echo "UTF prefix bounded left shift split a cluster" >&2
  cat "$utf_prefix_bounds_left_out" >&2
  exit 1
fi

printf '%s\n' \
  'command set zone 1 3' \
  'hit filearea 1 1 0' \
  'command set pending block ))' \
  'hit filearea 2 1 0' \
  'command set pending block ))' \
  'command sos doprefix' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_prefix_bounds_right" \
    >"$utf_prefix_bounds_right_out" 2>>"$err"

rg '"line":1,"cur":0,"t":"  AB"' "$utf_prefix_bounds_right_out" >/dev/null
rg '"line":2,"cur":1,"t":"  CD"' "$utf_prefix_bounds_right_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_prefix_bounds_right_out"; then
  echo "UTF prefix bounded right shift split a cluster" >&2
  cat "$utf_prefix_bounds_right_out" >&2
  exit 1
fi

printf '%s\n' \
  'command set zone 2 4' \
  'hit filearea 1 1 0' \
  'command set pending block ucc' \
  'hit filearea 2 1 0' \
  'command set pending block ucc' \
  'command sos doprefix' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_prefix_case" \
    >"$utf_prefix_case_out" 2>>"$err"

rg '"line":1,"cur":0,"t":"a中B"' "$utf_prefix_case_out" >/dev/null
rg '"line":2,"cur":1,"t":"c中D"' "$utf_prefix_case_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_prefix_case_out"; then
  echo "UTF prefix case conversion split a cluster" >&2
  cat "$utf_prefix_case_out" >&2
  exit 1
fi

printf '%s\n' \
  'hit filearea 1 1 1' \
  'command mark cua' \
  'hit filearea 2 1 3' \
  'command mark cua' \
  'hit filearea 3 1 1' \
  'command overlaybox' \
  'look filearea compact max=80 prefix=0 command=0 status=0 utf=all' \
  'quit' |
  TERM= THE_HOME_DIR="$release_dir" "$the_bin" --driver llm -n "$utf_cua_overlay" \
    >"$utf_cua_overlay_out" 2>>"$err"

rg '"line":3,"cur":1,"t":"中BY"' "$utf_cua_overlay_out" >/dev/null
rg '"line":4,"cur":0,"t":"C中D"' "$utf_cua_overlay_out" >/dev/null
if LC_ALL=C rg -q "$(printf '\357\277\275')" "$utf_cua_overlay_out"; then
  echo "UTF CUA stream overlay split a cluster" >&2
  cat "$utf_cua_overlay_out" >&2
  exit 1
fi

if rg -q 'Error opening terminal|setupterm|initscr' \
     "$out" "$utf_out" "$utf_fill_out" "$utf_copy_out" "$utf_move_out" \
     "$utf_shift_out" "$utf_prefix_shift_out" "$utf_prefix_bounds_left_out" \
     "$utf_prefix_bounds_right_out" "$utf_prefix_case_out" \
     "$utf_cua_overlay_out" "$utf_keycap_out" "$utf_flag_out" "$err"; then
  echo "llm driver appeared to initialize curses" >&2
  cat "$out" >&2
  cat "$utf_out" >&2
  cat "$utf_fill_out" >&2
  cat "$utf_copy_out" >&2
  cat "$utf_move_out" >&2
  cat "$utf_shift_out" >&2
  cat "$utf_prefix_shift_out" >&2
  cat "$utf_prefix_bounds_left_out" >&2
  cat "$utf_prefix_bounds_right_out" >&2
  cat "$utf_prefix_case_out" >&2
  cat "$utf_cua_overlay_out" >&2
  cat "$utf_keycap_out" >&2
  cat "$utf_flag_out" >&2
  cat "$err" >&2
  exit 1
fi

if rg -q 'Unable to update CREXX variable' \
     "$out" "$utf_out" "$utf_fill_out" "$utf_copy_out" "$utf_move_out" \
     "$utf_shift_out" "$utf_prefix_shift_out" "$utf_prefix_bounds_left_out" \
     "$utf_prefix_bounds_right_out" "$utf_prefix_case_out" \
     "$utf_cua_overlay_out" "$utf_keycap_out" "$utf_flag_out" "$err"; then
  echo "llm command modal continuation tried to write Rexx variables without an active macro" >&2
  cat "$out" >&2
  cat "$utf_out" >&2
  cat "$utf_fill_out" >&2
  cat "$utf_copy_out" >&2
  cat "$utf_move_out" >&2
  cat "$utf_shift_out" >&2
  cat "$utf_prefix_shift_out" >&2
  cat "$utf_prefix_bounds_left_out" >&2
  cat "$utf_prefix_bounds_right_out" >&2
  cat "$utf_prefix_case_out" >&2
  cat "$utf_cua_overlay_out" >&2
  cat "$utf_keycap_out" >&2
  cat "$utf_flag_out" >&2
  cat "$err" >&2
  exit 1
fi

exit 0
