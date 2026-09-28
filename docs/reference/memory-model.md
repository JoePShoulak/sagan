---
title: Memory model
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Memory model
Reference counting is the intended memory-management model.

That statement is **provisional design**, not implemented runtime behavior.
There is no Sagan runtime, object representation, ownership model, or generated
code yet.

**Open questions:** which values are references, retain/release insertion,
borrowing or weak references, cycle detection or collection, destruction order,
thread interaction, foreign ownership, value semantics, and observable lifetime
behavior.

The design also intends to prevent concurrent mutation of the same data when
parallel execution is eventually introduced, but parallel execution itself is
deferred.
