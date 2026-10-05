# Primary repository organization transfer

This runbook prepares the transfer of the intact `JoePShoulak/sagan`
repository to `Sagan-Shoulak/sagan`. It must be completed and verified before
any split repository is created. Do not execute the transfer from this
preparation branch, and do not combine the transfer with code extraction.

## Hard gates

Before scheduling the transfer:

1. Make `gh auth status` pass for an account allowed to transfer the source and
   create or receive repositories in `Sagan-Shoulak`.
2. Confirm that `Sagan-Shoulak/sagan` does not already exist and that no
   repository or fork will prevent GitHub from preserving the old URL redirect.
3. Preserve the source repository's public visibility. Record organization
   owner/team access, `dev`
   and `main` protection, Actions policy, environment reviewers, runner access,
   Pages/custom-domain behavior, and secret owners.
4. Reach a clean reviewed `dev` commit and run the relevant hosting, workflow,
   documentation, release-policy, and integration checks. Do not run the full
   repository suite solely for a transfer; it remains reserved for promotion
   from `dev` to `main` or a release.
   The local `dev` commit must match the remote `dev` commit at the freeze.
5. Create and verify an offline mirror backup and ref inventory. Store its
   location and hashes outside this checkout in the approved recovery system.
6. Freeze releases, documentation deployment, mirror synchronization, branch
   changes, and repository administration for the transfer window.

The inventory in `primary-transfer.toml` names repository settings and files;
it deliberately contains no secret values.
Use `bash scripts/rehearse_primary_backup.sh` to exercise a temporary remote
mirror and independent restore before the transfer window. That rehearsal is
recorded in `rehearsals/primary-backup.md`; it is not the durable transfer
backup described below and does not include GitHub release assets.

## Read-only preflight

Run from the repository root:

```bash
bash scripts/primary_repository_transfer_audit.sh
gh auth status
gh api orgs/Sagan-Shoulak --jq '{login: .login}'
gh api repos/JoePShoulak/sagan --jq '{name: .full_name, visibility, default_branch, archived}'
gh api repos/JoePShoulak/sagan/branches/dev/protection
gh api repos/JoePShoulak/sagan/branches/main/protection
```

Query secret and environment *names and policy only*. Never print secret
values. Verify the four environments and two secret names listed in
`primary-transfer.toml`, plus access for the `sagan-docs-hp1` runner.
The read-only October 5 snapshot is in `audits/github-transfer-2026-10-05.md`;
recheck it immediately before transfer because GitHub settings and `dev` move.

## Backup immediately before transfer

Choose an explicit backup directory outside the normal checkout:

```bash
backup_root=/approved/backup/location/sagan-transfer-YYYYMMDD
mkdir -p "$backup_root"
git clone --mirror https://github.com/JoePShoulak/sagan.git "$backup_root/sagan.git"
git -C "$backup_root/sagan.git" show-ref > "$backup_root/refs.txt"
git -C "$backup_root/sagan.git" fsck --full
(cd "$backup_root" && sha256sum refs.txt > SHA256SUMS)
mkdir "$backup_root/release-assets"
bash scripts/primary_release_backup.sh download "$backup_root/release-assets"
```

Record the backup path, ref-inventory hash, source `dev` commit, source `main`
commit, tags, release inventory, and transfer operator in the transfer record.
Restore a disposable clone from the mirror before treating it as recoverable.
The release-asset backup verifies each downloaded file against the digest and
size reported by GitHub. It currently requires simple tag and asset names; if
GitHub returns a name outside that safe set, stop and review the target path
before downloading. Copy the mirror, release assets, inventories, checksums,
and restore evidence to approved off-machine storage.

## Transfer window

Use GitHub's repository transfer flow while signed in as the authorized owner.
Confirm the destination owner and repository name character by character. The
transfer itself is the only intended remote mutation in this step.

Do not create a new repository at the old location. GitHub's redirect is part
of the migration contract. Do not create any of the six split repositories
during this window.

## Immediate verification

Before changing source files, verify both the new canonical endpoint and old
redirect:

```bash
git ls-remote https://github.com/Sagan-Shoulak/sagan.git
git ls-remote https://github.com/JoePShoulak/sagan.git
gh repo view Sagan-Shoulak/sagan --json nameWithOwner,visibility,defaultBranchRef,url
gh release list --repo Sagan-Shoulak/sagan --limit 100
```

Compare the remote refs and releases with the backup inventory. Then verify
teams, protections, required checks, Actions permissions, environments,
reviewers, secret names, runner access, Pages, webhooks, deploy keys, releases,
and security settings. A redirect working by itself is not success.

Update this checkout only after the new endpoint is verified:

```bash
git remote set-url origin https://github.com/Sagan-Shoulak/sagan.git
git remote -v
git fetch --prune origin
```

On a dedicated post-transfer branch, replace the legacy references listed in
`primary-transfer.toml`. Run focused tests for the changed installer, release,
hosting, and documentation paths; merge to `dev`; rerun those tests on `dev`.
Run the complete suite only when promoting the verified result to `main`.

## Acceptance gate

The transfer is complete only when:

- every backed-up ref and release exists at the destination;
- the old URL redirects and no namespace collision can break it;
- `dev` and `main` policies, teams, environments, approvals, secrets, runners,
  Pages, webhooks, deploy keys, and security settings are verified;
- all eight workflows are visible and the required `dev` checks pass;
- documentation, release signing, release mirroring, installer metadata, and
  download links use or safely resolve to the canonical organization path;
- a clean clone from the new URL passes the owner bootstrap procedure; and
- the rollback drill and evidence record are complete.

Only then may the first split repository be created.

## Recovery boundary

If refs, releases, access, automation, signing, documentation, or runners do
not match, stop releases and extraction work. Preserve the transferred state
and evidence; do not delete, recreate, or force-push either namespace. Use the
verified mirror for read-only recovery and coordinate any reverse transfer
through GitHub with the same inventory and freeze. A reverse transfer is not a
substitute for repairing an unverified backup.
