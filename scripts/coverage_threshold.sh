#!/usr/bin/env bash
set -euo pipefail

coverage_file="${1:-build/coverage.info}"
minimum="${SAGAN_MIN_LINE_COVERAGE:-90}"

[[ -f "$coverage_file" ]] || { echo "Missing coverage file: $coverage_file" >&2; exit 1; }
[[ "$minimum" =~ ^[0-9]+([.][0-9]+)?$ ]] || { echo "Invalid coverage threshold: $minimum" >&2; exit 2; }

awk -F: -v minimum="$minimum" '
  $1 == "LF" { found += $2 }
  $1 == "LH" { hit += $2 }
  END {
    if (found == 0) {
      print "Coverage report contains no instrumented lines." > "/dev/stderr"
      exit 2
    }
    percent = (100.0 * hit) / found
    printf "Line coverage %.2f%% (%d/%d); required %.2f%%.\n", percent, hit, found, minimum
    if (percent + 0.000001 < minimum) exit 1
  }
' "$coverage_file"
