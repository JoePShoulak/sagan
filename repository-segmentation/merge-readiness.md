# Preparation branch merge gate

This gate controls when the repository-segmentation **preparation branch** may
merge into `dev`. The owner transfer-start drill has passed; final branch and
integration checks remain before the merge is complete. Merging it does not
itself transfer the primary repository, create a split repository, move code,
change GitHub settings, or promote `dev` to `main`. Transfer and extraction
retain separate execution gates in `primary-repository-transfer.md` and
`history-extraction.md`.

The owner confirmed on October 5, 2026 that preparation should merge before
the intact primary repository is transferred, then clarified that the branch
must remain separate until the project is ready to **begin** segmentation.
Therefore this branch may merge only when the primary-transfer start gate is
ready to execute immediately afterward. The backup and ref comparison must
still be refreshed at the transfer freeze after the merge; the merge is not
permission to skip those dynamic checks.

## Preparation checks completed on the request branch

- `bash scripts/repository_segmentation_check.sh` passed, including exact
  tracked-file ownership manifests and transfer-reference inventory.
- `bash scripts/docs.sh check-structure` passed. Its MkDocs build reported an
  existing unlisted page, `contributing/late-bound-face-defaults-checkpoint.md`;
  the command still exited successfully. No executable documentation example
  changed in this preparation branch.
- `bash scripts/primary_release_backup_test.sh` passed inventory, mock asset
  download, publisher-digest verification, corruption rejection, and refusal
  to write into a nonempty backup directory. A separate local rehearsal then
  downloaded all 33 real release assets, verified their publisher digests and
  21 Git refs, and restored a checkout from the mirror. The one-command local
  backup creator then passed end-to-end into a second local directory, with
  the same refs and asset checksums. Neither rehearsal is the final backup of
  the post-merge frozen `dev` commit.
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
- The owner completed the separate clean-checkout transfer-start drill and
  confirmed the recovery limits after the guide was clarified. See
  `audits/owner-transfer-drill-2026-10-05.md` for the checkout and outputs.

## Before merging this branch

1. Close the primary-transfer start gate: preserve observed access and branch
   protection through transfer, identify the stable-release reviewer, keep all
   releases paused and signing deferred, verify the owner-approved local
   backup location and restoration, and confirm the recorded owner recovery
   drill in `owner-transfer-start-drill.md` remains valid.
   The local-only backup cannot protect against loss of this computer.
   Confirm source and destination access, visibility, redirect safety, and a
   realistic freeze procedure. Record any intentionally deferred component
   decisions as gates before their own extraction rather than silently
   treating them as approved.
2. Recheck `git status --short --branch`, the request branch's commit range
   against current `dev`, and `git diff --check`. Preserve concurrent edits,
   particularly the other chat's `sandbox/src/main.sagan` change; never stage
   it as part of this preparation work.
3. Confirm the segmentation contract, documentation structure, release-backup
   mock, and affected extension checks still pass at the final branch HEAD.
4. Merge the reviewed branch into current `dev` without sweeping in dirty
   working-tree files. If `dev` moved concurrently, inspect and resolve that
   integration deliberately rather than resetting either branch.
5. On integrated `dev`, repeat the relevant checks above. Do not run the full
   suite merely for this merge. The full suite remains a `main` promotion or
   release gate. Push only after the integrated result and its required checks
   are reviewed.

## Gates that still apply at the transfer freeze and later extraction

- Select the exact transfer freeze commit and synchronize local and remote
  `dev`; recheck access, destination availability, settings, release inventory,
  and backup destination just before the remote mutation.
- Refresh the approved local mirror and release-asset backup immediately
  before transfer, verify restoration, and record the freeze evidence.
- Transfer the intact primary repository, verify redirects and every external
  integration, then update its canonical references. Only after this passes
  may a split repository be created.
- Before each extraction, settle its repository visibility, artifact and
  compatibility contract, and exact ownership/rename list; install and pin
  the approved history tool, rehearse shared-file import, pass independent
  component and documentation checks, and complete owner-survivability and
  clean-chat drills.

Until these later gates pass, `readiness.toml` must continue to report the
unresolved decisions and unperformed operations rather than calling the
segmentation itself ready.
