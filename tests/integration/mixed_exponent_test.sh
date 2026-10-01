#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

echo "Sagan source:"
sed -n '1,100p' tests/fixtures/runtime/mixed_exponent.sagan
echo
echo "Program output:"
output="$(bin/sagan tests/fixtures/runtime/mixed_exponent.sagan)"
output="${output//$'\r'/}"
printf '%s\n' "$output"
[[ "$output" == *"Binet integer: 55"* ]]
[[ "$output" == *"nonfinite rounding rejected"* ]]
[[ "$output" == *"out-of-range rounding rejected"* ]]
[[ "$output" == *"zero negative power rejected"* ]]
for fixture in round_integer_argument_error round_missing_argument_error implicit_float_return_error; do
  if bin/sagan --types "tests/fixtures/semantic/$fixture.sagan"; then
    echo "Expected $fixture to fail type checking." >&2
    exit 1
  fi
done
echo "Mixed float/integer exponentiation tests passed."
