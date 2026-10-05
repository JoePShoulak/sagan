#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

capture="build/shape-text-demo/shape-text-demo.bmp"
rm -f "$capture"
export SAGAN_RENDER_CAPTURE_BMP="$capture"
export SAGAN_RENDER_AUTOCLOSE_MS=150
bash scripts/shape_text_demo.sh

[[ -f "$capture" ]]
[[ "$(head -c 2 "$capture")" == "BM" ]]
[[ "$(wc -c < "$capture")" -gt 2000000 ]]

dimensions="$(od -An -td4 -j18 -N8 "$capture" | tr -s ' ' | sed 's/^ //;s/ $//')"
[[ "$dimensions" == "960 -540" ]]

echo "Shape/text test passed: a 960x540 BMP contains the rendered static presentation."
