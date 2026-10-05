#!/usr/bin/env bash
set -euo pipefail

source_repo="${1:-$(pwd)}"
rehearsal_root="${2:-$(mktemp -d "${TMPDIR:-/tmp}/sagan-vscode-extraction.XXXXXX")}"
extracted_repo="$rehearsal_root/extracted"

mkdir -p "$rehearsal_root"
git clone --no-local "$source_repo" "$extracted_repo"

(
  cd "$extracted_repo"
  FILTER_BRANCH_SQUELCH_WARNING=1 git filter-branch \
    --prune-empty \
    --subdirectory-filter editors/vscode-sagan \
    -- HEAD
  git fsck --full
  test -f package.json
  test -f src/extension.js
  test ! -e editors/vscode-sagan
  npm ci
  npm test
  npm run test:bundle
  printf 'Extracted commit: %s\n' "$(git rev-parse HEAD)"
  printf 'Extracted commits: %s\n' "$(git rev-list --count HEAD)"
  printf 'Extracted files: %s\n' "$(git ls-tree -r --name-only HEAD | wc -l)"
)

printf 'Rehearsal retained at: %s\n' "$rehearsal_root"
