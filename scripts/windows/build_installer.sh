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
"$iscc" "/DSourceDir=$repo_root/build/windows-stage" "/DAppVersion=$version" \
  "/O$repo_root/build/installer" "$repo_root/packaging/windows/sagan.iss"

echo "Built build/installer/sagan-$version-windows-x64.exe"
