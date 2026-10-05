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
    `sagan-physics` 0.3.0 implements the narrow P0 two-body, P1 circular
    restricted-three-body, and solar-perturbed Lagrange experiment documented
    below. Broader astrodynamics,
    general N-body propagation, collisions, rigid bodies, atmospheres, and
    trajectory planning remain future work.

Physics is intended to be a first-party Sagan core library built around the
same mathematical and simulation vocabulary as the language. Unlike math, it
will require an explicit import so programs that do not need physics do not
incur its dependencies or runtime costs.

Native units are already a language feature; they do not require importing the
physics library. The library will use those checked units rather than inventing
a second unit system.

The physics package is first-party but independently versioned. Its `0.3.0`
version does not change Sagan's compiler version. A package imports it through
an explicit dependency and an exact lockfile selection:

```toml
[dependencies]
physics = { package = "sagan-physics", version = "^0.3.0" }
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

## Implemented P1 surface

The `physics.restricted_three_body` module adds:

| Symbol | Implemented contract |
| --- | --- |
| `LagrangeBodySeed` | Identity, inertial position/velocity, and a dimensionless rotating-frame reference coordinate for one massless tracer. |
| `LagrangeBodySnapshot` | The P0 identity/position/velocity shape plus rotating reference, stability error in metres, and latched `escaped()` state. |
| `RestrictedThreeBodySnapshot` | The massive P0 pair and exactly six read-only tertiary snapshots. |
| `RestrictedThreeBodySolver` | Advances the P0 pair and six independent massless tracers using the same fixed timestep and velocity-Verlet scheme. |

This is the circular restricted three-body problem, not a general eight-body
gravity solver. Tertiaries feel the gravity of both primaries but exert no
force themselves. They therefore do not perturb either primary and do not
interact with one another. This intentional restriction allows several
alternative trajectories to be compared in one deterministic run.

The checked fixture uses an Earth-Moon-like mass ratio of approximately
`0.01214`, below the triangular-point stability limit. It places tracers at
L1, L2, L3, L4, and L5 plus an off-point control, then advances 60 days with a
300-second step. L4 and L5 are the stable equilibrium points. L1 through L3
are unstable equilibrium points; starting there does not make them stable.

At every snapshot, the solver reconstructs each tracer's reference position
from the current primary-secondary axis and barycenter. `stability_error()` is
the tracer's distance from that rotating reference, making the six results
directly comparable without introducing rendering concerns into physics.

`escaped()` becomes true when a tertiary has moved beyond twice the current
primary-secondary separation, is moving outward from the barycenter, and has
positive instantaneous specific orbital energy relative to both primaries.
The result is latched once detected. This is a practical boundary for the
isolated restricted-three-body demo, not a claim about escape from an external
body's Hill sphere. Presentations should display `escaped` instead of the
stability-error number after the flag becomes true; the numeric error remains
available for diagnostics.

`stop_tertiary!(index)` is an explicit, idempotent performance boundary for a
tertiary whose `escaped()` flag is already true. Stopping freezes that
tertiary's position and velocity while the massive system and other tertiaries
continue stepping. The solver rejects attempts to stop a body before escape;
it does not import rendering state or choose a viewport cutoff itself.

Run the checked fixture with:

```bash
make lagrange-numeric-demo
```

The fixture verifies that the massive pair is identical to a standalone P0
run, checks its existing energy and angular-momentum tolerances, and requires
both L4 and L5 to remain closer to their references than the off-point control.
The demonstrated stability is specific to the stated mass ratio, starting
conditions, duration, and timestep.

## Solar-perturbed Lagrange experiment

The `physics.solar_lagrange` module adds a deliberately narrow planar
restricted-four-body experiment:

| Symbol | Implemented contract |
| --- | --- |
| `SolarLagrangeSeed` | One massless tracer's inertial state and Earth-Moon rotating-frame reference. |
| `SolarTertiarySnapshot` | Read-only tracer state, rotating-frame error, and Earth-Moon escape flag. |
| `SolarLagrangeSnapshot` | Sun, Earth, Moon, and exactly six massless tracer snapshots. |
| `SolarLagrangeSolver` | Mutually advances the three massive bodies and independently advances all six tracers under their combined gravity. |

The checked setup starts a circular Earth-Moon approximation with its
barycenter on a circular solar approximation at one astronomical unit. It is
two-dimensional and omits orbital inclination, eccentricity, nonspherical
gravity, radiation pressure, other planets, and ephemeris-fitted initial
conditions. It exists to isolate the qualitative effect of solar gravity; it
is not a real Earth-Moon ephemeris model.

Run the numerical comparison or windowed visualization with:

```bash
make solar-lagrange-numeric-demo
make solar-lagrange-demo
```

The window keeps an Earth-Moon-centered Lagrange view with colored trails and
adds a heliocentric inset plus a Sun-direction indicator. In the checked
60-day, 300-second-step fixture, the L4 and L5 rotating-frame errors grow to
approximately 39,844 km and 48,369 km, respectively. Those values demonstrate
the model difference; they are not an accuracy claim about the real system.

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
