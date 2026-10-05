#!/usr/bin/env bash
set -euo pipefail

source_repo="Sagan-Shoulak/sagan"
releases_api="repos/$source_repo/releases?per_page=100"
mode="${1:-inventory}"

inventory() {
  printf 'TAG\tASSETS\tBYTES\n'
  gh api "$releases_api" --paginate \
    --jq '.[] | [.tag_name, (.assets | length), ([.assets[].size] | add // 0)] | @tsv'
}

validate_asset_metadata() {
  local asset_inventory tag name digest size
  asset_inventory="$(gh api "$releases_api" --paginate \
    --jq '.[] | .tag_name as $tag | .assets[] | [$tag, .name, .digest, (.size | tostring)] | @tsv')"
  while IFS=$'\t' read -r tag name digest size; do
    [[ -z "$tag" ]] && continue
    [[ "$tag" =~ ^[A-Za-z0-9._-]+$ && "$tag" != . && "$tag" != .. &&
       "$name" =~ ^[A-Za-z0-9._-]+$ && "$name" != . && "$name" != .. &&
       "$digest" =~ ^sha256:[a-f0-9]{64}$ && "$size" =~ ^[0-9]+$ ]] || {
      printf 'Unsafe or incomplete GitHub release asset metadata.\n' >&2
      exit 1
    }
  done <<< "$asset_inventory"
}

if [[ "$mode" == "inventory" && "$#" -eq 1 ]]; then
  inventory
  validate_asset_metadata
  printf 'Asset names, digests, and sizes validated.\n'
  exit 0
fi

if [[ "$mode" != "download" || "$#" -ne 2 ]]; then
  printf 'Usage: bash scripts/primary_release_backup.sh inventory\n' >&2
  printf '   or: bash scripts/primary_release_backup.sh download ABSOLUTE_EMPTY_DIRECTORY\n' >&2
  exit 2
fi

backup_root="$2"
if [[ "$backup_root" != /* || ! -d "$backup_root" || "$backup_root" == / ]]; then
  printf 'Provide an existing empty absolute Bash path for the backup directory.\n' >&2
  exit 2
fi
if [[ -n "$(find "$backup_root" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
  printf 'Backup directory is not empty: %s\n' "$backup_root" >&2
  exit 2
fi
backup_root="$(realpath "$backup_root")"
validate_asset_metadata

mkdir -p "$backup_root/assets" "$backup_root/checksums"
inventory > "$backup_root/release-inventory.tsv"
gh api "$releases_api" --paginate \
  --jq '.[] | {tag_name, draft, prerelease, published_at, assets: [.assets[] | {name, size, digest}]} | @json' \
  > "$backup_root/releases.jsonl"

tags="$(gh api "$releases_api" --paginate --jq '.[].tag_name')"
if [[ -z "$tags" ]]; then
  printf 'No releases were returned; verify the source repository before transfer.\n' >&2
  exit 1
fi

while IFS= read -r tag; do
  [[ "$tag" =~ ^[A-Za-z0-9._-]+$ && "$tag" != . && "$tag" != .. ]] || {
    printf 'Unsafe release tag for backup path: %s\n' "$tag" >&2
    exit 1
  }
  asset_specs="$(gh api "repos/$source_repo/releases/tags/$tag" \
    --jq '.assets[] | [.name, .digest, (.size | tostring)] | @tsv')"
  asset_dir="$backup_root/assets/$tag"
  mkdir "$asset_dir"
  if [[ -z "$asset_specs" ]]; then
    continue
  fi

  while IFS=$'\t' read -r name digest size; do
    [[ "$name" =~ ^[A-Za-z0-9._-]+$ && "$name" != . && "$name" != .. &&
       "$digest" =~ ^sha256:[a-f0-9]{64}$ && "$size" =~ ^[0-9]+$ ]] || {
      printf 'Invalid asset name, digest, or size for release %s.\n' "$tag" >&2
      exit 1
    }
  done <<< "$asset_specs"

  gh release download "$tag" --repo "$source_repo" --dir "$asset_dir"
  latest_specs="$(gh api "repos/$source_repo/releases/tags/$tag" \
    --jq '.assets[] | [.name, .digest, (.size | tostring)] | @tsv')"
  [[ "$latest_specs" == "$asset_specs" ]] || {
    printf 'Release %s changed during backup; repeat at a frozen baseline.\n' "$tag" >&2
    exit 1
  }
  expected_count="$(printf '%s\n' "$asset_specs" | wc -l)"
  actual_count="$(find "$asset_dir" -maxdepth 1 -type f | wc -l)"
  [[ "$actual_count" -eq "$expected_count" ]] || {
    printf 'Release %s asset count changed during backup.\n' "$tag" >&2
    exit 1
  }

  while IFS=$'\t' read -r name digest size; do
    asset_path="$asset_dir/$name"
    [[ -f "$asset_path" ]] || {
      printf 'Missing downloaded release asset: %s\n' "$asset_path" >&2
      exit 1
    }
    actual_digest="$(sha256sum "$asset_path")"
    actual_digest="${actual_digest%% *}"
    actual_size="$(wc -c < "$asset_path")"
    [[ "$actual_digest" == "${digest#sha256:}" && "$actual_size" -eq "$size" ]] || {
      printf 'Release asset checksum or size mismatch: %s\n' "$asset_path" >&2
      exit 1
    }
  done <<< "$asset_specs"

  (cd "$asset_dir" && sha256sum ./* > "$backup_root/checksums/$tag.sha256")
  printf 'Verified release %s\n' "$tag"
done <<< "$tags"

inventory > "$backup_root/release-inventory-after.tsv"
gh api "$releases_api" --paginate \
  --jq '.[] | {tag_name, draft, prerelease, published_at, assets: [.assets[] | {name, size, digest}]} | @json' \
  > "$backup_root/releases-after.jsonl"
cmp "$backup_root/release-inventory.tsv" "$backup_root/release-inventory-after.tsv" &&
  cmp "$backup_root/releases.jsonl" "$backup_root/releases-after.jsonl" || {
  printf 'The release listing changed during backup; repeat at a frozen baseline.\n' >&2
  exit 1
}

printf 'Release assets and publisher digests verified in: %s\n' "$backup_root"
