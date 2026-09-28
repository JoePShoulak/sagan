#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:$PATH"

mkdir -p build/tmp
repo_tmp_native="$(cygpath -w "$PWD/build/tmp")"
export TMPDIR="$repo_tmp_native"
export TMP="$repo_tmp_native"
export TEMP="$repo_tmp_native"

make clean all get-version test
bin/sagan --version
