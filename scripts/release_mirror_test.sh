#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
test_root="build/release-mirror-test"

rm -rf "$test_root"
trap 'rm -rf "$test_root"' EXIT
mkdir -p "$test_root/v1.0.0" "$test_root/v1.2.0" "$test_root/v1.3.0-rc.1" \
  "$test_root/.staging.v9.0.0.test"
printf 'installer' > "$test_root/v1.2.0/sagan-1.2.0-windows-x64.exe"
printf 'portable' > "$test_root/v1.2.0/sagan-1.2.0-windows-x64.zip"
printf 'extension' > "$test_root/v1.2.0/sagan-language-0.2.0.vsix"
printf 'checksum' > "$test_root/v1.2.0/sagan-1.2.0-windows-x64.zip.sha256"
printf 'old' > "$test_root/v1.0.0/sagan-1.0.0-windows-x64.zip"
printf 'preview' > "$test_root/v1.3.0-rc.1/sagan-1.3.0-rc.1-windows-x64.zip"

python3 deploy/releases/index.py --root "$test_root"
python3 deploy/releases/github.py --help >/dev/null

grep -Fq '<h2>v1.3.0-rc.1 <span class="badge">Latest</span> <span class="badge preview">Preview</span>' "$test_root/index.html"
grep -Fq 'sagan-1.2.0-windows-x64.exe' "$test_root/index.html"
grep -Fq 'Windows installer' "$test_root/index.html"
grep -Fq 'Portable archive' "$test_root/index.html"
grep -Fq 'sagan-language-0.2.0.vsix' "$test_root/index.html"
grep -Fq 'VS Code extension' "$test_root/index.html"
grep -Fq 'sagan.release-mirror/1' "$test_root/releases.json"
grep -Fq '"tag": "v1.0.0"' "$test_root/releases.json"
if grep -Fq '.staging.v9.0.0.test' "$test_root/releases.json"; then
  echo "Staging directories must not appear in the public release index." >&2
  exit 1
fi

echo "Release mirror index tests passed."
