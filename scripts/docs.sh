#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv_python="$repo_root/build/docs-venv/Scripts/python.exe"

cd "$repo_root"

find_python() {
  local candidate
  for candidate in python python3 py; do
    if command -v "$candidate" >/dev/null 2>&1; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  echo "Python 3 is required and was not found on PATH." >&2
  return 1
}

case "${1:-}" in
  setup)
    bootstrap_python="$(find_python)"
    "$bootstrap_python" -m venv build/docs-venv
    "$venv_python" -m pip install --upgrade pip
    "$venv_python" -m pip install -r requirements-docs.txt
    ;;
  serve)
    test -x "$venv_python" || {
      echo "Documentation environment is missing. Run: bash scripts/docs.sh setup" >&2
      exit 1
    }
    "$venv_python" -m mkdocs serve
    ;;
  check)
    test -x "$venv_python" || {
      echo "Documentation environment is missing. Run: bash scripts/docs.sh setup" >&2
      exit 1
    }
    "$venv_python" -m mkdocs build --strict --clean
    ;;
  release-check)
    test -x "$venv_python" || {
      echo "Documentation environment is missing. Run: bash scripts/docs.sh setup" >&2
      exit 1
    }
    "$venv_python" scripts/docs_release_check.py
    ;;
  *)
    echo "Usage: bash scripts/docs.sh {setup|serve|check|release-check}" >&2
    exit 2
    ;;
esac
