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
local variables and individual enum members.

```sagan
let altitude: Float = 125_000.0
```

**Implemented in the parser:** top-level mutable variables, named block-bodied
functions, faces, classes, and simple enums. Faces accept method signatures and
default bodies. Classes accept `let` fields and methods. Enums contain
identifier-only members separated by newlines or commas. The initial executable
enum subset uses `EnumName.member` to produce nominal values; those values
support same-enum equality, `match` cases, interpolation, and printing by member
name.

**Implemented native class subset:** class fields require explicit types and may
have default initializers. Calling `ClassName()` constructs an instance from
those defaults; constructor arguments and custom constructors are not yet
defined. `self` resolves to the current instance. Fields can be read or mutated,
and block- or expression-bodied methods execute. A trailing `!` is allowed only
on methods and conventionally identifies a mutating alternative; it does not by
itself change dispatch or mutation rules. A leading dot makes a method private
to its declaring class; it remains callable from other methods of that class
but cannot be accessed externally or used to satisfy a face requirement.

**Implemented conformance subset:** `is` and `has` are interchangeable and do
not denote inheritance. A class composing a face must satisfy each required
method with an exact class implementation or unambiguous default. Missing or
incompatible methods are compile-time errors.

**Implemented face defaults:** an unambiguous default is inherited, an exact
class method overrides it, and competing defaults with the same signature
require an explicit class override. Defaults may call other requirements from
their own face through `self`.
Face composition is transitive: inherited requirements and defaults flow into
the composing face and ultimately into its classes. Cycles are rejected.

**Provisional semantics:** method overloading, private fields, interface-typed
values, and runtime dispatch.

**Open questions:** inference requirements, duplicate declarations, scope,
forward references, constructors, enum payloads and explicit values, generic
declarations, module resolution and visibility, and entry-point forms. These require semantic
analysis or future language revisions.
