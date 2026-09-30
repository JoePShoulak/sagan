#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

bin/sagan --ast examples/ast.sagan >/dev/null

if bin/sagan --ast tests/fixtures/syntax/parser_error.sagan; then
  echo "Expected tests/fixtures/syntax/parser_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Parser error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/postfix_error.sagan; then
  echo "Expected tests/fixtures/syntax/postfix_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Postfix-expression error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/string_error.sagan; then
  echo "Expected tests/fixtures/syntax/string_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "String-interpolation error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/collection_error.sagan; then
  echo "Expected tests/fixtures/syntax/collection_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Collection error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/block_error.sagan; then
  echo "Expected tests/fixtures/syntax/block_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Statement-block error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/control_flow_error.sagan; then
  echo "Expected tests/fixtures/syntax/control_flow_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Control-flow error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/match_error.sagan; then
  echo "Expected tests/fixtures/syntax/match_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Match error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/exception_error.sagan; then
  echo "Expected tests/fixtures/syntax/exception_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Exception error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/lambda_error.sagan; then
  echo "Expected tests/fixtures/syntax/lambda_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Lambda error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/module_error.sagan; then
  echo "Expected tests/fixtures/syntax/module_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Module-declaration error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/compound_assignment_error.sagan; then
  echo "Expected tests/fixtures/syntax/compound_assignment_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Compound-assignment error demo failed as expected."

if bin/sagan --ast tests/fixtures/syntax/documentation_error.sagan; then
  echo "Expected tests/fixtures/syntax/documentation_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Documentation-comment error demo failed as expected."
