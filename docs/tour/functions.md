---
title: Functions
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Functions
Functions use `fun`. Parameters may declare the type of value they accept, and
the annotation after `:` says what the function returns. More than one function
may share a name when their parameter types distinguish them; this is called
**overloading**.

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

Named functions, face defaults, and class methods may use either a block or
`=> expression`. Anonymous functions are called **lambdas**. A typed lambda can
be stored in a variable, passed to another function, returned, or called
immediately. A function that accepts or returns another function is often called
a **higher-order function**.

When a lambda uses a surrounding local variable, it **captures** that variable.
Sagan stores captured locals in shared reference-counted cells, so mutation and
lifetime still behave correctly after the original function returns. Run
`make closure-demo` for the larger executable demonstration.

Lambdas that refer to a method's contextual `self` are rejected in 1.0; the
required object-lifetime semantics remain deferred. Multiple returns,
destructuring, variadic parameters, and executable generator semantics for
`yield` are not part of the current language.
