#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:-.}"
cd "$repo_root"

files=()
while IFS= read -r file; do
  files+=("$file")
done < <(
  find src -type f \( -name '*.c' -o -name '*.h' \) \
    ! -path 'src/PDCursesMod/*' \
    ! -path 'src/contrib/*' \
    ! -name 'cursesdriver.c' \
    ! -name 'cursesdriver.h' \
    | sort
)

if [[ ${#files[@]} -eq 0 ]]; then
  echo "direct curses inventory: no files scanned"
  exit 0
fi

awk '
function category(line) {
  if (line ~ /(^|[^A-Za-z0-9_])curses_driver_[A-Za-z0-9_]*[[:space:]]*\(/)
    return "driver-wrapper"
  if (line ~ /(^|[^A-Za-z0-9_])(my_getch|wgetch|getch|get_mouse_info|wmouse_position)[[:space:]]*\(/)
    return "physical-input"
  if (line ~ /(^|[^A-Za-z0-9_])(wmove|mvwadd|wadd|wattr|touchline|touchwin|wnoutrefresh|doupdate|wrefresh|refresh|newwin|newpad|derwin|subwin|delwin|keypad|wbkgd|box|whline|prefresh|curs_set|draw_cursor)[[:space:]]*\(/)
    return "physical-paint"
  if (line ~ /(^|[^A-Za-z0-9_])(KEY_MOUSE|BUTTON_PRESSED|BUTTON_RELEASED|BUTTON_CLICKED)([^A-Za-z0-9_]|$)/)
    return "mouse-token"
  if (line ~ /(^|[^A-Za-z0-9_])(WINDOW|SCREEN_WINDOW|CURRENT_WINDOW|CURRENT_WINDOW_[A-Za-z0-9_]*|SCREEN_WINDOW_[A-Za-z0-9_]*|stdscr|chtype|cchar_t)([^A-Za-z0-9_]|$)/)
    return "window-state"
  return ""
}
function trim(s) {
  sub(/^[[:space:]]+/, "", s)
  sub(/[[:space:]]+$/, "", s)
  return s
}
FNR == 1 {
  fn = "file-scope"
}
/^[[:alpha:]_][[:alnum:]_[:space:]\*]*[[:space:]]+[[:alpha:]_][[:alnum:]_]*[[:space:]]*\(/ {
  sig = $0
  sub(/\(.*/, "", sig)
  n = split(sig, parts, /[[:space:]\*]+/)
  if (n > 0 && parts[n] != "")
    fn = parts[n]
}
{
  line = $0
  cat = category(line)
  if (cat != "") {
    printf "%s:%d:%s:%s:%s\n", FILENAME, FNR, fn, cat, trim(line)
  }
}
' "${files[@]}" |
  awk '
  BEGIN {
    print "direct curses inventory (excluding src/cursesdriver.*, PDCurses, contrib):"
  }
  {
    print
    count++
    split($0, parts, ":")
    bycat[parts[4]]++
  }
  END {
    if (count == 0) {
      print "none"
    } else {
      print "summary:"
      for (cat in bycat)
        printf "  %s: %d\n", cat, bycat[cat]
    }
  }'
