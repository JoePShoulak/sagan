---
title: Multi-repository fracture roadmap
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Multi-repository fracture roadmap

This roadmap governs the separation of the current Sagan repository into
independently tracked repositories without losing holistic development,
documentation, operational knowledge, or project context. It is the canonical
checked-in version of the repository-fracturing plan. Chat discussions do not
replace it.

No extraction is complete merely because files have moved. Each destination
must have independent ownership, history, versioning, tests, CI, documentation,
maintainer instructions, new-chat onboarding, and a verified place in the
locked Sagan ecosystem.

The creation and independence gates are deliberately separate. Before a new
public repository is created, its locally runnable candidate, exact history
and ownership map, focused checks, initial governance, recovery path, and
owner-reviewed public material must be ready. The new repository is then
created and populated with only reviewed `dev` and `main` refs. Hosted CI,
actual organization permissions, component documentation aggregation, and
owner drills are verified *there* before the repository is called independent.
The monorepo copy and live documentation remain in service until a later
reviewed cutover. Hosted CI cannot be a pre-creation condition.

## Target organization and repositories

Move the intact existing language repository into `Sagan-Shoulak` first, after
the transfer gate passes and before creating any split repository. This puts
all subsequent repository creation and integration work in its permanent
organization context from the beginning.

| Repository | Owns | Does not own |
| --- | --- | --- |
| `sagan` | Language, compiler, CLI, LSP, DAP, package system, compiler-owned math and units | Editor client, external libraries, integrated applications |
| `sagan-vscode` | VS Code extension, TextMate grammars, VSIX, Extension Host tests | Language semantics or compiler-owned diagnostics |
| `sagan-physics` | Physics packages, numerical models, headless examples and physics API docs | Rendering, editor integration, application gameplay |
| `sagan-render` | Rendering packages, native backends, graphical tests and rendering API docs | Physics state or application-specific scenes |
| `sagan-workspace` | Multi-repository bootstrap, exact ecosystem lock, integrated demos and cross-repository tests | Product implementations that belong to another repository |
| `sagan-docs` | Official holistic site assembly, navigation, search, version bundles and deployment | Duplicate canonical component documentation |
| `sagan-space-game` | The application currently named Space Game, its design, gameplay code, assets, tests and application releases | General-purpose language or library behavior |

The Space Game repository name is provisional until the product receives its
final name. Renaming it must preserve redirects and update every workspace,
documentation, package, CI, and onboarding reference.

## Dependency rules

The intended dependency direction is:

```text
sagan-vscode  --LSP/DAP protocols-->  sagan
sagan-physics --language/package---->  sagan
sagan-render  --language/native ABI->  sagan

sagan-space-game --> sagan installation
sagan-space-game --> released or workspace-locked sagan-physics
sagan-space-game --> released or workspace-locked sagan-render

sagan-workspace --> coordinates every repository at exact commits
sagan-docs      --> reads documentation exports at exact commits
```

Physics must not depend on rendering. Rendering must not depend on physics.
The extension must not implement language semantics. Space Game may consume
the language and libraries but must not become their implicit test fixture or
source of general-purpose APIs. Reusable functionality discovered in the game
must be deliberately designed and promoted into the appropriate library.

## Holistic development workspace

`sagan-workspace` restores the convenience lost by physical separation. It
must contain:

- `workspace.toml`, listing repositories, URLs, normal branches, and checkout
  locations;
- `workspace.lock`, recording exact tested commit IDs and released versions;
- generated local package indexes that point at sibling checkouts;
- a generated or maintained multi-root VS Code workspace;
- Bash scripts for bootstrap, status, update, build, focused tests, complete
  integration tests, lock refresh, and restoration of the locked state;
- integrated language/physics/rendering demonstrations;
- Space Game integration and compatibility checks without owning game source;
- cross-repository compatibility results; and
- clean-machine validation.

Do not make Git submodules the normal developer interface. A declarative
manifest and exact lock should clone ordinary sibling repositories into a
gitignored workspace directory. Developers must be able to enter any child
repository and use Git normally.

The minimum operator commands are intended to be:

```bash
bash scripts/bootstrap.sh
bash scripts/status.sh
bash scripts/update.sh
bash scripts/build.sh
bash scripts/test.sh
bash scripts/test-integration.sh
bash scripts/lock.sh
bash scripts/restore-lock.sh
```

Their final syntax, prerequisites, output, failure modes, and safe recovery
must be recorded in the workspace `MAINTAINERS.md`.

## Holistic official documentation

Component repositories own their documentation source and technical accuracy.
`sagan-docs` owns aggregation, global navigation, unified search, version
selection, preview builds, publication, and deployment to the official Sagan
documentation site.

Every component exports a documentation manifest containing its component ID,
source directory, official mount point, navigation fragment, version, and
compatibility declaration. `sagan-docs` maintains exact experimental and stable
locks for the language, extension, physics, rendering, workspace, and any
approved public Space Game documentation.

The resulting site remains one product:

```text
/language/
/tooling/vscode/
/libraries/physics/
/libraries/rendering/
/examples/
/projects/space-game/     # only material approved for public aggregation
```

The complete rules for canonical sources, aggregate previews, immutable
released bundles, cross-repository links, HP1 deployment, and recovery belong
in `sagan-docs/MAINTAINERS.md`. Private or unsettled game-design material must
not become public merely because the documentation aggregator can read the
game repository.

## Mandatory maintainer handoff

Every repository must satisfy the
[maintainer handoff and project-survivability roadmap](maintainer-handoff-roadmap.md).
In particular, each root must contain `MAINTAINERS.md`, exact Bash commands,
and a passed owner-survivability drill. Repository extraction, publication,
and organization transfer are blocked until this is true.

## Mandatory technology overview

Every repository root must also contain `TECHNOLOGY.md`. This document is the
conceptual companion to `MAINTAINERS.md`: it explains what the repository is
made of, why those technologies and boundaries exist, and how its moving parts
work together. `MAINTAINERS.md` remains the operational authority for exact
commands, prerequisites, branching, testing, release, deployment, diagnosis,
and recovery procedures.

Each `TECHNOLOGY.md` must explain, at a minimum:

- the repository's architectural responsibilities and explicit non-goals;
- its major subsystems, their responsibilities, and their dependency direction;
- important data, control, compilation, protocol, and artifact flows;
- language runtimes, native toolchains, frameworks, formats, and generated
  artifacts, including why each is present;
- public boundaries and contracts with every upstream and downstream Sagan
  repository;
- how local development differs from released and workspace-locked operation;
- platform-specific layers and where portability boundaries live;
- extension points, invariants, and common conceptual failure modes; and
- links to canonical detailed designs, decisions, schemas, manifests, and the
  corresponding operational sections of `MAINTAINERS.md`.

The technology overview must be understandable without relying on chat history
and must favor diagrams and concrete end-to-end examples where relationships
would otherwise be difficult to follow. It must not become a second command
reference or duplicate volatile version and file inventories that belong in
machine-readable manifests, locks, or maintainer procedures. Architecture or
integration changes are incomplete until the relevant `TECHNOLOGY.md` is
updated and returned to human documentation review.

## Mandatory new-chat onboarding prompt

Every repository must contain `CODEX_START.md` at its root. This is a
ready-to-paste prompt for starting a new Codex chat in that repository. It is
an onboarding aid, not a replacement for `AGENTS.md`, `TECHNOLOGY.md`,
`MAINTAINERS.md`, or canonical design documentation.

The prompt must instruct a new chat to begin read-only and read, in order:

1. repository `AGENTS.md` and applicable nested instructions;
2. root `TECHNOLOGY.md` for the conceptual system model;
3. root `MAINTAINERS.md` for exact operational procedures;
4. root `README.md`;
5. the repository's architecture, status, roadmap, compatibility, and release
   documents;
6. `sagan.toml`, package locks, workspace locks, catalog entries, and other
   machine-readable contracts applicable to the repository;
7. the organization-level ecosystem map and the workspace lock;
8. recent Git status and relevant history without disturbing concurrent work;
9. repository-specific canonical context listed below; and
10. current CI status when the task depends on it.

After reading, the chat must report:

- what the repository owns and does not own;
- upstream and downstream dependencies;
- current supported versions and platforms;
- build, focused-test, full-test, and documentation commands, plus the
  impact-based test matrix that says when each command is required;
- `dev`/`main`, branching, commit, promotion, and release rules;
- dirty or concurrent workspace state that must be preserved;
- relevant open roadmap decisions; and
- the safest next step for the user's requested task.

The prompt must tell the chat not to edit, commit, push, publish, transfer,
release, deploy, or resolve an unsettled design question during familiarization
unless the user explicitly asks. It must require Bash-facing commands and
preservation of unrelated changes.

After familiarization, every newly authorized Codex request that changes a
repository must use its own short-lived branch created from current `dev`. The
chat must test only what it changed first, diagnose and refine its work until
the focused tests pass, and keep unrelated or concurrent changes out of its
commits. It may merge the completed branch back into `dev` only after that
focused gate.
Immediately after the merge, it must rerun the relevant tests required by the
repository's impact-based test matrix on `dev` and resolve any integration
failure before reporting the request complete. A documentation-only change
that does not alter executable examples or the behavior supporting them runs
documentation structure, metadata, link, and publication checks without
re-running every documentation example. Changed examples and language,
package, tooling, or harness changes that can affect examples require the
relevant executable example tests. Every `CODEX_START.md` must state this
workflow as a mandatory shared clause and point to the exact commands and
impact matrix in `MAINTAINERS.md`. Repository-specific prompts may add stricter
requirements but may not omit or weaken this clause. The complete repository
suite is reserved for promotion from `dev` to `main` and release preparation;
promotion is contingent on that complete suite passing.

Every prompt must also state the owner's teaching-first preference. By default,
the chat should teach the owner what to write and why through explanations,
small decision packets, examples, pseudocode, implementation checklists,
review, and verification guidance instead of writing the code. Discussion,
planning, diagnosis, design exploration, and requests for a prompt or roadmap
do not authorize implementation.

Until the owner explicitly declares the language secure, language work may be
implemented only when the owner directly requests implementation. After that
declaration, the normal relationship becomes instruction and review rather
than implementation: chats must not write language, compiler, editor,
workspace, documentation-infrastructure, or release-system code for the owner.
Rendering, physics, and Space Game code remain eligible for chat implementation
only when the owner explicitly requests that code. No chat may infer either the
language-secure milestone or implementation permission from a roadmap item,
accepted design, bug report, or desired outcome.

Each prompt must explain the whole Sagan ecosystem in a short stable section,
then point to versioned canonical files for details. Do not paste volatile
version numbers, file inventories, or status claims into the prompt when they
can be read from manifests and locks. This reduces prompt drift.

### Cross-chat awareness and routing

Every `CODEX_START.md` must identify the intended specialized chat for each
repository in the ecosystem and explain the ownership boundary of those chats.
The list must come from a versioned organization-level chat map so repository
prompts do not silently drift apart as repositories are added or renamed.

A chat must recognize when a request belongs wholly or partly to another
repository's chat. In that case it should explain the boundary, recommend the
specific destination chat, and provide a concise ready-to-paste handoff prompt
containing the relevant goal, evidence, constraints, dependency versions, and
verification expectations. It must not use chat routing to abandon work that
belongs to its own repository, and it must not assume that another chat shares
unrecorded conversation context.

### Documentation edits invalidate publication review

Every `CODEX_START.md` must state that any documentation-content edit,
regardless of which chat or contributor makes it, automatically invalidates
the page's previous human publication review. The same change must reset the
page to `status: review-needed`, set `publication_ready: false`, and clear
`verified_in`, `verified_on`, and `verified_by`. A page may return to complete
and publication-ready only after a new human audit explicitly records the
review metadata.

This is a required repository workflow and CI invariant, not a suggestion for
chat behavior. Documentation checks must reject a changed page that retains
stale approval metadata, and aggregate documentation publication must refuse
that page until the fresh human review is complete.

### Prompt verification

Extraction requires a clean-chat onboarding drill. Start a new chat using only
the proposed `CODEX_START.md`, give it access to the repository and documented
ecosystem sources, and ask for a read-only orientation. Compare its report to
the maintainer guide and machine-readable metadata.

The drill fails if the chat:

- misunderstands repository ownership or dependency direction;
- invents implementation status or language semantics;
- cannot find the correct build and test commands;
- recommends the wrong branch or publication workflow;
- fails to create a request branch, iterate through focused tests, merge the
  passing work into `dev`, or rerun the relevant tests after integration;
- runs the complete suite for an ordinary `dev` merge or recommends promotion
  to `main` without a passing complete suite;
- runs every executable documentation example for an unrelated documentation-
  only edit, or skips affected example tests when examples or their supporting
  behavior change;
- misses the holistic documentation pipeline;
- does not know the other specialized chats or cannot produce a useful handoff
  when work crosses a repository boundary;
- overlooks required compatibility locks;
- treats proposals as accepted decisions;
- writes code when the prompt requires teaching, or mistakes discussion for an
  implementation request;
- edits documentation without resetting that page to `review-needed` and
  clearing its prior human verification metadata;
- edits during familiarization; or
- depends on conversation context that was not captured in source control.

Record the tested prompt revision, repository commit, date, model/tool context,
orientation report, discrepancies, and corrections. CI should validate prompt
links and required headings, while the human drill validates comprehension.

## Repository-specific onboarding sources

Every `CODEX_START.md` shares the common structure but adds canonical sources
for its repository:

- `sagan`: language design/status, compiler architecture, package contracts,
  LSP/DAP contracts, distribution and release lifecycle;
- `sagan-vscode`: extension architecture, grammar ownership, schema
  negotiation, supported-version matrix and VSIX lifecycle;
- `sagan-physics`: numerical methods, unit/frame assumptions, tolerances,
  package compatibility and rendering-independence rule;
- `sagan-render`: public API, native ABI, backend/platform matrix, assets,
  graphical evidence and physics-independence rule;
- `sagan-workspace`: repository manifest, exact lock, override rules,
  integrated demos and ecosystem CI;
- `sagan-docs`: component manifests, aggregate locks, navigation, preview,
  official publication, HP1 deployment and rollback; and
- `sagan-space-game`: `SPACE_GAME_DESIGN.md` as the sole source of game-design
  context, plus application architecture, required Sagan installation, package
  locks, and gameplay roadmap.

## Space Game extraction

The application currently named Space Game becomes an independent Sagan
application repository, provisionally `Sagan-Shoulak/sagan-space-game`.

Its root must contain:

- `SPACE_GAME_DESIGN.md` as the canonical living design document;
- `CODEX_START.md` as its new-chat prompt;
- `TECHNOLOGY.md` as its conceptual architecture and integration overview;
- `MAINTAINERS.md` as its operational handoff;
- a Sagan application `sagan.toml` with an explicit compiler requirement;
- an exact package lock for physics, rendering, and future dependencies;
- build, run, test, documentation, and packaging commands;
- a gameplay implementation roadmap separate from language/library roadmaps;
  and
- a record of which game material is public documentation versus private or
  unsettled design context.

### Required Sagan installation

Space Game is written in Sagan and must treat Sagan as an installed toolchain,
not reach into compiler source internals. Its maintainer guide and onboarding
prompt must explain:

- the minimum and tested Sagan versions;
- installation on each supported platform;
- how the game locates `sagan`, `sagan-lsp`, and the native toolchain;
- how a normal contributor uses released Sagan artifacts;
- how a language developer overrides them with the workspace-locked sibling
  checkout;
- how to restore the released/locked toolchain;
- which physics and rendering versions are required; and
- how incompatibility is diagnosed rather than silently worked around.

### Game prompt and clarification

The game `CODEX_START.md` must derive its game-design context exclusively from
`SPACE_GAME_DESIGN.md`. It must not mine, summarize, or depend on old Codex
chats, thread summaries, remembered conversations, or a separate context
ledger. If an idea matters to the game, the owner and chat should clarify it
and deliberately add the accepted result to the design document.

The prompt must explicitly invite the new chat to ask the owner as many
clarifying questions as it finds useful about the goals for the game. Those
questions may cover the intended player experience, scope, simulation depth,
physical realism, progression, entities, orbital correction, resources,
outposts, vehicles, population, research, narrative, combat, presentation,
accessibility, priorities, tradeoffs, and any ambiguity it discovers. The chat
should use the questions both to build accurate context and to help the owner
make deliberate design and execution decisions.

The chat may ask questions iteratively rather than forcing every decision into
one exchange. It must distinguish answers that confirm a decision from ideas
that remain exploratory, reflect its understanding back to the owner, and ask
follow-up questions whenever an answer reveals another meaningful ambiguity.
It must not silently treat an answer as canonical or edit the design document
until the owner explicitly asks it to record the decision.

## Versioning and compatibility

Versions remain independent:

```text
sagan             language/toolchain version
sagan-vscode      extension version
sagan-physics     physics package version
sagan-render      rendering package version
sagan-space-game  application version
```

The workspace lock records exact verified commits. Package manifests and the
extension compatibility matrix declare supported ranges. A component release
does not force every other component to release, but any changed compatibility
claim requires downstream verification and a refreshed ecosystem lock.

Space Game releases must identify the exact compiler, physics, rendering, and
workspace compatibility evidence used to build them.

## Branch and publication policy

Release publication and `main` promotion are currently paused by the owner;
signing is deferred because of its cost. The policy below describes how
publication will work only after the owner explicitly reopens it. Repository
segmentation preparation and the intact-repository transfer do not themselves
resume release publication.

Every product repository uses the same default policy:

- `dev` is the integration branch and is not the normal implementation
  workspace for a Codex request;
- every newly authorized Codex request that changes the repository starts from
  current `dev` on its own short-lived `codex/<request>` branch;
- the request branch is refined until its focused tests pass, then merged back
  into `dev`;
- the impact-required relevant tests are repeated on the resulting `dev`, and
  integration failures must be resolved before the request is complete;
- ordinary `dev` merges do not run the complete repository suite, and unrelated
  documentation-only changes do not re-run all executable documentation
  examples; affected examples must still be tested;
- commits follow the repository's Conventional Commit/version rules;
- `main` is reserved for reviewed publication promotion;
- promotion occurs by pull request from `dev` to `main` only after the complete
  repository suite and required release gates pass;
- tags and public releases originate only from approved `main`; and
- unrelated or concurrent changes are preserved and never swept into a move.

`MAINTAINERS.md` and `CODEX_START.md` must state this policy and any justified
repository-specific exception.

## CI model

Each repository must be independently green against released dependencies.
Cross-repository CI supplements rather than replaces local ownership:

1. component PRs test the changed repository against minimum and current
   supported dependency releases;
2. nightly integration tests all `dev` branches together;
3. workspace lock updates test exact proposed commits;
4. promotion gates test released artifacts rather than only sibling sources;
5. documentation PRs build an aggregate preview with the changed component;
6. Space Game tests both its released dependency path and workspace override;
   and
7. clean-chat prompt and owner-survivability drills block extraction and major
   ecosystem releases.

Shared workflows may move into an organization repository after the split is
stable. Until then, prefer visible component-local workflows over premature
indirection.

## Migration phases

### Phase 0 — Record contracts before moving code

- Approve repository names and ownership boundaries.
- Inventory source, tests, docs, examples, workflows, release assets, secrets,
  runners, mirrors, and deployments by destination.
- Record dependency direction and forbidden dependencies.
- Define package artifacts, compatibility metadata, workspace locks,
  documentation manifests, `TECHNOLOGY.md`, `MAINTAINERS.md`, and
  `CODEX_START.md` schemas.
- Define the versioned organization-level chat map, cross-chat handoff format,
  and documentation-review invalidation check shared by every repository.
- Confirm that `SPACE_GAME_DESIGN.md` contains the game context intended to
  survive extraction.
- Establish baseline versions and a known-good monorepo commit.

### Phase 1 — Transfer the intact language repository

- Back up the repository and record every ref before transfer.
- Verify organization ownership, teams, permissions, visibility, branch
  protection, Actions, secrets, environments, runners, Pages, releases,
  mirrors, package URLs, and transfer authority.
- Transfer the existing repository to `Sagan-Shoulak/sagan` without recreating
  the old path, so GitHub redirects remain intact.
- Update local and automation remotes, badges, links, documentation locks, and
  integrations that cannot follow the redirect safely.
- Run focused checks, hosted CI, documentation checks, onboarding, maintainer,
  and rollback drills in the organization context.
- Do not create any split repository until the transferred primary repository
  is verified and recoverable.

### Phase 2 — Make current components independently testable

- Give every future repository a focused build and test entry point.
- Remove accidental repository-relative coupling.
- Support external package manifest locations.
- Separate component documentation validation.
- Define native rendering metadata or document the temporary bridge.
- Test Space Game as a consumer of an installed Sagan toolchain.

### Phase 3 — Establish `sagan-workspace`

- Implement bootstrap, status, update, build, test, lock, and restore commands.
- Generate the local package index and multi-root editor workspace.
- Reproduce the current integrated demos.
- Add every current component and Space Game to an exact lock.
- Pass its maintainer and clean-chat drills.

### Phase 4 — Establish `sagan-docs`

- Move site theme, aggregation, versioning, deployment, and HP1 operations.
- Initially consume all documentation from the current `sagan` repository.
- Add component manifests and exact locks without changing the public site.
- Prove aggregate PR previews and experimental/stable publication.
- Pass its maintainer and clean-chat drills.

### Phase 5 — Extract `sagan-vscode`

- Preserve relevant history.
- Move extension docs and tests.
- Test against installed/pinned compiler and language-server artifacts.
- Restore its official-docs section through aggregation.
- Establish independent versioning, CI, releases, maintainer guide, and prompt.
- Remove the original only after parity.

### Phase 6 — Extract `sagan-physics`

- Preserve package history, fixtures, numerical examples, and docs.
- Publish and consume a real package artifact.
- Run headless numerical tests on supported platforms.
- Restore its official-docs section through aggregation.
- Establish independent versioning, CI, releases, maintainer guide, and prompt.
- Remove the original only after integrated demos pass.

### Phase 7 — Formalize native packages and extract `sagan-render`

- Define native sources, includes, flags, libraries, assets, subsystem, and
  unsupported-platform diagnostics in package metadata.
- Preserve renderer and backend history.
- Verify graphical evidence and installer consumption.
- Restore its official-docs section through aggregation.
- Establish independent versioning, CI, releases, maintainer guide, and prompt.
- Remove the original only after windowed demos pass.

### Phase 8 — Extract `sagan-space-game`

- Copy the owner-reviewed design document as the sole canonical source of
  game-design context.
- Create the Sagan application manifest and exact dependency lock.
- Make released-toolchain and workspace-override builds pass.
- Move only game-owned source, assets, tests, docs, and roadmap material.
- Add public game documentation to the official site only after explicit
  approval.
- Pass the game maintainer and clean-chat drills.
- Remove the original design/source only after canonical-location links and
  backups are verified.

### Phase 9 — Move integrated demonstrations

- Keep language-only examples in `sagan`.
- Keep package-specific headless examples with their packages.
- Place cross-package demonstrations in `sagan-workspace`.
- Keep application gameplay and game-specific vertical slices in Space Game.
- Verify documentation snippets against the exact workspace lock.

### Phase 10 — Establish independent publication

- Publish component artifacts, checksums, provenance, compatibility metadata,
  and documentation exports.
- Make distribution consume approved artifacts rather than copied source.
- Verify partial-failure recovery for tags, releases, catalogs, docs, mirrors,
  and deployments.
- Promote a tested ecosystem lock.

## Required decisions before extraction

- Final repository names and visibility.
- Component ownership and forbidden dependency directions.
- Package artifact and native-package formats.
- Workspace and documentation lock schemas.
- Documentation export and navigation manifest schemas.
- Compatibility-range meaning and release gates.
- `TECHNOLOGY.md`, `MAINTAINERS.md`, and `CODEX_START.md` required schemas.
- Space Game's provisional/final naming and initial public/private boundary.
- Minimum supported Sagan, physics, and rendering versions for the game.
- Cross-repository CI authorization and credentials.
- Organization project, teams, branch protection, secrets, and runner model.

## Deferrable decisions

- Public package registry beyond release artifacts.
- Package-manager installation.
- Linux and macOS rendering backends.
- Marketplace publication for the extension.
- Organization-wide reusable workflow centralization.
- Automated downstream dispatch on every commit.
- Final Space Game title, marketing site, storefront, and public release plan.
- Publication of private or unsettled game-design material.

## Definition of done

The fracture is complete only when:

- all seven repositories exist under the intended organization;
- each has independent history, `dev`/`main`, protection, CI, versioning,
  issues, releases where applicable, `TECHNOLOGY.md`, `MAINTAINERS.md`, and
  `CODEX_START.md`;
- each technology overview accurately explains the repository's moving parts
  and its conceptual place in the locked ecosystem without depending on chat
  history;
- all maintainer and clean-chat onboarding drills pass;
- every repository prompt and maintainer guide enforce and demonstrate the
  request-branch, focused-test, merge-to-`dev`, repeat-relevant-tests cycle and
  the full-suite-before-`main` rule, including when executable documentation
  examples are and are not required;
- every repository prompt can identify the other specialized chats, route
  cross-repository work, and produce a context-complete handoff prompt;
- `sagan-workspace` recreates a known-good ecosystem from exact commits;
- every component's canonical docs appear in one official Sagan site;
- every documentation edit automatically returns the affected page to
  `review-needed`, and CI prevents stale human approval metadata from reaching
  publication;
- Space Game builds using an installed Sagan toolchain and locked packages;
- its reviewed design document is the sole canonical source of game-design
  context in the game repository;
- released artifacts rather than accidental sibling paths pass integration;
- the owner can explain, develop, test, release, document, and recover every
  component using checked-in guidance; and
- no required operation depends solely on this or any other chat history.
