#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

python_command=""
for candidate in python python3 py; do
  if command -v "$candidate" >/dev/null 2>&1; then
    python_command="$candidate"
    break
  fi
done

if [[ -z "$python_command" ]]; then
  echo "Python 3 is required to validate repository segmentation contracts." >&2
  exit 1
fi

"$python_command" scripts/repository_segmentation_check.py
