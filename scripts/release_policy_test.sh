#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workflow="$repo_root/.github/workflows/release.yml"
lifecycle="$repo_root/docs/contributing/release-lifecycle.md"

bash -n \
  "$repo_root/scripts/release_metadata.sh" \
  "$repo_root/scripts/coverage_threshold.sh" \
  "$repo_root/scripts/windows/build_portable.sh" \
  "$repo_root/scripts/windows/build_release_assets.sh" \
  "$repo_root/scripts/windows/test_portable.sh" \
  "$repo_root/deploy/releases/mirror.sh"

grep -Fq 'Validate signed release tag' "$workflow"
grep -Fq 'gpg.ssh.allowedSignersFile=.github/allowed_signers verify-tag' "$workflow"
grep -Eq '^[^ ]+ ssh-ed25519 [A-Za-z0-9+/]+={0,2}$' "$repo_root/.github/allowed_signers"
grep -Fq 'verify_installer_artifact.sh --release' "$workflow"
grep -Fq 'Clean-machine evidence: PENDING' "$workflow"
grep -Fq 'environment: stable-release' "$workflow"
grep -Fq 'environment: release-signing' "$workflow"
grep -Fq 'SAGAN_SIGNTOOL_COMMAND' "$workflow"
grep -Fq 'bash scripts/coverage_threshold.sh' "$workflow"
grep -Fq 'sagan-${{ github.ref_name }}-release-assets' "$workflow"
grep -Fq 'Support is best effort' "$lifecycle"

echo "Release lifecycle policy tests passed."
