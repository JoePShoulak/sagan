---
title: Native units of measure
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Native units of measure

This document is the implemented pre-1.0 contract for Sagan's native unit
system. The compiler front end, native C++ backend, focused self-tests, and
`make units-demo` exercise the verified slice described here.

## Syntax

Dimensions, named quantities, linear units, and affine units are top-level
declarations:

```sagan
dimension Length
dimension Time

quantity Speed = Length / Time

unit meter: Length = base {
  symbol: "m"
  prefixes: [kilo, centi, milli, micro]
}

unit hour = 3600 * second {
  symbol: "h"
}

affine unit Celsius: Temperature {
  canonical: Kelvin
  scale: 1
  offset: 27315 / 100
  symbol: "°C"
  difference_symbol: "Δ°C"
}
```

Simple unit suffixes are adjacent names. Composite suffixes are parenthesized
so ordinary division remains unambiguous:

```sagan
let distance = 10 meter
let velocity = <10, 2, 0> (meter / second)
```

Unit-bearing scalar and geometry annotations use unit arguments:

```sagan
let distance: Float64<meter> = 10 kilometer
let velocity: Vector3<Float64, meter / second> =
  <10, 2, 0> (meter / second)
```

`value as unit` performs an explicit compatible conversion. Assignment,
argument passing, and return context may perform the same compatible conversion;
a future strict mode may narrow those implicit cases.

Unit-bearing annotations are valid anywhere Sagan accepts a type annotation,
including function, method, constructor, and lambda parameters and results;
fields and local bindings; generic arguments; and face requirements. Calls may
contextually convert compatible concrete units to the parameter unit. Signature
and face-conformance checking includes dimension, named quantity, concrete unit,
and affine category, so an incompatible unit is rejected before code generation.

Affine differences use `Delta<Unit>`. Greek capital delta (`Δ`, U+0394) is a
source-level synonym, so `Δ<Celsius>` and `Delta<Celsius>` are identical.

## Semantic model

A measured type keeps these independent properties:

- numeric representation;
- canonical dimension exponents;
- optional named quantity identity;
- concrete unit identity and an exact symbolic scale;
- exact symbolic affine offset; and
- unitless, linear, affine-point, or affine-difference category.

Canonical exponent maps use stable dimension identities and exact integer
exponents. Scale and offset use reduced rational coefficients plus exact decimal
and integer powers of π, allowing both decimal scientific constants and angle
conversion without baking floating approximations into the type model. Statically known
unit metadata is erased or encoded only in zero-size native type information;
it is not stored beside every runtime value.

Named quantity identity is stricter than dimensional equivalence. Unique
derivations such as `Length / Time` may infer `Speed`. An ambiguous result such
as `Force * Length` remains an unnamed dimensional value until expected context
selects `Energy`, `Torque`, or another declared quantity. Values already named
as Energy and Torque are incompatible even though their exponent maps match.

## Arithmetic

Linear addition and subtraction require dimensional and quantity compatibility;
the right operand converts to the left operand's concrete unit. Multiplication,
division, and integer powers combine and canonicalize exponents. Unitless
numeric behavior remains unchanged.

Affine operations are fixed:

- point minus point produces a difference;
- point plus or minus a difference produces a point;
- difference plus or minus difference produces a difference;
- point plus point is invalid;
- difference times a scalar produces a difference; and
- an affine point cannot participate in multiplication or division.

Absolute conversion uses `canonical = value * scale + offset`; difference
conversion ignores the offset. Existing spatial `Point` and `Vector` arithmetic
uses the same affine model while preserving its current geometry rules.

## Verified implementation

The implementation covers all seven SI base dimensions plus an explicit angle
dimension; common mechanical, electromagnetic, photometric, radiological, and
chemical quantities; all current SI decimal prefixes; a broad built-in unit
catalog; scalar and geometry inference; exact compatible conversion; affine
temperature points and differences; user-defined dimensions, quantities,
linear/affine units, aliases, and selected prefixes; and unit-constrained
functions, methods, constructors, lambdas, fields, locals, generics, and faces.
The complete spelling table is maintained in the
[built-in unit catalog](../reference/units.md).

Dynamic runtime units, logarithmic units, strict-mode policy, heterogeneous-unit
matrices, serialization formats, external unit packages, non-integer dimension
exponents, and generalized coordinate-frame units are deferred.
