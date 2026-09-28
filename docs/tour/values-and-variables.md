---
title: Values and variables
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Values and variables
**Settled design:** `let` declares a variable, variables are mutable by default,
and `=` is used for initialization and reassignment.

```sagan
let altitude: Float = 125_000.0
altitude = altitude + 500.0
altitude += 500.0
```

The parser supports `=`, `+=`, `-=`, `*=`, `/=`, `%=`, and `^=` reassignment
statements inside function bodies. `^=` uses Sagan's exponentiation operator;
there are no initial bitwise operators.

Numbers may use decimal integers, decimal floating point, scientific notation,
and separators between digits:

```sagan
let entities = 10_000
let gravity = 6.674_30e-11
```

**Implemented syntax:** the tokenizer recognizes these numeric forms and the
parser produces declarations and assignment statements. Neither stage decides
whether the declared type exists, whether a value is assignable, or what an
assignment does at runtime.

**Provisional design:** arrays use `[]`, dictionaries use `{key: value}`,
vectors use `<...>`, and coordinates use parenthesized lists of at least two
elements. Their syntax is parsed; construction, typing, and runtime behavior
await semantic analysis and later compiler stages. Physical units and
coordinate frames are deliberately not distinguished by the initial type
system.
