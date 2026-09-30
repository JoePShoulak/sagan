---
title: Memory model
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Memory model
Sagan manages class objects with reference counting. In plain language, the
runtime tracks how many owning references still point to an object and releases
the object when that count reaches zero.
Class instances use shared reference-counted storage. Assigning a class value or
converting it to a face value preserves the same underlying object, so mutation
through one reference is visible through the others. Face calls dispatch to the
concrete class implementation.

The generated C++ supplies this retain/release behavior. Sagan exposes
non-owning references through class fields declared with `weak let`, but does
not expose borrowing or explicit lifetime operations.

The safe absence model for weak references is executable:
`Optional<T>`, `Some(value)`, `None`, payload matching, safe `?.` propagation,
and lazy `??` fallback. A weak field begins empty, accepts a strong class or
face value on assignment, and returns `Optional<T>` when read. A live target is
`Some(target)`; an expired target is `None`, so no dangling value is exposed.

```sagan
class Observer {
  weak let target: Probe

  fun watch!(value: Probe): Void {
    self.target = value
  }
}

let answer = observer.target?.answer() ?? 0
```

Sagan does not have a general `null` value. A plain class or face value is
always present; `None` is valid only where an `Optional<T>` supplies the absent
case.

Escaping closures use the same reference-counted lifetime principle. Local
variables and parameters captured by a lambda reside in shared cells. The
declaring scope and every closure created from that scope refer to the same
cell, so mutation remains visible and the cell is released after the scope and
all capturing closures are gone. Function values are written as
`(Parameter, ...) => Result`. Capturing contextual `self` is rejected in 1.0
until object-capture lifetime rules are defined.

Reference cycles must contain an explicit weak edge to be reclaimed; an
all-strong cycle keeps itself alive. Borrowing, automatic cycle collection,
foreign ownership, contextual `self` capture, and observable destruction hooks
are deferred.

The design also intends to prevent concurrent mutation of the same data when
parallel execution is eventually introduced, but parallel execution itself is
deferred.
