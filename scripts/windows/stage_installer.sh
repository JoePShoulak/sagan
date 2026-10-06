#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
stage_dir="build/windows-stage"
default_toolchain="/ucrt64"
if [[ ! -x "$default_toolchain/bin/g++.exe" && -x /c/msys64/ucrt64/bin/g++.exe ]]; then
  default_toolchain="/c/msys64/ucrt64"
fi
toolchain_root="${SAGAN_TOOLCHAIN_ROOT:-$default_toolchain}"
version="$(bash "$repo_root/scripts/version.sh" numeric)"
debugger_python_version="$(bash "$repo_root/scripts/windows/debugger_python_version.sh" "$toolchain_root/bin/gdb.exe")"

if [[ ! -x "$toolchain_root/bin/g++.exe" ]]; then
  echo "The Windows installer requires a UCRT64 toolchain at $toolchain_root." >&2
  exit 1
fi

rm -rf "$stage_dir"
mkdir -p "$stage_dir/bin" "$stage_dir/toolchain/ucrt64" "$stage_dir/assets" \
  "$stage_dir/licenses" "$stage_dir/libraries" "$stage_dir/examples"

stage_lsp=build/tmp/sagan-lsp-release-stage
stage_dap=build/tmp/sagan-dap-release-stage
make -C "$repo_root" all "$stage_lsp" "$stage_dap" windows-launcher \
  LANGUAGE_SERVER="$stage_lsp" DEBUG_ADAPTER="$stage_dap" \
  OS=Windows_NT SAGAN_VERSION="$version"
cp "$repo_root/bin/sagan" "$stage_dir/bin/sagan.exe"
cp "$repo_root/$stage_lsp" "$stage_dir/bin/sagan-lsp.exe"
cp "$repo_root/$stage_dap" "$stage_dir/bin/sagan-dap.exe"
cp "$repo_root/bin/sagan-launch.exe" "$stage_dir/bin/sagan-launch.exe"
cp "$repo_root/packaging/windows/sagan.ico" "$stage_dir/assets/sagan.ico"
cp "$repo_root/obj/launcher/sagan-resource.o" "$stage_dir/assets/sagan-resource.o"
cp "$repo_root/libraries/index.tsv" "$stage_dir/libraries/index.tsv"
cp -a "$repo_root/libraries/physics" "$stage_dir/libraries/physics"
cp -a "$repo_root/libraries/render" "$stage_dir/libraries/render"
cp -a "$repo_root/examples/two_body_demo" "$stage_dir/examples/two_body_demo"
cp "$repo_root/LICENSE.txt" "$stage_dir/licenses/Sagan-GPL-3.0.txt"
cp "$repo_root/third_party/uni-algo/LICENSE.md" "$stage_dir/licenses/uni-algo-MIT.txt"
cp "$repo_root/third_party/unicode/LICENSE.txt" "$stage_dir/licenses/Unicode.txt"
mkdir -p "$stage_dir/licenses/toolchain"
copy_toolchain_license() {
  local license_package="$1"
  license_source="$toolchain_root/share/licenses/$license_package"
  if [[ ! -d "$license_source" ]]; then
    echo "The Windows toolchain is missing license material for $license_package." >&2
    exit 1
  fi
  cp -a "$license_source" "$stage_dir/licenses/toolchain/"
}

for license_package in binutils crt headers winpthreads; do
  copy_toolchain_license "$license_package"
done

# MSYS2 GCC 16.2.0-4 split the former gcc-libs payload into libgcc and
# libstdc++. Accept the legacy directory or require both replacement license
# directories, so old local installations and current clean CI runners remain
# redistributable.
if [[ -d "$toolchain_root/share/licenses/gcc-libs" ]]; then
  copy_toolchain_license gcc-libs
else
  copy_toolchain_license libgcc
  copy_toolchain_license libstdc++
fi

# libwinpthread was split from winpthreads in the same packaging generation.
# Its license is identical in older layouts but must be retained when present.
if [[ -d "$toolchain_root/share/licenses/libwinpthread" ]]; then
  copy_toolchain_license libwinpthread
fi

compiler_target="$("$toolchain_root/bin/g++.exe" -dumpmachine)"
compiler_version="$("$toolchain_root/bin/g++.exe" -dumpversion)"
staged_toolchain="$stage_dir/toolchain/ucrt64"

# Ship only the GCC C++ compilation closure Sagan needs. Copying the complete
# UCRT64 prefix also bundles unrelated development tools, debuggers, Python,
# tests, and documentation, making the offline installer unnecessarily large.
mkdir -p \
  "$staged_toolchain/bin" \
  "$staged_toolchain/include" \
  "$staged_toolchain/lib" \
  "$staged_toolchain/lib/gcc/$compiler_target" \
  "$staged_toolchain/$compiler_target"

for executable in g++.exe gcc.exe as.exe ld.exe; do
  if [[ ! -f "$toolchain_root/bin/$executable" ]]; then
    echo "The Windows toolchain is missing $toolchain_root/bin/$executable." >&2
    exit 1
  fi
  cp -L "$toolchain_root/bin/$executable" "$staged_toolchain/bin/"
done

# Compiler processes load these MinGW runtime dependencies. Generated Sagan
# programs also use the C++ runtime DLL unless callers request static linkage.
for runtime_dll in \
  libgcc_s_seh-1.dll libgmp-10.dll libiconv-2.dll libintl-8.dll \
  libisl-23.dll libmpc-3.dll libmpfr-6.dll libstdc++-6.dll \
  libwinpthread-1.dll libzstd.dll zlib1.dll; do
  cp -L "$toolchain_root/bin/$runtime_dll" "$staged_toolchain/bin/"
done
cp -a "$toolchain_root/include/." "$staged_toolchain/include/"
# The MinGW platform headers share this prefix with optional third-party SDKs.
# Remove packages that cannot be reached by Sagan's generated standard C++.
for unrelated_headers in \
  gdb isl libiberty lzma ncurses ncursesw openssl pkgconf "python$debugger_python_version" \
  readline tcl8.6 tk8.6 tre X11; do
  rm -rf "$staged_toolchain/include/$unrelated_headers"
done
cp -a "$toolchain_root/lib/gcc/$compiler_target/$compiler_version" \
  "$staged_toolchain/lib/gcc/$compiler_target/"
rm -rf \
  "$staged_toolchain/lib/gcc/$compiler_target/$compiler_version/install-tools" \
  "$staged_toolchain/lib/gcc/$compiler_target/$compiler_version/plugin"
rm -f \
  "$staged_toolchain/lib/gcc/$compiler_target/$compiler_version/cc1.exe" \
  "$staged_toolchain/lib/gcc/$compiler_target/$compiler_version/g++-mapper-server.exe" \
  "$staged_toolchain/lib/gcc/$compiler_target/$compiler_version/libgcov.a"

# GCC's default C++ link line resolves these CRT objects and import libraries
# from the prefix lib directory. Do not ship unrelated third-party archives.
for runtime_library in \
  crt2.o default-manifest.o libstdc++.a libstdc++.dll.a libmingw32.a \
  libgcc_s.a libmingwex.a libmsvcrt.a libkernel32.a libpthread.a \
  libadvapi32.a libshell32.a libuser32.a libgdi32.a; do
  cp -L "$toolchain_root/lib/$runtime_library" "$staged_toolchain/lib/"
done
cp -a "$toolchain_root/$compiler_target/." "$staged_toolchain/$compiler_target/"

if find "$staged_toolchain" -type f \( -iname 'python*.exe' -o -iname 'gdb*.exe' \) -print -quit | grep -q .; then
  echo "The staged compiler unexpectedly contains an unrelated Python or GDB executable." >&2
  exit 1
fi

bash "$repo_root/scripts/windows/stage_debugger.sh" "$toolchain_root" "$staged_toolchain"
mkdir -p "$stage_dir/licenses/debugger"
gpl_text="$repo_root/packaging/windows/GDB-GPL-3.0.txt"
[[ -f "$gpl_text" ]] || { echo "Missing debugger GPLv3 license text: $gpl_text" >&2; exit 1; }
cp "$gpl_text" "$stage_dir/licenses/debugger/GDB-GPL-3.0.txt"
for debugger_license in expat "python$debugger_python_version" readline xxhash xz; do
  copy_toolchain_license "$debugger_license"
done

printf '%s\n' "$version" > "$stage_dir/VERSION"

unset CXX
"$stage_dir/bin/sagan.exe" --version
"$stage_dir/bin/sagan.exe" "$repo_root/tests/fixtures/runtime/smoke.sagan"

toolchain_megabytes="$(du -sm "$staged_toolchain" | awk '{print $1}')"
echo "Staged GCC C++ toolchain size: ${toolchain_megabytes} MiB"
echo "Staged the self-contained Windows distribution at $repo_root/$stage_dir"
