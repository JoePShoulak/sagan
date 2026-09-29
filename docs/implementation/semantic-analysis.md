---
title: Semantic analysis
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Semantic analysis
**Status: executable-subset name resolution, type checking, and control-flow
validation implemented.**

The first semantic pass consumes the source-spanned AST and produces a printable
semantic model. It creates program and nested lexical scopes, installs built-in
type names, collects declarations, resolves identifier references, and reports
duplicate declarations or undefined names with source locations.

Functions form overload groups, while other duplicate names in one scope are
rejected. Function and lambda parameters, local declarations, loop variables,
type members, enum members, imports, exports, composition references, and
`self` participate in the current traversal. Built-in type symbols currently
include `Bool`, `Coordinate`, `Float`, `Float32`, `Float64`, `Frame`, `Int`,
`String`, `Vector`, and `Void`.

Name collection deliberately remains separate from type and control-flow
checking. Top-level names are collected before bodies are visited, permitting
forward and self references. Later checking establishes overload signatures,
types, lossless conversions, definite initialization, definite returns, and
unreachable code. Interface-typed values, exception-pattern binding, private
fields, and reference ownership remain future work.

The initial type checker infers scalar literals; validates annotations,
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
Vectors and coordinates require numeric components and carry dimension plus
component type, such as `Vector3<Float64>`; compatibility requires equal
dimensions and lossless component widening.

Dimensioned `.x`, `.y`, `.z`, and `.w` members resolve to their component type
when present. Typed expression lambdas retain parameter and result signatures
through local bindings; stored and immediate calls validate arity and lossless
argument compatibility. Simple enum members resolve through `Type.member` to
their nominal enum type; equality and `match` require matching enum types, and
unknown members are rejected. Class metadata supplies typed field and method access,
checked overload selection for `new(...)` constructors, default construction
when every field has a default, and `self` typing. Leading-dot private
methods are callable only while checking another method of their declaring
class, and private methods cannot satisfy public face requirements. Unknown
class members and unmatched constructor calls are rejected. Every field without
a declaration-site default must be assigned on every constructor path.
Escaping closure types and interface values remain open, but a class declaring `is` or `has` a face must satisfy
every required method with an exact class implementation or face default.
Both composition words have identical meaning and do not create inheritance.
An unambiguous face default satisfies its own requirement and becomes a class
method. A class method with the exact signature overrides it. Multiple composed
defaults with the same signature require an explicit class override. Within a
default body, `self` may call other methods declared by that face.
Faces may compose other faces. Their requirements and defaults are resolved
transitively before classes are validated, and composition cycles are rejected.
The generic model and multi-error recovery strategy also remain open.

Type annotations must resolve to built-in, declared, or imported type symbols.
Non-`Void` block-bodied functions must return on every statically guaranteed
path; `if` requires both branches and `match` requires a fallback. Reads before
definite initialization and statements after a guaranteed return are rejected.

Executable validation is separate from ordinary module analysis. An executable
must define exactly one parameterless `main` returning `Int` or `Void`. This
contract is implemented and feeds the initial C++ backend.
