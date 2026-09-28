---
title: Design philosophy
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Design philosophy

## Simulation-oriented, modular by default

Sagan is designed around geometry, numerical simulation, physics, and
rendering. These domains share types and conventions, but programs should not
need to carry the whole simulation stack merely to use the language.

The planned core therefore has three deliberately different inclusion levels:

- **Math** is built in and automatically available.
- **Physics** is a tightly coupled first-party core library and an explicit
  import.
- **Rendering** is a tightly coupled first-party core library and an explicit
  import.

All three are designed as a coherent foundation. Physics and rendering should
interoperate directly with Sagan's mathematical types, while remaining optional
for lightweight applications.
