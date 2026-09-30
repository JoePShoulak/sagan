---
title: Math library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Math library

!!! warning "Planned library"
    The math library is not implemented. This page records the intended
    boundary without promising types, functions, module names, or behavior
    that the repository does not yet provide.

Math is the automatically available foundation of Sagan's core-library
ecosystem. Programs will not need an import to use its eventual public surface.
Physics and rendering will share this mathematical vocabulary rather than
defining incompatible alternatives.

This does not mean Sagan currently has no mathematical behavior. Checked
numeric operations, Cartesian vectors and points, spherical forms, and native
units are language features today. This page covers the larger automatic math
library that will be designed after the 1.0 language release.

## Intended scope

The design direction includes the mathematical facilities needed by geometry,
astrodynamics, numerics, and simulation. Candidate subject areas include:

- scalar and numerical operations;
- vectors, matrices, and quaternions;
- points, coordinates, and transformations; and
- numerical algorithms and utilities needed across the core libraries.

This list describes the intended domain, not an approved API. Exact types,
operations, precision behavior, naming, and organization remain open.

## Language boundary

Automatic availability is a settled design choice, but it does not make every
mathematical facility a language keyword or primitive. The boundary between
compiler-provided behavior and library-provided behavior must be documented as
the implementation develops.

Physical units are distinguished by Sagan's native static type system and flow
through scalar and geometry arithmetic. Coordinate frames and the unit-aware
higher math API remain later design work.

## Documentation required before release

The completed section must define the public symbols, accepted value types,
precision and error behavior, coordinate conventions, examples, performance
expectations, and interactions with physics and rendering. Each documented
operation must be backed by implementation evidence and tests.

See the [standard-library status](status.md) for the current decision boundary.
