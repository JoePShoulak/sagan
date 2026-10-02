#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"

output="$(bin/sagan --run-package examples/lagrange_numeric_demo)"
for expected in \
  "day L1_m L2_m L3_m L4_m L5_m control_m" \
  "primary_energy_drift" \
  "primary_angular_momentum_drift" \
  "stable_L4_error" \
  "stable_L5_error" \
  "off_point_control_error" \
  "primary_back_reaction_error" \
  "secondary_back_reaction_error"
do
  if [[ "$output" != *"$expected"* ]]; then
    echo "Missing Lagrange output: $expected" >&2
    echo "$output" >&2
    exit 1
  fi
done

echo "Lagrange numeric test passed: P1 preserves the massive pair and distinguishes stable triangular points from the control."

