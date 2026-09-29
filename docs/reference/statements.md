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
ordinary `=` and compound `+=`, `-=`, `*=`, `/=`, `%=`, and `^=` assignment
statements, expression statements inside blocks,
`if`/`else` including `else if` chains, `for name in expression`, `while`, and
`until` loops, unlabeled `break` and `continue`, and bare or value-bearing
`return` and `yield`. Control-flow bodies may be braced blocks or exactly one
statement on the same logical line as the header. The parser also accepts
`match expression` with one or more `case` branches. An optional `case else`
must be unique and last.
Empty blocks are valid. `break` and `continue` outside a loop are syntax errors.
A block brace is recognized from its statement position, while `{...}` in an
expression remains a dictionary literal.

Exception statements use `hope` for the protected block, expression-shaped
`unless` handler patterns, optional final `finally` cleanup, and `scream
expression` to raise a value. A `hope` requires at least one handler or cleanup
clause. Handler meaning, propagation, and cleanup behavior remain semantic and
runtime questions.

At the program root, the parser accepts declarations only. Executable control
flow, expression statements, and assignment statements belong inside function
bodies. This preserves Sagan's declaration-only module scope while entry-point
semantics remain under design.

Compound assignment is statement-only, like ordinary `=` reassignment. The
target and value are retained separately in the AST along with the exact
operator. Semantic analysis will validate assignability, types, and operator
support.

`unless` is reserved for exception handling in Sagan; it is not an inverse
conditional spelling.

`yield expression` and bare `yield` are syntactically valid inside function
bodies. Semantic analysis will determine which functions are generators and
validate their yielded types; the runtime will define suspension behavior.

Enum and optional patterns bind typed payload names, and covering every case is
recognized as exhaustive. **Open questions:** iterable protocol semantics,
broader destructuring patterns, generator typing, unreachable-code rules, entry-point
selection, and the runtime behavior of control transfer.
