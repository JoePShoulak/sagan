#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make parser-demo

if bin/sagan --ast examples/parser_error.sagan; then
  echo "Expected examples/parser_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Parser error demo failed as expected."

if bin/sagan --ast examples/postfix_error.sagan; then
  echo "Expected examples/postfix_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Postfix-expression error demo failed as expected."

if bin/sagan --ast examples/string_error.sagan; then
  echo "Expected examples/string_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "String-interpolation error demo failed as expected."

if bin/sagan --ast examples/collection_error.sagan; then
  echo "Expected examples/collection_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Collection error demo failed as expected."

if bin/sagan --ast examples/block_error.sagan; then
  echo "Expected examples/block_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Statement-block error demo failed as expected."

if bin/sagan --ast examples/control_flow_error.sagan; then
  echo "Expected examples/control_flow_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Control-flow error demo failed as expected."

if bin/sagan --ast examples/match_error.sagan; then
  echo "Expected examples/match_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Match error demo failed as expected."

if bin/sagan --ast examples/exception_error.sagan; then
  echo "Expected examples/exception_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Exception error demo failed as expected."

if bin/sagan --ast examples/type_error.sagan; then
  echo "Expected examples/type_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Type-declaration error demo failed as expected."

if bin/sagan --ast examples/lambda_error.sagan; then
  echo "Expected examples/lambda_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Lambda error demo failed as expected."

if bin/sagan --ast examples/module_error.sagan; then
  echo "Expected examples/module_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Module-declaration error demo failed as expected."

if bin/sagan --ast examples/compound_assignment_error.sagan; then
  echo "Expected examples/compound_assignment_error.sagan to produce a syntax error." >&2
  exit 1
fi

echo "Compound-assignment error demo failed as expected."
