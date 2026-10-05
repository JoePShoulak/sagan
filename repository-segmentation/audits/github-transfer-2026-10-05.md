# GitHub transfer settings snapshot — October 5, 2026

This is read-only preparation evidence, not transfer approval. Recheck every
setting at the freeze because repository policy and branch tips can change.
No secret values were read or recorded.

## Identity and repository

- Authenticated account: `JoePShoulak`.
- `Sagan-Shoulak` membership: active admin. The organization allows members to
  create public and private repositories.
- Source: `JoePShoulak/sagan`, public, unarchived, default branch `dev`.
- `Sagan-Shoulak/sagan` returned HTTP 404 to the authenticated admin account.
  Confirm availability again immediately before transfer.
- A later read-only organization listing returned no repositories, and the
  source fork listing returned no forks. Recheck both at the freeze; these
  observations are not a reservation of the destination name.
- A later Actions query found no queued or in-progress runs. The most recent
  listed release, mirror, and `main` workflows were completed, but this is
  only a momentary observation; check again after the `dev` merge and before
  transfer.
- Remote `dev` was `1e1675037ca5273c1381588e6a8dadd395813bb6`;
  local `dev` was `c8fa3a17b8017d39d5d5e4cb1f929ab179471d6a`.
  A transfer baseline has not been selected.

## Actions and protection

- Actions enabled; allowed actions: `all`.
- `dev` branch protection endpoint returned `Branch not protected`.
- `main` requires linear history and enforces protection for admins. Force
  pushes and deletion are disabled. No required status-check contexts or
  required approving review count were reported by the queried fields.
- No repository rulesets were returned.
- Environments: `documentation`, `initial-unsigned-release`,
  `release-signing`, and `stable-release`. Only `stable-release` reported a
  required reviewer rule, with one reviewer.
- Repository secret names: `SAGAN_RELEASE_TAG_SSH_PRIVATE_KEY`.
- The `release-signing` environment returned no secret names, while the release
  workflow references `SAGAN_SIGNTOOL_COMMAND`. Verify whether the intended
  signing path is configured elsewhere before the next signed release.
- The repository runner `hp1-sagan-docs` was online with labels
  `self-hosted`, `Linux`, `X64`, and `sagan-docs-hp1`.
- No repository webhooks or deploy keys were returned.
- The GitHub Pages endpoint returned HTTP 404. The official documentation host
  is external to GitHub Pages; verify the HP1 deployment separately.

## Still required

- Preserve observed access and branch protection through transfer, then verify
  parity before making any new team or `dev` protection changes.
- Keep all releases paused. Signing is deferred; no signing-secret setup is
  required for this transfer.
- Record the exact transfer baseline, local mirror backup, ref inventory,
  release inventory, checksums, and recovery drill.
- Verify every setting above after transfer, then run the relevant CI, docs,
  installer, and mirror checks before allowing any split repository creation.

## Later read-only checks and owner decisions on October 5

- The source collaborator list contains only `JoePShoulak` with admin access.
  The target organization currently has no teams; its default repository
  permission is `read`. The owner chose to preserve the observed policy through
  transfer and consider tighter `dev` protection and team access afterward.
- `JoePShoulak` is the one required reviewer for `stable-release`. The source
  repository secret list contains `SAGAN_RELEASE_TAG_SSH_PRIVATE_KEY`; the
  `release-signing` environment has no secret names. The current token cannot
  list organization Actions secrets (`admin:org` scope required), so no claim
  is made about them. The owner abandoned signing work because of its cost and
  paused **all** release publication for now. Do not create or seek the missing
  `SAGAN_SIGNTOOL_COMMAND` credential as a transfer prerequisite.
- The owner approved a local-only backup on this machine. Its verified
  rehearsal is recorded in `local-backup-2026-10-05.md`; it must be refreshed
  from frozen `dev` after merge and before transfer.
- `bash scripts/verify_primary_transfer_parity.sh` passed its source-only
  rehearsal against the saved local backup. The destination and redirect
  checks can run only after the actual transfer.
- [GitHub's transfer documentation](https://docs.github.com/en/repositories/creating-and-managing-repositories/transferring-a-repository)
  says repository secrets and collaborators remain associated, while the
  organization's default permissions apply. It also warns that creating a
  repository at the old location destroys the redirect. These are verification
  expectations, not proof that the transfer has occurred.
