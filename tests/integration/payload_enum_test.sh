#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_root"

mkdir -p build build/tmp
export TMPDIR="$repo_root/build/tmp"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"

echo "Sagan input:"
echo "------------"
sed -n '1,240p' tests/fixtures/runtime/payload_enum.sagan

bin/sagan --emit-cpp tests/fixtures/runtime/payload_enum.sagan build/payload_enum_demo.cpp

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
echo "Payload-enum test passed: explicit Int64 tags, implicit continuation, typed payloads, and exhaustive matching executed natively."
