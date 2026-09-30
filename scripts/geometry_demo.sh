#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Geometry source:"
echo "----------------"
cat examples/geometry.sagan

echo
echo "Native result:"
echo "--------------"
bin/sagan examples/geometry.sagan

echo
echo "Geometry demo passed: points and vectors preserve affine meaning, and spherical points and vectors use native literals."
