#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_root"
mkdir -p build build/tmp
export TMPDIR="$repo_root/build/tmp" TMP="$repo_root/build/tmp" TEMP="$repo_root/build/tmp"

echo "Sagan input:"
echo "------------"
sed -n '1,240p' tests/fixtures/runtime/runtime_errors.sagan

bin/sagan --emit-cpp tests/fixtures/runtime/runtime_errors.sagan build/runtime_error_demo.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/runtime_error_demo.cpp -o build/runtime_error_demo

echo
echo "Runtime output:"
echo "---------------"
output=$(build/runtime_error_demo)
printf '%s\n' "$output"

for expected in \
  "Caught integer overflow" \
  "Caught division by zero" \
  "Caught array index out of bounds" \
  "Caught missing dictionary key" \
  "Caught: 4; cleanup: 1"
do
  if [[ "$output" != *"$expected"* ]]; then
    echo "Runtime-error demo did not print: $expected" >&2
    exit 1
  fi
done

echo
echo "Runtime-error test passed: native failures became catchable RuntimeError values and finally cleanup ran."
