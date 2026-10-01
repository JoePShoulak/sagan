#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

echo "Sagan source:"
sed -n '1,100p' tests/fixtures/runtime/array_times_parallel.sagan
echo
echo "Program output:"
output="$(bin/sagan tests/fixtures/runtime/array_times_parallel.sagan)"
output="${output//$'\r'/}"
printf '%s\n' "$output"
[[ "$output" == $'index 0: 0\nindex 1: 1\nindex 2: 1\nindex 3: 2\nindex 4: 3\nnegative count rejected\nfailed reassignment kept 5, 8' ]]
ast="$(bin/sagan --ast tests/fixtures/runtime/array_times_parallel.sagan)"
[[ "$ast" == *ParallelAssignment* ]]

for fixture in parallel_count_error parallel_duplicate_error parallel_target_error; do
  if bin/sagan --types "tests/fixtures/semantic/$fixture.sagan"; then
    echo "Expected $fixture to fail." >&2
    exit 1
  fi
done

echo "Array times and simultaneous reassignment tests passed."
