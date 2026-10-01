#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
extension_dir="editors/vscode-sagan"
output_dir="build/release"

cd "$repo_root"

extension_version="$(cd "$extension_dir" && npm pkg get version | tr -d '"[:space:]')"
[[ "$extension_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
  echo "Invalid VS Code extension version: $extension_version" >&2
  exit 1
}

mkdir -p "$output_dir"
output="$output_dir/sagan-language-$extension_version.vsix"

(
  cd "$extension_dir"
  npm ci
  npm test
  npm run package -- --out "../../$output"
)

(cd "$output_dir" && sha256sum "$(basename "$output")" > "$(basename "$output").sha256")

echo "Built build/release/$(basename "$output")"
echo "Wrote build/release/$(basename "$output").sha256"
