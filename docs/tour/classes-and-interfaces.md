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
  let name: String = "Explorer"

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
identifier members separated by newlines or commas.

The parser records these distinctions but does not yet enforce interface
conformance, privacy, dispatch, or field and method types.

**Open questions:** structural versus explicit conformance, default-method
conflict resolution, constructor rules, enum payloads and explicit values,
value versus reference behavior, visibility enforcement, and whether limited
implementation inheritance will exist.
