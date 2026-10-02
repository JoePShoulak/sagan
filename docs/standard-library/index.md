---
title: Standard library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Standard library
Sagan's core-library plan has three layers:

1. **Math** is built in and automatically available.
2. **Physics** is a tightly integrated, first-party core library requiring an import.
3. **Rendering** is a tightly integrated, first-party core library requiring an import.

Math will provide shared numerical vocabulary. Explicit imports keep physics
and rendering from adding dependencies or startup costs to programs that do not
need them. The inclusion policy is decided, but concrete APIs, module names,
package layout, and implementations are not finalized.

## [Math: automatically available](math.md)

Core math will be available without an import. The language already provides
checked scalars, vectors, points, spherical forms, and units; the post-1.0 math
library will add the carefully designed operations needed by Sagan programs and
the other core libraries.

## [Physics: explicit core library](physics.md)

Physics is a first-party core library, tightly coupled to Sagan's math types and
designed for its simulation use cases. Programs must import it explicitly. This
keeps the physics runtime and API out of applications that only need math or
other lightweight language facilities.

## [Rendering: explicit core library](rendering.md)

Rendering follows the same model as physics: first-party, deeply integrated
with Sagan's shared math and simulation vocabulary, and explicitly imported.
Programs that do not render should not incur its dependencies or runtime costs.

The exact public APIs, module names, package subdivision, and linking behavior
remain to be designed. The inclusion hierarchy itself is settled:

```text
automatically available: math
explicit core imports:   physics, rendering
```

For the first joint implementation, follow the [visible two-body orbit
roadmap](two-body-window-roadmap.md). It separates rendering, physics, math,
and language milestones and makes a runnable demo and documentation update
part of every step.
