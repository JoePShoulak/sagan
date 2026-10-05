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
3. Preserve the source repository's public visibility and observed policy
   through transfer; tighten access and branch protection only after verifying
   parity in the organization. Record owner/team access, `dev` and `main`
   protection, Actions policy, environment reviewers, runner access,
   Pages/custom-domain behavior, and configured secret names.
4. Reach a clean reviewed `dev` commit and run the relevant hosting, workflow,
   documentation, release-policy, and integration checks. Do not run the full
   repository suite solely for a transfer; it remains reserved for promotion
   from `dev` to `main` or a release.
   The local `dev` commit must match the remote `dev` commit at the freeze.
5. Create and verify a local mirror, ref inventory, release-asset backup, and
   independent restore outside this checkout. The owner chose a backup on this
   machine only. This protects against a transfer mistake but **not** against
   loss of this computer or its storage; do not call it an off-machine backup.
6. Freeze all releases, documentation deployment, mirror synchronization,
   branch changes, and repository administration for the transfer window.
   Signing is abandoned for now, and release publication remains paused until
   the owner makes a separate publication-policy decision. The hold is
   procedural: release workflows can still be triggered by a `main` push or
   manual dispatch. Do not do either, and wait for any in-flight workflow to
   finish before transferring.

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
values. Verify the four environments, the configured repository secret name,
the explicitly deferred signing-secret name, and access for the
`sagan-docs-hp1` runner. Do not procure a signing credential for this transfer.
The read-only October 5 snapshot is in `audits/github-transfer-2026-10-05.md`;
recheck it immediately before transfer because GitHub settings and `dev` move.

## Backup immediately before transfer

Choose a **new** directory outside the normal checkout on this machine. The
October 5 rehearsal at
`/c/Users/joeps/coding/sagan-pretransfer-script-rehearsal-2026-10-05` verified 21
refs, five releases, 33 assets, their publisher digests, and an independent
restore. It is a rehearsal from the pre-merge remote `dev`, not the final
transfer-freeze backup. Refresh after `dev` is merged, pushed, and frozen:

```bash
backup_root=/c/Users/joeps/coding/sagan-transfer-freeze-YYYYMMDD
bash scripts/create_primary_local_backup.sh "$backup_root"
bash scripts/verify_primary_local_backup.sh "$backup_root"
```

Record the backup path, ref-inventory hash, source `dev` commit, source `main`
commit, tags, release inventory, and transfer operator in the transfer record.
Restore a disposable clone from the mirror before treating it as recoverable.
The release-asset backup verifies each downloaded file against the digest and
size reported by GitHub. It currently requires simple tag and asset names; if
GitHub returns a name outside that safe set, stop and review the target path
before downloading. Keep the local backup directory, its inventories,
checksums, and restore evidence intact. Do not place the only backup in
`build/`, a temporary directory, or the checkout being transferred.

## Transfer window

Use [GitHub's repository transfer flow](https://docs.github.com/en/repositories/creating-and-managing-repositories/transferring-a-repository)
while signed in as the authorized owner.
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
teams, protections, affected checks, Actions permissions, environments,
reviewers, secret names, runner access, Pages, webhooks, deploy keys, releases,
and security settings. A redirect working by itself is not success.

Run the read-only parity checker with the **final frozen** backup directory:

```bash
bash scripts/verify_primary_transfer_parity.sh "$backup_root"
```

It independently restores the mirror, compares destination refs and release
asset metadata against the frozen backup, checks public `dev` policy, and
confirms that both old Git and API URLs resolve to the transferred repository.
Its pass does not replace the settings and integration checks above. Before
transfer, the source-only rehearsal form may be used with
`JoePShoulak/sagan` as the second argument; that form cannot prove redirects.

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
- `dev` and `main` policies, teams, environments, approvals, configured secret
  names, runners, Pages, webhooks, deploy keys, and security settings are
  verified;
- all eight workflows are visible and the affected `dev` checks pass;
- documentation, release mirroring, installer metadata, and
  download links use or safely resolve to the canonical organization path;
- release publication remains paused and no signed-release capability is
  claimed from the missing signing secret;
- a clean clone from the new URL passes the owner bootstrap procedure; and
- the rollback drill and evidence record are complete.

Only then may the first split repository be created.

## Recovery boundary

If refs, releases, access, automation, documentation, or runners do
not match, stop releases and extraction work. Preserve the transferred state
and evidence; do not delete, recreate, or force-push either namespace. Use the
verified mirror for read-only recovery and coordinate any reverse transfer
through GitHub with the same inventory and freeze. A reverse transfer is not a
substitute for repairing an unverified backup. The approved local-only backup
cannot recover from loss of this machine; that limitation is knowingly
accepted for this transfer and remains a separate disaster-recovery gap.
