#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export SAGAN_PACKAGE_INDEX="$repo_root/tests/fixtures/catalog/current-compiler-index.tsv"
actual="$(bin/sagan --run-package tests/fixtures/catalog/consumer-alias)"
actual="${actual//$'\r'/}"
expected=$'42\n7\n42'
if [[ "$actual" != "$expected" ]]; then
  echo "Aliased package and local-precedence output differed:" >&2
  printf '%s\n' "$actual" >&2
  exit 1
fi

transitive="$(bin/sagan --run-package tests/fixtures/catalog/consumer-transitive)"
transitive="${transitive//$'\r'/}"
if [[ "$transitive" != "42" ]]; then
  echo "Transitive package output differed: $transitive" >&2
  exit 1
fi

echo "Package dependency test passed: locked installed source, qualified aliases, local precedence, and transitive imports."
