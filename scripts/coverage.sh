#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/local/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# Git Bash and MSYS2 use different /usr trees. If lcov is installed in MSYS2,
# relaunch this script there before doing the expensive instrumented build.
if ! command -v lcov >/dev/null 2>&1 \
    && [[ -z "${SAGAN_COVERAGE_MSYS2:-}" ]] \
    && [[ -x /c/msys64/usr/bin/bash.exe ]] \
    && [[ -x /c/msys64/usr/local/bin/lcov ]]; then
  exec env SAGAN_COVERAGE_MSYS2=1 \
    /c/msys64/usr/bin/bash.exe "$repo_root/scripts/coverage.sh" "$@"
fi

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
mkdir -p build/tmp/lcov
coverage_tmp="$PWD/build/tmp/lcov"
if command -v cygpath >/dev/null 2>&1; then
  coverage_tmp="$(cygpath -m "$coverage_tmp")"
fi
export TMPDIR="$coverage_tmp"
export TMP="$coverage_tmp"
export TEMP="$coverage_tmp"

if ! command -v lcov >/dev/null 2>&1; then
  if [[ "${1:-}" == "--require-lcov" ]]; then
    echo "lcov is required to produce build/coverage.info." >&2
    exit 1
  fi
  echo "Coverage run completed. Install lcov to create build/coverage.info."
  echo "Instrumented objects were cleaned so ordinary builds remain usable."
  exit 0
fi

capture_compat=()
remove_compat=()
if lcov --version 2>&1 | grep -Eq 'LCOV version ([2-9]|[1-9][0-9])\.'; then
  capture_compat=(--ignore-errors mismatch)
  remove_compat=(--ignore-errors unused)
fi

lcov --capture \
  --directory obj \
  --base-directory "$repo_root" \
  --output-file build/coverage.raw.info \
  "${capture_compat[@]}"

lcov --remove build/coverage.raw.info \
  '/usr/*' \
  '*C:/msys64/*' \
  '*/third_party/*' \
  '*/obj/*' \
  --output-file build/coverage.info \
  "${remove_compat[@]}"

if command -v cygpath >/dev/null 2>&1; then
  repo_windows="$(cygpath -m "$repo_root")"
  sed -i "s#^SF:${repo_root}/${repo_windows}/#SF:${repo_root}/#" build/coverage.info
fi

lcov --summary build/coverage.info
echo "Coverage report: build/coverage.info"
