#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"

output="$(bin/sagan --run-package examples/solar_lagrange_numeric_demo)"
for expected in \
  "day L1_error L2_error L3_error L4_error L5_error control_error" \
  "final_time_s 5.184e+06" \
  "sun_earth_moon_barycenter_distance" \
  "earth_moon_distance" \
  "final_L4_error" \
  "final_L5_error"
do
  if [[ "$output" != *"$expected"* ]]; then
    echo "Missing solar Lagrange output: $expected" >&2
    echo "$output" >&2
    exit 1
  fi
done

echo "Solar Lagrange numeric test passed: the Sun, Earth, Moon, and six massless tracers advanced for 60 days."
