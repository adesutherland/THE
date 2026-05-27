#!/usr/bin/env bash
set -euo pipefail

mode="full"
repo_root="."
baseline_file="tests/inventory_direct_curses.baseline.tsv"

usage() {
  cat <<'USAGE'
usage: inventory_direct_curses.sh [--full|--summary|--baseline|--fail-on-new]
                                  [--baseline-file path] [repo-root]

  --full         print the full call-site inventory (default)
  --summary      print debt-oriented totals and top file/function buckets
  --baseline     print aggregate baseline rows: category file function count
  --fail-on-new  fail if actionable debt exceeds the checked-in baseline
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --full)
      mode="full"
      ;;
    --summary)
      mode="summary"
      ;;
    --baseline)
      mode="baseline"
      ;;
    --fail-on-new)
      mode="fail-on-new"
      ;;
    --baseline-file)
      shift
      if [[ $# -eq 0 ]]; then
        printf '%s\n' "missing path after --baseline-file" >&2
        exit 2
      fi
      baseline_file="$1"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      printf 'unknown option: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
    *)
      repo_root="$1"
      ;;
  esac
  shift
done

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

generate_findings() {
  awk '
function trim(s) {
  sub(/^[[:space:]]+/, "", s)
  sub(/[[:space:]]+$/, "", s)
  return s
}
function strip_code(s,    out, i, c, nextc, escaped) {
  out = ""
  escaped = 0
  for (i = 1; i <= length(s); i++) {
    c = substr(s, i, 1)
    nextc = substr(s, i + 1, 1)
    if (in_block_comment) {
      if (c == "*" && nextc == "/") {
        in_block_comment = 0
        i++
      }
      continue
    }
    if (in_string) {
      if (escaped)
        escaped = 0
      else if (c == "\\")
        escaped = 1
      else if (c == "\"")
        in_string = 0
      out = out " "
      continue
    }
    if (in_char) {
      if (escaped)
        escaped = 0
      else if (c == "\\")
        escaped = 1
      else if (c == "'\''")
        in_char = 0
      out = out " "
      continue
    }
    if (c == "/" && nextc == "*") {
      in_block_comment = 1
      i++
      continue
    }
    if (c == "/" && nextc == "/")
      break
    if (c == "\"") {
      in_string = 1
      out = out " "
      continue
    }
    if (c == "'\''") {
      in_char = 1
      out = out " "
      continue
    }
    out = out c
  }
  return out
}
function is_function_signature(line) {
  line = trim(line)
  return line ~ /^[[:alpha:]_][[:alnum:]_[:space:]\*]*[[:space:]\*]+[[:alpha:]_][[:alnum:]_]*[[:space:]]*\(/
}
function is_function_definition_line(line) {
  line = trim(line)
  if (!is_function_signature(line))
    return 0
  if (line ~ /;[[:space:]]*$/)
    return 0
  return line !~ /(^|[^A-Za-z0-9_])(if|for|while|switch|return)[[:space:]]*\(/
}
function is_preprocessor_define(line) {
  line = trim(line)
  return line ~ /^#[[:space:]]*define[[:space:]]/
}
function category(line, is_func_sig, is_define) {
  if (line ~ /(^|[^A-Za-z0-9_])curses_driver_[A-Za-z0-9_]*[[:space:]]*\(/)
    return "driver-wrapper"
  if (!is_func_sig && !is_define && line ~ /(^|[^A-Za-z0-9_])(my_getch|wgetch|getch|get_mouse_info|wmouse_position)[[:space:]]*\(/)
    return "physical-input"
  if (!is_func_sig && !is_define && line ~ /(^|[^A-Za-z0-9_])(getyx|getbegyx|getmaxyx|wmove|mvwadd|wadd|wattr|touchline|touchwin|wnoutrefresh|doupdate|wrefresh|refresh|newwin|newpad|derwin|subwin|delwin|keypad|wbkgd|box|whline|prefresh|curs_set|draw_cursor)[[:space:]]*\(/)
    return "physical-paint"
  if (!is_define && line ~ /(^|[^A-Za-z0-9_])(KEY_MOUSE|BUTTON_PRESSED|BUTTON_RELEASED|BUTTON_CLICKED)([^A-Za-z0-9_]|$)/)
    return "mouse-token"
  if (line ~ /(^|[^A-Za-z0-9_])(WINDOW|SCREEN_WINDOW|CURRENT_WINDOW|CURRENT_WINDOW_[A-Za-z0-9_]*|SCREEN_WINDOW_[A-Za-z0-9_]*|stdscr|chtype|cchar_t)([^A-Za-z0-9_]|$)/)
    return "window-state"
  return ""
}
FNR == 1 {
  fn = "file-scope"
  in_block_comment = 0
  skip_if_zero = 0
}
{
  raw = $0
  line = strip_code(raw)
  directive = trim(line)
  if (skip_if_zero > 0) {
    if (directive ~ /^#[[:space:]]*if([[:space:]]|$)/)
      skip_if_zero++
    else if (directive ~ /^#[[:space:]]*else([[:space:]]|$)/) {
      if (skip_if_zero == 1)
        skip_if_zero = 0
    }
    else if (directive ~ /^#[[:space:]]*endif([[:space:]]|$)/)
      skip_if_zero--
    next
  }
  if (directive ~ /^#[[:space:]]*if[[:space:]]+0([[:space:]]|$)/) {
    skip_if_zero = 1
    next
  }
  is_func_sig = is_function_signature(line)
  is_func_def = is_function_definition_line(line)
  is_define = is_preprocessor_define(line)
  if (is_func_def) {
    sig = line
    sub(/\{.*/, "", sig)
    sub(/\(.*/, "", sig)
    gsub(/\*/, " ", sig)
    n = split(sig, parts, /[[:space:]]+/)
    if (n > 0 && parts[n] != "")
      fn = parts[n]
  }
  cat = category(line, is_func_sig, is_define)
  if (cat != "")
    printf "%s\t%d\t%s\t%s\t%s\n", FILENAME, FNR, fn, cat, trim(raw)
}
' "${files[@]}"
}

aggregate_findings() {
  awk -F '\t' '
    {
      key = $4 "\t" $1 "\t" $3
      count[key]++
    }
    END {
      for (key in count)
        printf "%s\t%d\n", key, count[key]
    }
  ' | sort
}

print_summary() {
  local findings_file
  findings_file="$(mktemp)"
  cat > "$findings_file"
  awk -F '\t' '
    {
      bycat[$4]++
      key = $4 "\t" $1 "\t" $3
      bybucket[key]++
    }
    END {
      print "direct curses inventory summary (excluding src/cursesdriver.*, PDCurses, contrib):"
      order[1] = "physical-input"
      order[2] = "physical-paint"
      order[3] = "mouse-token"
      order[4] = "window-state"
      order[5] = "driver-wrapper"
      for (i = 1; i <= 5; i++) {
        cat = order[i]
        label = cat
        if (cat == "driver-wrapper")
          label = label " (allowed/migrated)"
        else
          label = label " (actionable)"
        printf "  %s: %d\n", label, bycat[cat] + 0
      }
    }
  ' "$findings_file"
  printf '%s\n' "top actionable buckets:"
  awk -F '\t' '
    {
      key = $4 "\t" $1 "\t" $3
      bybucket[key]++
    }
    END {
      for (key in bybucket) {
        split(key, parts, "\t")
        if (parts[1] == "driver-wrapper")
          continue
        printf "%d\t%s\t%s\t%s\n", bybucket[key], parts[1], parts[2], parts[3]
      }
    }
  ' "$findings_file" |
    sort -rn |
    head -20 |
    awk -F '\t' '{ printf "  %s %s:%s: %d\n", $2, $3, $4, $1 }'
  rm -f "$findings_file"
}

case "$mode" in
  full)
    findings="$(generate_findings)"
    printf '%s\n' "direct curses inventory (excluding src/cursesdriver.*, PDCurses, contrib):"
    if [[ -n "$findings" ]]; then
      printf '%s\n' "$findings" |
        awk -F '\t' '{ printf "%s:%d:%s:%s:%s\n", $1, $2, $3, $4, $5 }'
    else
      printf '%s\n' "none"
    fi
    printf '%s\n' "$findings" | awk -F '\t' '
      NF {
        count++
        bycat[$4]++
      }
      END {
        if (count > 0) {
          print "summary:"
          for (cat in bycat)
            printf "  %s: %d\n", cat, bycat[cat]
        }
      }'
    ;;
  summary)
    generate_findings | print_summary
    ;;
  baseline)
    printf '%s\n' "# category	file	function	count"
    generate_findings | aggregate_findings
    ;;
  fail-on-new)
    if [[ ! -f "$baseline_file" ]]; then
      printf 'missing direct curses inventory baseline: %s\n' "$baseline_file" >&2
      exit 1
    fi
    current_file="$(mktemp)"
    trap 'rm -f "$current_file"' EXIT
    generate_findings | aggregate_findings > "$current_file"
    awk -F '\t' '
      FNR == NR {
        if ($1 ~ /^#/ || NF < 4)
          next
        key = $1 "\t" $2 "\t" $3
        baseline[key] = $4 + 0
        next
      }
      {
        if ($1 == "driver-wrapper")
          next
        key = $1 "\t" $2 "\t" $3
        current[key] = $4 + 0
      }
      END {
        failed = 0
        for (key in current) {
          split(key, parts, "\t")
          if (parts[1] == "driver-wrapper")
            continue
          base = (key in baseline) ? baseline[key] : 0
          if (current[key] > base) {
            if (!failed)
              print "direct curses inventory ratchet failed; actionable debt increased:"
            printf "  %s %s:%s baseline=%d current=%d\n", parts[1], parts[2], parts[3], base, current[key]
            failed = 1
          }
        }
        if (failed)
          exit 1
        print "direct curses inventory ratchet passed"
      }
    ' "$baseline_file" "$current_file"
    ;;
esac
