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
  src/utflayout.c
  src/utflayout.h
  src/utfrepair.c
  src/utfrepair.h
  src/utfterm.c
  src/utfterm.h
  src/llmdriver.c
  src/llmdriver.h
)

violations="$(
  rg -n "$pattern" "${logical_files[@]}" 2>/dev/null || true
)"

if [[ -n "$violations" ]]; then
  printf '%s\n' "Unexpected curses-boundary references outside the approved physical/debt files:"
  printf '%s\n' "$violations"
  exit 1
fi

printf '%s\n' "curses boundary check passed for logical foundation modules"
