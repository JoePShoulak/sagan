#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

bin/sagan --semantic tests/fixtures/semantic/scopes.sagan >/dev/null

if bin/sagan --semantic tests/fixtures/semantic/semantic_undefined_error.sagan; then
  echo "Expected the undefined-name example to produce a semantic error." >&2
  exit 1
fi
echo "Undefined-name demo failed as expected."

if bin/sagan --semantic tests/fixtures/semantic/semantic_duplicate_error.sagan; then
  echo "Expected the duplicate-name example to produce a semantic error." >&2
  exit 1
fi
echo "Duplicate-name demo failed as expected."
