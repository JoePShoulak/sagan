---
title: Type system
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Type system
Sagan is designed as a strongly and statically typed language.

**Settled design:**

- implicit conversion is limited to conversions proven lossless;
- variables are mutable by default;
- interfaces are central to abstraction and polymorphism;
- physical units and coordinate frames are not distinct in the initial type system;
- no separate character-literal type is planned; and
- math types form built-in vocabulary.

**Implemented foundation:** `Int` is the canonical integer spelling. Literals
use the smallest fitting signed width from `Int8` through `Int64`; otherwise
`Int` defaults to `Int64`. Floating literals and unconstrained `Float` positions
default to `Float64`, with explicit `Float32` available. Only provably lossless
widening is implicit. Core scalar declarations, operators, conditions, calls,
overloads, and returns are checked. Variables need either an annotation or an
initializer.

Array literals infer invariant `Array<Element>` types, while dictionary literals
infer `Dictionary<Key, Value>` with homogeneous keys and values. Spreads must
provide compatible collections. Empty arrays and dictionaries cannot yet be
checked because contextual collection construction is not implemented. Cartesian
vector and point literals infer their dimension and common numeric component
type, for example `Vector3<Float64>` and `Point3<Float64>`; dimensions must
match for compatibility. Three-component spherical literals infer
`SphericalVector3<Component>` or `SphericalPoint3<Component>`.
The native backend preserves the vector-versus-point and representation distinctions and the
inferred dimension and component type. Generic dimensioned annotations are not
yet available in source code.
Named `.x`, `.y`, `.z`, and `.w` access on Cartesian values has the inferred component type and is
accepted only when the dimension contains that component. These members are
assignable and compound assignment preserves the normal lossless-conversion
rules.
Spherical points instead expose `.radius`, `.inclination`, and `.azimuth`;
spherical vectors expose `.magnitude`, `.inclination`, and `.azimuth`. Angles
are radians. Spherical arithmetic and Cartesian conversion are not yet
implemented and will be defined with the core math geometry API after 1.0.
Vector arithmetic requires equal dimensions and infers the lossless common
component type. Scalar multiplication and division likewise widen the component
type when necessary; compound assignment rejects a result that cannot be stored
losslessly in its target vector.
Points are affine locations, while vectors are directions or displacements.
Translating a point by a same-dimension vector returns a point;
subtracting points returns a vector. Point addition, vector-first
point translation, scaling, negation, and context-free conversion between the
families are rejected. These rules preserve the same lossless component
widening and checked integer arithmetic as vector operations.

**Implemented for the native numeric subset:** integer arithmetic overflow
raises the nominal `RuntimeError.integer_overflow` exception. Other native
numeric domain failures and collection lookup failures use the corresponding
`RuntimeError` case. Typed collections and interfaces continue to have
provisional semantics outside the implemented subset.

Generic enum annotations and inferred top-level generic functions are
implemented. Generic classes infer invariant type arguments from constructors,
and generic face conformance substitutes those arguments through required
method signatures. Function inference is call-site based and requires every type
parameter to appear in an inferable parameter position unless the call supplies
explicit type arguments; inference does not use the expected return type.
Class-level generic methods use the same argument-driven
inference after substituting their class specialization. Generic face defaults
may use the face's parameters, but face methods cannot add independent generic
parameters. Explicit function, method, and constructor arguments are supported.
Function and class parameters may declare `is` constraints naming a face;
specialized face conformance is checked for every inferred or explicit concrete
argument. Variance remains open.

**Open questions:** user-defined member inference, value versus
reference categories, broader generic semantics, variance,
compile-time constants, and representation. Deferred cases are currently
marked `Unknown` by the type model.

Annotations must name types. Typed variables may begin uninitialized, but they
must be definitely assigned before use; compound assignment counts as a read.
Non-`Void` functions must return along every guaranteed path, and statements
after a guaranteed return are rejected as unreachable.
