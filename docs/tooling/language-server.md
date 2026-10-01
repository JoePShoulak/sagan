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
to type source into. The VS Code extension is a separate project and is not
changed by this server work.

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

The server currently advertises incremental synchronization, diagnostics,
hover, definition/type definition, implementations, references and highlights,
signature help, completion, document/workspace symbols, full semantic tokens,
folding and selection ranges, import links, inlay hints, type and call
hierarchies, local rename, safe quick fixes/import organization, and
document/range/on-type formatting. `initialize` returns the normal LSP
capabilities plus Sagan's versioned compiler capability schema under
`experimental`. The transport schema is `sagan-lsp/1`.

Limits matter: rename is restricted to proven local bindings. Formatter edits
require strict source and preserve existing line breaks. Completion is
conservative and has no independent package registry. Module diagnostics may
have a generic location when an error originates in another file and the
underlying compiler error lacks source provenance. Test discovery, build/run
task requests, debugger attach, and Debug Adapter Protocol are not advertised.
Cancelled requests return LSP `RequestCancelled`; obsolete diagnostics are
not published for a newer open-document version.

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
