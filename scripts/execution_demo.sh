#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build/tmp
repo_tmp_native="$(cygpath -w "$repo_root/build/tmp")"
export TMPDIR="$repo_tmp_native"
export TMP="$repo_tmp_native"
export TEMP="$repo_tmp_native"

echo "Sagan input:"
echo "------------"
cat examples/execution_demo.sagan
echo

bin/sagan --emit-cpp examples/execution_demo.sagan build/execution_demo.cpp

echo
echo "Generated C++:"
echo "--------------"
cat build/execution_demo.cpp

native_output="build/execution_demo"
if [[ "${OS:-}" == "Windows_NT" ]]; then
  native_output="build/execution_demo.exe"
fi

g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/execution_demo.cpp -o "$native_output"

set +e
"$native_output"
status=$?
set -e

echo
echo "Native process exit code: $status"
if [[ "$status" -ne 0 ]]; then
  echo "Execution demo failed." >&2
  exit 1
fi
echo "Execution demo passed: Sagan executed compact bodies, collection spreads, matching, interpolation, typed collections, mutable loops, and Unicode output."
