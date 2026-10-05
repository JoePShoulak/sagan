# Intact primary repository transfer — October 5, 2026

The owner froze other Sagan work, Git pushes, tags, releases, and GitHub
settings changes for this transfer. The preparation branch had already been
integrated into `dev`; no split repository was created. Release publication
remains paused and signing remains deferred.

## Frozen baseline and local recovery material

- Source before transfer: `JoePShoulak/sagan`; destination:
  `Sagan-Shoulak/sagan`. Both identify GitHub repository ID `1391462225`.
- Frozen `dev`: `689e6ec9bcd82f802e40b7bf72f34222cbb20e62`;
  `main`: `72047d8fce713a188c73b201ce7a81ef6b67c31a`.
- Final backup:
  `/c/Users/joeps/coding/sagan-transfer-freeze-2026-10-05`, outside the
  checkout, with 21 Git refs, five releases, and 33 assets. The mirror passed
  `git fsck --full`, asset digest/size verification, source-ref stability,
  and an independent restore. `mirror.refs` SHA-256:
  `2a102e0a6cdc820a681ff79729688753d905db4aa6b1535ade7d89fa2379de3c`.
  `ASSETS.sha256` SHA-256:
  `052be56781b3e405dc8d99a930a14fdf92bab15831794f18e601d054d3cd530a`.
- This is **local-only** recovery material. Loss of this computer or storage
  loses the backup. GitHub account access, settings, secrets, issue and pull
  request discussions cannot be restored from it.

## Transfer and immediate parity

The authenticated owner submitted GitHub's repository-transfer API request
with `new_owner=Sagan-Shoulak`. The destination API then reported the same
repository ID, public visibility, default branch `dev`, and unarchived state.
Git transport briefly returned HTTP 403 ("repository is disabled") immediately
after the API switched owners; a later read-only retry succeeded without a
reverse transfer or any Git write.

`bash scripts/verify_primary_transfer_parity.sh` against the final backup
passed after Git transport recovered. It checked the independent local restore,
all destination refs, release/asset metadata, public `dev` policy, and old
Git and API redirects. Evidence from that invocation was retained at
`/tmp/sagan-transfer-parity.t1sXpj`. The old GitHub Releases URL also
redirected to `https://github.com/Sagan-Shoulak/sagan/releases`.

Post-transfer read-only settings checks found:

- `main` protection still enforces admins and linear history, forbids force
  pushes and deletion, and has no required status checks or approving count.
  `dev` remains unprotected; no rulesets were returned.
- Actions remains enabled with all actions allowed. All eight expected
  workflows are active. The last five `dev` runs at the frozen commit
  (Documentation, VS Code Extension, Coverage, Windows Installer, and
  Integrated development) completed successfully before transfer. No new
  workflow was dispatched to verify transfer.
- All four environments remain, with `JoePShoulak` still the required
  reviewer for `stable-release`. The repository secret name
  `SAGAN_RELEASE_TAG_SSH_PRIVATE_KEY` remains; no secret value was read.
- `JoePShoulak` remains an admin collaborator. The `hp1-sagan-docs` runner
  remains online with its original labels. No webhooks or deploy keys were
  returned, and the GitHub Pages endpoint still returned 404.
- The official docs homepage, `/downloads/`, and
  `/downloads/releases.json` returned HTTP 200. No documentation or mirror
  publication was dispatched during transfer.

A clean clone from `https://github.com/Sagan-Shoulak/sagan.git` at frozen
`dev` passed `scripts/repository_segmentation_check.sh`,
`scripts/primary_release_backup_test.sh`, and `scripts/docs.sh
check-structure` after documentation setup. The unrelated dirty edit to
`sandbox/src/main.sagan` in the shared checkout was not staged or touched.
That checkout's `origin` was updated to the organization URL only after
parity and the clean clone passed.

## Remaining before the first split repository

- Merge and validate the dedicated post-transfer canonical-reference update
  on `dev`, including installer, release mirror, extension metadata, docs,
  and backup tooling. Do not run the full suite merely for this `dev` merge.
- The new-organization Codecov badge currently renders **unknown** while the
  old-owner badge still shows 94%. The frozen `dev` Codecov upload succeeded
  *before* transfer. Check a fresh upload under the new owner before switching
  the badge; if it stays unknown, resolve the Codecov integration separately.
- Keep releases paused. Do not promote `dev` to `main` or run a release
  workflow without the owner's later publication decision and the full-suite
  promotion gate.
- Complete each split repository's separate visibility, extraction-tooling,
  shared-file, owner-drill, and rollback gates before creating it.
