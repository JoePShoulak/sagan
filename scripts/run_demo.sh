#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Sagan source:"
echo "-------------"
cat examples/run_demo.sagan

echo
echo "sagan examples/run_demo.sagan"
echo "------------------------------"
bin/sagan examples/run_demo.sagan

echo
echo "sagan --run-package examples/package_demo"
echo "-----------------------------------------"
bin/sagan --run-package examples/package_demo

echo
echo "Run demo passed: the Sagan CLI compiled and executed a source file and a manifest-backed package."
