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

**Implemented syntax:** the parser accepts a single optional leading `module`
declaration, imports with optional `from` and `as` clauses, and standalone
exports with an optional alias. See [Modules](modules.md) for the accepted
forms.

`///` and `/** ... */` documentation comments attach to the declaration that
immediately follows them. The AST preserves each comment separately with its
source span and text. Supported targets are modules, imports, exports, `let`,
`fun`, `face`, `class`, and `enum` declarations, including fields, methods, and
local variables. Individual enum-member documentation remains future syntax.

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
declarations, module resolution and visibility, and entry-point forms. These require semantic
analysis or later parser slices.
