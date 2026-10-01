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

## Advertised capabilities

The server advertises synchronization,
diagnostics, hover, definition, type definition, implementations, references,
highlights, signature help, completion, semantic tokens, document/workspace
symbols, folding, selection ranges, import links, inlay hints, rename, type and
call hierarchies, code actions, formatting, and required file notifications.

Individual refactorings are offered only when their safety proof is
implemented. Test discovery is advertised only after Sagan defines a language
or project test model. Debug Adapter Protocol support is separate; the language
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
The same JSON reports `sagan-operations-v1`, `sagan-cpp-source-map-v1`, and
`sagan-debug-metadata-v1`. Native check/build/run, source maps, debug metadata,
and launch plans are true; test discovery, attach, and optimized-local
evaluation are false.

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
server contract; they do not make test discovery, build/run LSP requests,
cross-file rename, or DAP available.

