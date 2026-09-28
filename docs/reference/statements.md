---
title: Statements
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Statements
**Settled design:** ordinary statements are newline-terminated rather than
semicolon-terminated. The tokenizer emits logical newline tokens while
suppressing continuation newlines in known contexts.

Intended statements include declarations, assignments, expression statements,
`if`/`else`, `match`/`case`, `for`/`in`, `while`, `until`,
`break`, `continue`, `return`, `yield`, and exception constructs.

**Implemented in the parser:** newline-separated blocks, `let` declarations,
ordinary `=` assignment statements, expression statements inside blocks,
`if`/`else` including `else if` chains, `for name in expression`, `while`, and
`until` loops, unlabeled `break` and `continue`, and bare or value-bearing
`return`. Empty blocks are valid. `break` and `continue` outside a loop are
syntax errors. A block brace is recognized from its statement position, while
`{...}` in an expression remains a dictionary literal.

At the program root, the parser accepts declarations only. Executable control
flow, expression statements, and assignment statements belong inside function
bodies. This preserves Sagan's declaration-only module scope while entry-point
semantics remain under design.

`unless` is reserved for exception handling in Sagan; it is not an inverse
conditional spelling.

**Open questions:** iterable protocol semantics, match semantics, yield
restrictions, unreachable-code rules, entry-point selection, and
parser-informed newline handling in the remaining statement forms.
