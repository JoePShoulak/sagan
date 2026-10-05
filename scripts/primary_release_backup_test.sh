#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/sagan-release-backup-test.XXXXXX")"
fixture="$test_root/asset.bin"
printf 'Sagan release backup fixture\n' > "$fixture"
export SAGAN_BACKUP_TEST_ASSET="$fixture"
export SAGAN_BACKUP_TEST_SIZE="$(wc -c < "$fixture")"
export SAGAN_BACKUP_TEST_DIGEST="$(sha256sum "$fixture" | cut -d' ' -f1)"

gh() {
  local query="" directory="" arg
  case "${1:-} ${2:-}" in
    'api repos/Sagan-Shoulak/sagan/releases?per_page=100')
      shift 2
      while (( $# )); do
        if [[ "$1" == --jq ]]; then
          query="$2"
          shift 2
        else
          shift
        fi
      done
      case "$query" in
        *'@json'*)
          printf '{"tag_name":"v-test","draft":false,"prerelease":false,"published_at":"2026-10-05T00:00:00Z","assets":[{"name":"asset.bin","size":%s,"digest":"sha256:%s"}]}\n' \
            "$SAGAN_BACKUP_TEST_SIZE" "$SAGAN_BACKUP_TEST_DIGEST"
          ;;
        *'.tag_name as $tag'*)
          printf 'v-test\tasset.bin\tsha256:%s\t%s\n' "$SAGAN_BACKUP_TEST_DIGEST" "$SAGAN_BACKUP_TEST_SIZE"
          ;;
        *'.[].tag_name'*)
          printf 'v-test\n'
          ;;
        *'@tsv'*)
          printf 'v-test\t1\t%s\n' "$SAGAN_BACKUP_TEST_SIZE"
          ;;
        *)
          printf 'Unexpected release-list query: %s\n' "$query" >&2
          return 1
          ;;
      esac
      ;;
    'api repos/Sagan-Shoulak/sagan/releases/tags/v-test')
      printf 'asset.bin\tsha256:%s\t%s\n' "$SAGAN_BACKUP_TEST_DIGEST" "$SAGAN_BACKUP_TEST_SIZE"
      ;;
    'release download')
      [[ "${3:-}" == v-test ]] || return 1
      shift 3
      while (( $# )); do
        arg="$1"
        shift
        if [[ "$arg" == --dir ]]; then
          directory="$1"
          shift
        elif [[ "$arg" == --repo ]]; then
          [[ "$1" == Sagan-Shoulak/sagan ]] || return 1
          shift
        else
          printf 'Unexpected download argument: %s\n' "$arg" >&2
          return 1
        fi
      done
      [[ -n "$directory" ]] || return 1
      if [[ "${SAGAN_BACKUP_TEST_TAMPER:-}" == 1 ]]; then
        printf 'wrong contents\n' > "$directory/asset.bin"
      else
        cp "$SAGAN_BACKUP_TEST_ASSET" "$directory/asset.bin"
      fi
      ;;
    *)
      printf 'Unexpected GitHub CLI call: %s\n' "$*" >&2
      return 1
      ;;
  esac
}
export -f gh

mkdir "$test_root/good" "$test_root/tampered" "$test_root/nonempty"
printf 'preserve me\n' > "$test_root/nonempty/existing.txt"

bash "$repo_root/scripts/primary_release_backup.sh" inventory > "$test_root/inventory.log"
bash "$repo_root/scripts/primary_release_backup.sh" download "$test_root/good" > "$test_root/good.log"
(cd "$test_root/good/assets/v-test" && sha256sum -c "$test_root/good/checksums/v-test.sha256")
cmp "$test_root/good/release-inventory.tsv" "$test_root/good/release-inventory-after.tsv"
cmp "$test_root/good/releases.jsonl" "$test_root/good/releases-after.jsonl"

if SAGAN_BACKUP_TEST_TAMPER=1 bash "$repo_root/scripts/primary_release_backup.sh" download "$test_root/tampered" > "$test_root/tampered.log" 2>&1; then
  printf 'Tampered release asset unexpectedly passed checksum verification.\n' >&2
  exit 1
fi
grep -Fq 'checksum or size mismatch' "$test_root/tampered.log"

if bash "$repo_root/scripts/primary_release_backup.sh" download "$test_root/nonempty" > "$test_root/nonempty.log" 2>&1; then
  printf 'Nonempty backup directory was unexpectedly accepted.\n' >&2
  exit 1
fi
grep -Fq 'Backup directory is not empty' "$test_root/nonempty.log"
test "$(cat "$test_root/nonempty/existing.txt")" == 'preserve me'

printf 'Release inventory, download, checksum rejection, and nonempty-target tests passed.\n'
printf 'Test evidence retained at: %s\n' "$test_root"
