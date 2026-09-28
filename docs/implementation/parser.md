---
title: Parser
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Parser
**Status: in progress.**

The parser consumes the token stream and constructs typed, ownership-safe Sagan
syntax trees with source-spanned diagnostics.

## Implemented slices

- Programs and newline-separated declarations
- `let` declarations with optional simple type annotations and initializers
- Identifier, numeric, Boolean, and grouped primary expressions
- Prefix and postfix operators
- Right-associative exponentiation
- Multiplicative and additive arithmetic
- Non-chainable comparisons and equality expressions
- Logical `and` and `or`
- Conditional expressions
- Right-associative value-producing `:=` assignment
- Human-readable AST output and success/error demonstrations

The next slices are calls, indexing, member access, strings and interpolation,
collection literals, statements and blocks, functions, and type declarations.
Error recovery beyond the first syntax error also remains future parser work.
