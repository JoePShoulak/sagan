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
| Built-in math availability | Settled | Not implemented |
| Math types and operations | Direction settled; APIs open | Not implemented |
| Physics as explicit first-party core library | Settled | Not implemented |
| Rendering as explicit first-party core library | Settled | Not implemented |
| Module and package names | Open | Not implemented |
| General standard-library boundary | Open | Not implemented |
| Basic output intrinsic | Provisional | `print(value)` implemented for the native subset |

No physics, rendering, or general standard-library API should be inferred from
the project's intended domains. Physical units and coordinate frames are not
distinguished by the initial type system; libraries and user-defined types may
model them.

The high-level core-library structure is decided, but the libraries are not yet
implemented.

| Core library | Coupling | Inclusion | Status |
| --- | --- | --- | --- |
| [Math](math.md) | Built into Sagan's language foundation | Automatic | Planned |
| [Physics](physics.md) | First-party and tightly coupled to math | Explicit import | Planned |
| [Rendering](rendering.md) | First-party and tightly coupled to math and simulation types | Explicit import | Planned |

Open design work includes concrete APIs, module and package names, dependency
boundaries, initialization behavior, linking strategy, and whether physics and
rendering have smaller independently importable components.
