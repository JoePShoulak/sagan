#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make clean

cleanup() {
  make clean >/dev/null
}
trap cleanup EXIT

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
  echo "Coverage run completed. Install lcov to create build/coverage.info."
  echo "Instrumented objects were cleaned so ordinary builds remain usable."
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
