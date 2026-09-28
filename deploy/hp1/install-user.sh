#!/usr/bin/env bash
set -euo pipefail

archive="${1:-sagan-docs-site.tar.gz}"
release_id="$(date -u +%Y%m%dT%H%M%SZ)"
data_dir="$HOME/.local/share/sagan-docs"
release_dir="$data_dir/releases/$release_id"
lib_dir="$HOME/.local/lib/sagan-docs"
bin_dir="$HOME/.local/bin"
cron_marker="# sagan-docs"

test -f "$archive" || { echo "Missing archive: $archive" >&2; exit 1; }
test -f server.py || { echo "Missing server.py" >&2; exit 1; }
test -f sagan-docs || { echo "Missing sagan-docs manager" >&2; exit 1; }

install -d -m 0755 "$release_dir" "$lib_dir" "$bin_dir"
tar -C "$release_dir" -xzf "$archive"
test -f "$release_dir/index.html" || {
  echo "Archive does not contain index.html at its root." >&2
  exit 1
}

install -m 0755 server.py "$lib_dir/server.py"
install -m 0755 sagan-docs "$bin_dir/sagan-docs"
ln -sfn "$release_dir" "$data_dir/current"

cron_file="$(mktemp)"
trap 'rm -f "$cron_file"' EXIT
crontab -l 2>/dev/null | grep -vF "$cron_marker" >"$cron_file" || true
printf '@reboot %s/.local/bin/sagan-docs start %s\n' "$HOME" "$cron_marker" >>"$cron_file"
crontab "$cron_file"

"$bin_dir/sagan-docs" restart
echo "Installed internal Sagan documentation release $release_id without sudo."
