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

let predicate: (Float, Float) => Bool = greater

fun square(value: Float): Float => value * value
```

A mutating counterpart may conventionally end in `!`, and the tokenizer emits
a distinct method-identifier token:

```sagan
vector.normalize!()
```

!!! note "Implementation status"
    Named, block-bodied functions with optional parameter and return type
    annotations parse into the AST. Named functions, interface defaults, and
    class methods may instead use `=> expression`. Anonymous lambdas accept
    typed parameters and an optional return annotation. The initial checker
    validates scalar parameters and returns and selects compatible overloads.
    Typed expression lambdas execute, can be stored in explicitly annotated
    function values, passed to higher-order functions, returned from functions,
    and called immediately or through variables and parameters. Captured locals
    and parameters use shared reference-counted cells, preserving mutation and
    lifetime when a closure escapes. Run `make closure-demo` for an executable
    example.

Lambdas that refer to a method's contextual `self` are rejected in 1.0; the
required object-lifetime semantics remain deferred. Other provisional areas
include multiple returns, destructuring, variadic parameters, yielding, and
broader inference rules.
