---
title: Standard-library status
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Standard-library status
| Area | Design status | Implementation status |
| --- | --- | --- |
| Built-in math availability | Decided | M0 operations implemented automatically |
| Math types and operations | Direction settled; wider APIs open | Minimal orbital M0 implemented |
| Physics as explicit first-party core library | Decided | P0/P1 implemented as `sagan-physics` 0.2.0 |
| Rendering as explicit first-party core library | Decided | Not implemented |
| Module and package names | Open | Not implemented |
| General standard-library boundary | Open | Not implemented |
| Basic output intrinsic | Provisional | `print(value)` implemented for the native subset |

No physics, rendering, or general standard-library API beyond each library's
documented implementation section should be inferred from the project's
intended domains. Physical units are implemented by the language type system
and available without importing physics; coordinate-frame identity remains
deferred. The physics library builds on the native unit model rather than
maintaining a parallel runtime wrapper.

The high-level core-library structure is decided. Math has its first narrow
implementation slice, and physics has independently versioned P0 and P1
slices; wider math, physics, and rendering work remains staged.

| Core library | Coupling | Inclusion | Status |
| --- | --- | --- | --- |
| [Math](math.md) | Built into Sagan's language foundation | Automatic | M0 implemented |
| [Physics](physics.md) | First-party and tightly coupled to math; independently versioned | Explicit import | P0/P1 implemented in 0.2.0 |
| [Rendering](rendering.md) | First-party and tightly coupled to math and simulation types | Explicit import | Planned |

Open design work includes concrete APIs, module and package names, dependency
boundaries, initialization behavior, linking strategy, and whether physics and
rendering have smaller independently importable components.

The [two-body window roadmap](two-body-window-roadmap.md) is the first
cross-library implementation plan. Only milestones carrying an implementation
status and checked commit are completed features.
