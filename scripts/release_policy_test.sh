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
  "$repo_root/scripts/vscode/test_release.sh" \
  "$repo_root/deploy/releases/mirror.sh"

grep -Fq 'Validate signed release tag' "$workflow"
grep -Fq 'gpg.ssh.allowedSignersFile=.github/allowed_signers verify-tag' "$workflow"
grep -Eq '^[^ ]+ ssh-ed25519 [A-Za-z0-9+/]+={0,2}$' "$repo_root/.github/allowed_signers"
grep -Fq 'verify_installer_artifact.sh' "$workflow"
grep -Fq 'verify_installer_artifact.sh --release' "$workflow"
grep -Fq 'Clean-machine acceptance: PENDING (post-publication follow-up).' "$workflow"
grep -Fq 'Documentation audit: PENDING' "$workflow"
grep -Fq 'Clean-machine evidence: VERIFIED' "$workflow"
grep -Fq 'environment: stable-release' "$workflow"
grep -Fq "'initial-unsigned-release' || 'release-signing'" "$workflow"
grep -Fq "if [[ \"\$TAG\" == v1.0.0 ]]" "$workflow"
grep -Fq 'SAGAN_SIGNTOOL_COMMAND' "$workflow"
grep -Fq 'bash scripts/docs.sh release-check' "$workflow"
grep -Fq '            diffutils' "$workflow"
grep -Fq 'unsigned-initial-1.0-exception' "$repo_root/scripts/release_metadata.sh"
grep -Fq 'unsigned-experimental-0.x-preview-exception' "$repo_root/scripts/release_metadata.sh"
grep -Fq 'SAGAN_RELEASE_TAG' "$workflow"
grep -Fq "github.ref_name != 'v0.88.0-rc.1'" "$workflow"
grep -Fq "github.ref_name != 'v4.9.5'" "$workflow"
grep -Fq "github.ref_name == 'v4.9.5'" "$workflow"
grep -Fq 'unsigned-experimental-4.9.5-exception' "$repo_root/scripts/release_metadata.sh"
grep -Fq 'Clean-machine evidence: PENDING' "$workflow"
grep -Fq 'Clean-machine evidence: DEFERRED by one-time owner exception' "$workflow"
grep -Fq '            mingw-w64-ucrt-x86_64-nodejs' "$workflow"
grep -Fq '            mingw-w64-ucrt-x86_64-gdb' "$workflow"
grep -Fq '            mingw-w64-ucrt-x86_64-gdb' "$repo_root/.github/workflows/windows-installer.yml"
grep -Fq 'authenticode-required-for-public-release' "$repo_root/scripts/release_metadata.sh"
grep -Fq 'bash scripts/coverage_threshold.sh' "$workflow"
grep -Fq 'sagan-${{ github.ref_name }}-release-assets' "$workflow"
grep -Fq "find release-assets -type f -print0" "$workflow"
if grep -Fq 'gh release create "$TAG" release-assets/*' "$workflow"; then
  echo 'Release upload must pass files, not artifact directories.' >&2
  exit 1
fi
grep -Fq 'build/release/*.vsix' "$workflow"
grep -Fq 'scripts/vscode/build_release.sh' "$repo_root/scripts/windows/build_release_assets.sh"
grep -Fq 'scripts/vscode/test_release.sh' "$workflow"
grep -Fq 'install_release_test.js' "$repo_root/scripts/vscode/test_release.sh"
grep -Fq 'sagan-lsp.exe' "$repo_root/scripts/windows/stage_installer.sh"
grep -Fq 'sagan-lsp.exe' "$repo_root/scripts/windows/test_portable.sh"
grep -Fq 'sagan-dap.exe' "$repo_root/scripts/windows/stage_installer.sh"
grep -Fq 'sagan-dap.exe' "$repo_root/scripts/windows/build_portable.sh"
grep -Fq 'SAGAN_DAP_TEST_REQUIRE_GDB=1' "$repo_root/scripts/windows/test_portable.sh"
grep -Fq 'vscode-extension' "$repo_root/scripts/release_metadata.sh"
grep -Fq 'Support is best effort' "$lifecycle"

echo "Release lifecycle policy tests passed."
