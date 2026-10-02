---
title: Architecture
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Architecture
## Current components

- `src/main.cpp`: CLI, token printing, self-tests, file loading, and version output.
- `src/parser/tokens.*`: token identifiers and printable token names.
- `src/parser/lex.*`: Sagan lexical scanning.
- `src/parser/unicode.*` and generated Unicode tables: UTF-8 validation,
  Unicode XID identifiers, emoji sequences, and NFC normalization.
- `src/parser/tokenizer.*`: generator-backed token-stream wrapper.
- `src/parser/parser.*`: recursive-descent parser for the current Sagan grammar.
- `src/parser/ast_node.*`: typed, source-spanned Sagan syntax tree.
- `src/parser/ast_render.*`: text, DOT, SVG, and interactive HTML AST output.
- `src/syntax/*`: lossless token/trivia retention, snapshot-local syntax
  identity, bounded lexical/parser recovery, and partial top-level trees.
- `src/semantic/analyzer.*`: lexical scopes, symbols, name resolution, and a
  printable semantic model.
- `src/semantic/type_checker.*`: scalar inference, compatibility,
  overload/call/return checks, and a printable type model.
- `src/semantic/semantic_error.hpp`: source-located semantic diagnostics.
- `src/codegen/cpp_generator.*`: C++ emission for checked programs, including
  portable identifier encoding, built-in output, and typed
  array/dictionary/index/iteration lowering driven by the checked type model.
- `src/codegen/source_map.*`: generated-to-Sagan source mapping for build errors
  and experimental debugging.
- `src/driver/native_runner.*`: temporary native builds, compiler invocation,
  executable launch, cleanup, and exit-code propagation.
- `src/source/*`: immutable document identity, version, text, byte ranges, and
  UTF-16-compatible line indexing, plus disk/overlay source providers.
- `src/diagnostics/*`: structured diagnostics/results, cancellation, and
  terminal/JSON presentation.
- `src/language_service/*`: reusable strict and recovering document analysis
  and versioned capability discovery; workspace state adds document lifecycle,
  cached analysis, dependency invalidation, cancellation, and stale-result
  rejection. Queries, formatting, safe edits, documentation, native operations,
  test discovery/execution, and the installed-package catalog live here.
- `src/modules/*`: package manifests, module resolution, the local package
  index, and exact lockfile validation for installed dependencies.
- `src/lsp/*`: framed stdio LSP transport over the shared compiler services.
- `src/dap/*`: experimental framed debug adapter using native GDB DAP; it is
  not yet a supported or packaged editor debugger.
- span, diagnostic, generator, and parse-error support retained or adapted
  from Schematic.
- `src/version.hpp` plus generated `obj/version.cpp`: build identity.
- `.github/workflows/coverage.yml` plus `scripts/coverage.sh`: repeatable GCC
  coverage collection and Codecov upload.

## Current data flow

The CLI reads a file and either prints its token stream explicitly or passes it
to the parser. AST modes print or render the resulting tree; semantic and
type modes perform name resolution and type checking. The C++ modes validate an
executable entry point and emit the checked program.
Direct source and package-run modes invoke the native compiler and program.
Lexical, syntax, and semantic failures are reported with source locations.

The compiler objects are archived into `build/lib/libsagan-compiler.a`; the
batch CLI, language server, and experimental debug adapter link the same
compiler-owned implementation. The language server does not reparse Sagan in
JavaScript. The module resolver consumes the shared source-provider interface,
so an unsaved imported file takes precedence over its disk version without
alternate parsing logic. Imported installed packages use manifest aliases and
validated exact `sagan.lock` pins; index lookup is offline.

```text
UTF-8 source -> tokenizer -> parser -> AST -> name analysis -> type checking -> C++ emission
                                  |              |               |              |
                                  +-> renderers   +-> model       +-> model      +-> native runner
```

Codecov measures the front-end, semantic pass, and CLI suite. Coverage includes
lifecycle and malformed-input paths, Unicode and emoji edges, parser and
semantic errors, renderer output, CLI behavior, and defensive invariants.

## Remaining boundaries

The current extension consumes the tested position queries, formatting, safe
edits, LSP transport, and cancellable build/run/test operations. Complete
package-aware completion and auto-import, reliable debugger values and
failure mapping, debugger release packaging, broader platform packaging,
and the math, physics, and rendering libraries remain open work. See the
[extension readiness checklist](../tooling/extension-readiness.md) for the
capability-by-capability boundary.
