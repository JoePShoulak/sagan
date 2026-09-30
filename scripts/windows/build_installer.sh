#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
iscc="${ISCC:-/c/Program Files (x86)/Inno Setup 6/ISCC.exe}"

if [[ ! -x "$iscc" ]]; then
  echo "Inno Setup 6 was not found. Set ISCC to its ISCC.exe path." >&2
  exit 1
fi

bash "$repo_root/scripts/windows/stage_installer.sh"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
mkdir -p "$repo_root/build/installer"
source_dir="$(cygpath -w "$repo_root/build/windows-stage")"
output_dir="$(cygpath -w "$repo_root/build/installer")"
script_path="$(cygpath -w "$repo_root/packaging/windows/sagan.iss")"
MSYS2_ARG_CONV_EXCL='*' "$iscc" "/DSourceDir=$source_dir" "/DAppVersion=$version" \
  "/O$output_dir" "$script_path"

echo "Built build/installer/sagan-$version-windows-x64.exe"
