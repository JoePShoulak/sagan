#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

echo "Module sources:"
echo "---------------"
for source in tests/fixtures/modules/module_demo/*.sagan; do
  echo
  echo "$source"
  cat "$source"
done

echo
echo "Resolved graph:"
echo "---------------"
bin/sagan --modules tests/fixtures/modules/module_demo/main.sagan

echo
echo "Linked native execution:"
echo "------------------------"
mkdir -p build/tmp
bin/sagan --emit-cpp-modules tests/fixtures/modules/module_demo/main.sagan build/module_demo.cpp
native_output="build/module_demo"
if [[ "${OS:-}" == "Windows_NT" ]]; then
  native_output="build/module_demo.exe"
fi
repo_tmp_native="$repo_root/build/tmp"
if command -v cygpath >/dev/null 2>&1; then
  repo_tmp_native="$(cygpath -w "$repo_tmp_native")"
fi
TMPDIR="$repo_tmp_native" TMP="$repo_tmp_native" TEMP="$repo_tmp_native" \
  g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/module_demo.cpp -o "$native_output"
"$native_output"

expect_failure() {
  local source="$1"
  local expected="$2"
  local output
  set +e
  output="$(bin/sagan --modules "$source" 2>&1)"
  local status=$?
  set -e
  if [[ "$status" -eq 0 || "$output" != *"$expected"* ]]; then
    echo "Expected module failure containing '$expected' from $source" >&2
    echo "$output" >&2
    exit 1
  fi
  echo "Confirmed module error: $expected"
}

expect_failure tests/fixtures/modules/module_cycle/alpha.sagan "Cyclic module dependency: alpha -> beta -> alpha"
expect_failure tests/fixtures/modules/module_export_error/main.sagan "does not export 'missing'"
expect_failure tests/fixtures/modules/module_name_error/main.sagan "declares 'wrong_name', expected 'main'"
expect_failure tests/fixtures/modules/module_missing/main.sagan "Could not open module"
expect_failure tests/fixtures/modules/module_declaration_error/main.sagan "must declare 'module support'"
expect_failure tests/fixtures/modules/module_undefined_export_error/main.sagan "exports undefined declaration 'missing'"
expect_failure tests/fixtures/modules/module_duplicate_export_error/main.sagan "exports duplicate public name 'value'"

set +e
type_output="$(bin/sagan --emit-cpp-modules tests/fixtures/modules/module_type_error/main.sagan 2>&1)"
type_status=$?
set -e
if [[ "$type_status" -eq 0 || "$type_output" != *"No matching overload for 'guidance__calculate'"* ]]; then
  echo "Expected cross-module type diagnostic." >&2
  echo "$type_output" >&2
  exit 1
fi
echo "Confirmed linked type error: imported function rejects String"

set +e
namespace_output="$(bin/sagan --emit-cpp-modules tests/fixtures/modules/module_namespace_error/main.sagan 2>&1)"
namespace_status=$?
set -e
if [[ "$namespace_status" -eq 0 || "$namespace_output" != *"Module 'support' does not export 'private_value'"* ]]; then
  echo "Expected module namespace visibility diagnostic." >&2
  echo "$namespace_output" >&2
  exit 1
fi
echo "Confirmed namespace visibility error: private member is inaccessible"

echo "Module test passed: sibling resolution, transitive dependencies, selective and namespace imports, export visibility, semantic/type linking, native execution, and cycle diagnostics are working."
