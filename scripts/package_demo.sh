#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Package manifest and sources:"
echo "-----------------------------"
cat examples/package/sagan.toml
while IFS= read -r source; do
  echo
  echo "$source"
  cat "$source"
done < <(find examples/package/src -type f -name '*.sagan' | sort)

echo
echo "Resolved package graph:"
echo "-----------------------"
bin/sagan --package examples/package

echo
echo "Linked native execution:"
echo "------------------------"
mkdir -p build/tmp
bin/sagan --emit-cpp-package examples/package build/package_demo.cpp
native_output="build/package_demo"
if [[ "${OS:-}" == "Windows_NT" ]]; then
  native_output="build/package_demo.exe"
fi
if command -v cygpath >/dev/null 2>&1; then
  repo_tmp_native="$(cygpath -w "$repo_root/build/tmp")"
else
  repo_tmp_native="$repo_root/build/tmp"
fi
TMPDIR="$repo_tmp_native" TMP="$repo_tmp_native" TEMP="$repo_tmp_native" \
  g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/package_demo.cpp -o "$native_output"
"$native_output"

echo "Package demo passed: the manifest selected the entry module, dotted modules mapped to nested source files, imports linked, and native execution succeeded."
