#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
installer="$repo_root/build/installer/sagan-$version-windows-x64.exe"
checksum="$installer.sha256"
require_signature=false

if [[ "${1:-}" == "--release" ]]; then
  require_signature=true
elif [[ $# -ne 0 ]]; then
  echo "usage: bash scripts/windows/verify_installer_artifact.sh [--release]" >&2
  exit 2
fi

[[ -f "$installer" ]] || { echo "Missing installer: $installer" >&2; exit 1; }
[[ -f "$checksum" ]] || { echo "Missing installer checksum: $checksum" >&2; exit 1; }
(cd "$(dirname "$installer")" && sha256sum -c "$(basename "$checksum")")

if command -v signtool.exe >/dev/null 2>&1; then
  if signtool.exe verify /pa /all "$(cygpath -w "$installer")" >/dev/null 2>&1; then
    echo "Installer Authenticode signature verified."
  elif $require_signature; then
    echo "The public-release installer must have a trusted Authenticode signature." >&2
    exit 1
  else
    echo "Unsigned development installer accepted; public releases require signing."
  fi
elif $require_signature; then
  echo "signtool.exe is required to verify a public-release installer." >&2
  exit 1
else
  echo "Signature verification unavailable; unsigned development installer accepted."
fi

echo "Windows installer artifact verification passed."
