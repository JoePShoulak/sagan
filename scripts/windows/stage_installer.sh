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

if [[ ! -x "$toolchain_root/bin/g++.exe" ]]; then
  echo "The Windows installer requires a UCRT64 toolchain at $toolchain_root." >&2
  exit 1
fi

rm -rf "$stage_dir"
mkdir -p "$stage_dir/bin" "$stage_dir/toolchain/ucrt64" "$stage_dir/assets" "$stage_dir/licenses"

make -C "$repo_root" all windows-launcher OS=Windows_NT SAGAN_VERSION="$version"
cp "$repo_root/bin/sagan" "$stage_dir/bin/sagan.exe"
cp "$repo_root/bin/sagan-launch.exe" "$stage_dir/bin/sagan-launch.exe"
cp "$repo_root/packaging/windows/sagan.ico" "$stage_dir/assets/sagan.ico"
cp "$repo_root/editors/vscode-sagan/LICENSE.txt" "$stage_dir/licenses/Sagan-GPL-3.0.txt"
cp "$repo_root/third_party/uni-algo/LICENSE.md" "$stage_dir/licenses/uni-algo-MIT.txt"
cp "$repo_root/third_party/unicode/LICENSE.txt" "$stage_dir/licenses/Unicode.txt"

for directory in bin include lib libexec x86_64-w64-mingw32; do
  if [[ -d "$toolchain_root/$directory" ]]; then
    cp -a "$toolchain_root/$directory" "$stage_dir/toolchain/ucrt64/"
  fi
done

printf '%s\n' "$version" > "$stage_dir/VERSION"

unset CXX
"$stage_dir/bin/sagan.exe" --version
"$stage_dir/bin/sagan.exe" "$repo_root/examples/run_demo.sagan"

echo "Staged the self-contained Windows distribution at $repo_root/$stage_dir"
