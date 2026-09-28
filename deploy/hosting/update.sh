#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:$PATH"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
frontdoor_repo="${FRONTDOOR_REPO:-$(cd "$repo_root/.." && pwd)/frontdoor}"
frontdoor_patch="$repo_root/deploy/frontdoor/frontdoor-sagan.patch"
mode="${1:-origin}"
temporary_frontdoor=""

cleanup() {
  if [[ -n "$temporary_frontdoor" && -d "$temporary_frontdoor" ]]; then
    rm -rf "$temporary_frontdoor"
  fi
}

trap cleanup EXIT

update_origin() {
  cd "$repo_root"
  bash deploy/docs/manage.sh stage-hp1
  ssh hp1 'cd sagan-docs-staging && bash install-user.sh'
  curl --fail --silent --show-error http://192.168.20.21:8781/healthz
  printf '\n'
}

update_hp1_local() {
  cd "$repo_root"
  SAGAN_DOCS_ALLOW_LOCAL_INSTALL=1 bash deploy/docs/manage.sh install-hp1-local
  curl --fail --silent --show-error http://127.0.0.1:8781/healthz
  printf '\n'
}

prepare_frontdoor_source() {
  test -d "$frontdoor_repo/.git" || {
    echo "Frontdoor repository not found: $frontdoor_repo" >&2
    exit 1
  }

  temporary_frontdoor="$(mktemp -d)"
  git -c safe.directory="$frontdoor_repo" -C "$frontdoor_repo" archive \
    --format=tar HEAD -o "$temporary_frontdoor/frontdoor.tar"
  install -d -m 0755 "$temporary_frontdoor/source"
  tar -C "$temporary_frontdoor/source" -xf "$temporary_frontdoor/frontdoor.tar"
  patch -d "$temporary_frontdoor/source" -p1 <"$frontdoor_patch"
  echo "Prepared a clean Frontdoor deployment overlay without changing $frontdoor_repo."
}

update_public_route() {
  prepare_frontdoor_source
  cd "$temporary_frontdoor/source"
  bash deploy/manage.sh deploy fdr-pair
  bash deploy/manage.sh cert-fix fdr-pair
  curl --fail --silent --show-error https://sagan.shoulak.org/healthz
  printf '\n'
}

case "$mode" in
  origin)
    update_origin
    ;;
  hp1-local)
    update_hp1_local
    ;;
  public-route)
    update_public_route
    ;;
  all)
    update_origin
    update_public_route
    ;;
  *)
    echo "Usage: bash deploy/hosting/update.sh {origin|hp1-local|public-route|all}" >&2
    exit 2
    ;;
esac
