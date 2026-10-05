---
title: Language-server capabilities
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Language-server capabilities

`bin/sagan-lsp` provides the stdio Language Server Protocol
transport over the shared compiler library. Its `initialize` response is the
authority for an individual client's enabled features. The protocol schema is
`sagan-lsp/1`, and experimental capability discovery embeds
`sagan.language-service/1`. The compiler still owns parsing, semantics,
module resolution, documentation, formatting, and safe edits. See the
[language-server guide](language-server.md) for a runnable demo and limitations.

## Baseline guarantees

The current server:

- speak Language Server Protocol over stdio without non-protocol stdout;
- identify its compiler, language, service-schema, project-model, and available
  metadata-catalog versions during initialization;
- use UTF-16 LSP positions while retaining exact UTF-8 byte ranges internally;
- analyze unsaved, versioned overlays across multiple open files;
- reject stale edits and suppress results older than the current document;
- cancel queued or active queries through `$/cancelRequest` and return LSP
  `RequestCancelled` for cancelled requests;
- publish stable coded diagnostics with related locations and fixes;
- negotiate and advertise only implemented capabilities;
- obtain syntax, semantics, formatting, project, build, and documentation data
  exclusively from reusable Sagan compiler libraries; and
- produce deterministic answers for identical snapshots and configuration.

Face-declared fields and private helper requirements are analyzed by the same
compiler path as batch builds. Diagnostics for missing initialization,
incompatible repeated promises, visibility, mutability, and member types are
published through ordinary LSP diagnostics; the extension does not need a
separate face-property schema or semantic implementation.

## Advertised capabilities

The server advertises synchronization,
diagnostics, hover, definition, type definition, implementations, references,
highlights, signature help, completion, semantic tokens, document/workspace
symbols, folding, selection ranges, import links, inlay hints, rename, type and
call hierarchies, code actions, formatting, and required file notifications.

Individual refactorings are offered only when their safety proof is
implemented. Document and project test discovery and execution use the
explicit Sagan test model. Debug Adapter Protocol support is separate; the language
server may expose debug metadata discovery but does not claim to be a debugger.

## Capability discovery

In addition to normal LSP initialization, Sagan-specific experimental metadata
uses the versioned `sagan.language-service/1` schema and reports:

- supported position encodings;
- strict and recovery analysis support;
- source, standard-library, and package documentation catalogs;
- formatter and refactoring action IDs;
- check/build/run/test operation availability;
- source-map and debugger-metadata availability;
- cancellation and incremental synchronization modes; and
- explicit unavailability reasons for compiler features not present in a build.

The compiler discovery JSON includes
`"sourceEditsSchema":"sagan-source-edits-v1"`. Its C++
`source_edit_capabilities()` action list reports which formatting and
proof-gated edit actions are available and why others are disabled. This is
embedded in LSP `initialize` under `experimental.compiler`.
The same JSON reports `sagan-operations-v2`, `sagan-cpp-source-map-v1`, and
`sagan-debug-metadata-v1`. The additive `debugAdapterExecutable` discovery
field names `sagan-dap.exe` on Windows or `sagan-dap` elsewhere, located beside
`sagan` and `sagan-lsp`; its presence does not override a false `debugAdapter`
capability. Native check/build/run and cancellable LSP operation
transport, source maps, debug metadata, and launch plans are true. Granular
test-document discovery/run and project discovery/run are true. The VS Code
extension uses those granular flags to provide Test Explorer. The aggregate
compiler field `testExplorer` remains false because the compiler does not own
or advertise a particular editor UI; it does not negate the four test
transport capabilities. Debug attach and optimized-local evaluation remain false.
The local package index reader, `sagan/packages/query`, and the
installed-source `sagan/packages/catalog` query are true. The catalog provides
real exported symbols and source navigation metadata where installed source
resolves; manifest aliases and exact lockfiles resolve installed external
dependencies into builds. Standard LSP now also serves incomplete module and
selective-export imports plus go-to-definition on module paths. Export names
inside selective imports also have source definition and hover targets,
including incomplete importer documents and installed-source overlays.
Package completion, package navigation, and package auto-import remain false. The
breakpoint mapping library is available, but every live DAP/debugger capability
remains false until the adapter and release payload pass end-to-end tests.

For an open document named `sagan.toml`, the server now validates the unsaved
buffer with the same manifest parser used for disk builds and publishes
versioned project diagnostics. It also offers section, `[package]` and
`[application]` key completions, plus the compiler-defined `console` and
`windowed` application-mode values, with replacement edits. Hover describes
recognized sections and keys from compiler-owned metadata. Document symbols
show section and key hierarchy, even while the buffer is incomplete. Entry
paths and locked dependency keys navigate to their compiler-resolved targets.
Quoted dependency requirements for an installed package, including the
`{ package, version }` alias form, can complete to the newest compatible
indexed version. The standard `textDocument/formatting` and
`textDocument/rangeFormatting` requests also format
complete, valid `sagan.toml` buffers conservatively: it normalizes section and
key spacing without changing comments, quoted values, or line endings. Range
formatting includes only complete edits wholly inside the requested range.
Invalid or incomplete manifests return no edits. Manifest on-type formatting
and quick fixes remain unsupported.

Clients must treat missing or false capabilities as unavailable. They must not
fill a missing compiler capability with duplicated language logic.

## Verification boundary

The compiler-library and executable protocol suites cover every advertised
LSP handler, UTF-16 positions, unsaved overlays, cancellation, framing, and
request lifecycle. The Phase 9 gate additionally covers multi-root workspaces,
edit bursts, stale versions, malformed input, repeated queries, overlay
cleanup, bounded frame size, and stderr-only optional logging. The
[reliability guide](language-server.md#reliability-gate) records repeatable
commands and performance limits. These tests establish the extension-facing
server contract; custom test discovery and selected document/project execution
are available, but DAP is not. Check,
build, and run LSP requests use the separate versioned `sagan/operation`
contract. Cross-file rename is available only for compiler-proven identity
groups; ambiguous identities are deliberately refused. Local F2 checks lexical
scope ancestry, so a name used only in a different function is permitted while
same-scope and nested collisions remain blocked. Unexported classes, faces,
and enums (including cases of unexported enums) can be renamed within one document when their constructor, type, and
conformance references rebind to the same declaration; exported types still
require workspace proof. Workspace rename refuses to edit installed dependency
source, including when F2 starts on a selective import of that dependency;
renaming a binding alias in the local project remains separate. Direct `bin/lsp-test` execution
includes a native run request and therefore needs the native C++ toolchain on
`PATH`; the repository test target supplies that environment.

