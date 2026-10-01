---
title: Statements
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Statements
Statements perform actions. Ordinary statements are newline-terminated rather than
semicolon-terminated. The tokenizer emits logical newline tokens while
suppressing continuation newlines in known contexts.

Statements include declarations, assignments, expression statements,
`if`/`else`, `match`/`case`, `for`/`in`, `while`, `until`,
`break`, `continue`, `return`, `yield`, and exception constructs.

The language supports newline-separated blocks, `let` declarations,
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
clause. Handlers are tried in source order, unmatched exceptions propagate
outward, and `finally` runs during both normal and exceptional exits. See
[Exceptions](exceptions.md).

At the program root, the parser accepts declarations only. Executable control
flow, expression statements, and assignment statements belong inside function
bodies. An executable program enters through `fun main()`.

Compound assignment is statement-only, like ordinary `=` reassignment. Its
target must be assignable, and the result must convert losslessly back into the
target's type.

Parallel reassignment uses one `=` with comma-separated variable names and
the same number of values:

```sagan
let a = 0
let b = 1
a, b = b, a + b
```

Every right-hand expression is evaluated from left to right before any target
changes. Then the variables are assigned from left to right, so the example
leaves `a` as `1` and `b` as `1`. Targets must be distinct, mutable variable
names; fields, indexes, compound operators, and mismatched list lengths are
not supported in this form. This is reassignment, not a declaration or tuple
expression.

`unless` is reserved for exception handling in Sagan; it is not an inverse
conditional spelling.

`yield expression` and bare `yield` are syntactically valid inside function
bodies, but generators do not execute yet. Generator typing and suspension are
deferred.

Enum and optional patterns bind typed payload names, and covering every enum
case is recognized as exhaustive. Non-`Void` functions must return on every
guaranteed path, and code after a guaranteed return is rejected as unreachable.
Custom iteration protocols, broader destructuring patterns, and generators are
deferred.
