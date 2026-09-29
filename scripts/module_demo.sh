#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Module sources:"
echo "---------------"
for source in examples/module_demo/*.sagan; do
  echo
  echo "$source"
  cat "$source"
done

echo
echo "Resolved graph:"
echo "---------------"
bin/sagan --modules examples/module_demo/main.sagan

expect_failure() {
  local source="$1"
  local expected="$2"
  local output
  set +e
  output="$(bin/sagan --modules "$source" 2>&1)"
  local status=$?
  set -e
  if [[ "$status" -eq 0 || "$output" != *"$expected"* ]]; then
    echo "Expected module failure containing '$expected' from $source" >&2
    echo "$output" >&2
    exit 1
  fi
  echo "Confirmed module error: $expected"
}

expect_failure examples/module_cycle/alpha.sagan "Cyclic module dependency: alpha -> beta -> alpha"
expect_failure examples/module_export_error/main.sagan "does not export 'missing'"
expect_failure examples/module_name_error/main.sagan "declares 'wrong_name', expected 'main'"
expect_failure examples/module_missing/main.sagan "Could not open module"
expect_failure examples/module_declaration_error/main.sagan "must declare 'module support'"
expect_failure examples/module_undefined_export_error/main.sagan "exports undefined declaration 'missing'"
expect_failure examples/module_duplicate_export_error/main.sagan "exports duplicate public name 'value'"

echo "Module demo passed: sibling resolution, transitive dependencies, exports, aliases, and cycle diagnostics are working."
