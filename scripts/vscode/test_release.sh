#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
extension_dir="$repo_root/editors/vscode-sagan"
extension_version="$(cd "$extension_dir" && npm pkg get version | tr -d '"[:space:]')"
vsix="${1:-$repo_root/build/release/sagan-language-$extension_version.vsix}"
checksum="$vsix.sha256"
compiler="${SAGAN_COMPILER_PATH:-$repo_root/build/portable-smoke/bin/sagan.exe}"
server="${SAGAN_LSP_PATH:-$repo_root/build/portable-smoke/bin/sagan-lsp.exe}"
unpack_dir="$repo_root/build/vsix-smoke"

[[ -f "$vsix" ]] || { echo "Missing VSIX: $vsix" >&2; exit 1; }
[[ -f "$checksum" ]] || { echo "Missing VSIX checksum: $checksum" >&2; exit 1; }
[[ -x "$compiler" ]] || { echo "Missing matching Sagan compiler: $compiler" >&2; exit 1; }
[[ -x "$server" ]] || { echo "Missing matching Sagan language server: $server" >&2; exit 1; }

(cd "$(dirname "$vsix")" && sha256sum -c "$(basename "$checksum")")
rm -rf "$unpack_dir"
mkdir -p "$unpack_dir"
unzip -q "$vsix" -d "$unpack_dir"
node "$repo_root/scripts/vscode/verify_release.js" \
  "$unpack_dir/extension/package.json" "$extension_version"
node "$repo_root/scripts/vscode/install_release_test.js" "$vsix" "$extension_version"

cd "$extension_dir"
SAGAN_COMPILER_PATH="$compiler" SAGAN_LSP_PATH="$server" npm run test:integration

echo "VSIX checksum, contents, isolated installation, and matching native-tooling activation checks passed."
