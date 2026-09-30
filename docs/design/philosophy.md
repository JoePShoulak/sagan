---
title: Design philosophy
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Design philosophy
## Simulation first

**Settled design.** Geometry, astrodynamics, physics, scientific computing,
rendering, and interacting entities are central use cases. Vectors, matrices,
quaternions, coordinates, scientific notation, and large numerical workloads
belong in the language's normal vocabulary.

Math is built in and automatically available. Physics and rendering are
first-party core libraries designed to interoperate closely with that math, but
must be imported explicitly. Their concrete APIs and module names are not yet
defined.

## Strong and explicit types

**Settled design.** Sagan minimizes implicit coercion. A conversion is implicit
only when the implemented checker can prove it lossless. The native executable
subset raises runtime errors for integer arithmetic overflow and division or
modulo by zero. Extending those guarantees to future numeric and user-defined
types remains ongoing work.

## Composition before inheritance

**Settled design.** Small interfaces are the primary tools for reuse,
abstraction, and polymorphism. Classes implement and combine interfaces, and
interfaces may compose other interfaces.

**Open question.** Whether any limited implementation inheritance will exist.

## Visible mutation and safety

**Settled design.** Variables are mutable by default. A trailing `!` is part of
a method name and conventionally identifies a mutating counterpart; it is not an
effect system.

**Implemented foundation.** Class/face values and escaping closure captures use
reference-counted storage; explicit weak class fields break object cycles.
Parallel execution and unsafe escape hatches are explicitly post-1.0 work.
Their concurrency and foreign-ownership rules are not part of the 1.0 contract.

## Simulation-oriented, modular by default

Sagan is designed around geometry, numerical simulation, physics, and
rendering. These domains share types and conventions, but programs should not
need to carry the whole simulation stack merely to use the language.

The planned core therefore has three deliberately different inclusion levels:

- **Math** is built in and automatically available.
- **Physics** is a tightly coupled first-party core library and an explicit
  import.
- **Rendering** is a tightly coupled first-party core library and an explicit
  import.

All three are designed as a coherent foundation. Physics and rendering should
interoperate directly with Sagan's mathematical types, while remaining optional
for lightweight applications.
