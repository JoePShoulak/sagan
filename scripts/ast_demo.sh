#!/usr/bin/env bash
set -euo pipefail

# Git Bash can inherit a reduced Windows PATH in some launch contexts.
export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

make ast-demo

demo_path="$repo_root/build/ast-demo.html"
echo
echo "Input:  examples/ast.sagan"
echo "Output: build/ast-demo.html"

if [[ "${1:-}" == "--no-open" ]]; then
  exit 0
fi

if command -v explorer.exe >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
  explorer.exe "$(cygpath -w "$demo_path")"
else
  echo "Open $demo_path in a browser to view the source and AST tree."
fi
