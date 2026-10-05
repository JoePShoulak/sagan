#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv_dir="$repo_root/build/docs-venv"

if [[ -x "$venv_dir/Scripts/python.exe" ]]; then
  venv_python="$venv_dir/Scripts/python.exe"
else
  venv_python="$venv_dir/bin/python"
fi

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
    "$bootstrap_python" -m venv "$venv_dir"
    if [[ -x "$venv_dir/Scripts/python.exe" ]]; then
      venv_python="$venv_dir/Scripts/python.exe"
    else
      venv_python="$venv_dir/bin/python"
    fi
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
    "$venv_python" scripts/maintainer_docs_test.py
    bash scripts/docs_examples_test.sh
    "$venv_python" -m mkdocs build --strict --clean
    "$venv_python" scripts/docs_syntax_test.py build/docs-site
    ;;
  release-check)
    test -x "$venv_python" || {
      echo "Documentation environment is missing. Run: bash scripts/docs.sh setup" >&2
      exit 1
    }
    "$venv_python" scripts/maintainer_docs_test.py
    "$venv_python" scripts/docs_release_check.py
    ;;
  *)
    echo "Usage: bash scripts/docs.sh {setup|serve|check|release-check}" >&2
    exit 2
    ;;
esac
