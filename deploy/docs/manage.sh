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
  package)
    bash scripts/docs.sh check
    bash scripts/docs.sh release-check
    tar -C build/docs-site -czf "$archive" .
    echo "Created $archive"
    ;;
  package-internal)
    bash scripts/docs.sh check
    tar -C build/docs-site -czf "$archive" .
    echo "Created internal-only archive $archive"
    ;;
  stage-hp1)
    bash "$0" package-internal
    ssh hp1 'install -d -m 0755 sagan-docs-staging'
    scp "$archive" deploy/hp1/server.py deploy/hp1/sagan-docs deploy/hp1/install-user.sh hp1:sagan-docs-staging/
    echo "Staged the internal site in hp1:~/sagan-docs-staging."
    echo "Install it without sudo: ssh hp1 'cd sagan-docs-staging && bash install-user.sh'"
    ;;
  *)
    echo "Usage: bash deploy/docs/manage.sh {build|release-check|package|package-internal|stage-hp1}" >&2
    echo "Public packaging remains guarded; HP1 installation is user-owned and needs no sudo." >&2
    exit 2
    ;;
esac
