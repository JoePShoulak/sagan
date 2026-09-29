#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root"
mkdir -p build build/tmp
export TMPDIR="$repo_root/build/tmp" TMP="$repo_root/build/tmp" TEMP="$repo_root/build/tmp"

echo "Sagan input:"
echo "------------"
sed -n '1,240p' examples/generic_sum_demo.sagan

bin/sagan --emit-cpp examples/generic_sum_demo.sagan build/generic_sum_demo.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/generic_sum_demo.cpp -o build/generic_sum_demo

echo
build/generic_sum_demo
echo
echo "Generic-sum demo passed: contextual type arguments selected both cases and payloads matched safely."
