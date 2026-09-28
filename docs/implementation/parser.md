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
- Calls with zero or more arguments
- Indexing, member access, safe member access, and chained postfix expressions
- Ruby-style mutating method names such as `normalize!()`
- Ordinary, raw, and multiline string expressions
- Interpolated strings with complete embedded expressions
- Arrays and dictionaries, including empty forms and expression keys
- Vectors and coordinates with a minimum of two elements
- Trailing commas in calls and every collection form
- Spread expressions and dictionary spread entries
- Right-associative exponentiation
- Multiplicative and additive arithmetic
- Non-chainable comparisons and equality expressions
- Logical `and` and `or`
- Conditional expressions
- Right-associative value-producing `:=` assignment
- Statement blocks, including empty and nested blocks
- Ordinary `=` assignment and expression statements inside blocks
- `if`/`else` statements and `else if` chains
- `for`/`in`, `while`, and `until` loops
- Unlabeled `break` and `continue`, restricted to loop bodies
- Bare and value-bearing `return` statements
- Block-bodied `match`/`case`, with an optional final `case else`
- Named block-bodied functions with optional parameter and return annotations
- Declaration-only program roots; executable statements are function-local
- Human-readable AST output and success/error demonstrations
- DOT and standalone SVG AST rendering
- A self-contained HTML view that places source input beside the visual tree

## Inspecting a tree

The quickest visual demonstration is:

```bash
bash scripts/ast_demo.sh
```

It parses the expanded `examples/parser_demo.sagan`, writes
`build/ast-demo.html`, and opens
the page in the default Windows browser. The page embeds its SVG tree and has no
Graphviz or network dependency. Its controls provide zoom in, zoom out, fit,
and 100% views; the mouse wheel zooms around the pointer and dragging pans the
tree. Pass `--no-open` when only the generated file is wanted.

Text, DOT, SVG, and HTML are also available from `bin/sagan` through `--ast`,
`--ast-dot`, `--ast-svg`, and `--ast-html` respectively. The renderer currently
supports every AST node the parser can produce; new node kinds must be added to
the renderer as their parser slices land.

The next slices are exception statements, expression-bodied functions, and type
declarations.
Error recovery beyond the first syntax error also remains future parser work.
