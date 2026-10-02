#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mkdir -p build/two-body-demo build/tmp

package_index="$repo_root/libraries/index.tsv"
if [[ "$(bin/sagan --version)" == *"0.0.0+gunknown"* ]]; then
  catalog_root="$repo_root/build/two-body-demo/source-checkout-catalog"
  package_index="$catalog_root/index.tsv"
  mkdir -p "$catalog_root/render/src" "$catalog_root/render/native" "$catalog_root/physics/src"
  cp libraries/render/sagan.toml "$catalog_root/render/sagan.toml"
  cp libraries/render/src/window.sagan "$catalog_root/render/src/window.sagan"
  cp libraries/render/src/canvas.sagan "$catalog_root/render/src/canvas.sagan"
  cp libraries/render/native/window_bridge.hpp "$catalog_root/render/native/window_bridge.hpp"
  cp libraries/render/native/window_bridge.cpp "$catalog_root/render/native/window_bridge.cpp"
  cp libraries/physics/sagan.toml "$catalog_root/physics/sagan.toml"
  cp libraries/physics/src/two_body.sagan "$catalog_root/physics/src/two_body.sagan"
  awk 'BEGIN { OFS="\t" } NR > 1 { $3="^0.0.0" } { print }' \
    libraries/index.tsv > "$package_index"
fi
export SAGAN_PACKAGE_INDEX="$package_index"
bin/sagan --run-package examples/two_body_demo
