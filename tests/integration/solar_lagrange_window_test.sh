#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

capture="build/solar-lagrange-demo/solar-lagrange-window.bmp"
rm -f "$capture"
output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_TEST_SPACE_FRAME=1 \
  SAGAN_RENDER_FRAME_LIMIT=600 \
  SAGAN_RENDER_CAPTURE_FRAME=600 \
  SAGAN_RENDER_CAPTURE_BMP="$capture" \
  bash scripts/solar_lagrange_demo.sh
)"

paused_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_FRAME_LIMIT=2 \
  bash scripts/solar_lagrange_demo.sh
)"
if [[ "$paused_output" != *"final_time_s 0 second"* ||
      "$paused_output" != *"final_elapsed 0 day"* ]]; then
  echo "Expected the solar window to start paused at day zero, got:" >&2
  echo "$paused_output" >&2
  exit 1
fi

if [[ "$output" != *"final_time_s 5.184e+06"* &&
      "$output" != *"final_time_s 5184000"* ]]; then
  echo "Expected a 60-day solar Lagrange snapshot, got:" >&2
  echo "$output" >&2
  exit 1
fi
if [[ "$output" != *"final_L4_error"* || "$output" != *"final_L5_error"* ]]; then
  echo "Missing solar-perturbed L4/L5 results." >&2
  echo "$output" >&2
  exit 1
fi
if [[ "$output" != *"final_elapsed 60 day"* ||
      "$output" != *"final_simulation_rate_days_per_real_second 5"* ||
      "$output" != *"final_L4_error "*" kilometer"* ||
      "$output" == *"meter / meter"* ||
      "$output" == *"second / second"* ]]; then
  echo "Expected elapsed days, selected simulation rate, and kilometer error readouts, got:" >&2
  echo "$output" >&2
  exit 1
fi
test -s "$capture"

echo "$output"
echo "Solar Lagrange window test passed: the window reached the same 60-day snapshot."
