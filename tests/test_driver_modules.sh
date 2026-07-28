#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 /path/to/the /path/to/the_driver_curses /path/to/the_driver_llm" >&2
  exit 2
fi

the_bin=$1
curses_module=$2
llm_module=$3

for path in "$the_bin" "$curses_module" "$llm_module"; do
  if [[ ! -e "$path" ]]; then
    echo "missing build artifact: $path" >&2
    exit 1
  fi
done

artifact_deps() {
  local artifact=$1
  if command -v otool >/dev/null 2>&1; then
    otool -L "$artifact" || true
  elif command -v ldd >/dev/null 2>&1; then
    ldd "$artifact" || true
  fi
}

artifact_symbols() {
  local artifact=$1
  if ! command -v nm >/dev/null 2>&1; then
    return 0
  fi
  if nm -gU "$artifact" >/dev/null 2>&1; then
    nm -gU "$artifact" 2>/dev/null
  elif nm -g --defined-only "$artifact" >/dev/null 2>&1; then
    nm -g --defined-only "$artifact" 2>/dev/null
  else
    nm -g "$artifact" 2>/dev/null
  fi | awk 'NF { print $NF }' | sed -e 's/^_//' -e 's/^\.refptr\.//'
}

main_deps=$(artifact_deps "$the_bin")
curses_deps=$(artifact_deps "$curses_module")
llm_deps=$(artifact_deps "$llm_module")

if printf '%s\n' "$main_deps" | rg -i 'curses|ncurses|pdcurses' >/dev/null; then
  echo "main the executable links curses directly:" >&2
  printf '%s\n' "$main_deps" >&2
  exit 1
fi

if ! printf '%s\n' "$curses_deps" | rg -i 'curses|ncurses|pdcurses' >/dev/null; then
  echo "curses driver module does not link a curses dependency:" >&2
  printf '%s\n' "$curses_deps" >&2
  exit 1
fi

if printf '%s\n' "$llm_deps" | rg -i 'curses|ncurses|pdcurses' >/dev/null; then
  echo "llm driver module links curses:" >&2
  printf '%s\n' "$llm_deps" >&2
  exit 1
fi

main_symbols=$(artifact_symbols "$the_bin")
raw_curses_symbol_re='^(curses_driver_|doupdate($|[$@])|initscr($|[$@])|endwin($|[$@])|wmove($|[$@])|wgetch($|[$@])|stdscr$|curscr$|newwin($|[$@])|newpad($|[$@])|derwin($|[$@])|subwin($|[$@])|delwin($|[$@])|touchwin($|[$@])|touchline($|[$@])|wnoutrefresh($|[$@])|wrefresh($|[$@])|refresh($|[$@])|curs_set($|[$@])|keypad($|[$@])|box($|[$@])|prefresh($|[$@])|getmouse($|[$@])|mousemask($|[$@]))'
raw_curses_symbols=$(
  printf '%s\n' "$main_symbols" | rg "$raw_curses_symbol_re" || true
)
if [[ -n "$raw_curses_symbols" ]]; then
  echo "main the executable exposes raw curses API symbols:" >&2
  printf '%s\n' "$raw_curses_symbols" >&2
  exit 1
fi

# Explicit compatibility naming debt that remains in the main executable while
# old editor call sites still share runtime state with the curses module.
compat_curses_symbol_re='^(my_wmove|curses_started|ncurses_screen_resized|suspend_curses|resume_curses|the_driver_is_curses|the_driver_use_curses)$'
unexpected_curses_shaped_symbols=$(
  printf '%s\n' "$main_symbols" |
    rg -i 'curses|ncurses|wmove|doupdate' |
    rg -v "$compat_curses_symbol_re" || true
)
if [[ -n "$unexpected_curses_shaped_symbols" ]]; then
  echo "main the executable exposes unexpected curses-shaped symbols:" >&2
  printf '%s\n' "$unexpected_curses_shaped_symbols" >&2
  exit 1
fi

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

sample="$work_dir/sample.txt"
printf 'alpha beta gamma\n' > "$sample"

mkdir -p "$work_dir/missing" "$work_dir/llm-only/drivers" "$work_dir/curses-only/drivers"
the_leaf=$(basename "$the_bin")
case "$the_leaf" in
  *.exe|*.EXE) ;;
  *) the_leaf=the ;;
esac
missing_the="$work_dir/missing/$the_leaf"
llm_the="$work_dir/llm-only/$the_leaf"
curses_the="$work_dir/curses-only/$the_leaf"
cp "$the_bin" "$missing_the"
cp "$the_bin" "$llm_the"
cp "$llm_module" "$work_dir/llm-only/drivers/"
cp "$the_bin" "$curses_the"
cp "$curses_module" "$work_dir/curses-only/drivers/"

missing_out="$work_dir/missing.out"
missing_err="$work_dir/missing.err"
set +e
printf '%s\n' capabilities quit |
  THE_DRIVER_PATH="$work_dir/missing/drivers" \
  "$missing_the" --driver missing -n "$sample" \
    >"$missing_out" 2>"$missing_err"
missing_rc=$?
set -e
if [[ $missing_rc -eq 0 ]]; then
  echo "missing driver unexpectedly succeeded" >&2
  cat "$missing_out" "$missing_err" >&2
  exit 1
fi
rg -i 'driver|load|not found|failed|expected --driver' "$missing_out" "$missing_err" >/dev/null

llm_out="$work_dir/llm.out"
printf '%s\n' capabilities 'look filearea compact max=40' quit |
  TERM= THE_HOME_DIR="$(dirname "$the_bin")/release" \
  THE_DRIVER_PATH="$work_dir/llm-only/drivers" \
  "$llm_the" --driver llm -n "$sample" \
    >"$llm_out"
rg '"driver":"llm"' "$llm_out" >/dev/null
rg '"curses":false' "$llm_out" >/dev/null
rg 'alpha beta gamma' "$llm_out" >/dev/null

if grep -aq "CREXX unavailable" "$the_bin"; then
  echo "Skipping curses module runtime smoke; CREXX profile support is unavailable" >&2
  exit 77
fi
if ! command -v script >/dev/null 2>&1; then
  echo "Skipping curses module runtime smoke; script(1) is missing" >&2
  exit 77
fi

profile="$work_dir/profile.the"
cat > "$profile" <<'PROFILE_EOF'
options levelb
import rxfnsb
address the
'qquit';
PROFILE_EOF

run_with_pty() {
  local transcript="$work_dir/typescript"
  local stdout="$work_dir/stdout.txt"
  local stderr="$work_dir/stderr.txt"

  if script -q "$work_dir/probe" true >/dev/null 2>&1; then
    TERM="${TERM:-xterm}" THE_HOME_DIR="$(dirname "$the_bin")/release" \
      THE_DRIVER_PATH="$work_dir/curses-only/drivers" \
      script -q "$transcript" "$@" >"$stdout" 2>"$stderr"
  else
    local quoted=""
    printf -v quoted "%q " "$@"
    TERM="${TERM:-xterm}" THE_HOME_DIR="$(dirname "$the_bin")/release" \
      THE_DRIVER_PATH="$work_dir/curses-only/drivers" \
      script -q -c "$quoted" "$transcript" >"$stdout" 2>"$stderr"
  fi
  cat "$stdout" "$stderr" "$transcript" > "$work_dir/curses-combined.txt"
}

run_with_pty "$curses_the" --driver curses -p "$profile" "$sample"

if rg -i 'driver module not found|load failed|error opening terminal' \
   "$work_dir/curses-combined.txt" >/dev/null; then
  cat "$work_dir/curses-combined.txt" >&2
  exit 1
fi

exit 0
