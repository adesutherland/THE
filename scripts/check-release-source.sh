#!/usr/bin/env bash
# Require a release tag's exact source commit to be on freshly fetched main.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "Usage: $0 tag (fetch origin/main first)" >&2
  exit 64
fi
release_commit=$(git rev-parse --verify "refs/tags/$1^{commit}")
git rev-parse --verify refs/remotes/origin/main >/dev/null
if ! git merge-base --is-ancestor "$release_commit" refs/remotes/origin/main; then
  echo "Release refused: $1 is not a commit published on origin/main." >&2
  exit 1
fi
printf 'Release source verified on origin/main: %s (%s)\n' "$1" "$release_commit"
