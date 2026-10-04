#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$repo_root"
mkdir -p build build/tmp
if command -v cygpath >/dev/null 2>&1; then
  repo_tmp_native=$(cygpath -w "$repo_root/build/tmp")
else
  repo_tmp_native="$repo_root/build/tmp"
fi
export TMPDIR="$repo_tmp_native" TMP="$repo_tmp_native" TEMP="$repo_tmp_native"

bin/sagan --emit-cpp tests/fixtures/runtime/class_inheritance.sagan build/class_inheritance.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/class_inheritance.cpp -o build/class_inheritance
output=$(build/class_inheritance)
test "$output" = "class inheritance passed"
echo "$output"
bin/sagan --emit-cpp tests/fixtures/runtime/face_member_promises.sagan build/face_member_promises.cpp
g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror build/face_member_promises.cpp -o build/face_member_promises
build/face_member_promises
echo "Face member promises passed"
bin/sagan --tokens tests/fixtures/syntax/super_keyword.sagan | grep -q KWD_SUPER

for fixture in inheritance_conflict inheritance_cycle inheritance_constructor inheritance_private \
  inheritance_override inheritance_method_conflict inheritance_missing_forward \
  inheritance_wrong_forward inheritance_forward_nonparent inheritance_forward_order \
  inheritance_forward_self inheritance_duplicate_parent inheritance_duplicate_forward \
  inheritance_virtual_nondefault default_argument_order default_argument_type \
  default_argument_capture default_argument_missing default_argument_untyped_lambda \
  super_nonparent super_private super_outside \
  super_field_initializer face_field_missing face_field_type face_field_private \
  face_field_readonly face_field_conflict face_field_external_private \
  face_field_mutability face_field_uninitialized; do
  if bin/sagan --types "tests/fixtures/semantic/$fixture.sagan" >"build/$fixture.log" 2>&1; then
    echo "Expected $fixture to be rejected" >&2
    exit 1
  fi
done
if bin/sagan --ast tests/fixtures/syntax/inheritance_missing_comma.sagan >build/inheritance_missing_comma.log 2>&1; then
  echo "Expected a missing comma between is and has to be rejected" >&2
  exit 1
fi
echo "Invalid inheritance and clause ordering were rejected"
