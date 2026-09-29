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

expect_runtime_error() {
  local source_file="$1"
  local expected_message="$2"
  local stem
  stem="$(basename "$source_file" .sagan)"
  local generated="build/${stem}.cpp"
  local executable="build/${stem}"
  if [[ "${OS:-}" == "Windows_NT" ]]; then
    executable="${executable}.exe"
  fi

  bin/sagan --emit-cpp "$source_file" "$generated" >/dev/null
  g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror "$generated" -o "$executable"
  set +e
  local failure_output
  failure_output="$("$executable" 2>&1)"
  local failure_status=$?
  set -e
  if [[ "$failure_status" -eq 0 || "$failure_output" != *"$expected_message"* ]]; then
    echo "Expected runtime error '$expected_message' from $source_file." >&2
    echo "$failure_output" >&2
    exit 1
  fi
  echo "Confirmed runtime error: $expected_message"
}

expect_runtime_error examples/execution_exponent_zero_error.sagan \
  "Sagan exponentiation does not define 0 ^ 0"
expect_runtime_error examples/execution_exponent_negative_error.sagan \
  "Sagan integer exponentiation requires a non-negative exponent"
expect_runtime_error examples/execution_exponent_overflow_error.sagan \
  "Sagan integer exponentiation overflow"
expect_runtime_error examples/execution_addition_overflow_error.sagan \
  "Sagan integer addition overflow"
expect_runtime_error examples/execution_subtraction_overflow_error.sagan \
  "Sagan integer subtraction overflow"
expect_runtime_error examples/execution_multiplication_overflow_error.sagan \
  "Sagan integer multiplication overflow"
expect_runtime_error examples/execution_division_zero_error.sagan \
  "Sagan division by zero"
expect_runtime_error examples/execution_division_overflow_error.sagan \
  "Sagan integer division overflow"
expect_runtime_error examples/execution_modulo_zero_error.sagan \
  "Sagan modulo by zero"
expect_runtime_error examples/execution_negation_overflow_error.sagan \
  "Sagan integer negation overflow"
expect_runtime_error examples/execution_increment_overflow_error.sagan \
  "Sagan integer addition overflow"
expect_runtime_error examples/execution_decrement_overflow_error.sagan \
  "Sagan integer subtraction overflow"
expect_runtime_error examples/execution_float_division_zero_error.sagan \
  "Sagan division by zero"
expect_runtime_error examples/execution_vector_overflow_error.sagan \
  "Sagan integer addition overflow"
expect_runtime_error examples/execution_vector_division_zero_error.sagan \
  "Sagan division by zero"

echo "Execution demo passed: Sagan enforced private methods, executed nominal enums, verified face composition, and executed default-constructed classes, fields, self, methods, lambdas, dimensioned values, checked arithmetic, collections, control flow, and Unicode output."
