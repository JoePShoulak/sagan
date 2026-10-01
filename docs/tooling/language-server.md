---
title: Sagan language server
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Sagan language server

`bin/sagan-lsp` is Sagan's editor-facing process. An editor starts it and sends
Language Server Protocol messages over standard input. Standard output is
reserved for framed protocol replies; it is not a terminal command for people
to type source into. The VS Code extension connects to this server without
reimplementing Sagan's parser or type checker.

From the repository, run `make lsp-demo` to see a small Sagan program, its
hover/navigation results, and the diagnostic after an unsaved edit. Run
`bash scripts/test.sh` for the full compiler and language-server test suite.

The server uses Sagan's compiler library for parsing, module resolution,
semantic queries, formatting, and proof-gated edits. Editors should not
duplicate those rules. The protocol uses UTF-16 line/character positions and
preserves the exact URI an editor opened; the compiler retains byte offsets
and a canonical filesystem path internally. Multiple unsaved modules resolve
through in-memory overlays before disk files. An edit to one open module
refreshes diagnostics for all open files, including importers.

The server accepts the current core syntax in unsaved buffers, including
`let i, a, b = 0, 0, 1`, simultaneous `a, b = b, a + b`, same-line
`while` bodies, postfix `i++`, integer `.times` arrays, exponentiation with a
floating-point base and integer exponent (`PHI ^ n`), and explicit
`Int.round(value)` conversion. Unannotated mutable integer bindings infer
`Int64`, including grouped bindings. Diagnostics and inferred
types come from the same compiler checks as the CLI. Member completion offers
`times` on integer receivers. Hover, semantic classification, the array result
type, and runtime-error documentation use the compiler's built-in member
metadata. It is a property, so the insertion is `.times`, not `.times()`.
Completion also offers `Int.round(` on the built-in `Int` type, with hover
and signature documentation for its tie rule and invalid-conversion error.

The root file can contain executable top-level statements and bindings, with
`exit(code)` for an explicit process status. Imported modules remain
declaration-only; `main` is an ordinary function name.

The server currently advertises incremental synchronization, diagnostics,
hover, definition/type definition, implementations, references and highlights,
signature help, completion, document/workspace symbols, full semantic tokens,
folding and selection ranges, import links, inlay hints, type and call
hierarchies, local rename, safe quick fixes/import organization, and
document/range/on-type formatting. `initialize` returns the normal LSP
capabilities plus Sagan's versioned compiler capability schema under
`experimental`. The transport schema is `sagan-lsp/1`.

Limits matter: rename is restricted to proven local bindings. Formatter edits
may handle locally proven complete lines of recovered source and preserve
existing line breaks. Completion is
conservative and has no independent package registry. Module diagnostics may
have a generic location when an error originates in another file and the
underlying compiler error lacks source provenance. Test discovery, debugger
attach, and Debug Adapter Protocol are not advertised. A custom
`sagan/operation` request supports check/build/run with `sagan/operationEvent`
progress, cancellation, version checks, and structured results; the extension
still needs to wire commands and tasks to that contract. Cancelled ordinary
queries return LSP `RequestCancelled`; cancelled operations return a structured
`cancelled` state. Obsolete diagnostics are
not published for a newer open-document version. The custom
`sagan/tests/discover` request accepts `textDocument.uri`, optional
`textDocument.version`, `scope` (`document` or `project`), and optional
`projectUri`. It returns `sagan-tests-v1`, state, source version, and stable
test IDs with UTF-16 name ranges and structured discovery diagnostics.
`sagan/tests/run` accepts the same document URI and optional version,
`scope` (`document` or `project`), optional `projectUri`, and optional
`testIds` (empty means all tests in that scope). A project URI may identify
a package root, manifest, or entry module. It returns `sagan-tests-v1`,
stable test IDs, package/module/suite hierarchy, UTF-16 source ranges,
queued/running/passed/failed/errored/skipped/cancelled case states, per-test
duration, exit status, captured output, and diagnostics. `sagan/testEvent`
notifications precede the response; project events are buffered until the
project snapshot is verified, so stale runs publish no project events or
case results. Tests run in resolved-module order, independently, with all
queued events first and then each case's running/output/final events. Cancel
using standard `$/cancelRequest`.

`sagan/packages/query` reads a compiler-owned local index. An optional
`indexUri` points to its file; otherwise the server uses `SAGAN_PACKAGE_INDEX`.
An optional `prefix` filters package names. The response uses
`sagan-package-index-v1` with `ready`, `unavailable`, or `invalid` state and
returns version, compiler requirement, compatibility, installed/available
state, and manifest URI where installed. It does not yet resolve dependencies,
provide export signatures, or enable package completion.

## Reliability gate

Run `bash scripts/lsp_reliability_test.sh` after building `bin/sagan-lsp`.
The full `make test` target runs it too. The gate uses the real stdio process,
two package roots, a 159-line showcase file, 20 queued edits, repeated semantic
requests, a stale edit, malformed JSON and source, workspace-folder removal and
restoration, and document cleanup. A separate library fixture probes cursor
boundaries, repeated open/close cycles, cancellation, and framing limits.

The generous Windows CI regression limits are 15 seconds per request,
30 seconds for ten showcase symbol requests, and 60 seconds for the 20-edit
burst. The test prints observed timings, so a regression can be investigated
before raising a limit. A protocol frame over 16 MiB is rejected before body
allocation; the input queue is bounded to 256 messages. The language-server
transport keeps no persistent semantic-query cache, and closing a document
removes its in-memory overlay.

To inspect protocol activity, set `SAGAN_LSP_LOG=stderr` when launching the
server. Logs include only sanitized method names, never source text or
protocol payloads. Standard output always remains reserved for LSP frames.
