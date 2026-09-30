#!/usr/bin/env bash

set -euo pipefail

executable="${1:-bin/sagan.exe}"
host="$(uname -s)"

if [[ "$host" != MINGW* && "$host" != MSYS* ]]; then
  echo "Windows runtime import check skipped on $host."
  exit 0
fi

if [[ -f "${executable}.exe" ]]; then
  executable="${executable}.exe"
fi

if [[ ! -f "$executable" ]]; then
  echo "Runtime import check failed: $executable does not exist." >&2
  exit 1
fi

if ! imports="$(objdump -p "$executable")"; then
  echo "Runtime import check failed: objdump could not inspect $executable." >&2
  exit 1
fi
imports_lower="${imports,,}"
for forbidden in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
  if [[ "$imports_lower" == *"$forbidden"* ]]; then
    echo "Runtime import check failed: $executable imports $forbidden." >&2
    exit 1
  fi
done

echo "Windows runtime import check passed: $executable has no MinGW runtime DLL dependencies."
