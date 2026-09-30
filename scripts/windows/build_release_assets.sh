#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

bash "$repo_root/scripts/windows/stage_installer.sh"
SAGAN_STAGE_READY=true bash "$repo_root/scripts/windows/build_installer.sh"
SAGAN_STAGE_READY=true bash "$repo_root/scripts/windows/build_portable.sh"
bash "$repo_root/scripts/release_metadata.sh"

echo "Built the complete Windows release asset set from one canonical stage."
