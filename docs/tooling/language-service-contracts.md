---
title: Language-service contracts
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Language-service contracts

These C++ contracts guide editor-tooling implementation. The source,
diagnostic, recovering-syntax, workspace, and semantic-index foundations are
implemented; later sections also describe APIs that remain on the roadmap.
Names may receive mechanical refinement, but their ownership boundaries and
invariants are the editor-integration contract.

## Library boundaries

```text
sagan-source       immutable snapshots, URI/path identity, line index, edits
sagan-diagnostics  structured diagnostics, fixes, result states, rendering
sagan-syntax       lossless tokens/trivia, strict and recovering parse trees
sagan-project      manifests, overlays, filesystem, module graph, invalidation
sagan-semantics    symbols, types, references, documentation, classifications
sagan-format       deterministic formatting and syntax-preserving rewrites
sagan-service      workspaces, compilation snapshots, queries, refactorings
sagan-operations   check/build/run/test contracts and progress
sagan-debug        generated-code maps and runtime/debug metadata
sagan-lsp          JSON-RPC framing and LSP translation only
sagan              existing batch CLI, implemented as a library client
```

The libraries may initially remain in one repository and build system. The
boundary is about dependencies and ownership, not forcing dynamic libraries or
premature ABI stability. The public stability promise is a versioned source API
inside Sagan plus the wire protocol exposed by `sagan lsp` or `sagan-lsp`.

## Source model

```cpp
namespace sagan::source {
  using document_version = std::int64_t;
  using byte_offset = std::uint32_t;

  struct document_uri { std::string value; };
  struct document_id { std::uint64_t value; };
  struct utf16_position { std::uint32_t line; std::uint32_t character; };
  struct byte_range { byte_offset begin; byte_offset end; };
  struct source_range { document_id document; byte_range bytes; };

  struct text_edit { source_range range; std::string replacement_utf8; };
  class line_index;
  class document_snapshot;
}
```

`document_snapshot` is immutable and owns normalized URI, canonical path when a
file URI has one, UTF-8 text, document version, and a line index. Conversion
between byte offsets and UTF-16 positions is snapshot-relative and returns a
structured error for invalid UTF-8 boundaries or out-of-range positions.
Internal compiler ranges use bytes; protocol conversion happens only at the
boundary. A URI need not have a filesystem path.

## Results, cancellation, and diagnostics

```cpp
enum class result_state { complete, recovered, incomplete, cancelled, stale };
enum class diagnostic_severity { error, warning, information, hint };
enum class diagnostic_phase { lexical, syntax, semantic, type, module,
                              project, build, entry_point, runtime };

struct diagnostic {
  std::string code;                 // e.g. SAG-SYN-0001
  diagnostic_severity severity;
  diagnostic_phase phase;
  source::source_range primary;
  std::string message;
  std::vector<related_location> related;
  std::vector<std::string> notes;
  std::vector<fix> fixes;
};

template<class T> struct analysis_result {
  result_state state;
  std::optional<T> value;
  std::vector<diagnostic> diagnostics;
  source::document_version analyzed_version;
};
```

Diagnostic codes and meanings are stable within a protocol major version.
Terminal diagnostics are a renderer over this model. Cancellation is
cooperative through a cheap `cancellation_token` checked at phase boundaries
and within potentially unbounded loops. Cancellation and stale results are not
reported as language errors.

## Documents, filesystems, and workspaces

```cpp
class source_provider {
public:
  virtual auto read(const source::document_uri&) const
    -> result<source::document_snapshot> = 0;
  virtual auto canonicalize(const source::document_uri&) const
    -> result<canonical_source> = 0;
};

class document_store final : public source_provider {
public:
  auto open(uri, version, utf8_text) -> result<void>;
  auto change(uri, expected_version, new_version, edits) -> result<void>;
  auto save(uri, version, optional_text) -> result<void>;
  auto close(uri) -> result<void>;
};
```

The store layers versioned overlays over a disk provider. Resolver and manifest
logic depend on `source_provider`, so imports see an overlay before disk. A
`workspace` owns project discovery, document state, dependency graph, semantic
index, compilation snapshots, cache budgets, and cancellation generations.
Changing exported declarations invalidates dependent modules; local-only
changes initially permit conservative module reanalysis and may later use finer
invalidation without changing the API.

## Syntax and semantic snapshots

Strict and editor parsing share grammar code:

```cpp
struct syntax_options { bool recover; bool retain_trivia; };
auto lex(document_snapshot, syntax_options, cancellation_token)
  -> analysis_result<token_stream>;
auto parse(token_stream, syntax_options, cancellation_token)
  -> analysis_result<syntax_tree>;
```

Tokens, trivia, and syntax/AST nodes receive stable IDs within one immutable
compilation snapshot. Missing and error nodes represent recovery and cannot
pass strict compilation. Semantic snapshots associate node IDs with symbol IDs,
resolved types, scopes, references, overload sets, conformances, documentation,
and classifications.

`symbol_id` is opaque and typed. Its serialized form includes schema version,
package/module identity, declaration kind, and a stable declaration key; it is
not a pointer, display name, or source-vector index. Local identities are stable
for the lifetime of a compatible document snapshot. Cross-snapshot rebinding is
explicit and may fail after an incompatible edit.

The implemented foundation serializes identities as `sagan-symbol-v1:<hex>`
and exposes typed symbol kind, visibility, origin, declaring scope, document,
declaration range, and identity-based references. Built-ins use a canonical
`sagan/core` identity namespace, so their IDs do not vary by source document.
Source declaration keys currently incorporate the package/module identity and
stable lexical-scope path. Workspace indexes link exported declarations to
selective-import bindings and imported-namespace member uses. Semantic records
also expose overload candidates, receiver-member candidates, explicit and
inferred generic specializations, face conformances, declaration documentation,
canonical type identities, and typed source ranges. Recovered documents may
produce an explicitly recovered partial index; strict compilation remains the
authority for executable programs. Explicit cross-snapshot rebinding remains a
future source-edit concern.

## Language queries

The concrete query layer is `src/language_service/queries.hpp`.
`document_queries` takes one immutable document snapshot and matching semantic
index, with optional workspace, semantic-model, and resolved-type indexes. It
returns structured byte-range results for symbols, definitions, type
definitions, references, highlights, implementations, type/call hierarchies,
resolved type, hover, documentation, signature help, completion, document
symbols, classifications, folding, selection, import links, inlay hints, and
position context. Symbol selection also accepts a UTF-16 position. A separate
workspace query searches indexed declarations. Completion offers lexical
symbols, built-ins, resolved members, face defaults, workspace exports,
missing-import edits, module/namespace members and conservative contextual
keywords. It does not invent external package APIs or blindly offer inaccessible
members. Strict calls expose parameter/result types, names, documentation,
overload alternatives and the active argument. A single unambiguous local
incomplete call can provide recovered signature help; uncertain cases decline.
`selection_ranges` returns nested, innermost-first byte ranges from tokens or
trivia through typed expressions where available, balanced delimiters,
declarations and the document. Invalid positions are `incomplete`, and a
version mismatch is `stale`.
A version or document-identity mismatch returns `stale`;
invalid UTF-16 boundaries return `incomplete`. Its selection ranges come from
recovering syntax tokens, so names in comments or strings do not masquerade as
declarations. These library APIs are not yet advertised as LSP features.

The following is a conceptual API sketch. Concrete names and result types are
in `queries.hpp`; the shared compiler layer returns source data rather than LSP
objects:

```cpp
auto symbol_at(position) -> query_result<symbol>;
auto definitions(symbol_id) -> query_result<std::vector<location>>;
auto type_definitions(symbol_id) -> query_result<std::vector<location>>;
auto implementations(symbol_id) -> query_result<std::vector<location>>;
auto references(symbol_id, reference_options) -> query_result<reference_set>;
auto resolved_type(node_id) -> query_result<type_info>;
auto hover(position) -> query_result<hover_info>;
auto signatures(position) -> query_result<signature_set>;
auto completions(position, completion_context) -> query_result<completion_list>;
auto classifications(range) -> query_result<std::vector<classification>>;
auto document_symbols(document_id) -> query_result<symbol_tree>;
auto workspace_symbols(query) -> query_result<std::vector<symbol_match>>;
auto folding_ranges(document_id) -> query_result<std::vector<source_range>>;
auto selection_ranges(positions) -> query_result<std::vector<selection_chain>>;
```

`completion_item` carries symbol identity/kind, replacement range, insertion
text, detail, documentation, source module, filter/sort text, deprecation and
additional-import edits. Keyword identities are namespaced synthetic IDs.
Built-in candidates come from compiler declarations; other standard-library
and external-package candidates require real metadata, not editor-maintained
lists. The server must not advertise completion until its protocol and
lifecycle tests pass.

## Structured edits and formatting

```cpp
struct versioned_document_edits {
  document_uri uri;
  document_version expected_version;
  std::vector<text_edit> edits;
};

struct workspace_edit {
  std::vector<versioned_document_edits> documents;
  std::vector<file_operation> files;
};
```

Edits are sorted, deterministic, non-overlapping, previewable, and validated
against exact document versions. Rename and refactoring operate on symbol IDs,
then rerun conflict, visibility, and type checks before returning an edit.

Formatting is a reusable library operation with whole-document, range, and
on-type entry points. It consumes a lossless syntax tree, preserves comments,
is deterministic and idempotent, and returns structured edits rather than
mutating files. Recovery-safe formatting may decline a region when preservation
cannot be proven.

## Documentation catalog

Source and built-in declarations share versioned `documentation_entry` metadata:
summary, detail, parameters, return value, generic parameters, examples,
deprecation, availability, declaring module, and optional source range. The
current `sagan-documentation-v1` schema is discoverable through
`--capabilities-json`. Compiler declarations are the sole built-in catalog;
there is no separately installed standard-library or package catalog yet.
Editors must not carry a parallel list.

## Operations and debugger metadata

Check, build, run, and eventually test operations return an operation handle,
progress events, ordered stdout/stderr events, structured diagnostics, exit
status, cancellation outcome, and produced artifacts. Test discovery remains
unsupported until the language supplies an authoritative test model.

Code generation emits a versioned source map connecting generated ranges and
function identities to Sagan source ranges and symbols. Debug metadata records
valid breakpoints, lexical scopes/lifetimes, variable/value representations,
generated function identities, exception mappings, and launch/attach support.
These contracts do not themselves implement a Debug Adapter Protocol server.

## Protocol identity and discovery

The initial service schema is `sagan.language-service/1`. Initialization
returns compiler version, language version, schema version, position encodings,
workspace/project model version, standard-library catalog version when present,
and granular capabilities. JSON diagnostic/testing output uses the same schema.
The LSP server negotiates UTF-16 and translates these records without embedding
language behavior.

