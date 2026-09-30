---
title: Language-server capabilities
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Language-server capabilities

Sagan does not yet ship a language server. The compiler does expose
`--capabilities-json` using the `sagan.language-service/1` schema and currently
reports strict checking, structured diagnostics, UTF-16 positions, and
cancellation as available. Bounded lexical/syntax recovery, lossless source
retention, versioned document overlays, overlay-first module resolution,
dependency invalidation, cached reanalysis, cancellation, and stale-result
rejection are also available. Strict semantic indexing now supplies opaque
stable symbol IDs, typed kinds/visibility/origin, document-owned declaration
ranges, overloads, generic specializations, conformances, receiver members,
canonical types, and identity-based document/workspace references. Selective
imports and namespace members link to exported identities. The library now has
tested read-only queries for symbols, hover, inferred type, definitions and
type definitions, references, highlights, face implementations, type and call
hierarchies, signatures, completion, source/builtin documentation,
document/workspace symbols, classifications, folding and selection ranges,
import links, inlay hints and position context. These are compiler-library
capabilities, not LSP server features. LSP transport remains unavailable.
The compiler library also provides versioned check and native build/run
operations for documents and projects, generated C++ source maps, debugger
metadata, and a native debug launch plan. These do not constitute a live
debugger or an LSP task server.
This document defines the guarantees
the completed Sagan 1.0 server must satisfy and the capability-discovery shape
clients may rely on. It is not a claim that blocked capabilities work today.

## Baseline guarantees

The completed server will:

- speak Language Server Protocol over stdio without non-protocol stdout;
- identify its compiler, language, service-schema, project-model, and available
  metadata-catalog versions during initialization;
- use UTF-16 LSP positions while retaining exact UTF-8 byte ranges internally;
- analyze unsaved, versioned overlays across multiple open files;
- reject stale edits and suppress results older than the current document;
- report partial, recovered, cancelled, stale, and complete states honestly;
- publish stable coded diagnostics with related locations and fixes;
- negotiate and advertise only implemented capabilities;
- obtain syntax, semantics, formatting, project, build, and documentation data
  exclusively from reusable Sagan compiler libraries; and
- remain deterministic for identical workspace snapshots and configuration.

## Target Sagan 1.0 capabilities

After the roadmap gates pass, the server guarantees synchronization,
diagnostics, hover, definition, type definition, implementations, references,
highlights, signature help, completion, semantic tokens, document/workspace
symbols, folding, selection ranges, import links, inlay hints, rename, type and
call hierarchies, code actions, formatting, and required file notifications.

Individual refactorings are advertised only when their safety proof is
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

The current compiler-library discovery JSON includes
`"sourceEditsSchema":"sagan-source-edits-v1"`. Its C++
`source_edit_capabilities()` action list reports which formatting and
proof-gated edit actions are available and why others are disabled. This is
not an LSP `initialize` response: the language server has not been implemented.
The same JSON reports `sagan-operations-v1`, `sagan-cpp-source-map-v1`, and
`sagan-debug-metadata-v1`. Native check/build/run, source maps, debug metadata,
and launch plans are true; test discovery, attach, and optimized-local
evaluation are false.

Clients must treat missing or false capabilities as unavailable. They must not
fill a missing compiler capability with duplicated language logic.

