#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

tests=(
  parser_test.sh
  semantic_test.sh
  type_test.sh
  entry_test.sh
  module_test.sh
  package_dependency_test.sh
  codegen_test.sh
  runtime_test.sh
  optional_test.sh
  payload_enum_test.sh
  generic_sum_test.sh
  generic_class_test.sh
  class_inheritance_test.sh
  closure_test.sh
  constants_test.sh
  orbit_math_test.sh
  orbit_numeric_test.sh
  lagrange_numeric_test.sh
  solar_lagrange_numeric_test.sh
  array_times_parallel_test.sh
  parallel_let_fibonacci_test.sh
  mixed_exponent_test.sh
  root_script_test.sh
  assert_test.sh
)

for test_script in "${tests[@]}"; do
  echo
  echo "==> tests/integration/$test_script"
  bash "tests/integration/$test_script"
done

echo
echo "All Sagan integration tests passed."
