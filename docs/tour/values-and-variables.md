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

**Implemented semantics:** the checker infers scalar values, enforces compatible
annotations and assignments, selects the smallest fitting signed integer width,
defaults floating literals to `Float64`, and permits only provably lossless
widening. `Int` and `Float` default to 64-bit widths when no initializer supplies
a narrower inference. A declaration must provide an annotation, initializer, or
both.

Arrays infer one homogeneous element type; dictionaries infer homogeneous key
and value types; and vectors and coordinates infer a numeric component type and
dimension. Empty arrays/dictionaries await generic annotation syntax. Native
construction, indexing, iteration, spreads, and display are implemented;
checked vector addition, subtraction, negation, and scalar scaling are also
implemented. Dimension-checked `.x`, `.y`, `.z`, and `.w` component reads and
updates work for vectors and coordinates. Physical units and
coordinate frames are deliberately not distinguished by the initial type system.
