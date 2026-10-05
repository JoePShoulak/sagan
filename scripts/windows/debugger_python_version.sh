#!/usr/bin/env bash
set -euo pipefail

gdb="${1:?expected GDB executable path}"
objdump="$(dirname "$gdb")/objdump.exe"
if [[ ! -x "$objdump" ]]; then
  objdump="$(command -v objdump.exe || command -v objdump || true)"
fi
if [[ -z "$objdump" || ! -x "$objdump" ]]; then
  echo 'The debugger Python version requires objdump.' >&2
  exit 1
fi

imports="$("$objdump" -p "$gdb")"
if [[ "$imports" =~ libpython([0-9]+)\.([0-9]+)\.dll ]]; then
  printf '%s.%s\n' "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
else
  echo "Could not identify the Python DLL imported by $gdb." >&2
  exit 1
fi
