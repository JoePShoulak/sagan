#!/usr/bin/env bash
set -euo pipefail

for candidate in python python3 py; do
  if command -v "$candidate" >/dev/null 2>&1; then
    "$candidate" tests/lsp_reliability_test.py
    exit 0
  fi
done

echo "Python 3 is required for the LSP reliability test" >&2
exit 1
