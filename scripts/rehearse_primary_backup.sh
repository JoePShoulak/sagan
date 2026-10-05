#!/usr/bin/env bash
set -euo pipefail

source_repo="${1:-https://github.com/Sagan-Shoulak/sagan.git}"
rehearsal_root="${2:-$(mktemp -d "${TMPDIR:-/tmp}/sagan-primary-backup.XXXXXX")}"
if [[ "$rehearsal_root" != /* || ! -d "$rehearsal_root" ||
      -n "$(find "$rehearsal_root" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
  printf 'Provide an existing empty absolute Bash path for the rehearsal directory.\n' >&2
  exit 2
fi
rehearsal_root="$(realpath "$rehearsal_root")"
mirror_repo="$rehearsal_root/sagan.git"
restore_repo="$rehearsal_root/restore"
git ls-remote --refs "$source_repo" | awk '{print $1 " " $2}' | sort > "$rehearsal_root/source-before.refs"
git clone --mirror "$source_repo" "$mirror_repo"
git -C "$mirror_repo" show-ref | awk '{print $1 " " $2}' | sort > "$rehearsal_root/mirror.refs"
git ls-remote --refs "$source_repo" | awk '{print $1 " " $2}' | sort > "$rehearsal_root/source-after.refs"

cmp "$rehearsal_root/source-before.refs" "$rehearsal_root/source-after.refs" || {
  printf 'Source refs changed during the mirror clone; repeat at a frozen baseline.\n' >&2
  exit 1
}
cmp "$rehearsal_root/source-after.refs" "$rehearsal_root/mirror.refs" || {
  printf 'Mirror refs differ from source refs. Do not use this backup.\n' >&2
  exit 1
}

git -C "$mirror_repo" fsck --full
git clone --no-local "$mirror_repo" "$restore_repo"
git -C "$restore_repo" fsck --full

mirror_head="$(git -C "$mirror_repo" rev-parse HEAD)"
restore_head="$(git -C "$restore_repo" rev-parse HEAD)"
[[ "$mirror_head" == "$restore_head" ]] || {
  printf 'Restored HEAD differs from mirror HEAD.\n' >&2
  exit 1
}
[[ -f "$restore_repo/README.md" ]] || {
  printf 'Restored checkout is missing README.md.\n' >&2
  exit 1
}

printf 'Mirror refs and independent restore passed.\n'
printf 'Source: %s\n' "$source_repo"
printf 'HEAD: %s\n' "$mirror_head"
printf 'Refs: %s\n' "$(wc -l < "$rehearsal_root/mirror.refs")"
sha256sum "$rehearsal_root/source-after.refs" "$rehearsal_root/mirror.refs"
printf 'Rehearsal retained at: %s\n' "$rehearsal_root"
