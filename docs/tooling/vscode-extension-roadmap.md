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

Remaining rename readiness work is acceptance-matrix refinement rather than a
missing core provider: preserve protocol cancellation coverage and expose more
specific compiler-owned refusal messages through VS Code where the client API
permits it.

## Extension-owned release work

- Run a manual VS Code smoke-test matrix on Windows, Linux, and macOS covering
  activation, diagnostics, completion, hover, navigation, signature help,
  semantic tokens, formatting, rename, code actions, and hierarchies. The
  automated Windows integration path is now verified; platform-specific visual
  behavior, including file-icon theme interaction, and features outside its
  representative request set remain.
- Keep the automated Extension Development Host integration suite passing on
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
- Release automation includes the matching native compiler and language server
  in Windows installer/portable payloads, verifies the VSIX checksum and
  contents, installs the packaged VSIX into an isolated VS Code profile, and
  activates the extension against that native pair before publication.
- Decide whether to publish through the Visual Studio Marketplace. Until then,
  distribute the VSIX with matching Sagan release assets.
- Add user-facing troubleshooting for server crashes, incompatible schemas,
  quarantined executables, and platform runtime dependencies as real failures
  are observed.

## Compiler- or protocol-blocked goals

- Add check/build/run commands and VS Code tasks after the server exposes
  asynchronous, cancellable operation transport.
- Add Test Explorer after Sagan defines and exposes an authoritative test model.
- Add debugging after a Debug Adapter Protocol implementation exists; current
  source maps and debug metadata alone are not a live debugger.
- Expand package/module completion after an external package catalog and its
  compatibility rules are finalized.
- Support formatting of incomplete source after the compiler formatter can do
  so without unsafe or destructive edits.

## Deliberate non-goals

- Reimplementing compiler syntax or semantic analysis in TypeScript.
- Guessing unavailable library, package, test, or debugger APIs.
- Enabling a feature that the running server does not advertise.
