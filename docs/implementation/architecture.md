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
- `src/semantic/analyzer.*`: lexical scopes, symbols, name resolution, and a
  printable semantic model.
- `src/semantic/type_checker.*`: scalar inference, compatibility,
  overload/call/return checks, and a printable type model.
- `src/semantic/semantic_error.hpp`: source-located semantic diagnostics.
- `src/codegen/cpp_generator.*`: C++ emission for the initial executable
  subset, including portable identifier encoding and built-in output.
- span, diagnostic, generator, and parse-error support retained or adapted
  from Schematic.
- `src/version.hpp` plus generated `obj/version.cpp`: build identity.
- `.github/workflows/coverage.yml` plus `scripts/coverage.sh`: repeatable GCC
  coverage collection and Codecov upload.

## Current data flow

The CLI reads a file, tokenizes it, and either prints the token stream or passes
it to the parser. AST modes print or render the resulting tree; semantic and
type modes perform name resolution and optionally initial type checking. The
C++ mode validates an executable entry point and emits the supported subset.
Lexical, syntax, and semantic failures are reported with source locations.

```text
UTF-8 source -> tokenizer -> parser -> AST -> name analysis -> type checking -> C++ emission
                                  |              |               |              |
                                  +-> renderers   +-> model       +-> model      +-> native demo
```

Codecov measures the front-end, semantic pass, and CLI suite. Coverage includes
lifecycle and malformed-input paths, Unicode and emoji edges, parser and
semantic errors, renderer output, CLI behavior, and defensive invariants.

## Planned components

Generic annotations, member/object typing, broader lowering, runtime support,
standard/core libraries, and native-toolchain invocation by the compiler itself
are not implemented.
