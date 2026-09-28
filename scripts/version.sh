#!/usr/bin/env bash
set -euo pipefail

export PATH="/ucrt64/bin:/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
config_path="$repo_root/version.conf"

if [[ ! -f "$config_path" ]]; then
  echo "Missing version configuration: $config_path" >&2
  exit 1
fi

# shellcheck source=../version.conf
source "$config_path"

validate_version() {
  [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Invalid semantic version: $1" >&2
    exit 1
  }
}

bump_version() {
  local version="$1"
  local bump="$2"
  local major minor patch
  validate_version "$version"
  IFS=. read -r major minor patch <<<"$version"
  case "$bump" in
    major) printf '%s.0.0\n' "$((major + 1))" ;;
    minor) printf '%s.%s.0\n' "$major" "$((minor + 1))" ;;
    patch) printf '%s.%s.%s\n' "$major" "$minor" "$((patch + 1))" ;;
    none) printf '%s\n' "$version" ;;
    *) echo "Unknown version impact '$bump'; expected major, minor, patch, or none" >&2; exit 1 ;;
  esac
}

commit_impact() {
  local commit="$1"
  local subject body
  local breaking_pattern='^[A-Za-z][A-Za-z0-9_-]*(\([^)]*\))?!:'
  local feature_pattern='^feat(\([^)]*\))?:'
  local fix_pattern='^fix(\([^)]*\))?:'

  subject="$(git -C "$repo_root" show -s --format=%s "$commit")"
  body="$(git -C "$repo_root" show -s --format=%b "$commit")"
  if [[ "$subject" =~ $breaking_pattern ]] || printf '%s\n' "$body" | grep -qE '^BREAKING CHANGE:'; then
    printf 'major\n'
  elif [[ "$subject" =~ $feature_pattern ]]; then
    printf 'minor\n'
  elif [[ "$subject" =~ $fix_pattern ]]; then
    printf 'patch\n'
  else
    printf 'none\n'
  fi
}

current_numeric_version() {
  local version="$VERSION_BASE"
  local commit impact
  validate_version "$version"
  git -C "$repo_root" cat-file -e "${VERSION_BASE_COMMIT}^{commit}" 2>/dev/null || {
    echo "Version base commit '$VERSION_BASE_COMMIT' is unavailable" >&2
    exit 1
  }
  while IFS= read -r commit; do
    [[ -n "$commit" ]] || continue
    impact="$(commit_impact "$commit")"
    version="$(bump_version "$version" "$impact")"
  done < <(git -C "$repo_root" rev-list --reverse "${VERSION_BASE_COMMIT}..HEAD")
  printf '%s\n' "$version"
}

current_build_version() {
  local version revision dirty=""
  version="$(current_numeric_version)"
  revision="$(git -C "$repo_root" rev-parse --short=8 HEAD 2>/dev/null || printf 'unknown')"
  if [[ -n "$(git -C "$repo_root" status --porcelain --untracked-files=normal 2>/dev/null)" ]]; then
    dirty=".dirty"
  fi
  printf '%s+g%s%s\n' "$version" "$revision" "$dirty"
}

prepare_badge() {
  local impact="$1"
  local next readme badge_pattern
  next="$(bump_version "$(current_numeric_version)" "$impact")"
  readme="$repo_root/README.md"
  badge_pattern='^\[!\[Development version [0-9]+\.[0-9]+\.[0-9]+\]\(https://img\.shields\.io/badge/development-[0-9]+\.[0-9]+\.[0-9]+-2563eb\)\]\(docs/contributing/versioning\.md\)$'
  grep -qE "$badge_pattern" "$readme" || {
    echo "Could not find the Sagan development-version badge in README.md" >&2
    exit 1
  }
  sed -i -E "s|$badge_pattern|[![Development version $next](https://img.shields.io/badge/development-$next-2563eb)](docs/contributing/versioning.md)|" "$readme"
  printf 'Prepared README badge for %s (%s change).\n' "$next" "$impact"
}

case "${1:-current}" in
  current) current_build_version ;;
  numeric) current_numeric_version ;;
  impact)
    [[ $# -eq 2 ]] || { echo "usage: bash scripts/version.sh impact COMMIT" >&2; exit 2; }
    commit_impact "$2"
    ;;
  bump)
    [[ $# -eq 3 ]] || { echo "usage: bash scripts/version.sh bump VERSION {major|minor|patch|none}" >&2; exit 2; }
    bump_version "$2" "$3"
    ;;
  next)
    [[ $# -eq 2 ]] || { echo "usage: bash scripts/version.sh next {major|minor|patch}" >&2; exit 2; }
    bump_version "$(current_numeric_version)" "$2"
    ;;
  prepare)
    [[ $# -eq 2 ]] || { echo "usage: bash scripts/version.sh prepare {major|minor|patch}" >&2; exit 2; }
    prepare_badge "$2"
    ;;
  check-badge)
    expected="$(current_numeric_version)"
    actual="$(sed -nE 's/^\[!\[Development version ([0-9]+\.[0-9]+\.[0-9]+)\].*/\1/p' "$repo_root/README.md" | head -n 1)"
    [[ "$actual" == "$expected" ]] || {
      echo "README badge is $actual, but commit history calculates $expected" >&2
      exit 1
    }
    printf 'README badge matches %s.\n' "$expected"
    ;;
  *)
    echo "usage: bash scripts/version.sh {current|numeric|impact COMMIT|next TYPE|prepare TYPE|check-badge|bump VERSION TYPE}" >&2
    exit 2
    ;;
esac
