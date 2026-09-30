#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mapfile -t tags < <(
  python3 "$repo_root/deploy/releases/github.py" list | sort -V
)

for tag in "${tags[@]}"; do
  [[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[1-9][0-9]*)?$ ]] || continue
  bash "$repo_root/deploy/releases/mirror.sh" "$tag"
done

python3 "$repo_root/deploy/releases/index.py" \
  --root "${SAGAN_RELEASE_MIRROR_ROOT:-$HOME/.local/share/sagan-releases}"

echo "Synchronized ${#tags[@]} stable Sagan release entries."
