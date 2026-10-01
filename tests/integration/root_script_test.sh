#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

output=$(bin/sagan tests/fixtures/runtime/root_script.sagan)
[[ "$output" == *42* ]]

output=$(bin/sagan tests/fixtures/runtime/root_order.sagan)
output=${output//$'\r'/}
[[ "$output" == $'before\ninitializer\n42' ]]

output=$(bin/sagan tests/fixtures/modules/root_script/main.sagan)
[[ "$output" == *42* ]]

set +e
output=$(bin/sagan tests/fixtures/runtime/root_exit.sagan)
status=$?
set -e
[[ $status -eq 7 && "$output" == *"cleanup before exit"* ]]

set +e
output=$(bin/sagan tests/fixtures/runtime/root_exit_invalid.sagan 2>&1)
status=$?
set -e
[[ $status -ne 0 && "$output" == *"Exit status must be between 0 and 255"* ]]

set +e
output=$(bin/sagan tests/fixtures/modules/root_script_invalid/main.sagan 2>&1)
status=$?
set -e
[[ $status -ne 0 && "$output" == *"cannot contain executable top-level statements"* ]]

echo 'Root script tests passed: ordered execution, shared bindings, explicit exit, and inert imports.'
