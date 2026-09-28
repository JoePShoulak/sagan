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

**Implemented lexically:** declaration keywords, identifiers, colons, assignment,
literals, and newline tokens.

**Provisional design:** explicit types, initialization, function signatures,
overloading, privacy via a leading member dot, and conformance with `is` or
`has`.

**Open questions:** inference requirements, duplicate declarations, scope,
forward references, constructors, enum members, generic declarations, module
visibility, and entry-point forms. These require parser and semantic analysis.
