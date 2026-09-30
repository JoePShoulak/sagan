#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_root"
mkdir -p build build/tmp
if command -v cygpath >/dev/null 2>&1; then
  repo_tmp_native="$(cygpath -w "$repo_root/build/tmp")"
else
  repo_tmp_native="$repo_root/build/tmp"
fi
export TMPDIR="$repo_tmp_native" TMP="$repo_tmp_native" TEMP="$repo_tmp_native"

echo "Sagan input:"
echo "------------"
sed -n '1,240p' tests/fixtures/runtime/generic_class.sagan

bin/sagan --emit-cpp tests/fixtures/runtime/generic_class.sagan build/generic_class_demo.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/generic_class_demo.cpp -o build/generic_class_demo

echo
build/generic_class_demo
echo
echo "Generic test passed: explicit constructor/function/method arguments and a specialized face constraint executed natively."
