---
title: Declarations
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Declarations
**Settled design:** `let` declares mutable variables; `fun` introduces
functions; `face`, `class`, and `enum` introduce named types; and
`module`, `import`, and `export` participate in modular source.

```sagan
let altitude: Float = 125_000.0
```

**Implemented in the parser:** top-level mutable variables, named block-bodied
functions, faces, classes, and simple enums. Faces accept method signatures and
default bodies. Classes accept `let` fields and methods. Enums currently contain
identifier-only members separated by newlines or commas.

**Provisional semantics:** overloading, privacy via a leading member dot, and
conformance declared with interchangeable `is` or `has` composition lists.

**Open questions:** inference requirements, duplicate declarations, scope,
forward references, constructors, enum payloads and explicit values, generic
declarations, module visibility, and entry-point forms. These require semantic
analysis or later parser slices.
