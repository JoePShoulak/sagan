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

- Decide and document the destination team's access and whether `dev` should
  gain protection before or after transfer.
- Verify the signing secret's intended location and stable-release reviewer
  identity without recording values in the repository.
- Record the exact transfer baseline, offline mirror backup, ref inventory,
  release inventory, checksums, and recovery drill.
- Verify every setting above after transfer, then run the relevant CI, docs,
  installer, and mirror checks before allowing any split repository creation.
