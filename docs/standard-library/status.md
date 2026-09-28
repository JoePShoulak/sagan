---
title: Standard-library status
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Standard-library status

The high-level core-library structure is decided, but the libraries are not yet
implemented.

| Core library | Coupling | Inclusion | Status |
| --- | --- | --- | --- |
| Math | Built into Sagan's language foundation | Automatic | Planned |
| Physics | First-party and tightly coupled to math | Explicit import | Planned |
| Rendering | First-party and tightly coupled to math and simulation types | Explicit import | Planned |

Open design work includes concrete APIs, module and package names, dependency
boundaries, initialization behavior, linking strategy, and whether physics and
rendering have smaller independently importable components.
