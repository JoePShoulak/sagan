#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

BUILD_DIR="build/native-declaration"
SYMBOL="sagan_5f5f746573745f6e61746976655f64697374616e6365"
mkdir -p "$BUILD_DIR"

bin/sagan --emit-cpp-package tests/fixtures/native_declaration/success "$BUILD_DIR/success.cpp"
grep -q "$SYMBOL" "$BUILD_DIR/success.cpp"
g++ -std=c++20 -Itests/fixtures/native_declaration/host \
    -include tests/fixtures/native_declaration/host/native_bridge.hpp \
    "$BUILD_DIR/success.cpp" tests/fixtures/native_declaration/host/native_bridge.cpp \
    -o "$BUILD_DIR/success"
"$BUILD_DIR/success"

for fixture in wrong_unit wrong_type wrong_arity wrong_return undeclared; do
    if bin/sagan --emit-cpp-package "tests/fixtures/native_declaration/$fixture" "$BUILD_DIR/$fixture.cpp"; then
        echo "expected native declaration fixture '$fixture' to fail" >&2
        exit 1
    fi
done

bin/sagan --types tests/fixtures/semantic/render_ui_bridge.sagan
