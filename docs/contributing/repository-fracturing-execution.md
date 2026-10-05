---
title: Repository fracture execution plan
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Repository fracture execution plan

This is the concise operating plan for starting the Sagan repository split.
The [canonical fracture roadmap](repository-fracturing-roadmap.md) remains the
authority when this checklist omits detail.

## Destination repositories

Create seven coordinated repositories under `Sagan-Shoulak`:

1. `sagan` — language, compiler, CLI, LSP, DAP, packages, math, and units.
2. `sagan-vscode` — VS Code client, grammars, VSIX, and editor tests.
3. `sagan-physics` — physics packages, numerical models, and headless tests.
4. `sagan-render` — rendering API, native backends, and graphical tests.
5. `sagan-workspace` — bootstrap, exact ecosystem lock, integrated demos, and
   cross-repository tests.
6. `sagan-docs` — one holistic official documentation site assembled from
   version-locked component documentation.
7. `sagan-space-game` — the Sagan application, assets, tests, releases, and
   owner-reviewed `SPACE_GAME_DESIGN.md`.

The language repository moves into the organization last. Physics never
depends on rendering, rendering never depends on physics, and the extension
never owns language semantics.

## Mandatory root contract

Before a repository is considered extracted, its root must contain:

- `README.md` for users and contributors;
- `TECHNOLOGY.md` for the conceptual architecture and how the moving parts fit;
- `MAINTAINERS.md` for exact Bash commands, branching, testing, releasing,
  deployment, diagnosis, recovery, manifests, locks, CI, and an impact-based
  test-selection matrix;
- `CODEX_START.md` for read-only onboarding, teaching-first behavior, ecosystem
  chat routing, and repository-specific canonical sources;
- machine-readable manifests, compatibility declarations, and exact locks; and
- independently runnable focused tests, complete tests, documentation checks,
  CI, and release procedures.

Any documentation edit resets its page to `status: review-needed`, sets
`publication_ready: false`, and clears all previous verification metadata until
a human audits it again.

## Workflow for every Codex change request

1. Read the repository instructions, technology overview, maintainer guide,
   roadmap, manifests, locks, Git status, and relevant history without editing.
2. Confirm repository ownership. If another specialized chat owns the work,
   recommend it and provide a ready-to-paste, context-complete handoff prompt.
3. Start from current `dev` and create one short-lived `codex/<request>` branch.
4. Implement only the authorized scope and preserve unrelated or concurrent
   work.
5. Test only what the request changed first. Diagnose, refine, and repeat until
   the changed component works and every focused test passes.
6. Commit only the intended paths and merge the completed request branch into
   `dev`.
7. Rerun the relevant tests selected by the repository's impact matrix on the
   resulting `dev`. Documentation-only edits that do not affect executable
   examples run structural checks without testing every example; changed
   examples or supporting behavior run the relevant example tests. Resolve
   failures before declaring the request complete.
8. Leave `main` untouched until reviewed publication promotion from `dev`.
   Promotion and release require the complete repository suite to pass.

Every repository's `CODEX_START.md` must repeat this impact-based test rule and
route the chat to the authoritative matrix and exact Bash commands in
`MAINTAINERS.md`. Repository-specific prompts may strengthen this shared
branch-test-refine-merge-verify clause, but none may omit or weaken it.

## Start gate

Do not move code until all of the following are true:

- repository names, visibility, ownership boundaries, and dependency direction
  are approved;
- the current monorepo baseline is backed up, recorded by commit, and green;
- schemas exist for component manifests, workspace locks, documentation
  exports, `TECHNOLOGY.md`, `MAINTAINERS.md`, and `CODEX_START.md`;
- organization teams, branch protection, CI credentials, runners, and transfer
  permissions are understood;
- canonical source, history-preservation, redirects, rollback, and
  partial-failure recovery are documented; and
- the owner can bootstrap, test, and recover the baseline using checked-in
  instructions rather than chat history.

## Extraction order

1. Record contracts and inventory every source, test, document, workflow,
   release artifact, secret, runner, mirror, and deployment by destination.
2. Make each future component independently buildable and testable in the
   monorepo.
3. Establish `sagan-workspace` with bootstrap, status, lock, restore, focused
   test, and complete integration commands.
4. Establish `sagan-docs` and prove version-locked aggregate previews without
   changing the public site.
5. Extract `sagan-vscode`; verify it against installed and pinned compiler/LSP
   artifacts and restore its official-docs section.
6. Extract `sagan-physics`; publish and consume a real package artifact, run
   headless numerical tests, and restore its docs.
7. Extract `sagan-render`; first define native-package metadata, then verify
   graphical evidence and installer consumption on supported platforms.
8. Extract `sagan-space-game`; use only `SPACE_GAME_DESIGN.md` for canonical
   game context and verify both released-toolchain and workspace-override builds.
9. Move cross-component demonstrations to `sagan-workspace`, leaving
   component-owned examples with their owners.
10. Establish independent releases and holistic documentation publication,
    promote a tested ecosystem lock, then transfer `sagan` into the organization.

## Repeatable extraction gate

For each repository, repeat this loop before removing the original copy:

1. Preserve relevant history and copy only owned material.
2. Pass focused build, test, documentation, packaging, and platform checks.
3. Pass independently against released dependencies.
4. Add the exact commit to the workspace lock and pass ecosystem integration.
5. Restore the component's official documentation section through aggregation.
6. Pass a clean-chat onboarding drill and an owner-survivability drill.
7. Verify artifacts, links, redirects, recovery, and parity with the source.
8. Remove the old copy only after every gate passes and rollback is proven.

## Completion gate

The fracture is complete only when all seven repositories are independently
maintainable and releasable, the exact workspace lock recreates a known-good
ecosystem, one official site publishes all approved component documentation,
Space Game builds from an installed Sagan toolchain, every onboarding and
survivability drill passes, and no required operation depends on chat history.
