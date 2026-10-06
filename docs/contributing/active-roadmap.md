---
title: Active development roadmap
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Active development roadmap

This is the ordered work queue after the 2.0 release. A milestone is complete
only when its stated behavior, tests, and documentation agree. Development
continues on focused branches from `dev`; only an approved promotion to `main`
starts stable-release preparation. This page tracks the work we explicitly
paused, not a promise to implement every possible language or library feature.

## 1. Restore green development CI

**Completed (2026-10-03, `dev` at `641d29d`):** all five integrated
development workflows pass. Hosted coverage is 92.09% (15,150 of 16,451
lines), above the unchanged 90% floor. The earlier multi-root LSP integration
failure is no longer a failing gate.

The baseline failing run is GitHub Actions `37055126294`; the coverage repair
was merged through PR #2. Continue adding behavior tests as features land,
without weakening the floor.

**Visible finish line:** five green `dev` workflows, including Coverage.

## 2. Establish an owner-operable maintenance handoff

**Release-blocking and repository-split-blocking requirement:** before Sagan is
divided into independently versioned repositories, create and verify the
[maintainer handoff and project-survivability guide](maintainer-handoff-roadmap.md).
Every repository must have a root-level `MAINTAINERS.md` that lets the owner
operate that repository and understand its place in the complete ecosystem
without relying on undocumented history, a particular contributor, or access
to an AI assistant.

The handoff must document exact Bash commands and decision rules rather than
high-level summaries. It must cover repository purpose and boundaries,
directory topology, dependency direction, every `sagan.toml` and lock/index
contract, local bootstrap and multi-repository workspace setup, build and test
entry points, branch and commit conventions, direct work on `dev`, promotion
to `main`, pull requests, CI workflows and recovery, version calculation,
release and rollback procedures, documentation aggregation, deployment,
secrets and external services, platform-specific behavior, troubleshooting,
and disaster recovery. Commands, filenames, expected outputs, prerequisites,
failure modes, and safe recovery steps must be kept current with the code.

No extraction milestone is complete merely because code was moved. The new
repository must pass an owner-survivability drill from a clean checkout, and
CI must reject missing required sections, stale command references, invalid
links, or undocumented workflow entry points. Any change to a command,
manifest, workflow, branch rule, release path, integration boundary, or
deployment mechanism must update the affected `MAINTAINERS.md` in the same
change.

**Visible finish line:** Joe can start from the organization landing page,
follow only checked-in maintainer documentation, reproduce a complete local
workspace, explain how all repositories connect, make and validate a safe
change, interpret or repair CI, promote it according to policy, build the
holistic documentation site, and recover the project from a new machine.

## 3. Fracture the repository without fracturing the project

Carry out the checked-in
[multi-repository fracture roadmap](repository-fracturing-roadmap.md). The
target organization contains independently versioned repositories for the
language, VS Code extension, physics, rendering, holistic documentation,
integrated workspace, and the application currently named Space Game. The
workspace and documentation aggregators must preserve one-command development
and one official documentation site across those boundaries.

Every extracted repository must satisfy the maintainer-handoff gate and begin
with a root `CODEX_START.md`: a one-time prompt for a new Codex chat that
teaches the agent both that repository and the Sagan ecosystem before work
begins. Prompts must name the canonical files to read, dependency boundaries,
workflow and validation commands, current compatibility metadata, and rules
for preserving unrelated changes. A clean-chat onboarding drill must verify
that each prompt produces an accurate repo map and safe first-step plan without
depending on hidden conversation history.
After that first chat reads the prompt, it deletes the tracked file through a
reviewed PR into `dev`. `AGENTS.md`, `TECHNOLOGY.md`, `MAINTAINERS.md`, and the
versioned maps retain the lasting onboarding and workflow knowledge.

Every prompt must preserve the owner's teaching-first preference: explain what
to code, why, and how before offering to implement it. Planning, diagnosis, and
design discussion are not implementation authorization. Before the owner
explicitly declares the language secure, language implementation still
requires a direct request. After that declaration, chats should teach and
review rather than write language/tooling code; only rendering, physics, and
Space Game code may be written, and only when explicitly requested.

Space Game becomes its own Sagan application repository. It must declare its
required Sagan installation and package dependencies, own the canonical
`SPACE_GAME_DESIGN.md`, and use that document as the sole source of game-design
context. Its `CODEX_START.md` must invite a new chat to ask the owner as many
clarifying questions as useful, both to understand the owner's goals and to
help the owner deliberately resolve design and execution decisions. Confirmed
decisions, exploratory ideas, and unresolved questions must remain distinct,
and old chat history must not be a required source.

**Visible finish line:** every component has independent history, CI,
versioning, issues, maintainer guidance, and new-chat onboarding; a clean
workspace can build the locked ecosystem and Space Game; all component docs
publish into the official Sagan site; and the owner can operate the result
without undocumented cross-repository knowledge.

**Split validation status (2026-10-05):** all seven public repositories exist
with independent focused CI. The workspace locks the language, official docs,
physics, rendering, and VS Code extension at exact commits; its local Windows
build and focused test pass, and its offline contracts pass in hosted CI on
Linux, macOS, and Windows. The game stays outside the default workspace but
runs against its combined package catalog. The official docs aggregate strictly
builds from locked component sources. The existing primary documentation
workflow still owns the experimental HP1 deployment and passed after this
roadmap update; the split docs repository has not taken over deployment. Its
copied primary-style workflow currently fails because the split repo does not
contain primary executable examples, although the aggregate-docs check passes.

The [cross-repository tracking issue](https://github.com/Sagan-Shoulak/sagan/issues/6)
still tracks installed-artifact validation, clean-chat and owner handoff drills,
reviewed publication/site cutover, and recoverable retirement of duplicate
sources. Cross-repository work is now tracked in the
[Sagan Development Project](https://github.com/orgs/Sagan-Shoulak/projects/4).
The release and
`main` promotion hold remains in force. Source-level integration is not the
entire definition of done.

## 4. Finish compiler-owned extension contracts

Complete these as separately testable increments; advertise each capability
only when its own library, protocol, and integration tests pass:

1. Contextual installed-package completion in imports, exports, qualified
   names, types, and member access; safe import edits, alias collisions,
   missing-source navigation, and multi-root behavior.
2. Production Sagan DAP launch: reliable Sagan scopes and values,
   source-mapped failures, breakpoint/step/termination coverage, cancellation,
   child-process cleanup, and packaged debugger dependencies. Keep attach and
   optimized-local evaluation false unless separately proven.
3. Complete compiler-owned `sagan.toml` document tooling beyond its initial
   overlay diagnostics, section/key and application-mode completion, and
   section/key hover and outline; finish remaining analysis
   cancellation checkpoints, as described in the
   [extension roadmap](../tooling/vscode-extension-roadmap.md).

**Visible finish line:** the running server advertises only tested package and
debugger capabilities; the release payload contains the executable and native
dependencies that those capabilities require. The separate extension work then
consumes those contracts, rebuilds its VSIX, and passes isolated activation,
live-host, and cross-platform smoke tests. Marketplace publication remains an
optional later decision.

## 5. Complete the unified Sagan error experience

Retain the agreed friendly error style while covering lexical, parse,
semantic, type, module, project, build, and runtime failures. Give each case a
stable code, source location, concise explanation, safe hint when available,
and Sagan-level traceback for runtime failures. Terminal and editor rendering
must share structured compiler diagnostics rather than separate error logic.

**Visible finish line:** representative failure fixtures produce consistent
CLI, LSP, and runtime output without raw native implementation messages.
This is explicitly deferred until the extension-contract milestone is stable.

## 6. Audit documentation with the owner

The existing pages remain `work-in-progress` until Joe's page-by-page review.
First reconcile stale version/status claims with the tested implementation,
then review the tour and reference as a newcomer would use them. The audit may
produce documentation fixes or concrete compiler issues; it does not silently
change language semantics. See the
[documentation audit plan](documentation-roadmaps.md).

**Visible finish line:** every page has an explicit reviewed status and
publication-ready pages pass the documentation release check.

## 7. Complete Windows distribution acceptance

Resume the SignPath Foundation application when a decision arrives. Set up
trusted Authenticode signing if accepted, then test the exact signed candidate
on a clean Windows x64 machine or VM: install, CLI, generated programs,
Explorer launch, upgrade, downgrade refusal, and uninstall. Record hashes and
results as required by the [installer gate](windows-installer-release.md).

**Visible finish line:** a signed, clean-machine-verified Windows release
candidate. Linux, macOS, Windows ARM64, and package-manager distribution remain
separate later milestones.

## Later language and library work

The two-body, restricted-three-body, and solar/Lagrange demonstrations are
implemented; they are not still waiting in this queue. Future library work is
deliberately incremental: a portable renderer backend (SDL3 before a GPU/UI
expansion), broader math and physics APIs, and only then more advanced models.
The language backlog includes coordinate-frame typing, spherical
arithmetic/conversion, advanced unit categories, package installation and
remote distribution, and contextual `self` capture. Each requires its own
design and observable demo before being promoted into this active queue.
Late-bound `self.member` dependencies in face defaults are a separate
object-model increment. The current inheritance work does not implement them;
the [implementation checkpoint](late-bound-face-defaults-checkpoint.md)
records the type-checking, lowering, dispatch, diagnostic, and tooling work
required before this can be advertised.
