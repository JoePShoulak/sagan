---
title: Language-service implementation roadmap
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Language-service implementation roadmap

The phases are dependency-ordered. Each phase ends with focused library tests
and keeps strict batch compilation independently usable.

## Phase 0 — contracts and baseline

**Status: implemented foundation.** The compiler now produces a reusable static
archive, the batch CLI links it, focused library tests run independently, and
the audit/contracts/capability baseline is checked into the documentation.

- Preserve the repository audit, architecture contracts, capability vocabulary,
  and extension readiness matrix.
- Capture batch behavior and performance baselines for representative single-
  and multi-module programs.
- Split the monolithic build into reusable compiler-library targets, a batch CLI
  target, focused test targets, and later a language-server target.

**Exit:** existing tests pass through library entry points; no editor protocol
code owns compiler logic.

## Phase 1 — source identity and structured diagnostics

**Status: foundation implemented; diagnostic migration remains.** URI/path and
versioned snapshots, byte/UTF-16 conversion, result states, cancellation,
structured diagnostic containers, terminal/JSON rendering, strict document
checking, and capability discovery are implemented. Existing module, project,
build, entry-point, and runtime paths still need to migrate from textual
exceptions to distinct structured codes.

- Implement URI/path identity, immutable snapshots, document versions, UTF-8
  byte ranges, UTF-16 positions, and a tested line index.
- Add structured result states, cancellation tokens, diagnostics, related
  locations, notes, and versioned fix edits.
- Give every existing lexical, syntax, semantic, type, module, project, build,
  and entry-point error a stable code.
- Make terminal output a diagnostic renderer and add JSON output for tests and
  automation.

**Exit:** batch behavior remains unchanged; position round trips and structured
diagnostic snapshots pass Unicode/emoji/CRLF tests.

## Phase 2 — lossless and recovering syntax

**Status: foundation implemented.** A reusable lossless syntax layer preserves
ordinary comments, documentation-comment tokens, whitespace, newlines, exact
source text, and snapshot-local token/node identities. Recovering analysis
collects bounded lexical and syntax diagnostics, partitions malformed top-level
input at declaration boundaries, and retains successfully parsed declarations
in a partial AST. Arbitrary cursor prefixes are covered by focused tests. Fine-
grained expression nodes, inserted missing-token nodes, and deeper nested-block
recovery remain future refinements for position queries.

- Preserve ordinary comments, documentation comments, whitespace, newlines, and
  required delimiter trivia in a lossless token/syntax representation.
- Add stable snapshot-local node IDs and precise name/type/operator ranges.
- Add lexer and parser recovery, missing/error nodes, synchronization points,
  multiple diagnostics, cascade suppression, and incomplete-source fixtures.
- Keep strict compilation rejecting recovered or malformed syntax.

**Exit:** arbitrary and partially typed UTF-8 input cannot crash; a useful
partial tree and bounded diagnostic set are produced where practical.

## Phase 3 — document store and workspace overlays

**Status: foundation implemented.** Thread-safe disk and overlay providers now
support open/change/save/close, monotonic versions, validated UTF-8 byte edits,
multiple simultaneous buffers, and overlay-first module resolution. Workspace
state caches current results, tracks dependencies, invalidates transitive
dependents conservatively on source changes, cancels superseded requests, and
turns obsolete completions into `stale` results. Project manifests remain disk
configuration; every Sagan module source read uses the provider. Export-surface
fingerprints and finer local-only invalidation are deferred optimizations that
do not change the service contract.

- Introduce disk and overlay `source_provider` implementations.
- Implement open/change/save/close with monotonic versions and validated edits.
- Refactor resolver/project discovery to read all source through the provider.
- Add multiple open documents, dependency graphs, exported-surface fingerprints,
  invalidation, conservative caching, cancellation generations, and stale-result
  rejection.

**Exit:** unsaved changes affect importing modules, old analysis cannot publish,
and overlay/module invalidation tests pass.

## Phase 4 — semantic index and stable identities

**Status: complete.** Analyzer output
now uses typed symbol kinds, visibility, origin, scope ownership, and opaque
deterministic `sagan-symbol-v1` identities instead of public string kinds.
Strict documents can be converted into a versioned, document-owned semantic
index with identity-based definition/reference lookup, shadowed-local and
overload separation, stable built-in identities, documentation, canonical
types, receiver/member candidates, explicit and inferred specializations, and
face conformances. The workspace index links selective imports and namespace
members to exported identities. Recovered trees retain valid declarations
without weakening strict compilation.

- Replace stringly public symbol records with typed symbol, node, scope, type,
  overload, specialization, conformance, visibility, and origin records.
- Index declarations, definitions, implementations, references, shadowing,
  built-ins, generated declarations, and documentation by document/module.
- Preserve existing analyzer/type-checker rules while allowing partial trees to
  yield incomplete semantic results without cascades.

**Exit:** identity-based definition/reference tests pass across modules,
overloads, generics, private members, shadowed locals, faces, and enum cases.

The exit behavior is exercised by the semantic-index coverage in `make test`; Phase 5 now owns
the position-based query and presentation APIs built on these records.

## Phase 5 — read-only language features

**Status: in progress.** A reusable `document_queries` API now resolves exact
identifier occurrences from recovering syntax tokens and the semantic index.
Focused tests cover UTF-16 and byte positions, version mismatch, hover source
documentation, resolved types, local and imported definitions, references,
highlights, face implementations, hierarchical document symbols, semantic
classifications, workspace symbol search, block/comment/string folding,
resolved import links, and lexical-scope completion. Completion is deliberately
limited to visible lexical declarations and built-ins; member/import context,
keyword validity, named arguments, and the standard-library catalog remain
unimplemented. Resolved-call signature information now reports the selected
argument and compiler-resolved parameter/result types for valid strict source;
parameter names, overload alternatives, documentation, and incomplete-call
recovery remain to do. Token/trivia, balanced-delimiter, declaration, and
document selection ranges are available, though expression-level AST expansion
remains to do. The API also lacks type definitions and inlay hints. None of
these library queries are advertised as editor
capabilities while LSP transport is absent.

- Implement symbol-at-position, hover, resolved types, definition, type
  definition, implementations, references, highlights, and signature help.
- Implement compiler-derived completions, semantic classifications, document and
  workspace symbols, import links, folding, selection ranges, type hierarchy,
  call hierarchy, and inlay hints.
- Add a compiler-readable documentation/availability catalog and standard-
  library metadata hook. Advertise standard-library completion only when a real
  catalog is installed.

**Exit:** each query has focused source/library tests and deterministic ordering.

## Phase 6 — formatter and safe source edits

- Implement the deterministic lossless formatter with document/range/on-type
  APIs, golden tests, and idempotence tests.
- Implement versioned workspace edits and proof gates.
- Add safe rename, import organization/add/remove, required-face-member
  generation, diagnostic fixes, extract-variable, and extract-function only as
  their safety analyses become complete.

**Exit:** no edit is offered through textual name matching; conflicts or
uncertain preservation cause a structured refusal rather than a risky edit.

## Phase 7 — operations and debugger prerequisites

- Refactor check/build/run into cancellable structured operations with progress,
  streams, diagnostics, exit states, and generated artifacts.
- Add source maps, valid breakpoint locations, generated function/symbol IDs,
  stack/source mapping, scopes/lifetimes, runtime value representations,
  exception mapping, and expression-evaluation hooks.
- Add test operations only after an authoritative Sagan test model exists.

**Exit:** generated/toolchain/runtime failures map to Sagan source and debugger
metadata round-trips in debug and optimized test builds.

## Phase 8 — Language Server Protocol transport

- Add a dedicated stdio JSON-RPC/LSP executable.
- Implement initialization and capability negotiation, workspace lifecycle,
  incremental document synchronization, diagnostics, all completed read-only
  queries, semantic tokens, links, hints, hierarchy, code actions, rename,
  formatting, and file notifications.
- Keep transport translation-only. Log to stderr or a configured file, never
  protocol stdout.

**Exit:** lifecycle and request/response fixtures pass against the executable;
advertised capabilities exactly match the service capability document.

## Phase 9 — reliability and release gate

- Add rapid-edit, cancellation, stale-version, malformed-input, multi-workspace,
  cleanup, cache-budget, and repeated-request stress tests.
- Benchmark representative files and package graphs and publish regression
  thresholds.
- Fuzz source/position/edit boundaries and enforce bounded diagnostic recovery.
- Run the complete compiler, CLI, documentation, installer, library, operation,
  and LSP suites on the supported Windows configuration.

**Exit:** the supported-capabilities document is evidence-backed and the VS Code
extension can enable each feature solely through negotiated server capabilities.

