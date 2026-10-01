---
title: VS Code extension roadmap
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# VS Code extension roadmap

Version 0.3.4 is a usable language client for the capabilities currently
advertised by `sagan-lsp`. This roadmap lists work that remains after the
initial client integration. The extension must continue to consume compiler
contracts rather than reproduce parsing, semantics, project rules, or catalogs.

## Dependable F2 rename

F2 rename is available for compiler-proven local bindings, constants,
parameters, loop and match bindings, private members, non-exported functions,
exported identity groups, public members of exported types, enum cases,
imports, aliases, and namespace-qualified references. A function named `main`
is an ordinary function; executable entry behavior belongs to the selected
root document rather than that spelling.

The language server advertises `prepareRename`, returns versioned atomic
workspace edits, revalidates renamed identity groups, and refuses collisions,
ambiguous receiver types, stale snapshots, incomplete source, and other cases
it cannot prove safe. The live Extension Development Host suite covers local
and declaration-position rename, imported public-member navigation, cross-file
public-member rename, exported names with independent import aliases, collision
refusal, emoji identifiers, private fields, private mutating methods, and
one-step undo of both single- and multi-file edits.

Rename refusals use the LSP `RequestFailed` response and preserve the
compiler-owned explanation, allowing VS Code to show actionable messages for
invalid names, collisions, ambiguous identities, stale snapshots, and other
unsafe cases. Remaining rename readiness work is acceptance-matrix refinement
rather than a missing core provider: preserve protocol cancellation coverage.

## Deferred external release validation

These items are intentionally saved for much later and are not prerequisites
for continued local extension development:

- Run a manual VS Code smoke-test matrix on Windows, Linux, and macOS covering
  activation, diagnostics, completion, hover, navigation, signature help,
  semantic tokens, formatting, rename, code actions, and hierarchies. The
  automated Windows integration path is now verified; platform-specific visual
  behavior, including file-icon theme interaction, and features outside its
  representative request set remain.
- Observe the first hosted run of the automated Extension Development Host
  integration suite after the relevant commits are pushed. Keep it passing on
  each supported CI platform. The dedicated workflow builds the matching
  compiler and server and runs unit, bundle, and live-host tests on Windows,
  Linux, and macOS; its first hosted run remains to be observed. The suite
  covers real activation, diagnostics, hover, definition, completion,
  signature help, symbols, and formatting without making ordinary unit tests
  network-dependent. A clean
  Pop!_OS 24.04 VM has passed the compiler/server build, extension tests,
  Extension Development Host integration, VSIX installation, activation,
  file-icon, highlighting, diagnostic, and tooling-discovery checks; CI and
  the complete manual Linux feature matrix remain.
- Decide whether to publish through the Visual Studio Marketplace. Until then,
  distribute the VSIX with matching Sagan release assets.

## Extension-owned release work

- Release automation includes the matching native compiler and language server
  in Windows installer/portable payloads, verifies the VSIX checksum and
  contents, installs the packaged VSIX into an isolated VS Code profile, and
  activates the extension against that native pair before publication.
- Add user-facing troubleshooting for server crashes, incompatible schemas,
  quarantined executables, and platform runtime dependencies as real failures
  are observed.

## Capability-driven extension features

- The compiler library and LSP now expose cancellable check/build/run
  operations with versioned results and progress. The extension consumes them
  through cancellable commands and document/project tasks; the server currently
  serializes requests rather than scheduling concurrent operations.
- Test Explorer consumes compiler-issued stable IDs, UTF-16 ranges, document
  and project discovery, selected document execution, structured outcomes, and
  cancellation. When `testProjectRun` is advertised, selections spanning
  multiple modules execute as one authoritative project run; older servers
  retain the document-run fallback.
- Recovered-source document, range, and on-type formatting uses the existing
  LSP providers when `recoveredFormatting` is advertised; uncertain regions
  receive no speculative edits.

## Compiler- or protocol-blocked goals

- Add debugging after a Debug Adapter Protocol implementation exists; current
  source maps and debug metadata alone are not a live debugger.
- Expand package/module completion after an external package catalog and its
  compatibility rules are finalized.

## Deliberate non-goals

- Reimplementing compiler syntax or semantic analysis in TypeScript.
- Guessing unavailable library, package, test, or debugger APIs.
- Enabling a feature that the running server does not advertise.
