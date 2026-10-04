---
title: Classes and faces
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes and faces

Sagan calls an interface a **face**. The two declarations have different jobs:

- a `class` creates a concrete kind of object, stores its data, and implements
  its behavior;
- a `face` names a set of methods that an object promises to provide; and
- `is` or `has` on a class explicitly declares that the class fulfills one or
  more faces.

A face does not create an object, add fields, or supply hidden state. It lets a
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
`name(): String`. `Probe` explicitly adopts that contract with `is Named` and
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
`probe` is valid because `Probe is Named` and supplies the exact required
method. A different class could also be passed if it explicitly adopted and
fulfilled `Named`.

Merely writing a `name()` method is not enough. Sagan requires the class to
declare conformance with `is Named` or `has Named`.

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

Because `ExplorerShip is Spacecraft`, it must satisfy the requirements inherited
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

## `is` and `has`

`is` and `has` have identical language semantics. Either introduces a
comma-separated list of faces on a class or another face. Replacing
`ExplorerShip is Spacecraft` with `ExplorerShip has Spacecraft` in the complete
composition example above would not change the program. The spelling can
express how the declaration reads to the author, but it does not change
storage, ownership, dispatch, or substitutability.

## Rules to remember

- Faces declare methods, not stored fields.
- A method without a body is a requirement.
- A method with a body is a default implementation.
- A class must explicitly name every face it adopts, directly or transitively.
- The class must implement every requirement not supplied by an unambiguous
  default.
- A class method overrides a face default with the same signature.
- Competing defaults require the class to provide an explicit override.
- Face composition is not class inheritance: no class fields, constructors, or
  implementation state are inherited from another class.
- A face-typed variable refers to the same reference-counted object as the
  concrete class value.

Continue to the
[classes, faces, and composition reference](../reference/classes-interfaces-composition.md)
for constructors, privacy, generic faces, default conflicts, ownership, weak
fields, and the precise conformance rules.
