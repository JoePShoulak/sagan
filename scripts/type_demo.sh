#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make type-demo

if bin/sagan --types examples/type_error.sagan; then
  echo "Expected the type-error demo to fail." >&2
  exit 1
fi

echo "Type-error demo failed as expected."

if bin/sagan --types examples/type_uninferred_error.sagan; then
  echo "Expected the uninferred-variable demo to fail." >&2
  exit 1
fi

echo "Uninferred-variable demo failed as expected."

if bin/sagan --types examples/type_array_error.sagan; then
  echo "Expected the heterogeneous-array demo to fail." >&2
  exit 1
fi

echo "Heterogeneous-array demo failed as expected."

if bin/sagan --types examples/type_dictionary_error.sagan; then
  echo "Expected the heterogeneous-dictionary demo to fail." >&2
  exit 1
fi

echo "Heterogeneous-dictionary demo failed as expected."

if bin/sagan --types examples/type_vector_error.sagan; then
  echo "Expected the invalid-vector demo to fail." >&2
  exit 1
fi

echo "Invalid-vector demo failed as expected."

if bin/sagan --types examples/type_annotation_error.sagan; then
  echo "Expected the invalid-annotation demo to fail." >&2
  exit 1
fi

echo "Invalid-annotation demo failed as expected."

if bin/sagan --types examples/type_return_error.sagan; then
  echo "Expected the missing-return demo to fail." >&2
  exit 1
fi

echo "Missing-return demo failed as expected."
