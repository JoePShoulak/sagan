#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
export SAGAN_PACKAGE_INDEX="$repo_root/libraries/index.tsv"

bin/sagan --run-package examples/solar_lagrange_numeric_demo
