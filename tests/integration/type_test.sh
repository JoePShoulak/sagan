#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

bin/sagan --types tests/fixtures/semantic/types.sagan >/dev/null

bin/sagan --types tests/fixtures/semantic/render_ui_bridge.sagan >/dev/null
bin/sagan --emit-cpp tests/fixtures/semantic/render_ui_bridge.sagan build/render_ui_bridge.cpp >/dev/null
if ! grep -q "sagan_5f5f72656e6465725f75695f6f70656e" build/render_ui_bridge.cpp; then
  echo "Responsive UI bridge did not retain the deterministic encoded-symbol convention." >&2
  exit 1
fi

if bin/sagan --types tests/fixtures/semantic/render_ui_bridge_argument_error.sagan; then
  echo "Expected the responsive UI bridge argument mismatch to fail." >&2
  exit 1
fi

echo "Responsive UI bridge argument mismatch failed as expected."

if bin/sagan --types tests/fixtures/semantic/render_ui_bridge_return_error.sagan; then
  echo "Expected the responsive UI bridge return mismatch to fail." >&2
  exit 1
fi

echo "Responsive UI bridge return mismatch failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_error.sagan; then
  echo "Expected the type-error demo to fail." >&2
  exit 1
fi

echo "Type-error demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_uninferred_error.sagan; then
  echo "Expected the uninferred-variable demo to fail." >&2
  exit 1
fi

echo "Uninferred-variable demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_array_error.sagan; then
  echo "Expected the heterogeneous-array demo to fail." >&2
  exit 1
fi

echo "Heterogeneous-array demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_dictionary_error.sagan; then
  echo "Expected the heterogeneous-dictionary demo to fail." >&2
  exit 1
fi

echo "Heterogeneous-dictionary demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_vector_error.sagan; then
  echo "Expected the invalid-vector demo to fail." >&2
  exit 1
fi

echo "Invalid-vector demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_annotation_error.sagan; then
  echo "Expected the invalid-annotation demo to fail." >&2
  exit 1
fi

echo "Invalid-annotation demo failed as expected."

if bin/sagan --types tests/fixtures/semantic/type_return_error.sagan; then
  echo "Expected the missing-return demo to fail." >&2
  exit 1
fi

echo "Missing-return demo failed as expected."
