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
- `src/parser/unicode.*` and generated Unicode tables: in-progress UTF-8,
  XID, emoji-sequence, and NFC support.
- `src/parser/tokenizer.*`: generator-backed token-stream wrapper.
- span, diagnostic, generator, parse-error, parser-support, and AST files:
  foundations retained or adapted from Schematic.
- `src/version.hpp` plus generated `obj/version.cpp`: build identity.

## Current data flow

The CLI reads a file into memory, constructs `programText`, repeatedly asks the
tokenizer for tokens, and prints each token. Lexical failures throw
`parser::parse_error` and are rendered with source context.

## Planned components

A Sagan parser and AST, semantic passes, runtime support, standard/core
libraries, C++ emission, and native-toolchain invocation are not implemented.
Existing generic AST/parser-support files must not be mistaken for a completed
Sagan parser.
