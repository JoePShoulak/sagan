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

## Critical priority: dependable F2 rename

Reliable rename is the highest-priority editor capability after the current
release-validation work. The present implementation is deliberately narrow:
it supports compiler-proven local bindings, private members, and eligible
non-entry functions within one document. It refuses exported or overloaded
functions, public members, imported symbols, entry points, and cross-file
edits. This safe boundary is preferable to an incorrect edit, but it is not
sufficient for normal project development.

Declaration-position selection is now fixed at the compiler query boundary and
covered for local variables, local constants, parameters, loop bindings, match
bindings, private fields, private mutating methods, and eligible functions. The
server also advertises and implements `prepareRename`, so VS Code can select the exact
identifier and show the compiler-owned reason when a known symbol is ineligible
before prompting for a name. The live-host suite applies a declaration-based
rename and verifies that one undo restores the complete document.
The remaining critical work is expanding the same proof model to the symbol
kinds and workspace cases below.

The compiler and language server must provide workspace-wide, identity-based
rename with collision, shadowing, visibility, overload, import/export, and
rebinding proofs. Requests must be versioned and cancellable, return either one
complete atomic workspace edit or a specific unsupported reason, and never
return a partial rename. VS Code should expose `prepareRename` eligibility and
the compiler-owned refusal reason instead of silently appearing inoperative.

The acceptance matrix must cover declarations and references for local
variables, parameters, functions, types, classes, faces, members, enum cases,
modules, imports, aliases, and exports across multiple files. It must also test
name collisions, shadowing, stale documents, unsaved overlays, Unicode and
emoji identifiers, mutating names ending in `!`, private names beginning with
`.`, cancellation, and undo as one workspace operation.

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
- Make release automation build the matching native compiler/server and VSIX,
  install them together in a clean environment, and verify checksums and basic
  activation before publishing assets.
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
