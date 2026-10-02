#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

capture="build/lagrange-demo/lagrange-window.bmp"
rm -f "$capture"
fast_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_FRAME_LIMIT=100 \
  SAGAN_RENDER_CAPTURE_FRAME=100 \
  SAGAN_RENDER_CAPTURE_BMP="$capture" \
  bash scripts/lagrange_demo.sh
)"

native_output="build/lagrange-demo/lagrange-demo-r3.exe"
slow_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=100 \
  SAGAN_RENDER_FRAME_LIMIT=20 \
  "$native_output"
)"

fast_final="$(printf '%s\n' "$fast_output" | grep '^final_')"
slow_final="$(printf '%s\n' "$slow_output" | grep '^final_')"
if [[ "$fast_final" != "$slow_final" ]]; then
  printf 'Restricted-three-body result changed with render timing.\nFast:\n%s\nSlow:\n%s\n' \
    "$fast_final" "$slow_final" >&2
  exit 1
fi
if [[ "$fast_final" != *"final_time_s 5.184e+06"* &&
      "$fast_final" != *"final_time_s 5184000"* ]]; then
  printf 'Expected a 60-day final snapshot, got:\n%s\n' "$fast_final" >&2
  exit 1
fi

test -s "$capture"
dimensions="$(od -An -j18 -N8 -t d4 "$capture" | tr -s ' ' | sed 's/^ //')"
if [[ "$dimensions" != "1180 -720" ]]; then
  echo "Expected a 1180x720 top-down BMP, got '$dimensions'." >&2
  exit 1
fi

printf '%s\n' "$fast_final"
echo "Lagrange window test passed: two display schedules produced the same 60-day eight-body snapshot."
