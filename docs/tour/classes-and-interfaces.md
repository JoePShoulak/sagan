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
  fun render() {
    // intended implementation
  }
}
```

`self` names the current object. A leading dot on a member is intended to mark
it private. `is` is also intended for type or conformance tests.

**Open questions:** structural versus explicit conformance, default methods,
conflict resolution, constructor rules, value versus reference behavior, and
whether limited implementation inheritance will exist.
