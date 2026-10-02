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

**Status: compiler-library exit reached; protocol transport remains Phase 8.**
`document_queries` resolves exact identifier occurrences from recovering syntax
tokens and the semantic index. Its tested read-only surface includes symbol,
hover, documentation, resolved type, definition/type definition, references,
highlights, face implementations and type hierarchy, call hierarchy, signature
help, document/workspace symbols, semantic classifications, folding, selection
ranges, import links, inlay hints, and position context (scope, containing
declaration, expression and type). Selection expansion includes typed
expressions on strict snapshots and a recovering syntax fallback. Signature
help includes parameter names, selected argument, overload alternatives and
source documentation, with conservative recovery for an unambiguous incomplete
local call. Completion derives lexical declarations, inferred receiver members,
face defaults, namespace exports, importable workspace modules/exports, missing
import edits, and statement-position keywords from compiler metadata. It
respects shadowing and known visibility. The versioned documentation model
reads source and built-in symbol comments, including parameter/return/generic
tags, examples, deprecation and availability. All answers carry the document
version and decline stale snapshots; uncertain resolutions return no target.

Limits are intentional: recovering signature help declines ambiguous overloads;
completion is a conservative candidate list, not a promise that every incomplete
syntax context has a suggestion; there is no independent installed-package
registry or standard-library catalog beyond the declarations currently known to
the compiler. None of these library queries are advertised as LSP capabilities
until Phase 8 implements and tests transport.

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

**Status: complete for the safety-gated Phase 6 scope.** The strict-parse,
token-preserving two-space formatter handles document/range/on-type requests,
tested token-gap rules, and trailing horizontal whitespace, with golden and
idempotence tests. It deliberately preserves line breaks and ambiguous gaps.
Versioned multi-document edit validation rejects stale, overlapping, or invalid
UTF boundaries. Identity-based local rename requires semantic recheck and
reference-rebinding proof. Comment-free top-of-file import blocks can be
organized; exact public workspace symbols can be imported with collision and
binding checks. Compiler-issued diagnostic fixes are version-gated and
rechecked. Capability discovery names available actions and explicit refusals.
Public/cross-module rename, unused-import removal, face synthesis, and
extraction remain disabled until their safety analyses exist. See
[formatting and source edits](formatting-and-edits.md).

- Implement the deterministic lossless formatter with document/range/on-type
  APIs, golden tests, and idempotence tests.
- Implement versioned workspace edits and proof gates.
- Add safe rename, import organization/add/remove, required-face-member
  generation, diagnostic fixes, extract-variable, and extract-function only as
  their safety analyses become complete.

**Exit met:** no edit is offered through textual name matching; conflicts or
uncertain preservation cause a structured refusal rather than a risky edit.
The explicitly disabled transformations are follow-on work, not advertised
Phase 6 capabilities.

## Phase 7 — operations and debugger prerequisites

**Status: compiler-library exit reached; debugger and editor transport remain
separate work.** Versioned document checking and native document/project build
and run operations return ordered progress, captured stdout/stderr, structured
diagnostics, exit status, retained artifacts, and cancellation state. Native
operations support unsaved imported-module overlays and reject results made
stale by dependency changes. Debug and optimized generated C++ carry source
maps, linked-module origin paths, candidate breakpoint ranges, generated
function identities, lexical scopes and lifetimes, local value representation
metadata, conservative expression-evaluation hooks, and a launch plan. Known
generated-toolchain and uncaught Sagan runtime errors map back to Sagan source.
Run `make operations-demo` to see Sagan input and both native results.

- Refactor check/build/run into cancellable structured operations with progress,
  streams, diagnostics, exit states, and generated artifacts.
- Add source maps, valid breakpoint locations, generated function/symbol IDs,
  stack/source mapping, scopes/lifetimes, runtime value representations,
  exception mapping, and expression-evaluation hooks.
- Add test operations only after an authoritative Sagan test model exists.

**Exit met:** source/error mapping and debugger metadata round-trip in focused
debug and optimized builds, including an imported overlay. A later increment
added asynchronous check/build/run job handles and cancellable LSP operation
requests. Explicit `test "name" { ... }` declarations and versioned
document/project discovery now exist in both the library and a custom LSP
request. A later increment added selected document and linked-project test
execution, including package roots and imported-module overlays. A later
increment added an experimental framed `sagan-dap` process backed by GDB's
native DAP. It has tested launch, Sagan-mapped breakpoint/stack probes, and
source-level stepping, but reliable Sagan values, exception mapping, complete
cleanup coverage, and release packaging are still missing. Attach and
optimized-local evaluation remain unsupported. Debugger capabilities stay
false until their individual end-to-end gates pass.

## Phase 8 — Language Server Protocol transport

**Status: implementation and focused protocol fixtures in place; Phase 9 is
the workload/release gate.** `bin/sagan-lsp` speaks JSON-RPC/LSP over stdio,
uses versioned unsaved overlays and UTF-16 positions, handles workspace
folders and file notifications, publishes diagnostics, and translates the
completed compiler queries and safe edit actions. Requests can be cancelled
with `$/cancelRequest`; protocol stdout contains frames only. The server now
provides custom check/build/run operation, test-discovery, and selected
document/project test execution requests, but not DAP. See the
[language-server guide](language-server.md) for the exact advertised surface
and limitations. `make lsp-demo` shows source, hover, navigation, and an edit
diagnostic.

- Add a dedicated stdio JSON-RPC/LSP executable.
- Implement initialization and capability negotiation, workspace lifecycle,
  incremental document synchronization, diagnostics, all completed read-only
  queries, semantic tokens, links, hints, hierarchy, code actions, rename,
  formatting, and file notifications.
- Keep transport translation-only. Log to stderr or a configured file, never
  protocol stdout.

**Exit met:** lifecycle, request/response, overlay, Unicode, cancellation, error,
hierarchy, edit, and framing fixtures pass against the shared server library
and executable; advertised capabilities match the
[capability document](language-server-capabilities.md).

## Phase 9 — reliability and release gate

**Status: complete for the advertised Sagan 1.0 editor contract.** The library
and executable fixtures exercise rapid edits, cancellation, stale versions,
malformed source and JSON, two package roots, workspace removal and return,
repeated queries, cleanup, UTF-16 cursor/edit boundaries, diagnostic recovery,
oversized and truncated frames, and stderr-only logging. The transport has a
16 MiB frame limit, a 256-message input queue, and no persistent semantic-query
cache. The repeatable Windows benchmark uses a representative showcase file
and package graphs; the [language-server guide](language-server.md#reliability-gate)
publishes conservative regression thresholds. `bash scripts/test.sh` and
`bash scripts/docs.sh check` pass on the supported Windows configuration.

- Add rapid-edit, cancellation, stale-version, malformed-input, multi-workspace,
  cleanup, cache-budget, and repeated-request stress tests.
- Benchmark representative files and package graphs and publish regression
  thresholds.
- Fuzz source/position/edit boundaries and enforce bounded diagnostic recovery.
- Run the complete compiler, CLI, documentation, installer, library, operation,
  and LSP suites on the supported Windows configuration.

**Exit:** the supported-capabilities document is evidence-backed and the VS Code
extension can enable each feature solely through negotiated server capabilities.

**Exit met:** the extension can begin integration against `bin/sagan-lsp` and
the advertised `initialize` capabilities. Deliberately unadvertised features
remain blocked as listed in the [readiness checklist](extension-readiness.md).

