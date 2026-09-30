#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Sagan escaping-closure source:"
echo "------------------------------"
cat examples/closure_demo.sagan
echo
echo "Program output:"
echo "---------------"
output="$(bin/sagan examples/closure_demo.sagan)"
output="${output//$'\r'/}"
printf '%s\n' "$output"

[[ "$output" == $'41\n42\n42' ]]
echo
echo "Closure demo passed: function annotations, higher-order calls, escaping captures, and shared mutation executed natively."
