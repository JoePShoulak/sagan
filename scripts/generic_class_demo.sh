#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
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
sed -n '1,240p' examples/generic_class_demo.sagan

bin/sagan --emit-cpp examples/generic_class_demo.sagan build/generic_class_demo.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/generic_class_demo.cpp -o build/generic_class_demo

echo
build/generic_class_demo
echo
echo "Generic class/face demo passed: Readable<T> supplied a virtual default, echo<U> inferred independently, and typed mutation preserved T."
