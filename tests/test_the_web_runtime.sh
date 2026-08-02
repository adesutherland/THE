#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 /path/to/the /path/to/web/assets" >&2
  exit 2
fi

the_bin=$1
web_assets=$2
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
printf 'alpha beta gamma\n' > "$sample"

THE_WEB_ROOT="$web_assets" \
THE_WEB_WORKSPACE="$work_dir" \
THE_HOME_DIR="$(dirname "$the_bin")/release" \
"$the_bin" --driver web "$sample" >"$stdout_log" 2>"$stderr_log" &
server_pid=$!

for _ in $(seq 1 100); do
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

THE_WEB_URL="$web_url" node "$repo_root/web/test/runtime-smoke.mjs"

for _ in $(seq 1 100); do
  if ! kill -0 "$server_pid" 2>/dev/null; then
    break
  fi
  sleep 0.05
done
if kill -0 "$server_pid" 2>/dev/null; then
  echo "web driver did not exit after quit" >&2
  exit 1
fi
wait "$server_pid"
server_pid=""

printf 'alphaX beta gamma\n' > "$work_dir/expected.txt"
cmp "$work_dir/expected.txt" "$sample"
