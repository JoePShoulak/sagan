#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 1 || "$1" != /* || -e "$1" || -L "$1" ]]; then
  printf 'Usage: bash scripts/create_primary_local_backup.sh NEW_ABSOLUTE_BASH_DIRECTORY\n' >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
backup_parent="$(realpath "$(dirname "$1")")"
backup_root="$backup_parent/$(basename "$1")"
case "$backup_root/" in
  "$repo_root/"*)
    printf 'The backup must be outside the repository checkout.\n' >&2
    exit 2
    ;;
esac
[[ -d "$backup_parent" && -w "$backup_parent" && ! -e "$backup_root" && ! -L "$backup_root" ]] || {
  printf 'The backup parent must exist and be writable, and the target must not exist.\n' >&2
  exit 2
}

source_repo="https://github.com/JoePShoulak/sagan.git"
mkdir "$backup_root"
printf 'Backup in progress at: %s\n' "$backup_root"
git ls-remote --refs "$source_repo" | awk '{print $1 " " $2}' | sort > "$backup_root/source-before.refs"
git clone --mirror "$source_repo" "$backup_root/sagan.git"
git -C "$backup_root/sagan.git" fsck --full
git -C "$backup_root/sagan.git" show-ref | awk '{print $1 " " $2}' | sort > "$backup_root/mirror.refs"
cmp "$backup_root/source-before.refs" "$backup_root/mirror.refs"

mkdir "$backup_root/release-assets"
bash "$repo_root/scripts/primary_release_backup.sh" download "$backup_root/release-assets"
(cd "$backup_root" && find release-assets/assets -type f -print0 | sort -z | xargs -0 -r sha256sum > ASSETS.sha256)

git ls-remote --refs "$source_repo" | awk '{print $1 " " $2}' | sort > "$backup_root/source-after.refs"
cmp "$backup_root/source-before.refs" "$backup_root/source-after.refs" || {
  printf 'Source refs changed during backup; retain the directory as evidence and repeat at a frozen baseline.\n' >&2
  exit 1
}

bash "$repo_root/scripts/verify_primary_local_backup.sh" "$backup_root"
printf 'Local backup is verified. This is not protection against loss of this machine.\n'
