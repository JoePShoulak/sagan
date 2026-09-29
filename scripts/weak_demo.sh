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
cat examples/weak_demo.sagan
echo

bin/sagan --emit-cpp examples/weak_demo.sagan build/weak_demo.cpp

echo "Generated weak-reference C++ excerpts:"
echo "--------------------------------------"
grep -E "weak_ptr|sagan_lock_weak|Live weak|Expired weak" build/weak_demo.cpp
echo

native_output="build/weak_demo"
if [[ "${OS:-}" == "Windows_NT" ]]; then
  native_output="build/weak_demo.exe"
fi
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/weak_demo.cpp -o "$native_output"

"$native_output"
echo "Weak-reference demo passed: a live target resolved to Some and an expired target resolved to None."
