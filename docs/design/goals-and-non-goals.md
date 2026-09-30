---
title: Goals and non-goals
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Goals and non-goals
## Goals

**Settled design goals:**

- make geometry, astrodynamics, numerical work, physics, and rendering natural;
- provide strong static typing with limited lossless implicit conversion;
- favor interface-based composition over deep inheritance hierarchies;
- produce native programs initially by translating validated Sagan to C++;
- target Windows, Linux, and macOS;
- make mutation visible in APIs through naming conventions; and
- pursue repeatable simulation results across supported platforms.

## Initial non-goals

- parallel execution;
- bitwise operators;
- binary, octal, hexadecimal, or numeric-suffix literals;
- encoding coordinate frames in the initial type system;
- requiring physics or rendering in lightweight programs;
- C or C++ interoperability in the first compiler; and
- optimizing before the parser, semantics, and runtime contracts are established.

## Boundaries still moving

Concrete core-library APIs, package resolution, type inference, generics,
runtime representation, reference-count cycle handling, optimization, ABI
details, and deterministic numeric guarantees remain unsettled.
