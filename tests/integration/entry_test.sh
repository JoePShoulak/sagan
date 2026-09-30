#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

bin/sagan --entry tests/fixtures/semantic/entry.sagan >/dev/null

if bin/sagan --entry tests/fixtures/semantic/entry_missing_error.sagan; then
  echo "Expected the missing-entry demo to fail." >&2
  exit 1
fi

echo "Missing-entry demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_uninitialized_error.sagan; then
  echo "Expected the uninitialized-read demo to fail." >&2
  exit 1
fi

echo "Uninitialized-read demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_unreachable_error.sagan; then
  echo "Expected the unreachable-statement demo to fail." >&2
  exit 1
fi

echo "Unreachable-statement demo failed as expected."
