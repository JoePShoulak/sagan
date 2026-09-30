#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

[[ "$(bash scripts/version.sh bump 0.1.14 patch)" == "0.1.15" ]]
[[ "$(bash scripts/version.sh bump 0.1.14 minor)" == "0.2.0" ]]
[[ "$(bash scripts/version.sh bump 0.1.14 major)" == "1.0.0" ]]
[[ "$(bash scripts/version.sh bump 0.1.14 none)" == "0.1.14" ]]
[[ "$(bash scripts/version.sh impact e2bb1d6)" == "minor" ]]
current_numeric="$(bash scripts/version.sh numeric)"
next_minor="$(bash scripts/version.sh next minor)"
[[ "$current_numeric" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
IFS=. read -r current_major current_minor current_patch <<<"$current_numeric"
[[ "$next_minor" == "$current_major.$((current_minor + 1)).0" ]]
bash scripts/version.sh current | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+\+g[0-9a-f]{8}(\.dirty)?$'

echo "Versioning tests passed."
