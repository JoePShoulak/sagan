---
title: Two-body window roadmap
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# A visible two-body orbit

The first finish line is a Sagan program that opens a responsive window and
shows **two moving, mutually gravitating bodies**. The bodies are drawn as
shapes, with readable names and simulation information drawn as text. The
physics library determines their positions; rendering only displays them.
If time and usage remain after that demo is reliable, extend the *same*
foundation to a simplified numerical three-body demo.

This is a development plan, not an implemented API or a claim that the core
libraries already exist. Exact import names, public type names, backend, and
packaging details remain decisions for their owning chats. The first target is
Windows; Linux and macOS remain future distribution targets. Math is
automatically available, while physics and rendering are separate, explicitly
imported first-party libraries. Physics and rendering keep their own package
versions rather than inheriting the compiler's numeric version.

## Recommended first-demo contract

- Use a 2D Cartesian world with +X right and +Y up, SI units, `Float64`, and
  an orthographic camera. Both bodies move about their common barycenter.
- Model point masses under Newtonian gravity. Use a fixed simulation step and
  velocity Verlet; rendering may interpolate immutable snapshots for smooth
  display but must never change the authoritative simulation state.
- Draw two filled circles, optional orbit trails, labels, and a small text
  readout for simulated time and diagnostic values. Resize and close cleanly.
- Start with one window and one local simulation. Defer collisions, rigid
  bodies, 3D, relativity, arbitrary N-body performance, and general UI/scene
  systems. A numerical three-body run is an optional extension, not a promise
  of stable or closed-form orbits.

These are recommended slice choices to confirm at the start of implementation,
not new language guarantees. In particular, Sagan does not yet type-check
coordinate-frame identity; the demo must document and consistently use one
world frame.

Each milestone below is owned by its corresponding development chat. At the
end of **every** milestone, that chat adds a runnable Sagan example, one
documented command, expected visible or textual output, focused tests, and a
plain-language update to its library/reference page and this roadmap. Commands
named below are *proposed targets*, not commands available today. Keep pages
`work-in-progress` until the owner's documentation audit approves them. Do
not invent public APIs in docs ahead of implementation.

Milestone status is recorded after its demo, tests, and documentation have been
checked at a named commit. Unmarked milestones remain **not started**.

## Rendering chat

**R0 — Window bridge.** Implement the smallest optional native bridge needed
to open one resizable window, clear it, poll input/close events, and exit
without leaking resources. Recommend SDL3 for window and input. Keep native
handles private and the Sagan-facing surface independent of a particular
graphics backend. **Demo:** proposed `make window-demo` opens a solid-color
window that resizes and closes cleanly. Document install/runtime requirements
and failure behavior. This milestone must work before drawing an orbit.

**Status: implemented at `d727bd1` and lifecycle-verified on 2026-10-01.**
`sagan-render` 0.1.0 provides the independently versioned `render.window`
package, a private Windows backend, `make window-demo`, and an automated
lifecycle smoke test. SDL3 remains the intended portable replacement for the
private backend; no backend-specific handle or type is exposed to Sagan. The
standalone window remained responsive until the verification process stopped
it, but the unavailable Windows UI capture service prevented screenshot-based
pixel and manual-resize evidence in this run.

**R1 — Shapes and text.** Add the smallest 2D drawing surface that can draw a
filled circle, a line or polyline, and legible text at specified positions.
Support a world-to-screen transform, basic color, resize/DPI behavior, and a
redistributable font or documented system-font choice. A simple SDL3 2D path
is a reasonable first implementation; Dawn/WebGPU remains a later backend
decision, not a prerequisite for the first visible orbit. **Demo:** proposed
`make shape-text-demo` shows two stationary circles, trails/axes, names, and a
text legend. Record a screenshot and the exact source used to produce it.

**Status: implemented at `b153000` and verified on 2026-10-01.** `sagan-render` 0.2.0
adds the `render.canvas` frame lifecycle, centered orthographic transform,
filled circles, line segments, world and screen text, RGB colors, resize-aware
back buffering, DPI-scaled Segoe UI text, `make shape-text-demo`, a BMP capture
test, and the recorded 960 by 540 demo frame. The private backend remains Win32
GDI; SDL3 and Dawn/WebGPU remain deferred backend choices.

**R2 — Snapshot animation.** Consume timestamped, immutable positions from
physics, update shapes and labels once per display frame, and optionally
interpolate between fixed simulation steps. Pause/resume and a basic speed
control are useful if they remain small; the indispensable test is that
render-frame rate cannot alter the simulation's result. **Demo:** proposed
`make two-body-demo` shows the actual physics-driven orbit, readable text,
responsive input, and a clean close. Record its source and a short capture.

**Status: implemented at `8f27ed5` and verified on 2026-10-01.** `sagan-render` 0.3.0
adds monotonic elapsed time and consumed key presses without taking ownership
of physics. The Sagan demo advances `sagan-physics` 0.1.0 only through fixed
300-second solver steps, draws each immutable snapshot, supports pause and
playback-speed controls, and closes through Escape, the window button, or the
test frame limit. Its deterministic test proves that 100 20-millisecond frames
and 20 100-millisecond frames reach the identical 172800-second snapshot. The
recorded 960 by 540 frame comes from the exact checked-in demo source.

**R3 — Lagrange stability display.** Reuse R2 to show eight bodies: the two
massive circular-orbit primaries plus six massless tertiary tracers from P1.
Place one tracer at each of L1 through L5 and one off-point control. Give every
body a distinct color and a trail in the same color. Show each tertiary's
rotating-frame stability error numerically until its snapshot reports escape,
then show `escaped` in place of the number. Use the trails to make the
stable L4/L5 behavior visibly comparable with unstable collinear points and
the off-point control. Do not create a second renderer or allow render timing
to alter physics. **Demo:** proposed `make lagrange-demo` consumes P1's
immutable snapshots and records a representative long-duration frame or
capture. The scene contains eight bodies total, despite the six tertiaries
remaining gravitationally independent of one another.

**Status: implemented at `7603ecc`, corrected at `2e767ff`, and verified on
2026-10-01.** `make lagrange-demo`
reuses `sagan-render` 0.5.1 and `sagan-physics` 0.3.0 without adding another
renderer. It draws all eight bodies with distinct matching-color eight-sample
trails sampled every six simulation hours, plus all six rotating-frame errors.
It begins at five simulated days per real second.
Space, Up/Down, and Escape retain R2 behavior; R resets physics and trails while
preserving the selected, displayed playback rate. The deterministic test
compares two display schedules at the same 60-day physics time, requires
identical final errors, and verifies reset-rate independence; its checked 1180
by 720 capture comes from the exact demo source.

## Physics chat

**P0 — Headless two-body solver.** Define two masses, positions, velocities,
and an explicit time step using native unit-checked values and shared math
types. Compute equal-and-opposite Newtonian forces and advance both bodies
with velocity Verlet. Reject zero separation and invalid mass/time inputs
clearly; collision response and force softening are separate design choices.
Expose immutable snapshots with stable body identities and enough state for
rendering, but no renderer dependency. **Demo:** proposed
`make orbit-numeric-demo` prints a short table
of time and both positions, plus energy, angular momentum, and barycenter
checks. Test a circular-orbit reference and bounded drift over a documented
duration/timestep; specify tolerances instead of claiming exact floating-point
equality across toolchains.

**Status: implemented at `b12bc40` and locally verified on 2026-10-01.**
`sagan-physics` 0.1.0 provides the independently versioned
`physics.two_body` package, fixed-step velocity-Verlet propagation, read-only
body and system snapshots, SI-unit-checked state and diagnostics,
`make orbit-numeric-demo`, a circular-reference drift fixture, and invalid
mass/timestep/overlap tests. The run uses one caller-defined 2D frame and makes
no collision, softening, or cross-toolchain bit-equality promise. Compiler
commit `d97f499` preserves grouping when composite units appear as divisors,
which is required for the solver's inverse-square force calculation.

**P1 — Circular restricted three-body solver.** Keep the P0 massive pair in a
near-circular orbit and advance six independent massless tertiary tracers with
the same fixed-step velocity-Verlet method. Each tracer feels both primaries;
none affects the primaries or another tracer. Place one tracer at each of L1
through L5 and an off-point control. Publish immutable snapshots with the same
identity/position/velocity shape as P0 plus each tracer's distance from its
rotating-frame reference. Verify that the primary pair is bit-identical to a
two-body reference run, remains within P0's conservation tolerances, and that
L4/L5 remain closer to their references than the off-point control over a
documented run. L1, L2, and L3 are equilibrium locations but are not stable;
do not describe all five points as stable. **Demo:** proposed
`make lagrange-numeric-demo` prints a time series comparing all six errors,
then hands the same snapshot contract to R3.

**Status: implemented at `abbc147` and locally verified on 2026-10-01.**
`sagan-physics` 0.3.0 provides `physics.restricted_three_body`, retaining P0's
massive pair while advancing six independent massless tracers. The checked
60-day fixture uses a 300-second step and an Earth-Moon-like circular pair;
the primary snapshots remain bit-identical to a separate P0 reference run.
At day 60, the L4 and L5 rotating-frame errors are approximately 13 micrometres
and 71 micrometres, versus 962 million metres for the off-point control. The
collinear-point traces remain available for comparison without claiming that
L1, L2, or L3 are stable. R3 can consume these immutable snapshots without
adding physics to the renderer.

**P2 — Solar-perturbed Lagrange experiment.** Preserve P1 as the ideal
reference and add a separate planar model in which the Sun, Earth, and Moon
mutually gravitate while the same six massless tracers feel all three massive
bodies. Center the main presentation on the Earth-Moon barycenter and include
a heliocentric inset so both scales remain legible. This is a controlled model
comparison, not an ephemeris claim. **Status: implemented at `1111d4a` and
locally verified on 2026-10-02.** `sagan-physics` 0.3.0 provides checked
numeric and windowed 60-day fixtures; the latter also records a representative
1180 by 720 frame.

## Math chat

**M0 — Minimal orbital math.** Audit the existing `Float64`, `Point2`,
`Vector2`, and native unit behavior before adding anything. Supply only the
missing operations P0 needs: displacement length/squared length, dot product
if used for diagnostics, square root, safe normalization or inverse-length,
and conversions between physical world coordinates and dimensionless display
coordinates at an explicit scale. Keep point-versus-vector distinctions and
unit dimensions intact. Define zero-length and non-finite behavior. **Demo:**
proposed `make orbit-math-demo` runs a small Sagan source file showing the
operations and expected numeric results, including one rejected unit mismatch.
Document rounding/precision limits and each actual public symbol.

**Status: implemented at `779c281` and locally verified on 2026-10-01.**
The automatically available M0 surface now has `sqrt`, `dot`, and
`display_coordinates` as functions, plus `vector.length()`,
`vector.squared_length()`, `vector.normalized()`, and unitless-only
`vector.normalized!()` as core Vector methods. The former standalone vector
forms were removed in development version 4.0.0. The checked
`make orbit-math-demo` source preserves measured point/vector distinctions,
produces the expected 3-4-5 results, and uses an explicit physical display
scale. Focused tests reject a mismatched time scale, zero-length normalization,
non-finite components, and a negative square root. M0 introduces no separate
math package version; it follows the main Sagan version.

**M1 — Optional three-body support.** Prefer no new public API: P1 should
reuse M0's primitives. Add a tested operation only if a measured need appears,
then extend the math demo and docs before P1 depends on it.

## Language chat

**L0 — Library/native boundary.** Verify that an explicitly imported,
independently versioned rendering package can call a narrow native window
bridge and that physics can remain Sagan code where practical. Implement only
compiler, linker, manifest, or runtime support proven missing by R0 or P0;
do not expose arbitrary C++ handles as a shortcut. Make a tiny package demo
that calls through the bridge and prints a recognizable result (proposed
`make bridge-demo`). Document the package/ABI contract, supported compiler
range, and Windows runtime dependencies. Existing hypercore vector, point,
unit, and module features should be reused rather than redesigned.

**L1 — Run and distribute the windowed demo.** Verify the existing
`[application] mode = "windowed"` contract with a manifest-backed orbit
program, command-line execution, and Explorer launch. Package the first-party
libraries and required native/font assets without making non-graphical Sagan
programs depend on them. **Demo:** a documented Bash command runs the project;
double-clicking its configured Windows entry opens the window without an
unwanted console. Test missing-library/resource errors and release contents.
The broader, deferred unified-error redesign is not a prerequisite for this
slice, but new failures must still produce honest, actionable diagnostics.

**L1 automated Windows verification (2026-10-01):** The orbit project's manifest now
selects windowed launch. Normal `sagan --run-package` and source-file execution
link the native bridge from the resolved `sagan-render` package; the Explorer
launcher uses the same path without a console. Windows staging includes both
first-party packages, native bridge source/header, and the orbit demo, while
non-rendering programs compile without the bridge. The two frame-rate runs
produce the same fixed-step snapshot. The staged launcher and portable ZIP
ran the orbit demo with MSYS2 removed from `PATH`; the ZIP checksum, bundled
toolchain, and package files were checked. The actual installer was built, but
a manual Explorer double-click and installation on a clean Windows machine
remain unverified.

## Cross-chat integration order and acceptance

| Step | Depends on | Observable handoff |
| --- | --- | --- |
| L0 then R0 | Current compiler/package foundation | Sagan opens and closes an empty window. |
| M0 and R1 | R0 for drawing; existing numerical types for math | A math source prints checked results; a window shows static circles and text. These can proceed in parallel. |
| P0 | M0 | A headless, unit-aware two-body run produces a tested numerical trace and immutable snapshots. |
| R2 and L1 | R1 and P0 | The same snapshots animate two bodies in a packaged, launchable window. **This is the required finish line.** |
| P1 then R3 | Accepted two-body demo, time and usage remaining | Optional numerical three-body trace and window using the existing contracts. |

The final two-body acceptance run must show the Sagan source, the command,
the moving window with shapes and text, and a numerical output/fixture that
can be checked without the renderer. Closing or pausing the window must not
leave a child process running. Record the exact compiler and library versions,
supported platform, test results, screenshot/capture, and known limitations.
Each chat updates only the contracts it owns and hands a tested version to the
next chat; integration changes should be coordinated rather than independently
rewriting shared types or demo files.
