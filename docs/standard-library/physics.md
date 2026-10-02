---
title: Physics library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Physics library

!!! info "Initial implementation"
    `sagan-physics` 0.1.0 implements the narrow P0 headless two-body surface
    documented below. Broader astrodynamics, N-body propagation, collisions,
    rigid bodies, atmospheres, and trajectory planning remain future work.

Physics is intended to be a first-party Sagan core library built around the
same mathematical and simulation vocabulary as the language. Unlike math, it
will require an explicit import so programs that do not need physics do not
incur its dependencies or runtime costs.

Native units are already a language feature; they do not require importing the
physics library. The library will use those checked units rather than inventing
a second unit system.

The physics package is first-party but independently versioned. Its `0.1.0`
version does not change Sagan's compiler version. A package imports it through
an explicit dependency and an exact lockfile selection:

```toml
[dependencies]
physics = { package = "sagan-physics", version = "^0.1.0" }
```

## Implemented P0 surface

The `physics.two_body` module exports:

| Symbol | Implemented contract |
| --- | --- |
| `GRAVITATIONAL_CONSTANT` | The fixed Newtonian value `6.67430e-11 m^3 kg^-1 s^-2`. |
| `BodySnapshot` | Read-only identity, mass, 2D SI position, and 2D SI velocity. |
| `TwoBodySnapshot` | Read-only time, both body snapshots, total mechanical energy, scalar 2D angular momentum, and barycenter. |
| `TwoBodySolver` | Exactly two mutually gravitating Newtonian point masses advanced by one explicit fixed step at a time. |

`TwoBodySolver.step!()` uses velocity Verlet. It calculates the force on the
first body and applies its equal-and-opposite value to the second body, so both
bodies move about their common barycenter. Authoritative positions and
velocities use `Float64` and SI units in one caller-defined 2D Cartesian frame.
Coordinate-frame identity is not yet represented by the type system.

Snapshots detach their position and velocity values from the solver's next
step. Their fields are private and their public API contains no mutating
method, so rendering can consume them without changing authoritative physics.
The solver has no rendering dependency.

Construction rejects non-finite or non-positive mass and timestep values,
non-finite position or velocity components, and overlapping initial positions.
Every force evaluation rejects zero separation. P0 deliberately provides no
collision response or force softening.

Public force and energy values use the named `newton` and `joule` units so
callers do not need to mentally reduce composite dimensions. The implementation
performs explicit checked conversions after calculating their dimensions.
Angular momentum remains `kilogram * meter^2 / second` because SI defines no
special named unit for it.

Run the checked numerical example with:

```bash
make orbit-numeric-demo
```

The example advances an Earth-Moon-scale circular reference for 28,800 seconds
using a 300-second fixed step. It prints five samples and checks absolute
energy and angular-momentum drift against relative tolerances of `1e-8` and
`1e-12`, respectively. It also requires the barycenter to remain within
`1e-6 m` of its initial origin. These are fixture tolerances, not a general
accuracy guarantee for every orbit or timestep.

## Intended role beyond P0

The library will eventually provide the supported physical-modeling facilities
for Sagan simulations. Its exact scope and subdivision must be decided from
concrete use cases and implementation evidence. Possible APIs must not be
treated as settled merely because a physical concept appears elsewhere in the
documentation or examples.

## Relationship to math and rendering

Physics will consume the automatically available math foundation. Rendering
may visualize physics-backed simulations, but neither library should require
the other unless a documented dependency decision explicitly establishes that
relationship.

Units, coordinate frames, integration methods, collision or rigid-body models,
determinism guarantees, and extension points remain design questions. Each
needs a recorded decision before its behavior can be presented as part of
Sagan's contract.

## Documentation required before a stable release

The completed section must define installation or distribution behavior,
imports, package organization, public APIs, units and conventions, numerical
methods, lifecycle and ownership, determinism limits, diagnostics, examples,
and interoperability with math and rendering.

See the [standard-library status](status.md) for the current decision boundary.
