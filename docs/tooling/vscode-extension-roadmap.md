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

## Extension-owned release work

- Run a manual VS Code smoke-test matrix on Windows, Linux, and macOS covering
  activation, diagnostics, completion, hover, navigation, signature help,
  semantic tokens, formatting, rename, code actions, and hierarchies. The
  automated Windows integration path is now verified; platform-specific visual
  behavior, including file-icon theme interaction, and features outside its
  representative request set remain.
- Run the automated Extension Development Host integration suite on each
  supported CI platform. The opt-in local suite now covers real activation,
  diagnostics, hover, definition, completion, signature help, symbols, and
  formatting without making ordinary unit tests network-dependent. A clean
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
- Expand rename beyond proven local edits after safe public and cross-file
  rename is available.
- Expand package/module completion after an external package catalog and its
  compatibility rules are finalized.
- Support formatting of incomplete source after the compiler formatter can do
  so without unsafe or destructive edits.

## Deliberate non-goals

- Reimplementing compiler syntax or semantic analysis in TypeScript.
- Guessing unavailable library, package, test, or debugger APIs.
- Enabling a feature that the running server does not advertise.
