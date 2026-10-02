#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

output=$(bin/sagan tests/fixtures/runtime/assert_pass.sagan)
[[ "$output" == *"assertions passed"* ]]

set +e
output=$(bin/sagan tests/fixtures/runtime/assert_fail.sagan 2>&1)
status=$?
set -e
[[ $status -eq 1 && "$output" == *"error[SAG-RUN-0200]"* &&
   "$output" == *"orbit escaped"* ]]

set +e
output=$(bin/sagan --types tests/fixtures/runtime/assert_type_error.sagan 2>&1)
status=$?
set -e
[[ $status -ne 0 && "$output" == *"assert condition requires Bool"* ]]

echo 'Assertion tests passed: success, structured Sagan diagnostic, and type rejection.'
