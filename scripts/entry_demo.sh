#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make entry-demo

if bin/sagan --entry examples/entry_missing_error.sagan; then
  echo "Expected the missing-entry demo to fail." >&2
  exit 1
fi

echo "Missing-entry demo failed as expected."

if bin/sagan --types examples/type_uninitialized_error.sagan; then
  echo "Expected the uninitialized-read demo to fail." >&2
  exit 1
fi

echo "Uninitialized-read demo failed as expected."

if bin/sagan --types examples/type_unreachable_error.sagan; then
  echo "Expected the unreachable-statement demo to fail." >&2
  exit 1
fi

echo "Unreachable-statement demo failed as expected."
