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

**Provisional design:** overloads select by parameter types, typed collections
and interfaces exist, and arithmetic overflow raises an exception.

**Open questions:** inference, numeric type set and widths, nullability, value
versus reference categories, generic semantics, possible sum types, variance,
conversion proofs, compile-time constants, and representation. There is no
semantic analyzer, so none of these rules is enforced today.
