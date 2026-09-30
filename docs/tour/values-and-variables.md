---
title: Values and variables
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Values and variables
`let` gives a value a name. Variables are mutable by default, which means the
program may assign them a new value later. `=` supplies the first value or
replaces the current one.

```sagan
let altitude: Float = 125_000.0
altitude = altitude + 500.0
altitude += 500.0
```

Use `const` when the name should never be assigned again:

```sagan
const DISTANCE = 100 meter
const TIME = 20 second
const SPEED = DISTANCE / TIME
print(SPEED)
```

The name must use ASCII `SCREAMING_SNAKE_CASE`. `const` is the rule that makes
it immutable; uppercase letters alone do not. Sagan rejects `let SPEED = 5`
so a mutable variable cannot look like a constant. `const` requires a value
immediately. It stops reassignment and direct mutation through that name,
including `SPEED += ...`, `VALUES[0] = ...`, and mutating `!` methods. It does
not deeply freeze a referenced object shared with a mutable alias, and it does
not promise that the value is evaluated at compile time.

The parser supports `=`, `+=`, `-=`, `*=`, `/=`, `%=`, and `^=` reassignment
statements inside function bodies. `^=` uses Sagan's exponentiation operator;
there are no initial bitwise operators.

Numbers may use decimal integers, decimal floating point, scientific notation,
and separators between digits:

```sagan
let entities = 10_000
let gravity = 6.674_30e-11
```

The checker infers scalar values, enforces compatible
annotations and assignments, selects the smallest fitting signed integer width,
defaults floating literals to `Float64`, and permits only provably lossless
widening. `Int` and `Float` default to 64-bit widths when no initializer supplies
a narrower inference. A declaration must provide an annotation, initializer, or
both.

Arrays infer one homogeneous element type, so one array does not silently mix
unrelated kinds of values. Dictionaries similarly infer one key type and one
value type. Cartesian and spherical vectors and points infer a numeric
component type and dimension. Empty arrays and dictionaries need an explicit
generic type because there is no element from which to infer one. Native
construction, indexing, iteration, spreads, and display are implemented;
checked vector addition, subtraction, negation, and scalar scaling are also
implemented. Points are affine locations: vectors translate them, and
subtracting two points yields the displacement vector. Dimension-checked
`.x`, `.y`, `.z`, and `.w` component reads and
updates work for Cartesian vectors and points. Three-dimensional spherical
values use `s<magnitude, inclination, azimuth>` and
`s(radius, inclination, azimuth)`, with radians and representation-specific
named members. Physical units are native static metadata and may annotate scalar
or geometry values; coordinate frames are deliberately deferred.
