---
title: Memory model
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Memory model
Reference counting is the implemented foundation of the native object model.
Class instances use shared reference-counted storage. Assigning a class value or
converting it to a face value preserves the same underlying object, so mutation
through one reference is visible through the others. Face calls dispatch to the
concrete class implementation.

This is an initial executable subset, not a complete ownership model. The C++
backend currently supplies retain/release behavior. Sagan exposes non-owning
references through class fields declared with `weak let`, but does not yet
expose borrowing or explicit lifetime operations.

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

Reference cycles must currently contain an explicit weak edge to be reclaimed;
all-strong cycles remain retained. **Open questions:** borrowing, automatic cycle
detection or collection, destruction order,
thread interaction, foreign ownership, value semantics, and observable lifetime
behavior.

The design also intends to prevent concurrent mutation of the same data when
parallel execution is eventually introduced, but parallel execution itself is
deferred.
