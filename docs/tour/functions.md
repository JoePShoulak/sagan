---
title: Functions
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Functions
**Settled design:** functions use `fun`, may have typed parameters and return
types, and may be overloaded by parameter types.

```sagan
fun distance(a: Vector, b: Vector): Float {
  return (a - b).magnitude()
}
```

Expression bodies and lambdas use `=>`:

```sagan
let greater = fun(a: Float, b: Float) => a > b
```

A mutating counterpart may conventionally end in `!`, and the tokenizer emits
a distinct method-identifier token:

```sagan
vector.normalize!()
```

!!! note "Implementation status"
    Named, block-bodied functions with optional parameter and return type
    annotations now parse into the AST. Calls also parse. Return statements,
    expression-bodied functions, lambdas, name resolution, typing, overloads,
    and execution remain future work.

**Provisional design:** multiple returns, destructuring, variadic parameters,
yielding, overload selection, capture behavior, and inference rules.
