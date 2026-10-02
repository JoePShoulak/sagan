#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

capture="build/lagrange-demo/lagrange-window.bmp"
rm -f "$capture"
fast_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_TEST_SPACE_FRAME=1 \
  SAGAN_RENDER_FRAME_LIMIT=600 \
  SAGAN_RENDER_CAPTURE_FRAME=600 \
  SAGAN_RENDER_CAPTURE_BMP="$capture" \
  bash scripts/lagrange_demo.sh
)"

slow_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=100 \
  SAGAN_RENDER_TEST_SPACE_FRAME=1 \
  SAGAN_RENDER_FRAME_LIMIT=120 \
  bash scripts/lagrange_demo.sh
)"

reset_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_TEST_SPACE_FRAME=1 \
  SAGAN_RENDER_FRAME_LIMIT=600 \
  SAGAN_RENDER_TEST_UP_FRAME=100 \
  SAGAN_RENDER_TEST_RESET_FRAME=300 \
  bash scripts/lagrange_demo.sh
)"

zoom_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_FRAME_LIMIT=1 \
  SAGAN_RENDER_TEST_SCROLL_FRAME=1 \
  bash scripts/lagrange_demo.sh
)"

zoom_out_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_FRAME_LIMIT=1 \
  SAGAN_RENDER_TEST_SCROLL_FRAME=1 \
  SAGAN_RENDER_TEST_SCROLL_DELTA=-20 \
  bash scripts/lagrange_demo.sh
)"

fast_final="$(printf '%s\n' "$fast_output" | grep '^final_')"
slow_final="$(printf '%s\n' "$slow_output" | grep '^final_')"
if [[ "$fast_final" != "$slow_final" ]]; then
  printf 'Restricted-three-body result changed with render timing.\nFast:\n%s\nSlow:\n%s\n' \
    "$fast_final" "$slow_final" >&2
  exit 1
fi
if [[ "$reset_output" != *"final_playback_rate 864000"* ]]; then
  printf 'Expected reset to preserve the selected doubled playback rate, got:\n%s\n' \
    "$reset_output" >&2
  exit 1
fi
if [[ "$reset_output" != *"final_time_s 0"* ]]; then
  printf 'Expected reset to restore a paused initial state, got:\n%s\n' \
    "$reset_output" >&2
  exit 1
fi
if [[ "$zoom_output" != *"final_kilometers_per_pixel 2000"* ||
      "$zoom_output" != *"final_time_s 0"* ]]; then
  printf 'Expected one upward wheel step to zoom from 2500 to 2000 km/pixel while paused, got:\n%s\n' \
    "$zoom_output" >&2
  exit 1
fi
if [[ "$zoom_out_output" != *"final_kilometers_per_pixel 20000"* ]]; then
  printf 'Expected downward wheel input to stop at 20000 km/pixel without a subpixel-circle failure, got:\n%s\n' \
    "$zoom_out_output" >&2
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
echo "Lagrange window test passed: schedules matched at day 60, reset paused at the initial state, and wheel zoom changed the scale."
