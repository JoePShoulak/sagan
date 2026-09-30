#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
archive="$repo_root/build/release/sagan-$version-windows-x64.zip"
checksum="$archive.sha256"
install_dir="$repo_root/build/portable-smoke"

[[ -f "$archive" ]] || { echo "Missing portable archive: $archive" >&2; exit 1; }
[[ -f "$checksum" ]] || { echo "Missing portable checksum: $checksum" >&2; exit 1; }
(cd "$(dirname "$archive")" && sha256sum -c "$(basename "$checksum")")

rm -rf "$install_dir"
mkdir -p "$install_dir"
if command -v unzip >/dev/null 2>&1; then
  unzip -q "$archive" -d "$install_dir"
elif [[ -x /c/Windows/System32/tar.exe ]]; then
  (cd "$(dirname "$archive")" && MSYS2_ARG_CONV_EXCL='*' /c/Windows/System32/tar.exe -xf "$(basename "$archive")" -C ../portable-smoke)
else
  echo "Portable testing requires unzip or Windows bsdtar." >&2
  exit 1
fi

[[ -x "$install_dir/bin/sagan.exe" ]]
[[ -x "$install_dir/toolchain/ucrt64/bin/g++.exe" ]]
[[ ! -e "$install_dir/bin/sagan-launch.exe" ]]

runtime_isolated_path="/usr/bin:/bin:/c/Windows/System32:/c/Windows"
PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" --version
PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" "$repo_root/tests/fixtures/runtime/smoke.sagan"

echo "Portable Windows archive checksum, layout, isolated CLI, compilation, and execution tests passed."
