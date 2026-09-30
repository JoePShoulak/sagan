#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_root"
mkdir -p build build/tmp
export TMPDIR="$repo_root/build/tmp" TMP="$repo_root/build/tmp" TEMP="$repo_root/build/tmp"

echo "Sagan input:"
echo "------------"
sed -n '1,240p' tests/fixtures/runtime/generic_sum.sagan

bin/sagan --emit-cpp tests/fixtures/runtime/generic_sum.sagan build/generic_sum_demo.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/generic_sum_demo.cpp -o build/generic_sum_demo

echo
build/generic_sum_demo
echo
echo "Generics test passed: call-site inference executed identity<T>, contextual construction selected Success, and Result<Int, String>.Failure supplied explicit enum arguments."
