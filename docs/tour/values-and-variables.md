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

**Implemented lexically:** the tokenizer recognizes those declarations and
numeric forms and rejects misplaced separators and incomplete exponents.

**Provisional design:** arrays use `[]`, dictionaries use `{key: value}`,
vectors use `<...>`, and coordinates use parenthesized lists of at least two
elements. Their construction, typing, and runtime behavior await the parser and
semantic analyzer. Physical units and coordinate frames are deliberately not
distinguished by the initial type system.
