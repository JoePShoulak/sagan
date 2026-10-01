#!/usr/bin/env bash
set -euo pipefail

for candidate in python python3 py; do
  if command -v "$candidate" >/dev/null 2>&1; then
    "$candidate" tests/lsp_protocol_test.py
    "$candidate" tests/lsp_package_test.py
    exit 0
  fi
done

echo "Python 3 is required for the LSP executable protocol test" >&2
exit 1
