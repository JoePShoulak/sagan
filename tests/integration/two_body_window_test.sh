#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

capture="$repo_root/build/two-body-demo/two-body-window.bmp"
rm -f "$capture"

fast_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=20 \
  SAGAN_RENDER_FRAME_LIMIT=100 \
  SAGAN_RENDER_CAPTURE_FRAME=50 \
  SAGAN_RENDER_CAPTURE_BMP="$capture" \
  bash scripts/two_body_demo.sh
)"
slow_output="$(
  SAGAN_RENDER_TEST_FRAME_MS=100 \
  SAGAN_RENDER_FRAME_LIMIT=20 \
  bash scripts/two_body_demo.sh
)"

fast_final="$(printf '%s\n' "$fast_output" | grep '^final_')"
slow_final="$(printf '%s\n' "$slow_output" | grep '^final_')"
if [[ "$fast_final" != "$slow_final" ]]; then
  printf 'Fixed-step result changed with render-frame timing.\nFast:\n%s\nSlow:\n%s\n' \
    "$fast_final" "$slow_final" >&2
  exit 1
fi

test -s "$capture"
dimensions="$(od -An -j18 -N8 -t d4 "$capture" | tr -s ' ' | sed 's/^ //')"
if [[ "$dimensions" != "960 -540" ]]; then
  echo "Expected a 960x540 top-down BMP, got '$dimensions'." >&2
  exit 1
fi

printf '%s\n' "$fast_final"
echo "Two-body window test passed: 100 fast frames and 20 slow frames produced the same fixed-step snapshot."
