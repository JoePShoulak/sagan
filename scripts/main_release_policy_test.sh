#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
main_workflow="$repo_root/.github/workflows/release-on-main.yml"
release_workflow="$repo_root/.github/workflows/release.yml"

grep -Fq '      - main' "$main_workflow"
grep -Fq 'bash scripts/version.sh check-badge' "$main_workflow"
grep -Fq '[[ "$version" != 0.* ]]' "$main_workflow"
grep -Fq 'git show-ref --verify --quiet "refs/tags/$tag"' "$main_workflow"
grep -Fq 'secrets.SAGAN_RELEASE_TAG_SSH_PRIVATE_KEY' "$main_workflow"
grep -Fq 'sagan-ci-signing-smoke-${GITHUB_RUN_ID}' "$main_workflow"
grep -Fq 'git tag -d "$smoke_tag"' "$main_workflow"
grep -Fq '.github/allowed_signers' "$main_workflow"
grep -Fq 'git push origin "refs/tags/$RELEASE_TAG"' "$main_workflow"
grep -Fq 'gh workflow run release.yml --ref "$RELEASE_TAG"' "$main_workflow"
grep -Fq '  workflow_dispatch:' "$release_workflow"
grep -Fq 'environment: stable-release' "$release_workflow"

echo 'Main-driven release policy tests passed.'
