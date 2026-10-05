#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

valid_output="$(bin/sagan examples/orbit_math.sagan)"
for expected in \
  "Squared length: 2.5e+07" \
  "Length: 5000" \
  "Direction: <0.6, 0.8>" \
  "Display coordinates: <3, 4>" \
  "Square root: 9"
do
  if [[ "$valid_output" != *"$expected"* ]]; then
    echo "Missing orbit math output: $expected" >&2
    echo "$valid_output" >&2
    exit 1
  fi
done

expect_failure() {
  local source_file="$1"
  local expected="$2"
  set +e
  local output
  output="$(bin/sagan "$source_file" 2>&1)"
  local status=$?
  set -e
  if [[ "$status" -eq 0 || "$output" != *"$expected"* ]]; then
    echo "Expected failure containing '$expected' from $source_file" >&2
    echo "$output" >&2
    exit 1
  fi
}

expect_failure tests/fixtures/semantic/orbit_math_unit_mismatch.sagan \
  "display_coordinates scale must use the same physical dimension as the Points"
expect_failure tests/fixtures/runtime/orbit_math_zero_length.sagan \
  "normalized cannot normalize a zero-length Vector"
expect_failure tests/fixtures/runtime/orbit_math_non_finite.sagan \
  "length requires finite components"
expect_failure tests/fixtures/runtime/orbit_math_sqrt_domain.sagan \
  "sqrt requires a non-negative value"
expect_failure tests/fixtures/semantic/vector_normalized_unit_mutation.sagan \
  "normalized! cannot change a unit-typed Vector"
expect_failure tests/fixtures/semantic/vector_normalized_const_mutation.sagan \
  "cannot be mutated"
expect_failure tests/fixtures/semantic/vector_legacy_builtin.sagan \
  "Undefined name 'length'"
expect_failure tests/fixtures/semantic/vector_method_arguments.sagan \
  "Vector.length expects no arguments"

echo "Orbit math test passed: checked orbital primitives preserve units and reject invalid inputs."
