#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: $0 /path/to/the /path/to/web/assets /path/to/chromium" >&2
  exit 2
fi

the_bin=$1
web_assets=$2
chromium=$3
repo_root=$(cd "$(dirname "$0")/.." && pwd)
work_dir=$(mktemp -d)
server_pid=""

cleanup() {
  if [[ -n "$server_pid" ]] && kill -0 "$server_pid" 2>/dev/null; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$work_dir"
}
trap cleanup EXIT

sample="$work_dir/sample.txt"
stdout_log="$work_dir/stdout.log"
stderr_log="$work_dir/stderr.log"
printf 'alpha beta gamma\ndelete me\ngamma\n' > "$sample"
printf 'second buffer\n' > "$work_dir/second.txt"
printf '/* crexx comment */\nsay "hello"\n' > "$work_dir/sample.crexx"
printf '# Heading\n\nA **bold** [link](target).\n' > "$work_dir/sample.md"
printf 'def greet(name):\n    return "hello " + name\n' > "$work_dir/sample.py"

THE_WEB_ROOT="$web_assets" \
THE_WEB_WORKSPACE="$work_dir" \
THE_HOME_DIR="$(dirname "$the_bin")/release" \
"$the_bin" --driver web "$sample" >"$stdout_log" 2>"$stderr_log" &
server_pid=$!

for _ in $(seq 1 120); do
  if rg '^THE web UI: ' "$stdout_log" >/dev/null 2>&1; then
    break
  fi
  if ! kill -0 "$server_pid" 2>/dev/null; then
    cat "$stdout_log" "$stderr_log" >&2
    exit 1
  fi
  sleep 0.05
done

web_url=$(sed -n 's/^THE web UI: //p' "$stdout_log" | tail -n 1 | tr -d '\r')
if [[ -z "$web_url" ]]; then
  echo "web driver did not publish its URL" >&2
  cat "$stdout_log" "$stderr_log" >&2
  exit 1
fi

THE_WEB_URL="$web_url" CHROMIUM_BIN="$chromium" \
  node "$repo_root/web/test/browser-smoke.mjs"

for _ in $(seq 1 100); do
  if ! kill -0 "$server_pid" 2>/dev/null; then
    break
  fi
  sleep 0.05
done
if kill -0 "$server_pid" 2>/dev/null; then
  echo "web driver did not exit after the File > Close action" >&2
  exit 1
fi
wait "$server_pid"
server_pid=""

printf 'Zalpha beta gamma\ngamma\n' > "$work_dir/expected.txt"
cmp "$work_dir/expected.txt" "$sample"
