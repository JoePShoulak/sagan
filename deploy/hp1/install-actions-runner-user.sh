#!/usr/bin/env bash
set -euo pipefail

runner_dir="$HOME/.local/share/actions-runner-sagan"
bin_dir="$HOME/.local/bin"
cron_marker="# sagan-docs-actions-runner"

test -x "$runner_dir/bin/runsvc.sh" || {
  echo "Install the GitHub Actions runner in $runner_dir first." >&2
  exit 1
}
test -f "$runner_dir/.runner" || {
  echo "Register the GitHub Actions runner before installing its user service." >&2
  exit 1
}
test -f sagan-docs-actions-runner || {
  echo "Run this installer from the directory containing sagan-docs-actions-runner." >&2
  exit 1
}

install -d -m 0755 "$bin_dir"
install -m 0755 sagan-docs-actions-runner "$bin_dir/sagan-docs-actions-runner"

cron_file="$(mktemp)"
trap 'rm -f "$cron_file"' EXIT
crontab -l 2>/dev/null | grep -vF "$cron_marker" >"$cron_file" || true
printf '@reboot %s/.local/bin/sagan-docs-actions-runner start %s\n' "$HOME" "$cron_marker" >>"$cron_file"
crontab "$cron_file"

"$bin_dir/sagan-docs-actions-runner" restart
echo "Installed the Sagan documentation runner as a user-owned reboot service."
