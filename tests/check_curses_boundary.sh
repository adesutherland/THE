#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-.}"
cd "$repo_root"

pattern='(^|[^A-Za-z0-9_])(WINDOW|SCREEN_WINDOW|CURRENT_WINDOW|getyx|wmove|mvwadd|wadd|wattr|touchline|touchwin|wnoutrefresh|doupdate|draw_cursor|curs_set|keypad)[[:space:]]*(\(|$)'

# These modules are already part of the driver-free logical foundation. They
# must remain curses-free while legacy files are migrated behind the driver.
logical_files=(
  src/logcursor.c
  src/logcursor.h
  src/textpos.c
  src/textpos.h
  src/textedit.c
  src/textedit.h
  src/inputevent.c
  src/inputevent.h
  src/mousehit.c
  src/mousehit.h
  src/agentdriver.c
  src/agentdriver.h
  src/utflayout.c
  src/utflayout.h
  src/utfrepair.c
  src/utfrepair.h
  src/utfterm.c
  src/utfterm.h
  src/llmdriver.c
  src/llmdriver.h
  src/transientui.c
  src/transientui.h
  src/uidriver.c
  src/uidriver.h
  tools/the_agent.c
)

violations="$(
  rg -n "$pattern" "${logical_files[@]}" 2>/dev/null || true
)"

if [[ -n "$violations" ]]; then
  printf '%s\n' "Unexpected curses-boundary references outside the approved physical/debt files:"
  printf '%s\n' "$violations"
  exit 1
fi

thedriver_public_violations="$(
  rg -n 'WINDOW|chtype|cchar_t' src/thedriver.h 2>/dev/null || true
)"

if [[ -n "$thedriver_public_violations" ]]; then
  printf '%s\n' "Unexpected curses public type in src/thedriver.h:"
  printf '%s\n' "$thedriver_public_violations"
  exit 1
fi

execute_pattern='(^|[^A-Za-z0-9_])(attrset|clear|wclear|move|mvaddstr|addch|refresh|wrefresh|getyx|getbegyx|wmove|newwin|newpad|derwin|subwin|delwin|keypad|wbkgd|wattrset|wclrtobot|box|waddstr|waddch|touchwin|wnoutrefresh|prefresh|wgetch|my_getch|wmouse_position|whline|draw_cursor|force_curses_background)[[:space:]]*\('
execute_violations="$(
  rg -n "$execute_pattern" src/execute.c 2>/dev/null || true
)"

if [[ -n "$execute_violations" ]]; then
  printf '%s\n' "Unexpected direct curses calls in src/execute.c; use cursesdriver wrappers:"
  printf '%s\n' "$execute_violations"
  exit 1
fi

extract_function() {
  local file=$1
  local name=$2
  awk -v name="$name" '
    $0 ~ "^[[:alpha:]_][[:alnum:]_[:space:]\\*]*[[:space:]]+" name "[[:space:]]*\\(" {
      in_func=1
    }
    in_func {
      print
      opens += gsub(/\{/, "{")
      closes += gsub(/\}/, "}")
      if (opens > 0 && opens == closes)
        exit
    }
  ' "$file"
}

transient_pattern='(^|[^A-Za-z0-9_])(attrset|clear|wclear|move|mvaddstr|addch|refresh|wrefresh|wnoutrefresh|getyx|getbegyx|wmove|newwin|newpad|derwin|subwin|delwin|keypad|wbkgd|wattrset|wclrtobot|box|waddstr|waddch|touchwin|wnoutrefresh|prefresh|wgetch|my_getch|wmouse_position|get_mouse_info|whline|draw_cursor|force_curses_background)[[:space:]]*\('
transient_violations="$(
  {
    extract_function src/commutil.c readv_cmdline
    extract_function src/execute.c execute_dialog
    extract_function src/execute.c execute_popup
  } | rg -n "$transient_pattern" 2>/dev/null || true
)"

if [[ -n "$transient_violations" ]]; then
  printf '%s\n' "Unexpected direct curses calls in cleaned readv/dialog/popup transient UI paths:"
  printf '%s\n' "$transient_violations"
  exit 1
fi

sos_pattern='(^|[^A-Za-z0-9_])(getyx|wmove|mvwadd|wadd|touchline|touchwin|wnoutrefresh|doupdate|draw_cursor|curs_set|keypad|curses_driver_capture_window_cursor)[[:space:]]*\('
sos_violations="$(
  rg -n "$sos_pattern" src/commsos.c 2>/dev/null || true
)"

if [[ -n "$sos_violations" ]]; then
  printf '%s\n' "Unexpected physical cursor/window calls in src/commsos.c SOS surface:"
  printf '%s\n' "$sos_violations"
  exit 1
fi

printf '%s\n' "curses boundary check passed for logical modules, agent, execute.c wrappers, and SOS cursor surface"
