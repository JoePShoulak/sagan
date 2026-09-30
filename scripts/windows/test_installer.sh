#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
installer="$repo_root/build/installer/sagan-$version-windows-x64.exe"
install_dir="$repo_root/build/installer-smoke"

if [[ ! -f "$installer" ]]; then
  echo "Missing installer: $installer" >&2
  exit 1
fi

rm -rf "$install_dir"
MSYS2_ARG_CONV_EXCL='*' "$installer" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER "/DIR=$(cygpath -w "$install_dir")" \
  /TASKS="addtopath,fileassociation"

"$install_dir/bin/sagan.exe" --version
"$install_dir/bin/sagan.exe" "$repo_root/examples/run_demo.sagan"
launcher_data="$repo_root/build/installer-launcher-data"
mkdir -p "$launcher_data"
LOCALAPPDATA="$(cygpath -w "$launcher_data")" \
  "$install_dir/bin/sagan-launch.exe" --windowed "$repo_root/examples/run_demo.sagan"
grep -Fq "Direct Sagan execution: 42" "$launcher_data/Sagan/logs/latest-launch.log"
MSYS2_ARG_CONV_EXCL='*' reg.exe query 'HKCU\Software\Classes\.sagan' /ve | grep -Fq 'Sagan.Source'
MSYS2_ARG_CONV_EXCL='*' reg.exe query 'HKCU\Environment' /v Path | grep -Fq "$(cygpath -w "$install_dir/bin")"

MSYS2_ARG_CONV_EXCL='*' "$install_dir/unins000.exe" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
if [[ -e "$install_dir/bin/sagan.exe" ]]; then
  echo "The installer smoke test could not remove the installed compiler." >&2
  exit 1
fi

echo "Windows installer install, execution, and uninstall smoke test passed."
