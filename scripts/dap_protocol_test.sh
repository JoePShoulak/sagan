#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${CXX:-}" ]] && command -v g++ >/dev/null 2>&1; then
  compiler="$(command -v g++)"
  if command -v cygpath >/dev/null 2>&1; then
    compiler="$(cygpath -m "$compiler")"
  fi
  export CXX="$compiler"
fi

for candidate in python python3 py; do
  if command -v "$candidate" >/dev/null 2>&1; then
    "$candidate" tests/dap_protocol_test.py
    exit 0
  fi
done

echo "Python 3 is required for the DAP executable protocol test" >&2
exit 1
