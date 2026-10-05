# Repository segmentation preparation

This directory contains Phase 0 evidence for separating the Sagan monorepo. It
does not create remote repositories, move canonical files, or authorize an
extraction. The canonical policy remains the
[repository fracture roadmap](../docs/contributing/repository-fracturing-roadmap.md).

## Current preparation artifacts

- `inventory.tsv` assigns current path groups to a destination or records that
  ownership still requires a decision.
- `ecosystem.toml` records repository roles, dependency direction, and forbidden
  dependencies; `chat-map.toml` records specialized-chat ownership and the
  required handoff fields.
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

- Review every `decision-required` inventory row and approve its destination.
- Record a clean, green baseline commit and a recoverable backup reference.
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
```

Run executable documentation examples only when examples or their supporting
compiler, package, tooling, or harness behavior changed:

```bash
bash scripts/docs.sh check-examples
```

No command in this preparation directory should mutate remotes or delete the
monorepo copy of a component.
