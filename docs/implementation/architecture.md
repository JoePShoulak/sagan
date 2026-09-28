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
- span, diagnostic, generator, and parse-error support retained or adapted
  from Schematic.
- `src/version.hpp` plus generated `obj/version.cpp`: build identity.
- `.github/workflows/coverage.yml` plus `scripts/coverage.sh`: repeatable GCC
  coverage collection and Codecov upload.

## Current data flow

The CLI reads a file, tokenizes it, and either prints the token stream or passes
it to the parser. AST modes then print or render the resulting tree. Lexical and
syntax failures throw `parser::parse_error` and are reported with source
locations.

## Planned components

Semantic passes, runtime support, standard/core libraries, C++ emission, and
native-toolchain invocation are not implemented. The parser and AST are complete
for the current syntax specification and form the input to semantic analysis.
