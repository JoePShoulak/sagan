#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"

valid_output="$(bin/sagan --run-package examples/orbit_numeric_demo)"
for expected in \
  "time_s primary_x_m primary_y_m secondary_x_m secondary_y_m energy_J angular_momentum" \
  "energy_absolute_drift" \
  "energy_absolute_tolerance" \
  "angular_momentum_absolute_drift" \
  "angular_momentum_absolute_tolerance" \
  "barycenter_distance_m"
do
  if [[ "$valid_output" != *"$expected"* ]]; then
    echo "Missing two-body output: $expected" >&2
    echo "$valid_output" >&2
    exit 1
  fi
done

expect_failure() {
  local package="$1"
  local expected="$2"
  set +e
  local output
  output="$(bin/sagan --run-package "$package" 2>&1)"
  local status=$?
  set -e
  if [[ "$status" -eq 0 || "$output" != *"$expected"* ]]; then
    echo "Expected failure containing '$expected' from $package" >&2
    echo "$output" >&2
    exit 1
  fi
}

expect_failure tests/fixtures/physics/invalid_mass "two-body masses must be positive"
expect_failure tests/fixtures/physics/invalid_step "two-body step duration must be positive"
expect_failure tests/fixtures/physics/overlap "two-body positions must not overlap"

echo "Orbit numeric test passed: P0 propagates both bodies, bounds invariant drift, and rejects invalid inputs."

