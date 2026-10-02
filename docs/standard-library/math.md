---
title: Math library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Math library

!!! info "Initial implementation"
    M0 implements the narrow orbital-math surface documented below. The wider
    scalar, geometry, matrix, quaternion, and numerical library remains future
    work and must not be inferred from the intended scope.

Math is the automatically available foundation of Sagan's core-library
ecosystem. Programs will not need an import to use its eventual public surface.
Physics and rendering will share this mathematical vocabulary rather than
defining incompatible alternatives.

This does not mean Sagan currently has no mathematical behavior. Checked
numeric operations, Cartesian vectors and points, spherical forms, and native
units are language features today. M0 adds automatically available operations
needed by the first two-body solver. Math is part of Sagan's main version and
does not carry an independent package version.

## Implemented M0 surface

The following names are automatically available without an import:

| Symbol | Implemented contract |
| --- | --- |
| `sqrt(value)` | Returns the square root of a finite, non-negative `Float32` or `Float64`. |
| `squared_length(vector)` | Returns the sum of squared components and preserves squared physical units. |
| `length(vector)` | Returns the Euclidean length of a finite floating-point vector in the component unit. |
| `dot(left, right)` | Returns the dot product of equal-dimension, unit-compatible floating-point vectors. |
| `normalized(vector)` | Returns a dimensionless unit vector; zero-length input is rejected. |
| `display_coordinates(point, origin, scale)` | Returns dimensionless coordinates `(point - origin) / scale` after checking compatible physical units. |

M0 accepts fixed-size Cartesian `Vector` and `Point` values whose components
are `Float32` or `Float64`; measured components retain their native unit
metadata. `display_coordinates` requires measured points and an explicit
compatible world-units-per-display-unit scale. It returns a vector rather than
a point because the result is an offset from the supplied display origin.

All M0 inputs must be finite, and intermediate squared sums and dot products
must remain finite. `sqrt` rejects negative input with
`RuntimeError.math_domain`; non-finite input or output uses
`RuntimeError.non_finite`; `normalized` rejects a zero vector with
`RuntimeError.zero_length`; and a zero display scale uses the existing
`RuntimeError.division_by_zero`. Floating results retain ordinary IEEE-754
rounding and are not promised to be exact or bit-identical across toolchains.

Run the checked example with:

```bash
make orbit-math-demo
```

`examples/orbit_math.sagan` demonstrates a measured 3-4-5 displacement, dot
product, normalization, square root, and conversion from metres to
dimensionless display coordinates at an explicit kilometre scale.

## Intended scope

The design direction includes the mathematical facilities needed by geometry,
astrodynamics, numerics, and simulation. Candidate subject areas include:

- scalar and numerical operations;
- vectors, matrices, and quaternions;
- points, coordinates, and transformations; and
- numerical algorithms and utilities needed across the core libraries.

Outside the M0 surface above, this list describes intended domain rather than
an approved API. Exact additional types, operations, precision behavior,
naming, and organization remain open.

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
