#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 1 || "$1" != /* || ! -d "$1" ]]; then
  printf 'Usage: bash scripts/verify_primary_local_backup.sh ABSOLUTE_BASH_BACKUP_DIRECTORY\n' >&2
  exit 2
fi

backup_root="$(realpath "$1")"
mirror_root="$backup_root/sagan.git"
for required in sagan.git mirror.refs source-before.refs source-after.refs ASSETS.sha256 release-assets/release-inventory.tsv release-assets/release-inventory-after.tsv release-assets/releases.jsonl release-assets/releases-after.jsonl; do
  [[ -e "$backup_root/$required" ]] || {
    printf 'Backup is missing: %s\n' "$required" >&2
    exit 1
  }
done

cmp "$backup_root/source-before.refs" "$backup_root/source-after.refs"
cmp "$backup_root/source-before.refs" "$backup_root/mirror.refs"
cmp "$backup_root/release-assets/release-inventory.tsv" "$backup_root/release-assets/release-inventory-after.tsv"
cmp "$backup_root/release-assets/releases.jsonl" "$backup_root/release-assets/releases-after.jsonl"
git -c safe.directory="$mirror_root" -C "$mirror_root" fsck --full
git -c safe.directory="$mirror_root" -C "$mirror_root" show-ref | awk '{print $1 " " $2}' | sort | cmp - "$backup_root/mirror.refs"
(cd "$backup_root" && sha256sum --status -c ASSETS.sha256)

restore_root="$(mktemp -d "${TMPDIR:-/tmp}/sagan-local-restore.XXXXXX")"
git -c safe.directory="$mirror_root" clone --no-local "$mirror_root" "$restore_root/restore"
git -C "$restore_root/restore" fsck --full
mirror_head="$(git -c safe.directory="$mirror_root" -C "$mirror_root" rev-parse HEAD)"
restore_head="$(git -C "$restore_root/restore" rev-parse HEAD)"
[[ "$mirror_head" == "$restore_head" && -f "$restore_root/restore/README.md" ]] || {
  printf 'The restored checkout does not match the mirror default branch.\n' >&2
  exit 1
}

printf 'Local mirror, %s release assets, metadata stability, and independent restore passed.\n' "$(wc -l < "$backup_root/ASSETS.sha256")"
printf 'Mirror HEAD: %s\n' "$mirror_head"
printf 'Backup: %s\n' "$backup_root"
printf 'Restored checkout retained at: %s\n' "$restore_root/restore"
