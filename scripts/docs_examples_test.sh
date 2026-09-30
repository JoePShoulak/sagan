#!/usr/bin/env bash
set -euo pipefail

export PATH="/c/msys64/ucrt64/bin:/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
examples_dir="docs/examples/executable"
tmp_root="build/tmp"

cd "$repo_root"

test -d "$examples_dir" || {
  echo "Executable documentation example directory is missing: $examples_dir" >&2
  exit 1
}

mkdir -p "$tmp_root"
work_dir="$(mktemp -d "$tmp_root/docs-examples.XXXXXXXX")"
trap 'rm -rf "$work_dir"' EXIT

make --no-print-directory -B all

mapfile -d '' sources < <(find "$examples_dir" -type f -name '*.sagan' -print0 | sort -z)
mapfile -d '' outputs < <(find "$examples_dir" -type f -name '*.stdout' -print0 | sort -z)

if [[ ${#sources[@]} -eq 0 ]]; then
  echo "At least one executable documentation example is required." >&2
  exit 1
fi

for expected in "${outputs[@]}"; do
  source_file="${expected%.stdout}.sagan"
  if [[ ! -f "$source_file" ]]; then
    echo "Orphaned expected output without a Sagan source: $expected" >&2
    exit 1
  fi
done

for source_file in "${sources[@]}"; do
  relative_source="$source_file"
  expected="${source_file%.sagan}.stdout"
  test -f "$expected" || {
    echo "Missing expected output for $relative_source: $expected" >&2
    exit 1
  }

  stem="${relative_source//\//_}"
  actual="$work_dir/$stem.stdout"
  errors="$work_dir/$stem.stderr"
  expected_normalized="$work_dir/$stem.expected.normalized"
  actual_normalized="$work_dir/$stem.actual.normalized"

  set +e
  bin/sagan "$source_file" >"$actual" 2>"$errors"
  status=$?
  set -e

  if [[ $status -ne 0 ]]; then
    echo "Executable documentation example failed ($status): $relative_source" >&2
    cat "$errors" >&2
    exit 1
  fi

  if [[ -s "$errors" ]]; then
    echo "Executable documentation example wrote to stderr: $relative_source" >&2
    cat "$errors" >&2
    exit 1
  fi

  sed 's/\r$//' "$expected" >"$expected_normalized"
  sed 's/\r$//' "$actual" >"$actual_normalized"

  if ! diff -u "$expected_normalized" "$actual_normalized"; then
    echo "Executable documentation output differs: $relative_source" >&2
    exit 1
  fi

  echo "Executable documentation example passed: $relative_source"
done
