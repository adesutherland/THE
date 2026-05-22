#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/the_agent" >&2
  exit 2
fi

exe=$1
if [[ ! -x "$exe" ]]; then
  echo "not executable: $exe" >&2
  exit 1
fi

deps=""
if command -v otool >/dev/null 2>&1; then
  deps=$(otool -L "$exe" || true)
elif command -v ldd >/dev/null 2>&1; then
  deps=$(ldd "$exe" || true)
fi

if printf '%s\n' "$deps" | rg -i 'curses|ncurses|pdcurses' >/dev/null; then
  echo "the_agent links a curses dependency:" >&2
  printf '%s\n' "$deps" >&2
  exit 1
fi

if command -v nm >/dev/null 2>&1; then
  symbols=$(nm "$exe" 2>/dev/null || true)
  if printf '%s\n' "$symbols" \
     | rg 'curses_driver|initscr|endwin|wmove|wgetch|doupdate|stdscr' \
          >/dev/null; then
    echo "the_agent exposes curses or curses-driver symbols" >&2
    exit 1
  fi
fi

exit 0
