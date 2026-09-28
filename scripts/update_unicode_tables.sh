#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
unicode_version="17.0.0"
data_dir="$repo_root/build/unicode-data/$unicode_version"
generator="$repo_root/scripts/generate_unicode_tables.py"
output="$repo_root/src/parser/unicode_tables.hpp"

find_python() {
  local candidate
  for candidate in "$repo_root/build/docs-venv/Scripts/python.exe" python python3 py; do
    if command -v "$candidate" >/dev/null 2>&1 || test -x "$candidate"; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  echo "Python 3 is required to regenerate Unicode tables." >&2
  return 1
}

mkdir -p "$data_dir"
curl -fL "https://www.unicode.org/Public/$unicode_version/ucd/DerivedCoreProperties.txt" \
  -o "$data_dir/DerivedCoreProperties.txt"
curl -fL "https://www.unicode.org/Public/$unicode_version/ucd/emoji/emoji-data.txt" \
  -o "$data_dir/emoji-data.txt"

python_cmd="$(find_python)"
"$python_cmd" "$generator" \
  "$data_dir/DerivedCoreProperties.txt" \
  "$data_dir/emoji-data.txt" \
  "$output"

echo "Updated $output from Unicode $unicode_version."
