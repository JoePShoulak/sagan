#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

export SAGAN_RENDER_AUTOCLOSE_MS=100
bash scripts/window_demo.sh

echo "Window bridge test passed: the versioned Sagan package opened, cleared, polled, and closed one native window."
