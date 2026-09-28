#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make clean
make \
  CXXFLAGS="-std=c++23 -Wall -Wextra -Wpedantic -Werror -DUNI_ALGO_STATIC_DATA -Ithird_party/uni-algo/include --coverage -O0 -g" \
  LDFLAGS="--coverage" \
  test

mkdir -p build

if ! command -v lcov >/dev/null 2>&1; then
  if [[ "${1:-}" == "--require-lcov" ]]; then
    echo "lcov is required to produce build/coverage.info." >&2
    exit 1
  fi
  echo "Coverage data generated in obj/. Install lcov to create build/coverage.info."
  exit 0
fi

lcov --capture \
  --directory obj \
  --base-directory "$repo_root" \
  --output-file build/coverage.raw.info \
  --ignore-errors mismatch

lcov --remove build/coverage.raw.info \
  '/usr/*' \
  '*/third_party/*' \
  '*/obj/*' \
  --output-file build/coverage.info \
  --ignore-errors unused

lcov --summary build/coverage.info
echo "Coverage report: build/coverage.info"
