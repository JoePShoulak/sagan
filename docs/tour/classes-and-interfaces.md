---
title: Classes, faces, composition, and inheritance
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes, faces, composition, and inheritance

Sagan calls an interface a **face**. The two declarations have different jobs:

- a `class` creates a concrete kind of object, stores its data, and implements
  its behavior;
- a `face` names a set of methods and typed fields that an object promises to provide; and
- `is` on a class lists parent classes; `has` explicitly adopts one or more faces.

A face does not create an object by itself. Its typed field promises add
storage to an adopting class, making faces state-providing mixins. It lets a
function say, “I can work with any object that provides this behavior,” without
depending on one particular class.

The examples below are complete programs. Every referenced type and method is
declared in the example, every program produces useful output, and every output
is checked during the documentation build.

Build the compiler once from the repository root before running them:

```bash
make all
```

## 1. A class without a face

Start with a class by itself. `FuelTank` owns a private `fuel` field. Its
constructor initializes that field, `remaining()` reads it, and `burn!()`
changes it.

```sagan
--8<-- "docs/examples/executable/class_fuel_tank.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/class_fuel_tank.stdout"
```

Run it from the repository root:

```bash
bin/sagan docs/examples/executable/class_fuel_tank.sagan
```

This program needs no face because no code needs to accept multiple kinds of
fuel-holding object. The class alone provides storage, construction, and
methods.

## 2. A class fulfilling one face

`Named` declares one requirement: anything used as `Named` must provide
`name(): String`. `Probe` explicitly adopts that contract with `has Named` and
implements the required method.

```sagan
--8<-- "docs/examples/executable/face_contract.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/face_contract.stdout"
```

```bash
bin/sagan docs/examples/executable/face_contract.sagan
```

The important interaction happens at `report_name(probe)`. The function accepts
`Named`, not `Probe`, so it can call only behavior promised by `Named`. Passing
`probe` is valid because `Probe has Named` and supplies the exact required
method. A different class could also be passed if it explicitly adopted and
fulfilled `Named`.

Merely writing a `name()` method is not enough. Sagan requires the class to
declare conformance with `has Named`.

## A face promising a class field

A default method can read a class field through `self` when the face declares
that field as a promise. The face does not store a second copy:

```sagan
--8<-- "docs/examples/executable/face_field_promise.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/face_field_promise.stdout"
```

```bash
bin/sagan docs/examples/executable/face_field_promise.sagan
```

The dot in `let .mass` makes the added field private. `Planet` does not
repeat the declaration, but its constructor must assign the field. Calls
through a `Massive` value can use `getMass()`, but cannot access `.mass`
directly. The promise remains in force even if `Planet` overrides
`getMass()`.

## 3. A face composed from other faces

Faces can build larger contracts from smaller ones. This example declares
every name it uses: `Named` requires a name, `Powered` requires a power level,
and `Spacecraft` composes both faces.

```sagan
--8<-- "docs/examples/executable/face_composition.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/face_composition.stdout"
```

```bash
bin/sagan docs/examples/executable/face_composition.sagan
```

The relationship is:

```text
Named ---------\
                > Spacecraft <----- ExplorerShip
Powered -------/
```

Because `ExplorerShip has Spacecraft`, it must satisfy the requirements inherited
from both `Named` and `Powered`. Its `name()` and `power()` methods do that.

`Spacecraft.status()` is a **default method**: the face supplies its body. The
default can call `self.name()` and `self.power()` because those methods are
requirements of the composed contract. At runtime, those calls dispatch to the
methods implemented by `ExplorerShip`.

`report_status()` depends only on `Spacecraft`. It does not need to know how an
`ExplorerShip` stores its name or power level.

## 4. A face view shares the class object

A class value can be assigned to a face-typed variable when the class explicitly
fulfills that face. The two variables refer to the same object; converting to a
face does not copy the object.

```sagan
--8<-- "docs/examples/executable/composition.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/composition.stdout"
```

```bash
bin/sagan docs/examples/executable/composition.sagan
```

Here, `counter` has the concrete type `MissionCounter`, while `view` has the
face type `Counter`. `view.describe()` uses the face's default method, which
dispatches `self.value()` to `MissionCounter.value()`.

After `counter.increment!()` mutates the class object, calling
`view.describe()` observes the new value. Both variables share one object and
one identity.

## Parent classes and faces

For classes, `is` lists parent classes and `has` lists faces. Both lists can
have multiple entries, with `is` before `has`: `class GunShip is Ship, Aircraft,
has Weapons, Navigable { ... }`. A class can use either clause alone. Inherited
fields and methods come from parent classes; face methods remain checked
behavioral contracts. Faces themselves may compose other faces with `is` or
`has`.

Here is a complete example. `GunShip` inherits from both `Ship` and `Aircraft`,
adopts two faces, passes its `callsign` into `Ship`'s constructor, and overrides
`kind()`. The `Ship`-typed view calls that override on the original object.

```sagan
--8<-- "docs/examples/executable/class_inheritance.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/class_inheritance.stdout"
```

```bash
bin/sagan docs/examples/executable/class_inheritance.sagan
```

`GunShip is Ship` checks a class relationship; `GunShip has Weapons` checks a
face relationship. These are checks between declared type names, not runtime
tests of individual objects.
Inside an override, `super.Ship.kind()` invokes `Ship`'s implementation
directly. The parent name keeps calls unambiguous when a class has multiple
parents. Constructors can provide trailing defaults too, so callers may omit
those arguments while `new(...) is Parent(...)` still forwards the resulting
values.

## Rules to remember

- Faces can promise typed fields. A class adopting the face receives compatible
  storage unless it declares that field itself; its constructor still initializes
  the field.
- A method without a body is a requirement.
- A method with a body is a default implementation.
- A class must explicitly name every face it adopts, directly or transitively.
- The class must implement every requirement not supplied by an unambiguous
  default.
- A class method overrides a face default with the same signature.
- Repeated paths to the same default declaration collapse into one default.
- Competing defaults from different declarations require the class to provide
  an explicit override, even when their bodies are textually identical.
- Class inheritance includes parent fields and methods. Face composition adds
  promised fields and default methods, while required methods still need an
  implementation.
- A face-typed variable refers to the same reference-counted object as the
  concrete class value.

Continue to the
[classes, interfaces, composition, and inheritance reference](../reference/classes-interfaces-composition.md)
for constructors, parent construction, overrides, privacy, generic faces,
default conflicts, ownership, weak fields, and the precise conformance rules.
