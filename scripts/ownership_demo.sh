#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root"

bash scripts/weak_demo.sh

echo
echo "Rejected all-strong ownership cycle:"
echo "------------------------------------"
sed -n '1,160p' examples/type_strong_ownership_cycle_error.sagan
echo

set +e
diagnostic=$(bin/sagan --types examples/type_strong_ownership_cycle_error.sagan 2>&1)
status=$?
set -e
printf '%s\n' "$diagnostic"
if [[ "$status" -eq 0 || "$diagnostic" != *"Strong ownership cycle requires an explicit weak field edge"* ]]; then
  echo "Expected the all-strong ownership cycle diagnostic." >&2
  exit 1
fi

echo
echo "Ownership demo passed: weak targets expire safely and all-strong cycles are rejected."
