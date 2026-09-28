---
title: Semantic analysis
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Semantic analysis
**Status: name resolution and initial scalar type checking implemented.**

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

This pass deliberately does not establish initialization order, overload
signatures, match or exception-pattern binding, types, conversions, interface
conformance, mutation rules, or control-flow correctness. Top-level names are
collected before bodies are visited, so name resolution alone permits forward
and self references; later passes must decide whether those uses are valid.

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

Member access, lambda callability, interface conformance, and several
user-defined-type relationships currently remain `Unknown`. The generic model
and multi-error recovery strategy also remain open.

Type annotations must resolve to built-in, declared, or imported type symbols.
Non-`Void` block-bodied functions must return on every statically guaranteed
path; `if` requires both branches and `match` requires a fallback. Reads before
definite initialization and statements after a guaranteed return are rejected.

Executable validation is separate from ordinary module analysis. An executable
must define exactly one parameterless `main` returning `Int` or `Void`. This
contract is implemented even though code generation has not begun.
