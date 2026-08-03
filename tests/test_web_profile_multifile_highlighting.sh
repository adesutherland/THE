#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 /path/to/the /path/to/web-profile.the" >&2
  exit 2
fi

the_bin=$1
web_profile=$2

for parser in rxc mdp pyp; do
  if [[ ! -x "${HOME}/.local/bin/$parser" ]]; then
    echo "$parser syntax highlighter is not installed; skipping" >&2
    exit 77
  fi
done

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
crexx="$work_dir/sample.crexx"
markdown="$work_dir/sample.md"
python="$work_dir/sample.py"
out="$work_dir/out.jsonl"
err="$work_dir/err.log"

printf '/* crexx comment */\nsay "hello"\n' > "$crexx"
printf '# Heading\n\nA **bold** [link](target).\n' > "$markdown"
printf 'def greet(name):\n    return "hello " + name\n' > "$python"

printf '%s\n' \
  'command sdslhwait 5000' \
  'look filearea compact max=200 prefix=0 command=0 status=0' \
  "command edit $markdown" \
  'command sdslhwait 5000' \
  'look filearea compact max=200 prefix=0 command=0 status=0' \
  "command edit $python" \
  'command sdslhwait 5000' \
  'look filearea compact max=200 prefix=0 command=0 status=0' \
  "command edit $crexx" \
  'command sdslhwait 5000' \
  'look filearea compact max=200 prefix=0 command=0 status=0' \
  "command edit $markdown" \
  'command sdslhwait 5000' \
  'look filearea compact max=200 prefix=0 command=0 status=0' \
  'quit' |
  TERM= THE_HOME_DIR="$(dirname "$the_bin")/release" \
    "$the_bin" --driver llm -p "$web_profile" "$crexx" >"$out" 2>"$err"

node - "$out" <<'NODE'
const { readFileSync } = require("node:fs");
const { basename } = require("node:path");

const snapshots = readFileSync(process.argv[2], "utf8")
  .trim()
  .split("\n")
  .map((line) => JSON.parse(line))
  .filter((message) => message.mode === "filearea");
const expected = ["sample.crexx", "sample.md", "sample.py", "sample.crexx", "sample.md"];
const actual = snapshots.map((snapshot) => basename(snapshot.buffer.path));
if (JSON.stringify(actual) !== JSON.stringify(expected)) {
  throw new Error(`unexpected buffer sequence: ${JSON.stringify(actual)}`);
}

function styles(snapshot) {
  return new Set(snapshot.screen_rows.flatMap((row) => (row.s || []).map((run) => run[2])));
}

function requireStyles(snapshot, required, forbidden = []) {
  const found = styles(snapshot);
  for (const style of required) {
    if (!found.has(style)) throw new Error(`${basename(snapshot.buffer.path)} is missing ${style}`);
  }
  for (const style of forbidden) {
    if (found.has(style)) throw new Error(`${basename(snapshot.buffer.path)} retained stale ${style}`);
  }
}

requireStyles(snapshots[0], ["comment", "keyword", "string"], ["preprocessor", "identifier"]);
requireStyles(snapshots[1], ["preprocessor", "operator", "punctuation"], ["comment", "string", "identifier"]);
requireStyles(snapshots[2], ["keyword", "function", "identifier", "string", "operator"], ["comment", "preprocessor"]);
requireStyles(snapshots[3], ["comment", "keyword", "string"], ["preprocessor", "identifier"]);
requireStyles(snapshots[4], ["preprocessor", "operator", "punctuation"], ["comment", "string", "identifier"]);
NODE
