---
title: Classes and interfaces
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes and interfaces
**Settled design:** Sagan favors small, composable interfaces over inheritance
hierarchies. `face` introduces an interface and `class` introduces a class.
`is` and `has` are intended as interchangeable conformance words.

```sagan
face Renderable {
  fun render()
}

face Spacecraft is Renderable, Movable {
  fun trajectory(): Vector
}

class ExplorerShip has Spacecraft {
  let name: String

  new(name: String) {
    self.name = name
  }

  fun render() {
    self.renderer.draw(self)
  }

  fun .calculate_internal_state(): Vector {
    return self.position + self.velocity
  }
}

enum GuidanceStatus {
  waiting
  ready
  failed
}
```

**Implemented in the parser:** a `face` contains method signatures or
block-bodied default methods. A `class` contains `let` fields and block-bodied
methods. `is` and `has` introduce interchangeable comma-separated composition
lists on faces and classes. `self` parses as the current-object expression, and
a leading dot marks a private class method in the AST. Simple enums contain
identifier members separated by newlines or commas. Their nominal values use
`EnumName.member` and can be compared, matched, interpolated, and printed.

The initial native class subset checks typed fields and methods, supports field
defaults, typed `new(...)` constructors and checked `ClassName(...)` construction,
types `self`, and executes
field reads, mutation, and method calls. Methods may use a trailing `!` naming
convention to identify a mutating alternative. Leading-dot methods are private
to their declaring class and may be called by its other methods; outside calls
are rejected. Constructor overload selection uses the same lossless argument
compatibility rules as function calls, and every non-defaulted field must be
assigned on every constructor path. Constructors cannot return. Dynamic face dispatch, reference ownership,
payload-bearing enums, and explicit enum values are not implemented yet. Faces
participate in semantic checking: a class using either `is` or `has` must satisfy
every required method with an exact class implementation or default. This is
structural conformance attached to an explicit
declaration, not superclass inheritance. Unambiguous default methods are
composed into the class and may call other face requirements through `self`.
An exact class method overrides a default; competing defaults require an
explicit override. Faces may compose other faces transitively; requirements and
defaults flow through the chain, and cycles are rejected. Face-typed values and
dynamic dispatch are later slices.

**Open questions:** enum payloads and explicit values,
value versus reference behavior, private fields, and whether limited
implementation inheritance will exist.
