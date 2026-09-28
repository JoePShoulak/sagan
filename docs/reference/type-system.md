---
title: Type system
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Type system
Sagan is designed as a strongly and statically typed language.

**Settled design:**

- implicit conversion is limited to conversions proven lossless;
- variables are mutable by default;
- interfaces are central to abstraction and polymorphism;
- physical units and coordinate frames are not distinct in the initial type system;
- no separate character-literal type is planned; and
- math types form built-in vocabulary.

**Implemented foundation:** `Int` is the canonical integer spelling. Literals
use the smallest fitting signed width from `Int8` through `Int64`; otherwise
`Int` defaults to `Int64`. Floating literals and unconstrained `Float` positions
default to `Float64`, with explicit `Float32` available. Only provably lossless
widening is implicit. Core scalar declarations, operators, conditions, calls,
overloads, and returns are checked. Variables need either an annotation or an
initializer.

Array literals infer invariant `Array<Element>` types, while dictionary literals
infer `Dictionary<Key, Value>` with homogeneous keys and values. Spreads must
provide compatible collections. Empty arrays and dictionaries cannot yet be
checked because generic annotation syntax is not implemented. Vector and
coordinate literals infer their dimension and common numeric component type,
for example `Vector3<Float64>`; dimensions must match for compatibility.

**Implemented for the native numeric subset:** integer arithmetic overflow
raises a runtime exception. Typed collections and interfaces continue to have
provisional semantics outside the implemented subset.

**Open questions:** generic annotation syntax, member inference, nullability, value versus
reference categories, generic semantics, possible sum types, variance,
compile-time constants, and representation. Deferred cases are currently
marked `Unknown` by the type model.

Annotations must name types. Typed variables may begin uninitialized, but they
must be definitely assigned before use; compound assignment counts as a read.
Non-`Void` functions must return along every guaranteed path, and statements
after a guaranteed return are rejected as unreachable.
