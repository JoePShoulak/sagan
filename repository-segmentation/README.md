# Repository segmentation preparation

This directory contains preparation and primary-transfer evidence for separating
the Sagan monorepo. The intact primary repository moved to
`Sagan-Shoulak/sagan` on October 5, 2026; no split repository has been created
or canonical component files extracted. The canonical policy remains the
[repository fracture roadmap](../docs/contributing/repository-fracturing-roadmap.md).

## Current preparation artifacts

- `inventory.tsv` assigns current path groups to a destination or records that
  ownership still requires a decision.
- `ecosystem.toml` records repository roles, dependency direction, and forbidden
  dependencies; `chat-map.toml` records specialized-chat ownership and the
  required handoff fields.
- `components.toml` records extraction order, source roots, documentation mount
  points, and artifact classes. `readiness.toml` records approved, provisional,
  and still-required start-gate decisions without storing credentials.
- `workspace.toml` is the proposed clone layout and repository URL map; it does
  not imply that its planned remotes exist. `history-extraction.md` is the
  preservation-first filter, verification, publication, and rollback draft.
- `primary-transfer.toml` inventories the intact repository's transfer surface;
  `primary-repository-transfer.md` is the transfer, verification, and recovery
  runbook. `audits/` records dated read-only GitHub settings snapshots. The
  primary transfer result is recorded in
  `audits/primary-transfer-result-2026-10-05.md`; remaining integration gates
  must pass before any split repository exists.
- `merge-readiness.md` separates this preparation branch's merge checks from
  the later transfer and per-repository extraction gates. The preparation
  branch merged into `dev` before the primary transfer.
- `owner-transfer-start-drill.md` is the owner's clean-checkout, local-backup
  recovery rehearsal required before this branch may merge.
- `vscode-relocation.toml` assigns destination paths and rewrites for every
  extension-owned file outside `editors/vscode-sagan/`; the validator checks it
  against the exact ownership manifest.
- `schemas/` defines the initial component, workspace-lock, documentation-
  export, and chat-map contracts. `templates/` provides the shared root files
  that each repository must specialize and test.
- `sandbox/` is the current Space Game seed project. Despite its historical
  name, it is source to preserve and extract into `sagan-space-game` alongside
  `SPACE_GAME_DESIGN.md`.
- The root `TECHNOLOGY.md`, `MAINTAINERS.md`, and `CODEX_START.md` are the first
  working instances of the required repository contract.
- `scripts/docs.sh` exposes separate structural and executable-example checks
  so documentation-only work can use the impact-based test policy.

## Before moving any code

- Finish post-transfer canonical references and the new-organization Codecov
  integration check; confirm the recorded redirects, governance, CI,
  documentation, owner recovery, and rollback before creating a split repository.
- Resolve every `required` decision in `readiness.toml` and record its evidence.
- Resolve every `needs-split` and `needs-rewrite` inventory row into exact
  extraction ownership before deleting any monorepo copy.
- Record a clean, green baseline commit and a recoverable backup reference.
- Select and pass the relevant checks for transfer or extraction impact. The
  full repository suite remains reserved for `dev` to `main` promotion or a
  release.
- Finalize schemas for component metadata, workspace locks, documentation
  exports, repository prompts, maintainer guides, and technology overviews.
- Record GitHub organization teams, repository visibility, branch protection,
  secrets, environments, runners, and transfer permissions without committing
  secret values.
- Document history-preserving extraction, redirect behavior, rollback, and
  partial-failure recovery.
- Perform the owner bootstrap/recovery drill from checked-in instructions.

## Safe audit commands

```bash
git status --short --branch
git rev-parse HEAD
git ls-files
bash scripts/docs.sh check-structure
bash scripts/repository_segmentation_check.sh
bash scripts/rehearse_primary_backup.sh
bash scripts/primary_release_backup.sh inventory
bash scripts/primary_release_backup_test.sh
bash scripts/verify_primary_transfer_parity.sh /c/Users/joeps/coding/sagan-transfer-freeze-2026-10-05
```

Run executable documentation examples only when examples or their supporting
compiler, package, tooling, or harness behavior changed:

```bash
bash scripts/docs.sh check-examples
```

No command in this preparation directory should mutate remotes or delete the
monorepo copy of a component.
