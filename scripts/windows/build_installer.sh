#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
system_iscc="/c/Program Files (x86)/Inno Setup 6/ISCC.exe"
local_app_data="$(cygpath -u "${LOCALAPPDATA:-C:\\Users\\${USERNAME:-user}\\AppData\\Local}")"
user_iscc="$local_app_data/Programs/Inno Setup 6/ISCC.exe"
iscc="${ISCC:-$system_iscc}"
if [[ -z "${ISCC:-}" && ! -x "$iscc" && -x "$user_iscc" ]]; then
  iscc="$user_iscc"
fi

if [[ ! -x "$iscc" ]]; then
  echo "Inno Setup 6 was not found. Set ISCC to its ISCC.exe path." >&2
  exit 1
fi

if [[ "${SAGAN_STAGE_READY:-false}" != true ]]; then
  bash "$repo_root/scripts/windows/stage_installer.sh"
fi
version="$(bash "$repo_root/scripts/version.sh" numeric)"
mkdir -p "$repo_root/build/installer"
source_dir="$(cygpath -w "$repo_root/build/windows-stage")"
output_dir="$(cygpath -w "$repo_root/build/installer")"
script_path="$(cygpath -w "$repo_root/packaging/windows/sagan.iss")"
iscc_arguments=("/DSourceDir=$source_dir" "/DAppVersion=$version" "/O$output_dir")
if [[ -n "${SAGAN_SIGNTOOL_COMMAND:-}" ]]; then
  if [[ "$SAGAN_SIGNTOOL_COMMAND" != *'$f'* ]]; then
    echo 'SAGAN_SIGNTOOL_COMMAND must contain Inno Setup''s $f file placeholder.' >&2
    exit 1
  fi
  iscc_arguments+=("/SSaganSign=$SAGAN_SIGNTOOL_COMMAND" "/DSignToolName=SaganSign")
fi
MSYS2_ARG_CONV_EXCL='*' "$iscc" "${iscc_arguments[@]}" "$script_path"

installer="$repo_root/build/installer/sagan-$version-windows-x64.exe"
(cd "$(dirname "$installer")" && sha256sum "$(basename "$installer")" > "$(basename "$installer").sha256")

echo "Built build/installer/sagan-$version-windows-x64.exe"
echo "Wrote build/installer/sagan-$version-windows-x64.exe.sha256"
