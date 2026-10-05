#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
cd "$repo_root"
archive="build/release/sagan-$version-windows-x64.zip"
checksum="$archive.sha256"
install_dir="build/portable-smoke"

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
[[ -x "$install_dir/bin/sagan-lsp.exe" ]]
[[ -x "$install_dir/bin/sagan-dap.exe" ]]
[[ -x "$install_dir/toolchain/ucrt64/bin/g++.exe" ]]
[[ -x "$install_dir/toolchain/ucrt64/bin/gdb.exe" ]]
python_version="$(bash "$repo_root/scripts/windows/debugger_python_version.sh" "$install_dir/toolchain/ucrt64/bin/gdb.exe")"
[[ -f "$install_dir/toolchain/ucrt64/bin/libpython$python_version.dll" ]]
[[ -f "$install_dir/toolchain/ucrt64/lib/python$python_version/asyncio/__init__.py" ]]
[[ -f "$install_dir/toolchain/ucrt64/share/gdb/python/gdb/dap/__init__.py" ]]
[[ -f "$install_dir/toolchain/ucrt64/etc/gdbinit" ]]
compiler_version="$("$install_dir/toolchain/ucrt64/bin/g++.exe" -dumpfullversion)"
[[ -f "$install_dir/toolchain/ucrt64/share/gcc-$compiler_version/python/libstdcxx/v6/printers.py" ]]
[[ -f "$install_dir/licenses/debugger/GDB-GPL-3.0.txt" ]]
grep -q 'GNU GENERAL PUBLIC LICENSE' "$install_dir/licenses/debugger/GDB-GPL-3.0.txt"
[[ -f "$install_dir/assets/sagan-resource.o" ]]
[[ -f "$install_dir/libraries/index.tsv" ]]
[[ -f "$install_dir/libraries/render/native/window_bridge.cpp" ]]
[[ -f "$install_dir/libraries/render/native/window_bridge.hpp" ]]
[[ -f "$install_dir/libraries/physics/src/two_body.sagan" ]]
[[ -f "$install_dir/examples/two_body_demo/src/main.sagan" ]]
[[ ! -e "$install_dir/bin/sagan-launch.exe" ]]

runtime_isolated_path="/usr/bin:/bin:/c/Windows/System32:/c/Windows"
PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" --version
PATH="$runtime_isolated_path" "$install_dir/toolchain/ucrt64/bin/gdb.exe" --version
PATH="$runtime_isolated_path" "$install_dir/toolchain/ucrt64/bin/gdb.exe" --batch \
  -ex 'info pretty-printer' | grep -Fq 'libstdc++-v6'
PATH="$runtime_isolated_path" "$install_dir/bin/sagan.exe" "$repo_root/tests/fixtures/runtime/smoke.sagan"
unset SAGAN_PACKAGE_INDEX
orbit_output="$(PATH="$runtime_isolated_path" \
  SAGAN_RENDER_FRAME_LIMIT=3 SAGAN_RENDER_TEST_FRAME_MS=5 \
  "$install_dir/bin/sagan.exe" --run-package "$install_dir/examples/two_body_demo")"
grep -Fq 'final_time_s ' <<< "$orbit_output"

test_python="$(command -v python || command -v python3)"
PATH="$runtime_isolated_path" \
  SAGAN_DAP_BINARY="$(cygpath -w "$PWD/$install_dir/bin/sagan-dap.exe")" \
  SAGAN_TEST_GDB="$(cygpath -w "$PWD/$install_dir/toolchain/ucrt64/bin/gdb.exe")" \
  SAGAN_DAP_TEST_REQUIRE_GDB=1 \
  "$test_python" tests/dap_protocol_test.py

echo "Portable Windows archive checksum, libraries, isolated CLI, DAP, and windowed orbit execution tests passed."
