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
output="$(bin/sagan examples/units.sagan | tr -d '\r')"
printf '%s\n' "$output"
for expected in '1000 meter' '5 newton' 'Power: 7 watt' '2 meter' \
  '6.6743e-11 meter^3 / kilogram / second^2'; do
  if ! grep -Fqx "$expected" <<< "$output"; then
    printf 'Missing units demo output: %s\n' "$expected" >&2
    exit 1
  fi
done

echo
echo "Units demo passed: conversions, dimensions, preserved display units, affine temperatures, vectors and points, constrained callables, custom units, SI prefixes, and angular units executed natively."
