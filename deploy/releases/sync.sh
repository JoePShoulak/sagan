#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
command -v gh >/dev/null 2>&1 || { echo "GitHub CLI is required." >&2; exit 1; }

mapfile -t tags < <(
  gh release list --repo JoePShoulak/sagan --exclude-drafts \
    --limit 100 --json tagName --jq '.[].tagName' | sort -V
)

for tag in "${tags[@]}"; do
  [[ "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[1-9][0-9]*)?$ ]] || continue
  bash "$repo_root/deploy/releases/mirror.sh" "$tag"
done

python3 "$repo_root/deploy/releases/index.py" \
  --root "${SAGAN_RELEASE_MIRROR_ROOT:-$HOME/.local/share/sagan-releases}"

echo "Synchronized ${#tags[@]} stable Sagan release entries."
