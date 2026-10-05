# Preparation branch merge gate

This gate permits the repository-segmentation **preparation branch** to merge
into `dev`. It does not authorize the primary repository transfer, create a
split repository, move code, change GitHub settings, or promote `dev` to
`main`. The transfer and each extraction retain their separate gates in
`primary-repository-transfer.md` and `history-extraction.md`.

The owner confirmed on October 5, 2026 that preparation should merge before
the intact primary repository is transferred. This reconciles the earlier
instruction to keep the branch separate until segmentation is genuinely ready:
the merge contributes reviewed plans, inventories, runbooks, tests, and a
staged sequence, not an unverified extraction.

## Preparation checks completed on the request branch

- `bash scripts/repository_segmentation_check.sh` passed, including exact
  tracked-file ownership manifests and transfer-reference inventory.
- `bash scripts/docs.sh check-structure` passed. Its MkDocs build reported an
  existing unlisted page, `contributing/late-bound-face-defaults-checkpoint.md`;
  the command still exited successfully. No executable documentation example
  changed in this preparation branch.
- `bash scripts/primary_release_backup_test.sh` passed inventory, mock asset
  download, publisher-digest verification, corruption rejection, and refusal
  to write into a nonempty backup directory. Actual release assets have not
  been downloaded; that belongs to the transfer freeze and durable backup.
- The VS Code extension passed `npm test`, `npm run test:bundle`, and
  `npm run test:integration` in the current checkout. Its copied, history-
  filtered subtree passed `npm ci`, the unit and bundle tests, compiler-demo
  validation, and the live Extension Host integration test with explicit
  compiler and language-server paths. The shared-file import in
  `vscode-relocation.toml` remains a later extraction gate.
- `bash -n` passed for the new Bash scripts, and `git diff --check` passed for
  the intended changes. The bundle and Extension Host checks required normal
  filesystem access because the restricted Codex sandbox denied esbuild a
  parent-directory read; they passed with that access.

## Before merging this branch

1. Recheck `git status --short --branch`, the request branch's commit range
   against current `dev`, and `git diff --check`. Preserve concurrent edits,
   particularly the other chat's `sandbox/src/main.sagan` change; never stage
   it as part of this preparation work.
2. Confirm the segmentation contract, documentation structure, release-backup
   mock, and affected extension checks still pass at the final branch HEAD.
3. Merge the reviewed branch into current `dev` without sweeping in dirty
   working-tree files. If `dev` moved concurrently, inspect and resolve that
   integration deliberately rather than resetting either branch.
4. On integrated `dev`, repeat the relevant checks above. Do not run the full
   suite merely for this merge. The full suite remains a `main` promotion or
   release gate. Push only after the integrated result and its required checks
   are reviewed.

## Gates that remain after this preparation merge

- Select the transfer freeze commit and synchronize local and remote `dev`.
- Approve organization access, new-repository visibility, `dev` protection,
  signing configuration, artifact and compatibility contracts, and the exact
  per-component ownership/rename lists at their relevant start gates.
- Make a durable, off-machine mirror and release-asset backup immediately
  before transfer, verify restoration, and record owner recovery evidence.
- Transfer the intact primary repository, verify redirects and every external
  integration, then update its canonical references. Only after this passes
  may a split repository be created.
- Before each extraction, install and pin the approved history tool, rehearse
  shared-file import, pass independent component and documentation checks,
  and complete owner-survivability and clean-chat drills.

Until these later gates pass, `readiness.toml` must continue to report the
unresolved decisions and unperformed operations rather than calling the
segmentation itself ready.
