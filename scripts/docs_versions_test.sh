#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv_dir="$repo_root/build/docs-venv"

if [[ -x "$venv_dir/Scripts/mike.exe" ]]; then
  mike="$venv_dir/Scripts/mike.exe"
else
  mike="$venv_dir/bin/mike"
fi

test -x "$mike" || {
  echo "Documentation environment is missing. Run: bash scripts/docs.sh setup" >&2
  exit 1
}

export PATH="$venv_dir/bin:$venv_dir/Scripts:$PATH"

cd "$repo_root"
mkdir -p build/tmp
test_root="$(mktemp -d -p build/tmp docs-versions-test.XXXXXXXX)"
[[ "$test_root" == build/tmp/docs-versions-test.* ]]

cleanup() {
  cd "$repo_root"
  rm -rf "$test_root"
}

trap cleanup EXIT

cp -R docs "$test_root/docs"
install -d -m 0755 "$test_root/scripts"
cp scripts/docs_status.py "$test_root/scripts/docs_status.py"
cp scripts/docs_sagan_lexer.py "$test_root/scripts/docs_sagan_lexer.py"
cp mkdocs.yml "$test_root/mkdocs.yml"

git -C "$test_root" init -q
git -C "$test_root" config user.name "Sagan documentation test"
git -C "$test_root" config user.email "sagan-docs-test@example.invalid"
git -C "$test_root" add docs scripts mkdocs.yml
git -C "$test_root" commit -qm initial

cd "$test_root"
"$mike" deploy --branch docs-site experimental
"$mike" set-default --branch docs-site experimental

git show docs-site:versions.json | grep -q '"version": "experimental"'
git show docs-site:index.html | grep -q 'experimental'
git show docs-site:experimental/index.html | grep -q \
  'Current documentation version: <strong>experimental</strong>'
git show docs-site:experimental/index.html | grep -q 'noindex,nofollow,noarchive'

SAGAN_DOCS_VERSION=1.0.0 SAGAN_DOCS_CHANNEL=released \
  "$mike" deploy --branch docs-site --update-aliases 1.0.0 latest
"$mike" set-default --branch docs-site latest

git show docs-site:versions.json | grep -q '"version": "1.0.0"'
git show docs-site:versions.json | grep -q '"latest"'
git show docs-site:index.html | grep -q 'latest'
if git show docs-site:1.0.0/index.html | grep -q 'noindex,nofollow,noarchive'; then
  echo "Released documentation unexpectedly contains the experimental noindex marker." >&2
  exit 1
fi

echo "Documentation versioning tests passed."
