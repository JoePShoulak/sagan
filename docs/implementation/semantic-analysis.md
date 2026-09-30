---
title: Semantic analysis
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Semantic analysis
The semantic passes consume the source-spanned AST and produce a printable
semantic model. It creates program and nested lexical scopes, installs built-in
type names, collects declarations, resolves identifier references, and reports
duplicate declarations or undefined names with source locations.

Functions form overload groups, while other duplicate names in one scope are
rejected. Function and lambda parameters, local declarations, loop variables,
type members, enum members, imports, exports, composition references, and
`self` participate in the current traversal. Built-in type symbols currently
include `Bool`, `Float`, `Float32`, `Float64`, `Frame`, `Int`, `Point`,
`SphericalPoint`, `SphericalVector`, `String`, `Vector`, and `Void`.

Name collection deliberately remains separate from type and control-flow
checking. Top-level names are collected before bodies are visited, permitting
forward and self references. Later checking establishes overload signatures,
types, lossless conversions, definite initialization, definite returns, and
unreachable code. Weak fields are restricted to explicitly annotated class or
face types, reject declaration initializers, accept strong values on assignment,
and type reads as `Optional<T>`. Exception-pattern binding, automatic cycle handling,
and borrowing remain future work.

The type checker infers scalar literals; validates annotations,
initializers, assignments, Boolean conditions, conditional branches, core
operators, calls, overload selection, and returns; and visits every current
statement family.

`Int` is the source spelling. Literals and initialized `Int` variables use the
smallest fitting signed `Int8`, `Int16`, `Int32`, or `Int64`; non-inferable
`Int` positions default to `Int64`. Floating literals and non-inferable `Float`
positions default to `Float64`, while `Float32` may be explicit. Only widening
that preserves every source value is implicit. A variable with neither an
annotation nor initializer is rejected; a typed uninitialized variable is
valid.

Array literals infer one losslessly widened element type, dictionary literals
infer homogeneous key and value types, and indexing returns the stored type.
Their empty forms are rejected until explicit generic annotations exist.
Vectors and points require numeric components and carry dimension plus component
type, such as `Vector3<Float64>` or `Point3<Float64>`; compatibility requires
equal dimensions and lossless component widening. Spherical forms are distinct
three-dimensional families with radial/angular named components.

Dimensioned `.x`, `.y`, `.z`, and `.w` members resolve to their component type
when present. Typed expression lambdas retain parameter and result signatures
through local bindings; stored and immediate calls validate arity and lossless
argument compatibility. Simple enum members resolve through `Type.member` to
their nominal enum type; equality and `match` require matching enum types, and
unknown members are rejected. Class metadata supplies typed field and method access,
checked overload selection for `new(...)` constructors, default construction
when every field has a default, and `self` typing. Leading-dot private
methods are callable only while checking another method of their declaring
class, and private methods cannot satisfy public face requirements. Leading-dot
fields use the same declaring-class access boundary. Unknown
class members and unmatched constructor calls are rejected. Every field without
a declaration-site default must be assigned on every constructor path.
Face annotations accept a class value only when that class declares transitive
`is` or `has` conformance; calls through the face use its checked method set.
Function types and escaping shared-reference closure captures are implemented.
Contextual `self` capture remains deferred and is rejected explicitly. A class declaring `is` or `has` a face must satisfy
every required method with an exact class implementation or face default.
Both composition words have identical meaning and do not create inheritance.
An unambiguous face default satisfies its own requirement and becomes a class
method. A class method with the exact signature overrides it. Multiple composed
defaults with the same signature require an explicit class override. Within a
default body, `self` may call other methods declared by that face.
Faces may compose other faces. Their requirements and defaults are resolved
transitively before classes are validated, and composition cycles are rejected.
Generic enum cases substitute declared payload parameters from either an
expected result type or explicit `Enum<...>.Case(...)` qualification. Top-level
generic calls infer type arguments structurally from their arguments, then
check the instantiated parameter and return types. Constraints, generic
members/types beyond the implemented class and face subset, and the multi-error
recovery strategy remain open. Generic class construction infers parameters
from constructor arguments or an expected type. Member access substitutes
specialized field and method types, while generic face conformance checks the
instantiated signature and remains invariant. Generic face defaults are checked
after substituting the face specialization. Class method calls infer any
method-specific parameters from their arguments after class substitution.

Type annotations must resolve to built-in, declared, or imported type symbols.
Non-`Void` block-bodied functions must return on every statically guaranteed
path; `if` requires both branches and `match` requires a fallback. Reads before
definite initialization and statements after a guaranteed return are rejected.

Executable validation is separate from ordinary module analysis. An executable
must define exactly one parameterless `main` returning `Int` or `Void`. This
contract is implemented and feeds the C++ backend.
