#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build/lagrange-demo build/tmp

package_index="$repo_root/libraries/index.tsv"
if [[ "$(bin/sagan --version)" == *"0.0.0+gunknown"* ]]; then
  catalog_root="$repo_root/build/lagrange-demo/source-checkout-catalog"
  package_index="$catalog_root/index.tsv"
  mkdir -p "$catalog_root/render/src" "$catalog_root/physics/src"
  cp libraries/render/sagan.toml "$catalog_root/render/sagan.toml"
  cp libraries/render/src/window.sagan "$catalog_root/render/src/window.sagan"
  cp libraries/render/src/canvas.sagan "$catalog_root/render/src/canvas.sagan"
  cp libraries/physics/sagan.toml "$catalog_root/physics/sagan.toml"
  cp libraries/physics/src/two_body.sagan "$catalog_root/physics/src/two_body.sagan"
  cp libraries/physics/src/restricted_three_body.sagan \
    "$catalog_root/physics/src/restricted_three_body.sagan"
  awk 'BEGIN { OFS="\t" } NR > 1 { $3="^0.0.0" } { print }' \
    libraries/index.tsv > "$package_index"
fi
export SAGAN_PACKAGE_INDEX="$package_index"

bin/sagan --emit-cpp-package examples/lagrange_demo build/lagrange-demo/program.cpp

native_output="build/lagrange-demo/lagrange-demo-${BASHPID:-$$}.exe"
cleanup_native_output() {
  rm -f "$native_output"
}
trap cleanup_native_output EXIT
native_tmp="$repo_root/build/tmp"
if command -v cygpath >/dev/null 2>&1; then
  native_tmp="$(cygpath -w "$native_tmp")"
fi
TMPDIR="$native_tmp" TMP="$native_tmp" TEMP="$native_tmp" \
  g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror -Wno-error=switch \
  -include "$repo_root/libraries/render/native/window_bridge.hpp" \
  build/lagrange-demo/program.cpp libraries/render/native/window_bridge.cpp \
  -o "$native_output" -static -static-libgcc -static-libstdc++ -lgdi32 -luser32

"$native_output"
