---
title: Classes, interfaces, and composition
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes, interfaces, and composition

Sagan separates stored objects from behavioral contracts:

- a `class` defines storage, construction, and implementation;
- a `face` defines behavior that other code may depend on; and
- `is` or `has` explicitly declares composition and conformance.

This is the primary reuse and polymorphism model. There is no class inheritance,
superclass state, superclass constructor, or implicit conformance based only on
matching method names.

## The basic model

`face` and `class` are top-level declarations. A face contains required method
signatures and optional default implementations. A class contains fields,
constructors, and implemented methods. `self` refers to the receiving object.

```sagan
face Named {
  fun name(): String
  fun description(): String => "Object: ${self.name()}"
}

class Probe is Named {
  let name_value: String

  new(name: String) {
    self.name_value = name
  }

  fun name(): String => self.name_value
}
```

`Probe` explicitly promises to satisfy `Named`. Its `name()` method satisfies
the requirement, and it receives the unambiguous `description()` default. That
default dispatches `self.name()` to `Probe.name()`.

## Declaring composition

`is` and `has` have identical language semantics. Either may introduce a
comma-separated list of faces on a class or another face:

```sagan
face Identified {
  fun identifier(): String
}

face Reportable is Identified, Named {
  fun report(): String => "${self.identifier()}: ${self.name()}"
}

class Probe has Reportable {
  -- implementations
}
```

The two spellings let authors express intent in prose, but choosing one does not
change storage, ownership, dispatch, or substitutability. Composition is
transitive: `Probe` must satisfy `Reportable`, `Identified`, and `Named`, and it
may be used through any of those declared faces.

Conformance is both explicit and checked. A class that merely happens to have
the right methods is not a value of that face. Conversely, declaring a face
without supplying every requirement is a compile-time error.

## Requirements, defaults, and conflict resolution

A face method without a body is a requirement. A method with a body is a
default. The compiler resolves each required signature as follows:

1. An exact class method wins and overrides a face default.
2. Otherwise, one unambiguous default is composed into the class.
3. If no implementation or default exists, conformance fails.
4. If multiple composed faces provide competing defaults with the same
   signature, the class must write an explicit override.

Private class methods cannot satisfy public face requirements. Parameter and
return types are part of the required signature; an approximately compatible
method is not an override.

A composing face may redeclare an inherited signature, with or without a new
default. Cyclic face composition is rejected, so the transitive contract always
has a finite dependency graph.

## Classes, fields, and construction

Classes are created by calling the class name. Constructor arguments select a
matching `new(...)` overload using the same lossless argument-compatibility
rules as function calls:

```sagan
class Probe is Named {
  let name_value: String
  let samples: Int = 0

  new(name: String) {
    self.name_value = name
  }

  fun name(): String => self.name_value
}

let voyager = Probe("Voyager")
```

Every field without a declaration initializer must be assigned on every path
through every constructor. Constructors cannot return a value. A class with no
required initialization may be called with `ClassName()`.

Fields declared with `let` are mutable by default. A `const` field requires a
type and initializer and cannot be reassigned. A method name ending in `!` is a
visible naming convention for mutation; the compiler also uses it when
enforcing mutation through a `const` view.

## Privacy

A leading dot makes a field or method private to its declaring class:

```sagan
class Counter {
  let .count: Int = 0

  fun .advance!(): Void {
    self.count++
  }
}
```

Methods of `Counter` may access a private member through `self` or another
`Counter` instance. Code outside the declaring class may not. Privacy does not
flow through faces, and a private method cannot implement a face requirement.

## Face values and dynamic dispatch

A class value converts to a face only when its declaration names that face
directly or through transitive composition:

```sagan
let probe = Probe("Voyager")
let named: Named = probe
print(named.description())
```

The face value and class value share the same object identity. Calling a
required method through `named` dynamically selects the concrete `Probe`
implementation. Mutating the object through one reference is visible through
the other. Face conversion does not copy, slice, or wrap the object as an
independent value.

The runnable [composition example](../examples/executable/composition.sagan)
demonstrates default dispatch and shared identity, and its expected output is
checked during every documentation build.

## Generic classes and faces

Classes and faces may be parameterized:

```sagan
face Readable<T> {
  fun get(): T
}

class Box<T> is Readable<T> {
  let value: T

  new(value: T) {
    self.value = value
  }

  fun get(): T => self.value
}

let box = Box(42)
let readable: Readable<Int8> = box
```

Constructor arguments can infer a class specialization. Generic parameters are
invariant, and face conformance is checked after substituting the class's type
arguments. Function and class type parameters may use `is Face` constraints.
Face methods cannot introduce independent method-level generic parameters.

## Ownership and weak fields

Class and face values use shared reference counting. An all-strong ownership
cycle would keep itself alive, so the compiler rejects statically visible
all-strong class cycles and strong face fields. Put the non-owning back edge in
a `weak let` field:

```sagan
class Parent {
  let child: Child
}

class Child {
  weak let parent: Parent
}
```

A weak field needs an explicit class or face type, has no initializer, and
begins empty. Assignment accepts a strong value. Reading returns `Optional<T>`:
a live target is `Some(target)` and an expired target is `None`. See the
[memory model](memory-model.md) for safe access, coalescing, closure capture,
and deferred ownership features.

## What composition deliberately does not provide

- no implementation or storage inheritance between classes;
- no implicit conformance from method shape alone;
- no superclass calls or constructor chaining;
- no automatic resolution of competing defaults;
- no borrowing, user-visible retain/release operations, or cycle collector;
- no variance between generic specializations; and
- no contextual `self` capture by escaping closures.

These limits keep object layout, ownership, and behavioral dependencies visible.
They also mean that changes to a widely composed face can affect every
conforming class. Adding a requirement is therefore a compatibility change
unless an unambiguous default preserves existing conformers.

## Tooling support

The semantic index records face declarations, explicit conformances, concrete
implementations, member identities, and inferred types. The language server
uses those identities for hover, definition, implementations, references,
rename, completion, semantic tokens, and type hierarchy. Tooling queries do not
define conformance independently; they report the compiler's checked model.

When changing composition behavior, maintainers should update the parser,
semantic/type rules, C++ lowering and runtime dispatch, semantic index and LSP
queries, positive and negative fixtures, this reference, and the language tour.
The repository-wide checklist is in the
[technology stack and change map](../implementation/technology-stack.md).
