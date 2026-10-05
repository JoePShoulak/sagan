#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build/solar-lagrange-demo build/tmp

package_index="$repo_root/libraries/index.tsv"
if [[ "$(bin/sagan --version)" == *"0.0.0+gunknown"* ]]; then
  catalog_root="$repo_root/build/solar-lagrange-demo/source-checkout-catalog"
  package_index="$catalog_root/index.tsv"
  mkdir -p "$catalog_root/render/src" "$catalog_root/physics/src"
  rm -f "$catalog_root/physics/src/restricted_three_body.sagan"
  cp libraries/render/sagan.toml "$catalog_root/render/sagan.toml"
  cp libraries/render/src/window.sagan "$catalog_root/render/src/window.sagan"
  cp libraries/render/src/canvas.sagan "$catalog_root/render/src/canvas.sagan"
  cp libraries/physics/sagan.toml "$catalog_root/physics/sagan.toml"
  cp libraries/physics/src/two_body.sagan "$catalog_root/physics/src/two_body.sagan"
  cp libraries/physics/src/solar_lagrange.sagan \
    "$catalog_root/physics/src/solar_lagrange.sagan"
  awk 'BEGIN { OFS="\t" } NR > 1 { $3="^0.0.0" } { print }' \
    libraries/index.tsv > "$package_index"
fi
export SAGAN_PACKAGE_INDEX="$package_index"

action="${1:-build-and-run}"
native_output="build/solar-lagrange-demo/solar-lagrange-demo.exe"

build_demo() {
  bin/sagan --emit-cpp-package examples/solar_lagrange_demo \
    build/solar-lagrange-demo/program.cpp
  native_tmp="$repo_root/build/tmp"
  if command -v cygpath >/dev/null 2>&1; then
    native_tmp="$(cygpath -w "$native_tmp")"
  fi
  TMPDIR="$native_tmp" TMP="$native_tmp" TEMP="$native_tmp" \
    g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror -Wno-error=switch \
    -include "$repo_root/libraries/render/native/window_bridge.hpp" \
    build/solar-lagrange-demo/program.cpp \
    libraries/render/native/window_bridge.cpp \
    "$repo_root/obj/launcher/sagan-resource.o" \
    -o "$native_output" -static -static-libgcc -static-libstdc++ -lgdi32 -luser32
  echo "Built $native_output"
}

run_demo() {
  if [[ ! -x "$native_output" ]]; then
    echo "Missing $native_output; build it first." >&2
    exit 1
  fi
  "$native_output"
}

case "$action" in
  build) build_demo ;;
  run) run_demo ;;
  build-and-run) build_demo; run_demo ;;
  *) echo "Usage: $0 [build|run|build-and-run]" >&2; exit 2 ;;
esac
