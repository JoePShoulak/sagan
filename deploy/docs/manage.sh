#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
archive="$repo_root/build/sagan-docs-site.tar.gz"

cd "$repo_root"

case "${1:-}" in
  build)
    bash scripts/docs.sh check
    ;;
  release-check)
    bash scripts/docs.sh check
    bash scripts/docs.sh release-check
    ;;
  package-experimental)
    bash scripts/docs.sh check
    tar -C build/docs-site -czf "$archive" .
    echo "Created experimental documentation archive $archive"
    ;;
  package-versioned)
    git fetch --quiet origin docs-site
    versioned_dir="$repo_root/build/docs-versioned-site"
    rm -rf "$versioned_dir"
    install -d -m 0755 "$versioned_dir"
    git archive --format=tar origin/docs-site | tar -C "$versioned_dir" -xf -
    test -f "$versioned_dir/index.html" || {
      echo "The docs-site branch does not contain a default index.html." >&2
      exit 1
    }
    tar -C "$versioned_dir" -czf "$archive" .
    echo "Created versioned documentation archive $archive"
    ;;
  stage-hp1)
    bash "$0" package-versioned
    ssh hp1 'install -d -m 0755 sagan-docs-staging'
    scp "$archive" deploy/hp1/server.py deploy/hp1/sagan-docs deploy/hp1/install-user.sh hp1:sagan-docs-staging/
    echo "Staged the versioned site in hp1:~/sagan-docs-staging."
    echo "Install it without sudo: ssh hp1 'cd sagan-docs-staging && bash install-user.sh'"
    ;;
  install-hp1-local)
    test "${SAGAN_DOCS_ALLOW_LOCAL_INSTALL:-}" = "1" || {
      echo "Local HP1 installation requires SAGAN_DOCS_ALLOW_LOCAL_INSTALL=1." >&2
      exit 1
    }
    bash "$0" package-versioned
    staging_dir="$(mktemp -d)"
    trap 'rm -rf "$staging_dir"' EXIT
    cp "$archive" deploy/hp1/server.py deploy/hp1/sagan-docs deploy/hp1/install-user.sh "$staging_dir/"
    cd "$staging_dir"
    bash install-user.sh
    ;;
  *)
    echo "Usage: bash deploy/docs/manage.sh {build|release-check|package-experimental|package-versioned|stage-hp1|install-hp1-local}" >&2
    echo "Released versions remain gated; HP1 installation is user-owned and needs no sudo." >&2
    exit 2
    ;;
esac
