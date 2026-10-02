#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

fixture=tests/fixtures/runtime/parallel_let_fibonacci.sagan
output="$(bin/sagan "$fixture")"
output="${output//$'\r'/}"
[[ "$output" == $'144\n32\n7' ]]

tree="$(bin/sagan --ast "$fixture")"
[[ "$tree" == *ParallelLet* && "$tree" == *ParallelAssignment* && "$tree" == *Postfix* ]]
types="$(bin/sagan --types "$fixture")"
[[ "$types" == *"i: Int64"* && "$types" == *"a: Int8"* ]]

for fixture in parallel_let_missing_value parallel_let_extra_value; do
  if bin/sagan --ast "tests/fixtures/syntax/$fixture.sagan"; then
    echo "Expected $fixture to fail syntax checking." >&2
    exit 1
  fi
done
for fixture in parallel_let_duplicate_error parallel_let_unbound_error; do
  if bin/sagan --types "tests/fixtures/semantic/$fixture.sagan"; then
    echo "Expected $fixture to fail semantic checking." >&2
    exit 1
  fi
done

echo "Parallel let and exact Fibonacci syntax tests passed."
