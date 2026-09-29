#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root"

mkdir -p build build/tmp
export TMPDIR="$repo_root/build/tmp"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"

echo "Sagan input:"
echo "------------"
sed -n '1,240p' examples/payload_enum_demo.sagan

bin/sagan --emit-cpp examples/payload_enum_demo.sagan build/payload_enum_demo.cpp

echo
echo "Generated tagged-variant excerpts:"
echo "----------------------------------"
grep -E 'struct sagan_526573756c74|std::variant|std::get|enum class Tag| = 200| = 500| = -1' build/payload_enum_demo.cpp

g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/payload_enum_demo.cpp -o build/payload_enum_demo

echo
build/payload_enum_demo
exit_code=$?

echo
echo "Native process exit code: $exit_code"
echo "Payload-enum demo passed: explicit Int64 tags, implicit continuation, typed payloads, and exhaustive matching executed natively."
