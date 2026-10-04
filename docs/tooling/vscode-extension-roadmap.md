---
title: VS Code extension roadmap
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# VS Code extension roadmap

Version 0.3.5 is a usable language client for the capabilities currently
advertised by `sagan-lsp`. This roadmap lists work that remains after the
initial client integration. The extension must continue to consume compiler
contracts rather than reproduce parsing, semantics, project rules, or catalogs.

## Paused compiler handoff checkpoint

Extension work remains paused at the current `dev` checkpoint while the
compiler-side package and debugger contracts below are completed. The earlier
1.8.0 checkpoint is historical, not the present handoff. The extension must
not enable capabilities that discovery still reports as false.

Most ordinary editor support is ready. The language server supplies compiler-
backed diagnostics, formatting, hover, definition, references, completion,
signature help, symbols, semantic tokens, rename, hierarchies, code actions,
and cancellable build/run/test operations. Offline package resolution through
manifest aliases and `sagan.lock` is also available. Imported package symbols
have tested hover, definition, and completion paths, and incomplete selective
and module imports have compiler-owned suggestions and navigation.

Reaching the next extension handoff requires several focused compiler-side
increments followed by a release-payload validation pass. The preferred order
is package completion and safe auto-import first, then debugger values and
runtime-failure mapping, then debugger packaging and cross-platform validation.

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

- Add a compiler-owned `sagan.toml` document service with precise diagnostics,
  completion, hover, navigation, document symbols, formatting, and safe quick
  fixes. The extension already recognizes and highlights the current manifest
  schema, but must not reproduce manifest parsing or validation in TypeScript.
  The server needs an explicit manifest document selector/capability, UTF-16
  ranges, overlays, cancellation, and tests for required, duplicate, and
  unknown keys and sections, dependency aliases and requirements, paths, entry
  modules, application modes, lockfile state, and installed-package targets.
- Finish package completion in every relevant import, export, qualified-name,
  type, expression, and receiver context. Add compiler-owned safe auto-import
  edits, alias and collision handling, navigation when installed source is
  missing, and focused multi-root conflict tests. Existing lockfile resolution,
  selective-import navigation, catalog queries, and tested package hover,
  definition, and completion cases are foundations rather than the completed
  contract. Keep `packageCompletion`, `packageNavigation`, and
  `packageAutoImport` false until their individual end-to-end gates pass.
- Finish the experimental Debug Adapter Protocol implementation. `sagan-dap`
  can launch programs and exercise breakpoints, mapped stacks, and source-level
  stepping, but it still needs reliable Sagan values, runtime-failure and
  exception mapping, broader source-context and cleanup coverage, Linux
  execution validation, and release packaging of GDB and its runtime verified
  outside an MSYS2 environment. The extension must not register a supported
  Sagan debug type while all granular debugger capability flags remain false.

## Resume sequence

When compiler work resumes, complete and advertise each capability separately
rather than waiting for one large final patch:

1. Complete contextual package completion and its protocol tests.
2. Complete safe package auto-import edits, aliases, and multi-root conflicts.
3. Complete missing-source package navigation behavior.
4. Complete Sagan debugger values and runtime-failure/exception mapping.
5. Expand DAP source-context, cancellation, termination, and cleanup tests.
6. Package the debugger backend and validate Windows outside MSYS2 plus Linux.
7. Hand the tested schemas, executable-discovery contract, capability truth
   table, runtime dependencies, and validation results back to the extension.
8. Register only the newly advertised VS Code features, rebuild the VSIX, and
   rerun unit, bundle, isolated-install, activation, and live-host tests.

## Deliberate non-goals

- Reimplementing compiler syntax or semantic analysis in TypeScript.
- Guessing unavailable library, package, test, or debugger APIs.
- Enabling a feature that the running server does not advertise.
