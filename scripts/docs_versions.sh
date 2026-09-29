#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv_dir="$repo_root/build/docs-venv"
branch="docs-site"

if [[ -x "$venv_dir/Scripts/mike.exe" ]]; then
  mike="$venv_dir/Scripts/mike.exe"
else
  mike="$venv_dir/bin/mike"
fi

test -x "$mike" || {
  echo "Documentation environment is missing. Run: bash scripts/docs.sh setup" >&2
  exit 1
}

export PATH="$venv_dir/bin:$venv_dir/Scripts:$PATH"

cd "$repo_root"

configure_git_identity() {
  git config user.name "Sagan documentation automation"
  git config user.email "sagan-docs@users.noreply.github.com"
}

deploy_experimental() {
  configure_git_identity
  "$mike" deploy --branch "$branch" --push --update-aliases experimental

  git fetch --quiet origin "$branch"
  if ! git show "origin/$branch:versions.json" | grep -q '"latest"'; then
    "$mike" set-default --branch "$branch" --push experimental
  fi
}

deploy_release() {
  local version="${1:-}"
  [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Release documentation version must use MAJOR.MINOR.PATCH." >&2
    exit 2
  }

  export SAGAN_DOCS_VERSION="$version"
  export SAGAN_DOCS_CHANNEL="released"

  git fetch --quiet origin "$branch" 2>/dev/null || true
  if "$mike" list --branch "$branch" "$version" 2>/dev/null | grep -Eq "(^|[[:space:]])$version([[:space:]]|$)"; then
    echo "Documentation release $version already exists and is immutable." >&2
    exit 1
  fi

  bash scripts/docs.sh check
  bash scripts/docs.sh release-check
  configure_git_identity
  "$mike" deploy --branch "$branch" --push --update-aliases "$version" latest
  "$mike" set-default --branch "$branch" --push latest
}

case "${1:-}" in
  deploy-experimental)
    deploy_experimental
    ;;
  deploy-release)
    deploy_release "${2:-}"
    ;;
  *)
    echo "Usage: bash scripts/docs_versions.sh {deploy-experimental|deploy-release VERSION}" >&2
    exit 2
    ;;
esac
