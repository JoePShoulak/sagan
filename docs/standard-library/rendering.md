---
title: Rendering library
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Rendering library

!!! warning "Work-in-progress library"
    Version 0.1.0 implements only the R0 single-window bridge on Windows. It
    does not yet draw shapes or text, and its public API may change before 1.0.

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

## Implemented R0 window bridge

The independently versioned `sagan-render` package exports `open`, `poll`,
`clear`, and `close` from `render.window`. It owns one resizable native window
per process. Native handles remain private. The current private backend uses
the Windows API; this is not part of the Sagan-facing contract and may be
replaced by SDL3 without changing callers.

`open` validates positive dimensions and reports native setup failures through
the existing runtime-failure path. `clear` accepts integer RGB channels from 0
through 255. `poll` processes pending window events and becomes false after a
close request. `close` releases the window and registration resources and is
safe to call after the user closes the window.

From a source checkout with the documented MSYS2 UCRT64 toolchain, run:

```bash
make window-demo
```

The command compiles the Sagan package and its private native bridge, then opens
a resizable 960 by 540 solid blue-gray window. Close it with the standard
window close button. `make window-demo-test` uses the private
`SAGAN_RENDER_AUTOCLOSE_MS` smoke-test setting to exercise the same lifecycle
without waiting for input. Version 0.1.0 supports Windows only and links the
system `user32` and `gdi32` libraries. The demo statically links its GCC/C++
runtime support, so the resulting executable does not require MSYS2 runtime
directories on `PATH`. It has no SDL or GPU dependency yet.
The catalog declares compiler compatibility `^2.0.0`. A shallow development
checkout whose unavailable version baseline makes the compiler identify itself
as `0.0.0+gunknown` receives a demo-local compatibility index only; released
package metadata retains the declared 2.x range.

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
