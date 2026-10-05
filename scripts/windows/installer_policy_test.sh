#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
installer_script="$repo_root/packaging/windows/sagan.iss"
build_script="$repo_root/scripts/windows/build_installer.sh"
workflow="$repo_root/.github/workflows/windows-installer.yml"

bash -n "$build_script" \
  "$repo_root/scripts/windows/test_installer.sh" \
  "$repo_root/scripts/windows/verify_installer_artifact.sh"

grep -Fq 'ArchitecturesAllowed=x64os' "$installer_script"
grep -Fq 'ArchitecturesInstallIn64BitMode=x64os' "$installer_script"
grep -Fq 'CompareSemanticVersions(ExistingVersion, ' "$installer_script"
grep -Fq 'Uninstall it before installing the older' "$installer_script"
grep -Fq 'SignTool={#SignToolName}' "$installer_script"
grep -Fq 'SignedUninstaller=yes' "$installer_script"
grep -Fq 'SAGAN_SIGNTOOL_COMMAND' "$build_script"
grep -Fq 'DestDir: "{app}\libraries"' "$installer_script"
grep -Fq 'DestDir: "{app}\examples"' "$installer_script"
grep -Fq 'libgdi32.a' "$repo_root/scripts/windows/stage_installer.sh"
grep -Fq 'two_body_demo' "$repo_root/scripts/windows/test_installer.sh"
grep -Fq 'two_body_demo' "$repo_root/scripts/windows/test_portable.sh"
grep -Fq 'etc/gdbinit' "$repo_root/scripts/windows/stage_debugger.sh"
grep -Fq 'libstdcxx/v6/printers.py' "$repo_root/scripts/windows/test_portable.sh"
grep -Fq 'sha256sum' "$build_script"
grep -Fq 'verify_installer_artifact.sh' "$workflow"
grep -Fq 'build/installer/*.exe.sha256' "$workflow"

echo "Windows installer policy tests passed."
