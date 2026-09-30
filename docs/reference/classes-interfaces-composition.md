---
title: Classes, interfaces, and composition
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes, interfaces, and composition
Interfaces, spelled `face`, are Sagan's primary composition mechanism.
Interfaces may compose interfaces; classes declare conformance with `is` or
`has`; multiple interfaces are comma-separated; and `self` refers to the
current object.

Composition means building a type from small contracts instead of placing it in
a deep parent/child class hierarchy. Sagan does not provide implementation
inheritance. A leading dot marks a private field or method.

`face` and `class` are top-level declarations.
Both accept `is` or `has` followed by comma-separated interface names. Faces
contain signature-only methods or block-bodied defaults. Classes contain `let`
fields and block-bodied methods. `self` is an expression, and a leading dot on
a class method is retained as private-member syntax in the AST.

A leading dot makes a field or method private to its declaring class. Methods
of that class may access it through `self` or another instance of the same
class. Outside access is rejected, and a private method cannot satisfy a face
requirement.

An unambiguous default is composed into
the class and may call other requirements from the same face through `self`.
An exact-signature class method overrides the default. If multiple directly
composed faces provide the same default signature, the class must provide an
explicit override. This is static composition; it does not create a superclass.

Faces may compose other faces using the same `is` or `has` spelling. Required
signatures and defaults flow transitively to the final class. A face declaration
may replace an inherited default with its own exact-signature declaration or
default. Cyclic face composition is invalid.

Classes are created by calling the class name. A matching `new(...)`
constructor initializes the fields:

```sagan
face Named {
  fun label(): String
}

class Probe is Named {
  let name: String

  new(name: String) {
    self.name = name
  }

  fun label(): String => self.name
}

let voyager = Probe("Voyager")
```

Class and face values have shared reference identity. Calls through a face use
dynamic dispatch, meaning the concrete class implementation runs even when the
variable's declared type is the face. See the [memory model](memory-model.md)
for ownership and weak references.
