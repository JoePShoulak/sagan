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
backend currently supplies the retain/release behavior; Sagan does not yet
expose borrowing, weak references, or explicit lifetime operations.

The safe absence foundation for weak references is now executable:
`Optional<T>`, `Some(value)`, `None`, payload matching, safe `?.` propagation,
and lazy `??` fallback. Weak fields will use this model so reading an expired
reference cannot produce a dangling value.

Sagan does not have a general `null` value. A plain class or face value is
always present; `None` is valid only where an `Optional<T>` supplies the absent
case.

**Open questions:** weak-field syntax and lowering, borrowing, cycle detection or collection, destruction order,
thread interaction, foreign ownership, value semantics, and observable lifetime
behavior.

The design also intends to prevent concurrent mutation of the same data when
parallel execution is eventually introduced, but parallel execution itself is
deferred.
