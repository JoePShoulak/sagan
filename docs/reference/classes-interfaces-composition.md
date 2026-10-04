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
- `is` lists parent classes on a class, while `has` lists adopted faces;
  faces continue to use `is` or `has` for face composition.

Class inheritance and face conformance are separate, explicit relationships.
Inheritance shares superclass state and methods. Conformance promises behavior
without implicit adoption based only on matching method names.

For a guided introduction using only complete, runnable programs, begin with
the [Classes and faces tour](../tour/classes-and-interfaces.md). This page is the
compact rules reference.

## The basic model

`face` and `class` are top-level declarations. A face contains required method
signatures, optional default implementations, and explicit field promises. A class contains fields,
constructors, and implemented methods. `self` refers to the receiving object.

```sagan
face Named {
  fun name(): String
  fun description(): String => "Object: ${self.name()}"
}

class Probe has Named {
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

A face default may also use a field or helper method supplied by the adopting
class. Declare that dependency in the face: an undeclared `self.member` is not
implicitly inferred from the default body.

```sagan
face Massive {
  let .mass: Float64<kilogram>
  fun getMass(): Float64<kilogram> => self.mass
}

class Planet has Massive {
  new(mass: Float64<kilogram>) { self.mass = mass }
}

let earth = Planet(5.97e24 kilogram)
assert(earth.getMass() > 0.0 kilogram)
```

The `let .mass` line is a typed storage promise. Adopting `Massive` adds
one private `mass` field to `Planet`, so the class does not repeat its
declaration. The constructor must still initialize it. A class may explicitly
declare the same field with the same type when it needs a declaration-site
initializer. Compatible promises from multiple faces share one field; differing
required types are rejected. A private face field can be read by its defaults,
but callers through the face cannot access it directly. Public face fields
produce public storage. `const .NAME: String` promises read-only access and
still needs an explicit class field with an initializer, because face fields
do not have inherited initializers. `let` promises mutable access. A class
override of `getMass()` does not remove the field promise. A face can
similarly declare a private required helper such as `fun .compute(): Int`
and call `self.compute()` from a default.

## Declaring composition

On a class, `is` introduces a comma-separated list of parent classes and
`has` introduces a comma-separated list of faces. When both appear, `is` comes
first and the clauses are separated by a comma:

```sagan
class GunShip is Ship, Aircraft, has Weapons, Navigable {
  -- members
}
```

Either clause may appear alone. A class may have multiple parent classes and
multiple faces. A face may still compose other faces with either `is` or `has`:

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

Face composition is transitive: `Probe` must satisfy `Reportable`, `Identified`,
and `Named`, and it may be used through any of those declared faces.

These relationships are queryable in conditions. `GunShip is Ship` checks
class ancestry; `GunShip has Weapons` checks transitive face conformance; and
`Assault has Weapons` checks transitive face composition. The operands here are
declared type names, not object values. These checks yield `Bool`; object
instance type tests are a separate feature.

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

Private class methods cannot satisfy public face requirements, but they can
satisfy private face helper requirements. Parameter and
return types are part of the required signature; an approximately compatible
method is not an override.

Field promises are checked by name, exact type, visibility, and mutability.
Uninitialized or conflicting promises are compile-time errors. Composed faces
may share a field promise only when its required type is the same; the class
gets one suitable field. Face defaults access that field through the face
contract, including calls made through face-typed values.

A composing face may redeclare an inherited signature, with or without a new
default. Cyclic face composition is rejected, so the transitive contract always
has a finite dependency graph.

## Classes, fields, and construction

Classes are created by calling the class name. Constructor arguments select a
matching `new(...)` overload using the same lossless argument-compatibility
rules as function calls:

```sagan
class Probe has Named {
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

The runnable [classes-and-faces examples](../examples/index.md#executable-documentation-examples)
separate class storage, basic conformance, transitive composition, default
dispatch, and shared identity into focused programs. Their expected output is
checked during every documentation build.

## Generic classes and faces

Classes and faces may be parameterized:

```sagan
face Readable<T> {
  fun get(): T
}

class Box<T> has Readable<T> {
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

## Current inheritance limits

Parent classes are constructed before the child body. A child constructor can
pass arguments to each parent in inheritance-list order:

```sagan
new(name: String, altitude: Int) is Ship(name), Aircraft(altitude) {
  -- initialize this class's own fields here
}
```

An omitted parent must be default-constructible. A class with no explicit
constructor needs default-constructible parents. Parent initializer arguments
may use constructor parameters but not `self`, which does not exist until the
parents have been built. A child inherits fields and methods, and a method
with the exact same signature overrides a
parent method with dynamic dispatch through a parent-typed reference. Multiple
paths through the same ancestor share its state (a diamond is not duplicated).
An override may invoke a named direct parent's implementation with
`super.Parent.method(arguments)`. This qualified call bypasses virtual
dispatch; ordinary `self.method()` and calls through parent-typed values remain
dynamic. Naming the parent is required because there may be more than one.
Parent construction instead uses the `new(...) is Parent(...)` forwarding list.
Different parents may not contribute ambiguous fields. If they contribute the
same method from different implementations, the child must implement one
exact-signature override. Constructors are
not inherited. Generic classes may be used as parents, but generic
methods do not participate in virtual dispatch.

Because multiple-inheritance diamonds share one virtual ancestor, an indirect
ancestor must currently be default-constructible when it appears above an
intermediate parent. Sagan diagnoses this rather than emitting native code that
would fail to build. Direct parents can receive arguments through the `new`
forwarding list above.

## What composition deliberately does not provide

- no implicit conformance from method shape alone;
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
