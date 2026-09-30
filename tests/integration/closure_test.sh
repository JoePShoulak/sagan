#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

echo "Sagan escaping-closure source:"
echo "------------------------------"
cat tests/fixtures/runtime/closure.sagan
echo
echo "Program output:"
echo "---------------"
output="$(bin/sagan tests/fixtures/runtime/closure.sagan)"
output="${output//$'\r'/}"
printf '%s\n' "$output"

[[ "$output" == $'41\n42\n42' ]]
echo
echo "Closure test passed: function annotations, higher-order calls, escaping captures, and shared mutation executed natively."
