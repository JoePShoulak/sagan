#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
mkdir -p build/tmp
repo_tmp_native="$(cygpath -w "$repo_root/build/tmp")"
export TMPDIR="$repo_tmp_native"
export TMP="$repo_tmp_native"
export TEMP="$repo_tmp_native"

echo "Sagan input:"
echo "------------"
cat tests/fixtures/runtime/optional.sagan
echo

bin/sagan --emit-cpp tests/fixtures/runtime/optional.sagan build/optional_demo.cpp

echo "Generated C++:"
echo "--------------"
cat build/optional_demo.cpp
echo

native_output="build/optional_demo"
if [[ "${OS:-}" == "Windows_NT" ]]; then
  native_output="build/optional_demo.exe"
fi
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/optional_demo.cpp -o "$native_output"

"$native_output"
echo "Optional test passed: payload matching, safe member access, safe method calls, lazy ?? fallback, and chaining executed natively."
