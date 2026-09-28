#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make parser-demo

if bin/sagan --ast examples/parser_error.sagan; then
  echo "Expected examples/parser_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Parser error demo failed as expected."
