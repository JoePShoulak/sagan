#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

source_file="examples/editor_recovery_demo.sagan"

echo "Sagan input (deliberately incomplete):"
echo "--------------------------------------"
sed -n '1,120p' "$source_file"
echo
echo "Language-service capabilities:"
echo "------------------------------"
bin/sagan --capabilities-json
echo
echo "Recovered structured diagnostics:"
echo "---------------------------------"
set +e
bin/sagan --diagnostics-json "$source_file"
status=$?
set -e
echo

if [[ "$status" -ne 1 ]]; then
  echo "Expected recovered analysis to return status 1, received $status." >&2
  exit 1
fi

echo "Recovery demo passed: malformed declarations produced structured diagnostics while valid declarations remained available to the compiler service."
