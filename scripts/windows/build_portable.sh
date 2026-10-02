#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
cd "$repo_root"
windows_stage="build/windows-stage"
portable_stage="build/windows-portable-stage"
output_dir="build/release"
archive="$output_dir/sagan-$version-windows-x64.zip"

if [[ "${SAGAN_STAGE_READY:-false}" != true ]]; then
  bash "$repo_root/scripts/windows/stage_installer.sh"
fi

if [[ ! -x "$windows_stage/bin/sagan.exe" || ! -x "$windows_stage/bin/sagan-lsp.exe" ||
      ! -d "$windows_stage/toolchain" ]]; then
  echo "The canonical Windows stage is incomplete." >&2
  exit 1
fi
rm -rf "$portable_stage"
mkdir -p "$portable_stage/bin" "$output_dir"
cp "$windows_stage/bin/sagan.exe" "$portable_stage/bin/sagan.exe"
cp "$windows_stage/bin/sagan-lsp.exe" "$portable_stage/bin/sagan-lsp.exe"
cp -a "$windows_stage/toolchain" "$portable_stage/toolchain"
cp -a "$windows_stage/libraries" "$portable_stage/libraries"
cp -a "$windows_stage/examples" "$portable_stage/examples"
cp -a "$windows_stage/licenses" "$portable_stage/licenses"
cp "$windows_stage/VERSION" "$portable_stage/VERSION"

rm -f "$archive" "$archive.sha256"
if command -v zip >/dev/null 2>&1; then
  (cd "$portable_stage" && zip -q -r "../release/$(basename "$archive")" \
    bin toolchain libraries examples licenses VERSION)
elif [[ -x /c/Windows/System32/tar.exe ]]; then
  archive_relative="../release/$(basename "$archive")"
  (cd "$portable_stage" && MSYS2_ARG_CONV_EXCL='*' /c/Windows/System32/tar.exe -a \
    --options zip:compression=deflate,zip:compression-level=9 \
    -cf "$archive_relative" bin toolchain libraries examples licenses VERSION)
else
  echo "Portable packaging requires zip or Windows bsdtar." >&2
  exit 1
fi
(cd "$output_dir" && sha256sum "$(basename "$archive")" > "$(basename "$archive").sha256")

echo "Built build/release/$(basename "$archive")"
echo "Wrote build/release/$(basename "$archive").sha256"
