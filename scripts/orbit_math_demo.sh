#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Orbital math source:"
echo "--------------------"
cat examples/orbit_math.sagan

echo
echo "Checked native result:"
echo "----------------------"
bin/sagan examples/orbit_math.sagan

echo
echo "Orbit math demo passed: units, point/vector meaning, finite checks, and explicit display scale were preserved."
