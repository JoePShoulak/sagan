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

# Inspect imports before launching so a regression fails non-interactively
# instead of presenting a missing-DLL dialog on a CI desktop.
objdump="${OBJDUMP:-}"
if [[ -z "$objdump" ]]; then
  objdump="$(command -v objdump.exe || command -v objdump || true)"
fi
if [[ -z "$objdump" || ! -x "$objdump" ]]; then
  echo "Installer testing requires objdump; set OBJDUMP to its path." >&2
  exit 1
fi
for executable in "$install_dir/bin/sagan.exe" "$install_dir/bin/sagan-lsp.exe" \
                  "$install_dir/bin/sagan-launch.exe"; do
  imports="$("$objdump" -p "$executable" | grep 'DLL Name')"
  if printf '%s\n' "$imports" | grep -Eiq 'lib(gcc|stdc\+\+|winpthread)'; then
    printf 'Installed executable imports a MinGW runtime DLL:\n%s\n' "$imports" >&2
    exit 1
  fi
done

# Run installed executables without any MinGW/MSYS2 compiler-runtime directory.
# This catches accidental dependencies on libgcc_s_seh-1.dll,
# libstdc++-6.dll, or libwinpthread-1.dll that are otherwise hidden by the CI
# shell's inherited PATH.
runtime_isolated_path="/usr/bin:/bin:/c/Windows/System32:/c/Windows"
case "$runtime_isolated_path" in
  *ucrt64*|*mingw32*|*mingw64*|*clang32*|*clang64*)
    echo "Installer smoke-test PATH unexpectedly contains a compiler runtime directory." >&2
    exit 1
    ;;
esac

PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" --version
# This invokes the bundled compiler and then executes the generated program,
# proving Sagan can add only the child toolchain environment it actually needs.
PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" "$repo_root/tests/fixtures/runtime/smoke.sagan"

# Simulate a newer installed release and prove the older package is refused.
# The test owns this current-user installation and restores its metadata before
# continuing, so it cannot disturb a machine-wide Sagan installation.
uninstall_key='HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{09D785A5-B94E-4D75-9CC9-E57831637DD5}_is1'
downgrade_log="$repo_root/build/installer-downgrade.log"
MSYS2_ARG_CONV_EXCL='*' reg.exe add "$uninstall_key" /v DisplayVersion /t REG_SZ /d 999.0.0 /f >/dev/null
rm -f "$downgrade_log"
if MSYS2_ARG_CONV_EXCL='*' "$installer" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER \
  "/DIR=$(cygpath -w "$install_dir")" "/LOG=$(cygpath -w "$downgrade_log")"; then
  echo "The installer unexpectedly allowed a downgrade from 999.0.0 to $version." >&2
  exit 1
fi
grep -Fq 'InitializeSetup returned False; aborting.' "$downgrade_log"
MSYS2_ARG_CONV_EXCL='*' reg.exe add "$uninstall_key" /v DisplayVersion /t REG_SZ /d "$version" /f >/dev/null

# Re-running the same installer exercises the supported in-place upgrade path.
MSYS2_ARG_CONV_EXCL='*' "$installer" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CURRENTUSER "/DIR=$(cygpath -w "$install_dir")" \
  /TASKS="addtopath,fileassociation"
PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" --version
launcher_data="$repo_root/build/installer-launcher-data"
mkdir -p "$launcher_data"
PATH="$runtime_isolated_path" LOCALAPPDATA="$(cygpath -w "$launcher_data")" \
  "$install_dir/bin/sagan-launch.exe" --windowed "$repo_root/tests/fixtures/runtime/smoke.sagan"
grep -Fq "Direct Sagan execution: 42" "$launcher_data/Sagan/logs/latest-launch.log"
MSYS2_ARG_CONV_EXCL='*' reg.exe query 'HKCU\Software\Classes\.sagan' /ve | grep -Fq 'Sagan.Source'
MSYS2_ARG_CONV_EXCL='*' reg.exe query 'HKCU\Environment' /v Path | grep -Fq "$(cygpath -w "$install_dir/bin")"

MSYS2_ARG_CONV_EXCL='*' "$install_dir/unins000.exe" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
if [[ -e "$install_dir/bin/sagan.exe" ]]; then
  echo "The installer smoke test could not remove the installed compiler." >&2
  exit 1
fi
if [[ -e "$install_dir/bin/sagan-lsp.exe" ]]; then
  echo "The installer smoke test could not remove the installed language server." >&2
  exit 1
fi

echo "Windows installer isolated-PATH CLI, generated-program, Explorer launch, and uninstall smoke test passed."
