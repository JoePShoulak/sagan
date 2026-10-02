---
title: Classes and interfaces
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes and interfaces
Sagan favors small, composable interfaces over inheritance hierarchies. An
interface describes behavior without choosing how it is stored. Sagan calls an
interface a `face`; `class` introduces stored objects. `is` and `has` are
interchangeable words for declaring that a class or face provides other faces.

The important idea is dependency direction: code asks for the smallest face it
needs, while a class can assemble several faces into one concrete object. This
makes behavior reusable without forcing unrelated objects into one family tree.
It also makes dependencies and test substitutes easier to see: a function that
accepts `Renderable` does not need to know whether the value is a spacecraft,
plot, or future simulation view.

```sagan
face Renderable {
  fun render()
}

face Spacecraft is Renderable, Movable {
  fun trajectory(): Vector
}

class ExplorerShip has Spacecraft {
  let .name: String

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

A `face` contains method signatures or
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
assigned on every constructor path. Constructors cannot return. Payload-bearing
enums and explicit signed `Int64` enum values construct and execute. Faces
participate in semantic checking: a class using either `is` or `has` must satisfy
every required method with an exact class implementation or default. This is
structural conformance attached to an explicit
declaration, not superclass inheritance. Unambiguous default methods are
composed into the class and may call other face requirements through `self`.
An exact class method overrides a default; competing defaults require an
explicit override. Faces may compose other faces transitively; requirements and
defaults flow through the chain, and cycles are rejected. Face-typed values and
dynamic dispatch execute through shared reference-counted objects. A class may
convert only to a face named by its declared transitive composition; matching
method shapes without declared conformance are insufficient. Leading-dot fields
and methods are accessible only from within their declaring class.

Weak class fields use `weak let`, start empty, accept a strong class or face
value, and return `Optional<T>` when read. Use them to break ownership cycles;
expired targets read as `None`.

All-strong reference cycles are not collected automatically; use `weak let` for
the back edge of an ownership relationship. Borrowing and implementation
inheritance are outside the 1.0 language.

Continue to the full
[classes, interfaces, and composition reference](../reference/classes-interfaces-composition.md)
for constructors, privacy, defaults and conflicts, transitive and generic
composition, face-valued dispatch, ownership, tooling support, and the
compatibility implications of changing a face.
