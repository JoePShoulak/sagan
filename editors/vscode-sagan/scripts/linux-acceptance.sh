#!/usr/bin/env bash
set -euo pipefail

extension_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repository_root="$(cd "$extension_root/../.." && pwd)"
extension_version="$(node -p "require('$extension_root/package.json').version")"
output_path="${1:-$extension_root/sagan-language-$extension_version.vsix}"

cd "$repository_root"
make all bin/sagan-lsp

cd "$extension_root"
npm ci
npm test
npm run test:bundle
npm run test:integration
npm run package -- --out "$output_path"

test -s "$output_path"
printf 'Linux acceptance passed. VSIX: %s\n' "$output_path"
