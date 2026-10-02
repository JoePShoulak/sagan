#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"

echo "Two-body numeric source:"
echo "------------------------"
cat examples/orbit_numeric_demo/src/main.sagan

echo
echo "Checked headless result:"
echo "------------------------"
bin/sagan --run-package examples/orbit_numeric_demo

echo
echo "Orbit numeric demo passed: the fixed-step solver preserved the documented circular-orbit invariants."

