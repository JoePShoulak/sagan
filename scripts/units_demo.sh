#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Sagan units source:"
echo "-------------------"
cat examples/units.sagan

echo
echo "Native result:"
echo "--------------"
bin/sagan examples/units.sagan

echo
echo "Units demo passed: conversions, affine temperatures, vectors and points, constrained callables, custom units, SI prefixes, and angular units executed natively."
