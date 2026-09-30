#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 || ! "$1" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[1-9][0-9]*)?$ ]]; then
  echo "usage: bash deploy/releases/mirror.sh vMAJOR.MINOR.PATCH[-rc.NUMBER]" >&2
  exit 2
fi
tag="$1"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
release_root="${SAGAN_RELEASE_MIRROR_ROOT:-$HOME/.local/share/sagan-releases}"
destination="$release_root/$tag"
mkdir -p "$release_root"
exec 9>"$release_root/.mirror.lock"
flock 9

publish_index() {
  local latest
  local -a stable_tags=()
  mapfile -t stable_tags < <(
    find "$release_root" -mindepth 1 -maxdepth 1 -type d -name 'v*.*.*' -printf '%f\n' \
      | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V
  )
  if [[ ${#stable_tags[@]} -gt 0 ]]; then
    latest="${stable_tags[${#stable_tags[@]} - 1]}"
    ln -sfn "$latest" "$release_root/latest"
  fi
  python3 "$script_dir/index.py" --root "$release_root"
}

if [[ -d "$destination" ]]; then
  echo "The immutable HP1 mirror already contains $tag; leaving it unchanged."
  publish_index
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

python3 "$script_dir/github.py" download "$tag" "$staging"
(
  cd "$staging"
  shopt -s nullglob
  checksums=(./*.sha256)
  if [[ ${#checksums[@]} -gt 0 ]]; then
    sha256sum -c "${checksums[@]}"
  else
    assets=(./*)
    [[ ${#assets[@]} -gt 0 ]] || { echo "GitHub release $tag has no downloadable assets." >&2; exit 1; }
    for asset in "${assets[@]}"; do
      sha256sum "$(basename "$asset")" >"$(basename "$asset").sha256"
    done
    echo "Generated mirror-side SHA-256 files for legacy release $tag."
  fi
)

mv "$staging" "$destination"
trap - EXIT
publish_index
echo "Mirrored and verified $tag at $destination"
