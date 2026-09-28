---
title: Determinism
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Determinism
Cross-platform deterministic simulation is a **settled goal**, not an
implemented guarantee.

The intended result is that identical inputs produce identical results on every
supported platform covered by the determinism contract. Translating to C++ does
not provide that property by itself. A useful contract will need to constrain at
least:

- floating-point behavior and permitted transformations;
- generated C++ and compiler flags;
- mathematical and standard-library implementations;
- iteration order and other observable container behavior;
- runtime scheduling once concurrency exists; and
- the definition of supported platforms and compiler versions.

**Open questions:** whether determinism is bit-for-bit or tolerance-based for
particular operations, which transcendental functions are covered, how external
I/O participates, and how deterministic modes interact with performance.

Until these questions are answered and tested, documentation and examples must
describe determinism as a project goal rather than a current compiler property.
