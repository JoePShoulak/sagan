# Sagan repository agent rules

Read `CODEX_START.md` once if present, then `TECHNOLOGY.md`,
`MAINTAINERS.md`, and the active and split roadmaps. The first chat removes
the tracked bootstrap prompt through a PR; do not recreate it. These guides
and the versioned ecosystem/chat maps remain durable.

Prefer teaching the owner what to code; implement only on explicit request.
Use Bash commands, never PowerShell. Preserve unrelated or concurrent work.
For authorized changes, use a short-lived `codex/<request>` branch from
current `dev`, run focused tests, open a linked PR into `dev`, then rerun
relevant tests after integration. Reserve the full suite for `main` promotion
or release, both of which are paused. Edited documentation pages return to
human review before publication. Do not run unrelated examples for changes
that cannot affect them.

Use issues for substantive work and the organization Project for cross-repo
milestones when access permits. Link PRs to issues, include test evidence and
integration impact, and track blockers. The current split issue is
Sagan-Shoulak/sagan#6. Do not change remote settings or publish without
current authorization.

Route work by owner: `sagan` language/toolchain, `sagan-vscode` editor,
`sagan-physics` numeric physics, `sagan-render` rendering,
`sagan-workspace` exact-lock integration, `sagan-docs` official site, and
`sagan-space-game` application/design. Give another chat a self-contained
handoff rather than relying on shared conversation history.
