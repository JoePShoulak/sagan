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
    Version 0.2.0 implements the R0 single-window bridge and the R1 basic 2D
    canvas on Windows. It is not a general scene or UI system, and its public
    API may change before 1.0.

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

## Window and frame lifecycle

The independently versioned `sagan-render` package exports `open`, `poll`,
`clear`, and `close` from `render.window`. It owns one resizable native window
per process. Native handles remain private. The current private backend uses
the Windows API; this is not part of the Sagan-facing contract and may be
replaced by SDL3 without changing callers.

`open` validates positive dimensions and reports native setup failures through
the existing runtime-failure path. `poll` processes pending window events and
becomes false after a close request. `close` releases the window, back buffer,
and registration resources and is safe to call after the user closes the
window.

`clear` starts a frame by filling a resize-aware private back buffer. Drawing
operations modify that frame, and `present` copies it to the window. Callers
redraw every display frame; the renderer preserves the most recently presented
frame for native paint events.

## Basic 2D canvas

The `render.canvas` module exports:

- `set_view(center_x, center_y, pixels_per_unit)` for a centered orthographic
  world-to-screen transform;
- `circle(x, y, radius, red, green, blue)` for filled world-space circles;
- `line(start_x, start_y, end_x, end_y, width, red, green, blue)` for
  world-space line segments;
- `text(x, y, value, point_size, red, green, blue)` for world-positioned text;
- `text_screen(x, y, value, point_size, red, green, blue)` for fixed overlays;
  and
- `present()` to display the completed frame.

World coordinates use +X right and +Y up. The view center maps to the current
client-area center, so resizing keeps the world origin centered. Circle radii
scale with `pixels_per_unit`; line widths and screen text positions are physical
pixels. Colors are integer RGB channels from 0 through 255. Coordinates, scale,
radii, widths, point sizes, and colors are checked before drawing. Version 0.2
does not yet expose alpha, clipping, rotation, font selection, paths as one
object, or retained drawables.

Text uses the Windows system `Segoe UI` font with ClearType quality. Point sizes
use the drawing device's current DPI. No font file is distributed at R1; a
portable backend must establish an equivalent font-loading contract.

## Demos and verification

From a source checkout with the documented MSYS2 UCRT64 toolchain, run:

```bash
make window-demo
make shape-text-demo
```

The command compiles the Sagan package and its private native bridge, then opens
a resizable 960 by 540 solid blue-gray window. Close it with the standard
window close button. `make window-demo-test` uses the private
`SAGAN_RENDER_AUTOCLOSE_MS` smoke-test setting to exercise the same lifecycle
without waiting for input. `make shape-text-demo` opens the R1 static
presentation with two bodies, axes, line segments, world labels, and a screen
legend. `make shape-text-demo-test` captures and validates the same frame as a
960 by 540 top-down BMP.

![R1 shapes and text demo](../assets/images/render-r1-shape-text.bmp)

Version 0.2.0 supports Windows only and links the
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
