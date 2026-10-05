#!/usr/bin/env bash
set -euo pipefail

toolchain_root="${1:?expected UCRT64 toolchain root}"
stage_root="${2:?expected staged UCRT64 root}"

compiler_version="$("$toolchain_root/bin/g++.exe" -dumpfullversion)"
gcc_python="share/gcc-$compiler_version/python"
for required in bin/gdb.exe bin/libpython3.12.dll lib/python3.12 share/gdb/python \
  etc/gdbinit "$gcc_python/libstdcxx/v6/printers.py"; do
  if [[ ! -e "$toolchain_root/$required" ]]; then
    echo "The debugger toolchain is missing $toolchain_root/$required." >&2
    exit 1
  fi
done

mkdir -p "$stage_root/bin" "$stage_root/lib" "$stage_root/share/gdb" \
  "$stage_root/share/gcc-$compiler_version" "$stage_root/etc"
cp -L "$toolchain_root/bin/gdb.exe" "$stage_root/bin/gdb.exe"
cp -a "$toolchain_root/lib/python3.12" "$stage_root/lib/"
cp -a "$toolchain_root/share/gdb/python" "$stage_root/share/gdb/"
cp -a "$toolchain_root/$gcc_python" "$stage_root/share/gcc-$compiler_version/"
cp "$toolchain_root/etc/gdbinit" "$stage_root/etc/gdbinit"

# The Python regression suite is not a debugger runtime dependency. Confirm
# this exact generated target is inside the repository before pruning it.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
staged_absolute="$(cd "$stage_root" && pwd -P)"
case "$staged_absolute" in
  "$repo_root"/build/*) ;;
  *) echo "Refusing to prune a debugger stage outside $repo_root/build." >&2; exit 1 ;;
esac
python_tests="$staged_absolute/lib/python3.12/test"
if [[ "$(realpath -m "$python_tests")" != "$staged_absolute/lib/python3.12/test" ]]; then
  echo "Refusing to prune an unexpected Python test directory." >&2
  exit 1
fi
rm -rf -- "$python_tests"

# GDB's direct MinGW imports not already included by the C++ toolchain stage.
for dependency in \
  libexpat-1.dll liblzma-5.dll libncursesw6.dll libpython3.12.dll \
  libreadline8.dll libtermcap-0.dll libxxhash.dll; do
  if [[ ! -f "$toolchain_root/bin/$dependency" ]]; then
    echo "The debugger toolchain is missing $toolchain_root/bin/$dependency." >&2
    exit 1
  fi
  cp -L "$toolchain_root/bin/$dependency" "$stage_root/bin/"
done
