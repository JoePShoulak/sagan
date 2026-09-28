#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:$PATH"

mkdir -p build/tmp
repo_tmp_native="$(cygpath -w "$PWD/build/tmp")"
export TMPDIR="$repo_tmp_native"
export TMP="$repo_tmp_native"
export TEMP="$repo_tmp_native"

bash scripts/version_test.sh
make clean all get-version test
bin/sagan --version
