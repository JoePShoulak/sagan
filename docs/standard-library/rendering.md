---
title: Rendering library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Rendering library

!!! warning "Planned library"
    The rendering library is not implemented. No backend, windowing system,
    scene model, module name, import statement, or public API is established by
    this page.

Rendering is intended to be a first-party Sagan core library integrated with
the language's mathematical and simulation vocabulary. It will require an
explicit import so non-graphical programs do not incur rendering dependencies
or runtime costs.

## Intended role

The library will eventually provide the supported path from Sagan simulation
data to visual output. Its public abstractions must be designed together with
real examples rather than inferred from the current compiler, AST visualization
features, or any particular graphics ecosystem.

The compiler's existing DOT, SVG, and interactive HTML AST renderers are
developer tools. They are not implementations or previews of the future Sagan
rendering library.

## Relationship to math and physics

Rendering will use the automatically available math foundation. It should be
able to visualize data produced by physics, but physics remains a separate,
explicitly imported core library. Concrete dependency direction, shared scene
or geometry types, coordinate conventions, and runtime integration remain
open.

## Documentation required before release

The completed section must define supported platforms and backends, imports,
package organization, public APIs, coordinate and color conventions, resource
ownership, frame lifecycle, error behavior, performance expectations,
headless operation, examples, and interoperability with math and physics.

See the [standard-library status](status.md) for the current decision boundary.
