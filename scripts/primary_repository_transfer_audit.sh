#!/usr/bin/env bash
set -uo pipefail

expected_origin="https://github.com/JoePShoulak/sagan"
legacy_namespace="JoePShoulak/sagan"
status=0

pass() {
  printf 'PASS: %s\n' "$1"
}

block() {
  printf 'BLOCKED: %s\n' "$1"
  status=1
}

for command_name in git gh rg; do
  if command -v "$command_name" >/dev/null 2>&1; then
    pass "$command_name is available"
  else
    block "$command_name is unavailable"
  fi
done

if ! git rev-parse --show-toplevel >/dev/null 2>&1; then
  block "run this audit from a Git checkout"
  exit "$status"
fi

branch="$(git branch --show-current)"
origin="$(git config --get remote.origin.url || true)"

printf 'INFO: branch=%s\n' "$branch"
printf 'INFO: origin=%s\n' "$origin"
printf 'INFO: head=%s\n' "$(git rev-parse HEAD)"

if [[ -z "$(git status --porcelain)" ]]; then
  pass "working tree is clean"
else
  block "working tree is not clean"
fi

if [[ "$branch" == "dev" ]]; then
  pass "current branch is dev"
else
  block "transfer must start from reviewed dev, not $branch"
fi

if [[ "$origin" == "$expected_origin" || "$origin" == "$expected_origin.git" ]]; then
  pass "origin is the expected pre-transfer repository"
else
  block "origin does not match the recorded pre-transfer repository"
fi

local_dev="$(git rev-parse refs/heads/dev 2>/dev/null || true)"
remote_dev="$(git ls-remote origin refs/heads/dev 2>/dev/null | cut -f1 || true)"
printf 'INFO: local_dev=%s\n' "$local_dev"
printf 'INFO: remote_dev=%s\n' "$remote_dev"
if [[ -n "$local_dev" && "$local_dev" == "$remote_dev" ]]; then
  pass "local dev matches origin dev"
else
  block "local dev does not match origin dev or the remote is unavailable"
fi

if gh auth status >/dev/null 2>&1; then
  pass "GitHub CLI authentication is valid"
  source_policy="$(gh api repos/JoePShoulak/sagan --jq '[.visibility, .default_branch] | join(" ")' 2>/dev/null || true)"
  if [[ "$source_policy" == "public dev" ]]; then
    pass "source repository is public with dev as its default branch"
  else
    block "source visibility or default branch differs from the transfer inventory"
  fi

  organization_role="$(gh api user/memberships/orgs/Sagan-Shoulak --jq '[.state, .role] | join(" ")' 2>/dev/null || true)"
  if [[ "$organization_role" == "active admin" ]]; then
    pass "authenticated account is an active organization admin"
  else
    block "authenticated account lacks confirmed organization admin membership"
  fi

  if destination_response="$(gh api repos/Sagan-Shoulak/sagan --jq .full_name 2>&1)"; then
    block "destination repository already exists"
  elif [[ "$destination_response" == *"HTTP 404"* ]]; then
    pass "destination repository returned HTTP 404 to the organization admin"
  else
    block "destination repository availability could not be verified"
  fi
else
  block "GitHub CLI authentication is invalid"
fi

printf 'INFO: legacy namespace references that require post-transfer review:\n'
rg -n --glob '!build/**' --glob '!node_modules/**' "$legacy_namespace" \
  --glob '!primary_repository_transfer_audit.sh' \
  --glob '!repository_segmentation_check.py' \
  README.md MAINTAINERS.md docs packaging deploy scripts || true

printf 'INFO: declared workflow environments and secrets:\n'
rg -n 'environment:|secrets\.[A-Z0-9_]+' .github/workflows || true

printf 'INFO: declared workflow runners:\n'
rg -n -A 3 'runs-on:' .github/workflows || true

if [[ "$status" -eq 0 ]]; then
  printf 'READY FOR ONLINE POLICY AND BACKUP CHECKS; TRANSFER IS NOT AUTHORIZED.\n'
else
  printf 'NOT READY FOR TRANSFER. Resolve every BLOCKED item first.\n'
fi

exit "$status"
