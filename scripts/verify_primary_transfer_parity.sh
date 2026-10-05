#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -lt 1 || "$#" -gt 2 || "$1" != /* || ! -d "$1" ]]; then
  printf 'Usage: bash scripts/verify_primary_transfer_parity.sh ABSOLUTE_BASH_BACKUP_DIRECTORY [REHEARSAL_SOURCE_REPOSITORY]\n' >&2
  exit 2
fi

backup_root="$(realpath "$1")"
source_repo="JoePShoulak/sagan"
target_repo="${2:-Sagan-Shoulak/sagan}"
if [[ "$target_repo" != "Sagan-Shoulak/sagan" && "$target_repo" != "$source_repo" ]]; then
  printf 'Only the planned destination or source rehearsal repository is accepted.\n' >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bash "$repo_root/scripts/verify_primary_local_backup.sh" "$backup_root"

evidence_root="$(mktemp -d "${TMPDIR:-/tmp}/sagan-transfer-parity.XXXXXX")"
git ls-remote --refs "https://github.com/$target_repo.git" |
  awk '{print $1 " " $2}' | sort > "$evidence_root/target.refs"
cmp "$backup_root/mirror.refs" "$evidence_root/target.refs" || {
  printf 'Target Git refs differ from the frozen backup.\n' >&2
  exit 1
}

repository_policy="$(gh api "repos/$target_repo" --jq '[.visibility, .default_branch] | join(" ")')"
[[ "$repository_policy" == "public dev" ]] || {
  printf 'Target visibility or default branch differs from public dev.\n' >&2
  exit 1
}

gh api "repos/$target_repo/releases?per_page=100" --paginate \
  --jq '.[] | {tag_name, draft, prerelease, published_at, assets: [.assets[] | {name, size, digest}]} | @json' \
  > "$evidence_root/target-releases.jsonl"
cmp "$backup_root/release-assets/releases.jsonl" "$evidence_root/target-releases.jsonl" || {
  printf 'Target releases or asset metadata differ from the frozen backup.\n' >&2
  exit 1
}

if [[ "$target_repo" == "Sagan-Shoulak/sagan" ]]; then
  git ls-remote --refs "https://github.com/$source_repo.git" |
    awk '{print $1 " " $2}' | sort > "$evidence_root/legacy-redirect.refs"
  cmp "$backup_root/mirror.refs" "$evidence_root/legacy-redirect.refs" || {
    printf 'The legacy Git URL does not resolve to the transferred refs.\n' >&2
    exit 1
  }
  redirected_identity="$(gh api "repos/$source_repo" --jq .full_name)"
  [[ "$redirected_identity" == "$target_repo" ]] || {
    printf 'The legacy GitHub API URL does not resolve to the transferred repository.\n' >&2
    exit 1
  }
  printf 'Primary Git refs, release metadata, public dev policy, and legacy redirects match the backup.\n'
else
  printf 'Pre-transfer rehearsal passed against the source; destination and redirects were not tested.\n'
fi
printf 'Read-only parity evidence retained at: %s\n' "$evidence_root"
