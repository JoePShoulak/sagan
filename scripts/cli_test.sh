#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
binary="./bin/sagan"
mkdir -p build
work_dir="$(mktemp -d build/cli-test.XXXXXXXXXX)"

cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT

expect_output() {
  local name="$1"
  local expected="$2"
  shift 2
  local output
  output="$("$@" 2>&1)" || {
    echo "[FAIL] $name: command failed" >&2
    echo "$output" >&2
    return 1
  }
  grep -Fq "$expected" <<<"$output" || {
    echo "[FAIL] $name: expected output containing '$expected'" >&2
    echo "$output" >&2
    return 1
  }
  echo "[PASS] $name"
}

expect_failure() {
  local name="$1"
  local expected_status="$2"
  local expected="$3"
  shift 3
  local output
  local status
  set +e
  output="$("$@" 2>&1)"
  status=$?
  set -e
  if [[ "$status" -ne "$expected_status" ]]; then
    echo "[FAIL] $name: expected status $expected_status, got $status" >&2
    echo "$output" >&2
    return 1
  fi
  grep -Fq "$expected" <<<"$output" || {
    echo "[FAIL] $name: expected output containing '$expected'" >&2
    echo "$output" >&2
    return 1
  }
  echo "[PASS] $name"
}

expect_output "default tokenizer input" "tokenizer: examples/tokenizer_demo.sagan" "$binary"
expect_output "explicit tokenizer input" "KWD_LET" "$binary" examples/tokenizer_demo.sagan
expect_output "text AST output" "Program" "$binary" --ast examples/parser_demo.sagan
expect_output "DOT AST output" "digraph SaganAST" "$binary" --ast-dot examples/parser_demo.sagan

expect_output "SVG AST file" "Wrote SVG AST" \
  "$binary" --ast-svg examples/parser_demo.sagan "$work_dir/parser.svg"
grep -Fq "<svg" "$work_dir/parser.svg"

expect_output "HTML AST file" "Wrote visual AST demo" \
  "$binary" --ast-html examples/parser_demo.sagan "$work_dir/parser.html"
grep -Fq "Input source" "$work_dir/parser.html"

expect_failure "invalid arguments" 2 "usage: sagan" "$binary" --ast one.sagan extra.sagan
expect_failure "missing input file" 1 "Could not open" "$binary" "$work_dir/missing.sagan"
expect_failure "lexical diagnostic" 1 "lexical error at" "$binary" examples/tokenizer_error.sagan
expect_failure "syntax diagnostic" 1 "syntax error at" "$binary" --ast examples/parser_error.sagan
expect_failure "unwritable output" 1 "Could not write" \
  "$binary" --ast-svg examples/parser_demo.sagan "$work_dir/missing/output.svg"

echo "All command-line integration tests passed."
