# Maintaining Sagan

This repository currently owns the Sagan compiler, language service, LSP and
DAP executables, core runtime, first-party math, physics and rendering sources,
the VS Code extension, documentation sources, tests, and Windows distribution
machinery. The [repository split roadmap](docs/contributing/repository-fracturing-roadmap.md)
describes the intended future owners of those components; it has **not** yet
happened. Do not assume the proposed sibling repositories or their locks exist.

This repository does not own the user's installed toolchain, GitHub secrets,
the HP1 host, or a signing certificate. Those are external dependencies.
Downstream consumers include installed Sagan programs, the extension, the
first-party libraries, and the documentation deployment. The compiler and
language-service contracts are authoritative; editor code must not duplicate
Sagan parsing or semantics.

The current known-good source revision is a Git commit, not a handwritten
version in this file. Inspect `git rev-parse HEAD`, `git status --short`,
`version.conf`, and `bash scripts/version.sh current` together. Package
projects use `sagan.toml` and a lockfile as described in the
[package catalog contract](docs/tooling/package-catalog-contract.md). The
multi-repository lock described in the split roadmap is future work.

## First safe checkout and verification

On Windows x64, use Git Bash and the MSYS2 UCRT64 toolchain. `git`, `make`,
`g++` with C++23 support, `cygpath`, and the repository's vendored
`third_party/uni-algo` must be available. Do not assume a clean macOS or Linux
bootstrap is equally validated. From a clean checkout:

```bash
git clone https://github.com/JoePShoulak/sagan.git
cd sagan
git switch dev
bash scripts/test.sh
bash scripts/docs.sh setup
bash scripts/docs.sh check
```

For a smaller compiler build, use `make -j1`; the executables are `bin/sagan`,
`bin/sagan-lsp`, and `bin/sagan-dap` (with `.exe` on Windows). `make clean`
removes ignored build outputs; inspect `git status --short` before and after
using it. The [development setup](docs/contributing/development-setup.md)
and [testing guide](docs/contributing/testing.md) contain further commands.
Open the repository in VS Code only after the checkout and local build are
known-good; editor support is under `editors/vscode-sagan`.

## Where to look

| Question | Canonical starting point |
| --- | --- |
| Compiler phases, source-to-C++ flow, and runtime | [Architecture](docs/implementation/architecture.md), [code generation](docs/implementation/code-generation.md) |
| CLI, LSP, and DAP behavior | [CLI](docs/tooling/command-line.md), [language service](docs/tooling/language-service-contracts.md), [LSP](docs/tooling/language-server-capabilities.md), [DAP](docs/tooling/debug-adapter-contract.md) |
| Package manifests, indexes, and locks | [Package catalog contract](docs/tooling/package-catalog-contract.md) |
| Daily Git and release promotion | [Change lifecycle](docs/contributing/change-lifecycle.md), [versioning](docs/contributing/versioning.md), [release lifecycle](docs/contributing/release-lifecycle.md) |
| Release assets and signing | [Windows installer gate](docs/contributing/windows-installer-release.md), [code signing](docs/contributing/code-signing-policy.md) |
| Documentation source, status, and hosting | [Documentation workflow](docs/contributing/documentation.md), [hosting](docs/contributing/hosting.md) |
| Planned repo ownership and compatibility | [Split roadmap](docs/contributing/repository-fracturing-roadmap.md), [ecosystem readiness](docs/contributing/ecosystem-release-readiness.md) |
| Primary repository organization transfer | [Transfer runbook](repository-segmentation/primary-repository-transfer.md), [owner start drill](repository-segmentation/owner-transfer-start-drill.md), [transfer inventory](repository-segmentation/primary-transfer.toml) |
| Remaining work and handoff standard | [Active roadmap](docs/contributing/active-roadmap.md), [handoff roadmap](docs/contributing/maintainer-handoff-roadmap.md) |

Major subtrees: `src/` implements the compiler and protocols; `libraries/`
contains first-party library sources; `editors/` contains editor integration;
`tests/` and `scripts/` contain validation and maintenance tools; `docs/`
contains canonical prose; `packaging/` and `deploy/` contain distribution and
deployment configuration; `.github/workflows/` defines CI; `third_party/`
contains vendored dependencies. `obj/`, `bin/`, and `build/` are generated
outputs, not source or release evidence.

## Safe daily change sequence

Inspect `git status --short --branch`, `git log -1 --oneline`, and the diff
before editing. Concurrent work may be present. Every authorized Codex request
that changes this repository starts from current `dev` on its own short-lived
`codex/<request>` branch. Run the smallest tests that fully exercise the changed
surface and refine the work until those focused tests pass. Stage only intended
paths, then inspect `git diff --cached --check` and
`git diff --cached --name-only`. Merge passing work into `dev`, then run the
same relevant checks against the integrated state, selected by the impact
matrix below. Do not run the full repository suite for an ordinary `dev`
merge. `main` is publication-only: promotion from `dev` is contingent on the
full suite passing, and pushing to `main` triggers release automation.
Never use `git reset --hard` or overwrite a shared branch to recover a failed
check. Preserve the state, inspect the failure, and make a corrective commit.

Version impact comes from Conventional Commit subjects after the baseline in
`version.conf`: `fix:` is patch, `feat:` is minor, and `!` or a `BREAKING CHANGE:`
footer is major; `docs:` and `test:` have no numeric impact. For a versioned
change, run `bash scripts/version.sh prepare patch`,
`bash scripts/version.sh prepare minor`, or
`bash scripts/version.sh prepare major` as appropriate before staging the README badge. Verify with
`bash scripts/version.sh check-badge`. The [change lifecycle](docs/contributing/change-lifecycle.md)
controls review, promotion, tagging, CI, and rollback; a local commit is not
a publication approval.

## Current release and recovery boundaries

All release publication and `main` promotion are currently paused by the
owner; signing is not being pursued because of its cost. Do not create a tag,
publish a release, or assume the missing signing secret will be configured
during repository segmentation. The local release-policy scripts are
`bash scripts/release_policy_test.sh` and
`bash scripts/main_release_policy_test.sh`. `bash scripts/docs.sh check` checks
documentation build/examples; `bash scripts/docs.sh release-check` is a
separate publication gate. Do not mark work-in-progress pages publication-ready
just to pass it. The post-1.0 Windows signing gate requires protected external
configuration; source control contains no signing credential. The portable
Windows artifact can be built and tested with `bash scripts/windows/build_portable.sh`
and `bash scripts/windows/test_portable.sh`; installer smoke modifies a test
installation and belongs on an isolated runner unless the owner approves it
on a workstation. Consult the linked installer and release guides before
tagging, mirroring, deployment, or recovery.

## Impact-based test matrix

| Change | Focused branch check | Repeat after merge to `dev` |
| --- | --- | --- |
| Documentation prose, navigation, or metadata that cannot affect executable examples | `bash scripts/docs.sh check-structure` | Documentation structure and publication-policy checks; do not run unrelated examples |
| Executable documentation example or behavior supporting examples | `bash scripts/docs.sh check-examples` plus structural checks | Complete documentation check with `bash scripts/docs.sh check` |
| Compiler, runtime, language service, package resolver, or shared test harness | The narrowest relevant test target | The same relevant target, expanded only for actual integration surfaces |
| VS Code extension | Relevant extension unit or integration target | The same relevant extension target against integrated `dev` |
| Physics or rendering package | Relevant headless or graphical package test | The same affected package checks plus directly affected integration demonstrations |
| Release, packaging, installer, deployment, or cross-component integration | Relevant policy or artifact test | The same affected policy, artifact, platform, or integration checks |
| Repository segmentation contracts or ownership inventory | `bash scripts/repository_segmentation_check.sh` | Segmentation contract check plus affected documentation checks |
| Primary repository transfer preparation | `bash scripts/primary_release_backup_test.sh` and `bash scripts/repository_segmentation_check.sh`; run `bash scripts/primary_repository_transfer_audit.sh` to inventory remaining transfer blockers. Use `bash scripts/verify_primary_local_backup.sh ABSOLUTE_BASH_BACKUP_DIRECTORY` to verify a completed local backup. | Repeat the focused backup and contract tests plus the read-only audit; create a fresh local backup only after the `dev` freeze and never treat these checks alone as transfer authorization |

When a change spans rows, use the union of their requirements. An apparently
documentation-only edit is not exempt when it changes executable snippets,
generated references, test discovery, or tooling behavior. See the
[testing guide](docs/contributing/testing.md) for exact component commands.
Before promoting `dev` to `main` or publishing a release, run the complete
repository suite and every required release/platform gate. Promotion is blocked
until all of them pass.

If a workstation is lost, start by cloning the last reviewed commit, verifying
its tag and release checksums, then rebuilding; do not reconstruct a release
from an unverified local `build/` directory. If GitHub, signing, HP1, or a runner
is unavailable, stop publication and keep the last known-good artifact. The
[hosting guide](docs/contributing/hosting.md) and release lifecycle describe
the parts currently automated. Exact credential recovery and an owner-performed
clean-machine drill remain open handoff work, not assumptions.
The [transfer-start recovery matrix](repository-segmentation/owner-transfer-start-drill.md)
explains precisely what the approved local-only backup can and cannot restore.

## Handoff status

This is the first root entry point, **not** a completed survivability gate.
The [handoff roadmap](docs/contributing/maintainer-handoff-roadmap.md) still
requires a complete operations inventory, automated checks of links and
commands, secret/service recovery procedures, and a recorded owner drill.
The documentation audit remains a separate gate. Signed-release verification
is deferred and must not be represented as passed.
