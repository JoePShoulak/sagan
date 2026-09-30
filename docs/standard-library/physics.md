---
title: Physics library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Physics library

!!! warning "Planned library"
    The physics library is not implemented. No module name, import statement,
    public type, solver, or runtime behavior is established by this page.

Physics is intended to be a first-party Sagan core library built around the
same mathematical and simulation vocabulary as the language. Unlike math, it
will require an explicit import so programs that do not need physics do not
incur its dependencies or runtime costs.

## Intended role

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

## Documentation required before release

The completed section must define installation or distribution behavior,
imports, package organization, public APIs, units and conventions, numerical
methods, lifecycle and ownership, determinism limits, diagnostics, examples,
and interoperability with math and rendering.

See the [standard-library status](status.md) for the current decision boundary.
