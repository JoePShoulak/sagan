#!/usr/bin/env bash
set -euo pipefail

git_dir="${1:?usage: verify_initial_split_refs.sh BARE_GIT_DIRECTORY}"
[[ -d "$git_dir" ]] || { echo "Missing bare Git directory: $git_dir" >&2; exit 1; }
[[ "$(git --git-dir="$git_dir" rev-parse --is-bare-repository)" == true ]] || {
  echo "Expected a bare Git repository: $git_dir" >&2
  exit 1
}

default_ref="$(git --git-dir="$git_dir" symbolic-ref HEAD)"
[[ "$default_ref" == refs/heads/dev ]] || {
  echo "Expected dev as the initial default branch, found $default_ref" >&2
  exit 1
}

mapfile -t heads < <(git --git-dir="$git_dir" for-each-ref --format='%(refname)' refs/heads | sort)
[[ "${#heads[@]}" == 2 && "${heads[0]}" == refs/heads/dev && "${heads[1]}" == refs/heads/main ]] || {
  printf 'Expected only dev and main heads, found: %s\n' "${heads[*]}" >&2
  exit 1
}

tags="$(git --git-dir="$git_dir" for-each-ref --format='%(refname)' refs/tags)"
[[ -z "$tags" ]] || {
  printf 'Initial split must not publish inherited language tags:\n%s\n' "$tags" >&2
  exit 1
}

git --git-dir="$git_dir" fsck --full --no-reflogs
printf 'Initial split has dev/main heads, dev default, no tags, and valid Git objects.\n'
