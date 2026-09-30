#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || ! "$1" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "usage: bash deploy/releases/mirror.sh vMAJOR.MINOR.PATCH" >&2
  exit 2
fi
command -v gh >/dev/null 2>&1 || { echo "GitHub CLI is required." >&2; exit 1; }

tag="$1"
release_root="${SAGAN_RELEASE_MIRROR_ROOT:-$HOME/.local/share/sagan-releases}"
destination="$release_root/$tag"
mkdir -p "$release_root"

if [[ -d "$destination" ]]; then
  echo "The immutable HP1 mirror already contains $tag; leaving it unchanged."
  exit 0
fi

staging="$(mktemp -d "$release_root/.staging.$tag.XXXXXXXX")"
cleanup() {
  case "$staging" in
    "$release_root"/.staging.*) rm -rf "$staging" ;;
    *) echo "Refusing to clean unexpected mirror staging path: $staging" >&2 ;;
  esac
}
trap cleanup EXIT

gh release download "$tag" --repo JoePShoulak/sagan --dir "$staging"
(cd "$staging" && sha256sum -c ./*.sha256)

mv "$staging" "$destination"
trap - EXIT
ln -sfn "$tag" "$release_root/latest"
echo "Mirrored and verified $tag at $destination"
