---
title: Active development roadmap
status: work-in-progress
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

**Current evidence (2026-10-03, `dev` at `dbf6b14`):** integrated development,
documentation, VS Code Extension, and Windows Installer workflows pass.
Coverage alone fails at 88.6% (14,175 of 15,998 lines) against the 90% floor.
The earlier multi-root LSP integration failure is no longer a failing gate.

- Add focused tests for real, uncovered compiler/service behavior. Do not lower
  the threshold or exclude implementation code merely to change the score.
- Run the local coverage suite and confirm a hosted `dev` coverage run reaches
  at least 90%, with the other four workflows still green.
- Keep the generated coverage report and the failing run ID available as
  evidence: GitHub Actions run `37055126294` contains the baseline LCOV artifact.

**Visible finish line:** five green `dev` workflows, including Coverage.

## 2. Finish compiler-owned extension contracts

Complete these as separately testable increments; advertise each capability
only when its own library, protocol, and integration tests pass:

1. Contextual installed-package completion in imports, exports, qualified
   names, types, and member access; safe import edits, alias collisions,
   missing-source navigation, and multi-root behavior.
2. Production Sagan DAP launch: reliable Sagan scopes and values,
   source-mapped failures, breakpoint/step/termination coverage, cancellation,
   child-process cleanup, and packaged debugger dependencies. Keep attach and
   optimized-local evaluation false unless separately proven.
3. Compiler-owned `sagan.toml` document tooling and remaining analysis
   cancellation checkpoints, as described in the
   [extension roadmap](../tooling/vscode-extension-roadmap.md).

**Visible finish line:** the running server advertises only tested package and
debugger capabilities; the release payload contains the executable and native
dependencies that those capabilities require. The separate extension work then
consumes those contracts, rebuilds its VSIX, and passes isolated activation,
live-host, and cross-platform smoke tests. Marketplace publication remains an
optional later decision.

## 3. Complete the unified Sagan error experience

Retain the agreed friendly error style while covering lexical, parse,
semantic, type, module, project, build, and runtime failures. Give each case a
stable code, source location, concise explanation, safe hint when available,
and Sagan-level traceback for runtime failures. Terminal and editor rendering
must share structured compiler diagnostics rather than separate error logic.

**Visible finish line:** representative failure fixtures produce consistent
CLI, LSP, and runtime output without raw native implementation messages.
This is explicitly deferred until the extension-contract milestone is stable.

## 4. Audit documentation with the owner

The existing pages remain `work-in-progress` until Joe's page-by-page review.
First reconcile stale version/status claims with the tested implementation,
then review the tour and reference as a newcomer would use them. The audit may
produce documentation fixes or concrete compiler issues; it does not silently
change language semantics. See the
[documentation audit plan](documentation-roadmaps.md).

**Visible finish line:** every page has an explicit reviewed status and
publication-ready pages pass the documentation release check.

## 5. Complete Windows distribution acceptance

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
