---
title: Maintainer handoff and project survivability
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Maintainer handoff and project survivability

Sagan must remain operable by its owner if every current contributor and every
AI-assisted development session becomes unavailable. This is a release gate,
a repository-extraction gate, and an ongoing maintenance contract—not an
optional documentation cleanup.

The intended repository split makes this requirement more important. Separate
language, extension, physics, rendering, workspace, and documentation
repositories improve ownership and release tracking, but they also create
more manifests, compatibility boundaries, workflows, credentials, artifacts,
and failure modes. The split may proceed only as quickly as those mechanisms
can be explained and reproduced by a human maintainer.

## Required artifact in every repository

Every Sagan organization repository must contain `MAINTAINERS.md` and
`CODEX_START.md` at its root. `MAINTAINERS.md` is the guaranteed human
operations entry point. `CODEX_START.md` is the ready-to-paste prompt for a new
Codex chat required by the
[multi-repository fracture roadmap](repository-fracturing-roadmap.md). Either
may link to detailed material under `docs/maintaining/`, but neither may
require its reader to guess which canonical document to read next.

The root document must begin with:

1. what the repository owns;
2. what it deliberately does not own;
3. its upstream and downstream dependencies;
4. the shortest safe path to build and test it from a clean checkout;
5. links to every detailed operating procedure; and
6. the exact compatibility versions or lock file that define a known-good
   ecosystem state.

README material for users does not replace maintainer documentation. Public
official documentation does not replace it either. `MAINTAINERS.md` is an
operational, source-controlled handoff for the person responsible for keeping
the project alive.

## Mandatory contents

Each repository's maintainer guide must cover every applicable item below.
Omissions must be explicit: write `not applicable` with a reason rather than
silently leaving a topic out.

### Repository identity and architecture

- Repository purpose, supported products, and non-goals.
- Top-level directory map and the owner of every major subtree.
- Executable entry points and the path from source input to produced artifact.
- Public interfaces, private interfaces, and compatibility boundaries.
- Dependency direction and a statement of forbidden dependencies.
- Generated files, vendored files, caches, ignored outputs, and which of them
  may safely be removed.
- A diagram or ordered narrative showing how this repository participates in
  the complete Sagan ecosystem.

### Manifests, catalogs, locks, and configuration

- Every `sagan.toml` table and key used by the repository, with a complete
  working example.
- Package names, module names, source roots, entry modules, application modes,
  compiler constraints, and dependency constraints.
- Package-index row syntax, path resolution rules, installed versus available
  status, and the command that validates the index.
- Lock-file schema, how locks are generated, when they change, and how to
  review a lock update.
- Multi-repository workspace manifest and workspace lock semantics.
- Environment variables, configuration precedence, defaults, and safe example
  values.
- Native-library metadata, platform libraries, assets, compiler flags, and
  link behavior where applicable.
- Exact procedures for diagnosing a manifest, index, lock, compatibility, or
  source-resolution failure.

### Local development from a clean machine

- Supported operating systems and architectures.
- Exact prerequisites and where they come from.
- Exact Bash commands to clone, bootstrap, build, test, clean, and rebuild.
- Expected important output and the location of each resulting artifact.
- How to open the supported multi-root VS Code workspace.
- How the compiler, language server, extension, physics package, renderer, and
  documentation aggregator find one another locally.
- How to substitute a sibling development checkout for a released component.
- How to return from a development override to the known-good locked state.
- Common first-run problems, especially Windows executable locks, PATH
  contamination, missing runtime libraries, and native-toolchain discovery.

Instructions must use Bash. They must not depend on complex nested quoting
through PowerShell into Bash.

### Daily Git workflow

- The exact remote names and their purpose.
- The project's normal rule that each authorized Codex change begins on a
  short-lived `codex/<request>` branch from current `dev`.
- The focused-test gate before merging a request branch into `dev`, followed
  by the relevant integration tests on `dev`; reserve the full suite for
  `main` promotion and releases.
- How to inspect status and concurrent changes before editing.
- How to stage only intended paths.
- Conventional Commit syntax, including examples for patch, minor, major,
  documentation-only, test-only, and maintenance changes.
- When a commit changes a component version and when it does not.
- Exact commands for committing the request branch, merging it into `dev`,
  rerunning relevant tests, and pushing `dev`.
- The rule that `main` is reserved for reviewed publication promotion.
- Exact pull-request and merge procedure for promoting `dev` to `main`.
- How branch protection, required checks, linear history, tags, and release
  workflows interact.
- How to recover safely from a rejected push, merge conflict, failed check, or
  accidental local commit without destroying unrelated work.

The guide must clearly identify destructive Git operations that are forbidden
in normal recovery. It must prefer preservation, inspection, and reversible
steps.

### Testing and CI

- Every local test command, from focused tests through the full release gate.
- Which tests are authoritative for parser, semantics, code generation,
  packages, LSP, DAP, extension, physics, rendering, documentation, and
  distribution behavior.
- CI workflow filenames, triggers, path filters, runner platforms, required
  tools, expected artifacts, timeouts, and required status checks.
- Which workflows are component-local and which verify the integrated
  ecosystem.
- How reusable workflows are versioned and referenced.
- How to find a failing job, download its artifacts, reproduce it locally, and
  distinguish a product failure from runner or infrastructure failure.
- How to rerun safely, when not to rerun, and when a failure requires a code
  change.
- Coverage collection, exclusions, thresholds, upload, and recovery from
  corrupt or missing coverage data.
- The process for changing a workflow without temporarily removing a required
  protection.

### Versions, releases, and rollback

- Version source of truth and how the development version is calculated.
- Compatibility-range meaning for compiler, extension, physics, and renderer.
- Exact version-preparation commands and how to review their output.
- Preview versus stable tags and which branch may create them.
- Artifact inventory, filenames, checksums, provenance, and signing state.
- Release workflow stages and the evidence required before approval.
- Package-catalog publication and documentation-bundle lock updates.
- Windows installer and portable archive construction and isolated testing.
- Extension VSIX construction and installation testing.
- Library archive construction and consumer verification.
- What may be rolled back, what is immutable, and how to issue a corrective
  release without replacing published artifacts.
- How to recover when tagging succeeds but packaging, publication, mirroring,
  documentation, or deployment fails.

### Holistic documentation system

- Which documentation source is canonical in the repository.
- Its mount point in the official Sagan documentation site.
- The component documentation manifest and navigation fragment format.
- Local component validation and aggregate-preview commands.
- How a component update requests an experimental aggregate build.
- How stable component releases update the exact documentation lock.
- How language, extension, physics, rendering, workspace, and site-wide pages
  become one navigation tree and search index.
- Cross-repository link syntax, asset handling, redirects, version selection,
  and page-status metadata.
- Exact publication path to the official site and HP1, including verification
  and rollback.
- How an `Edit this page` link maps an aggregated page back to its canonical
  repository source.

### Secrets, services, and external state

- Every GitHub organization, repository, environment, project, runner,
  release, Pages, package, mirror, DNS, hosting, signing, and deployment
  dependency.
- The purpose and scope of every required secret or credential without storing
  the secret value in Git.
- Where access is administered and how the owner can rotate or replace it.
- Minimum permissions and which repositories may consume organization-level
  credentials or reusable workflows.
- HP1 and any other deployment host roles, paths, services, logs, backups, and
  recovery checks.
- What continues to work when an external service is unavailable.

### Troubleshooting and disaster recovery

- Symptom-to-check tables for common build, runtime, editor, package,
  rendering, CI, installer, release, and documentation failures.
- Read-only diagnostic commands before mutation.
- Backup and verification requirements before destructive recovery.
- Recovery from a lost workstation using only GitHub, documented credentials,
  released artifacts, and backups.
- Recovery from a deleted local checkout, unavailable runner, failed hosting
  machine, compromised token, broken package index, missing release asset, or
  incorrect documentation deployment.
- How to verify that recovery restored the intended source, versions,
  artifacts, documentation, and deployment state.
- A list of known manual steps that have not yet been automated.

## Repository-specific additions

The common outline is the floor, not the complete requirement.

### Language repository

Document compiler architecture, generated C++ flow, native toolchain lookup,
package resolution, built-in math and units, LSP/DAP schemas, Windows release
construction, and compatibility responsibilities toward every downstream
repository.

### VS Code extension repository

Document compiler/server discovery, schema negotiation, grammar generation and
tests, Extension Development Host tests, supported-version matrix, VSIX
packaging, reinstall and rollback, and the difference between extension code
and compiler-owned semantics.

### Physics repository

Document compiler compatibility, package modules, unit assumptions, numerical
methods, timestep and determinism limits, fixture tolerances, headless tests,
release packaging, and the strict absence of a rendering dependency.

### Rendering repository

Document the public rendering API, native ABI, platform backends, native
sources and system libraries, application mode, fonts and assets, graphical
test evidence, platform limitations, and the strict absence of a physics
dependency.

### Workspace repository

Document cloning and updating every sibling repository, exact lock semantics,
local overrides, generated package indexes, multi-root editor setup, integrated
demos, cross-repository CI, compatibility promotion, and returning a workspace
to a known-good state.

### Documentation repository

Document component checkout and mounting, aggregate locks, navigation and
search construction, preview builds, experimental and stable channels,
cross-repository dispatch, official publication, HP1 deployment, immutable
historical bundles, and rollback.

## Enforcement

Every repository must provide an automated maintainer-documentation check. At
minimum it must verify:

- `MAINTAINERS.md` exists and links to all mandatory detailed sections;
- `CODEX_START.md` exists, links only to valid onboarding sources, and covers
  both repository-local and whole-ecosystem orientation, including the owner's
  teaching-first and explicit-implementation policy;
- referenced files, commands, workflows, scripts, and documentation pages
  exist;
- the repository's current manifests and workflow filenames are enumerated;
- example manifests and configuration snippets parse;
- internal and cross-repository documentation links are valid;
- version and compatibility examples agree with machine-readable metadata;
- the clean bootstrap and focused verification commands are exercised in CI
  where practical; and
- changes to manifests, workflows, scripts, branch policy, release machinery,
  or documentation aggregation require an accompanying maintainer-document
  review.

Automation cannot establish comprehensibility. It supplements the human drill
below.

## Owner-survivability drill

Before extracting a repository, before declaring the multi-repository layout
complete, and before each major stable ecosystem release, perform a recorded
drill from a clean location or machine.

Using only checked-in documentation, the owner must be able to:

1. identify all repositories and obtain the known-good revisions;
2. bootstrap the holistic workspace;
3. explain the dependency and documentation-publication paths;
4. build each relevant component;
5. run focused and complete tests;
6. make a small representative change on a request branch from `dev`;
7. run its focused tests, stage and commit only that change, merge it into
   `dev`, rerun the relevant tests, push, and interpret the resulting CI;
8. diagnose at least one deliberately introduced failure;
9. restore the known-good state without destructive shortcuts;
10. prepare a promotion to `main` without publishing it accidentally;
11. construct and inspect release artifacts without publishing them;
12. build the complete official documentation site from the component lock;
13. identify where secrets, runners, mirrors, deployment services, and backups
    are administered; and
14. locate the recovery procedure for loss of the development machine or an
    external service.

Record the date, operating system, repository commits, commands used, failures
encountered, corrections made, and the owner's assessment of unclear areas.
An unclear or undocumented step fails the drill and creates a roadmap item.

## Definition of done

This milestone is complete only when:

- every repository has its root `MAINTAINERS.md` and required detail;
- every repository has a tested root `CODEX_START.md` that successfully
  orients a clean chat without hidden conversation history;
- the organization landing page points unambiguously to the workspace and
  maintainer entry points;
- the official holistic documentation explains the ecosystem at a user and
  contributor level;
- machine checks enforce structural accuracy;
- a clean owner-survivability drill has passed for each repository and the
  combined workspace;
- the results are recorded in source control; and
- repository extraction, release, and documentation checklists treat this
  requirement as blocking rather than advisory.

The standard is not that an experienced contributor can infer how Sagan
works. The standard is that its owner has a realistic, documented path to keep
the project alive, diagnose it, change it safely, and recover it without the
people or tools that originally helped build it.
