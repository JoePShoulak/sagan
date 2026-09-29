---
title: Code generation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Code generation
**Status: initial executable subset implemented.**

`bin/sagan --emit-cpp` translates a semantically validated executable unit into
C++. With an output path it writes a `.cpp` file; without one it prints the
translation. `scripts/execution_demo.sh` compiles that file with `g++` and runs
the resulting native executable.

The current subset supports typed functions, scalar literals, plain and
interpolated strings,
local declarations, assignment, calls, grouping, common unary/binary operators,
prefix and postfix numeric increment and decrement, checked integer arithmetic,
dimensioned vector and coordinate values, their spreads, indexing, named components, iteration,
printing and interpolation,
typed expression lambdas with local lexical captures and immediate or stored calls,
classes with typed fields, default or overloaded `new(...)` construction, `self`, and
public/private methods,
statically composed face default methods with explicit conflict resolution,
nominal enums with named output, equality, and match cases,
checked integer and floating-point exponentiation through `^` and `^=`,
conditional expressions, `if`/`else`, `while`/`until`, loop control, and
returns. Homogeneous array literals preserve the element width inferred by the
type checker, array indexing is bounds-checked, and `for … in` iterates values
by local copy. Array spreads evaluate each operand once and append its elements
in source order. The built-in `print(value)`
accepts one checked value, writes its
human-readable form followed by a newline, and lowers to the generated C++
output helper. Generated entry points select UTF-8 console input and output on
Windows, preventing Unicode text from being interpreted through a legacy code
page; other platforms keep their existing UTF-8 environment. Names are
deterministically encoded as legal C++ identifiers, so
Unicode and emoji Sagan names do not depend on a C++ compiler's identifier
spelling support. `main(): Int` maps to C++ `int main()`; `main(): Void` receives
an implicit native success return.

Unsupported AST forms produce a source-located backend diagnostic rather than
silently generating incorrect code. The emitter currently requires explicit
function signatures and supports only function declarations at the program
root.

Homogeneous dictionary literals with backend-supported scalar key and value
types lower to `std::unordered_map`. Key lookup is checked and raises the native
out-of-range exception when a key is absent. Dictionary spreads evaluate each
operand once and apply entries from left to right; later entries replace earlier
values for the same key. Dictionary iteration is not lowered yet.

`match` evaluates its subject exactly once, compares expression-shaped cases in
source order, and executes the first equal case or the final `case else`
fallback. Destructuring and type-pattern matching are not part of the current
language subset.

Integer addition, subtraction, multiplication, division, remainder, negation,
increment, decrement, exponentiation, and their compound-assignment forms use
generated helpers. Integer overflow and zero divisors raise runtime errors;
floating-point remainder uses `fmod` and floating-point division also rejects a
zero divisor.

Vectors and coordinates lower to separate fixed-size native runtime types, so a
coordinate is not silently interchangeable with a vector. Their dimensions and
component types come from the checked semantic model. Vectors support checked
addition, subtraction, unary negation, scalar multiplication and division,
equality, and compound forms. Vector-vector multiplication is deliberately not
assigned an implicit dot, cross, or component-wise meaning. Coordinate
arithmetic and generic source annotations are not implemented. Both dimensioned
families provide `.x`, `.y`, `.z`, and `.w` component access where their
dimension permits it; components are assignable because variables are mutable
by default.

Faces are erased before code generation in the currently implemented,
statically composed subset. Semantic analysis first verifies every required
signature and resolves defaults, then the backend copies inherited default
method bodies into the concrete class. Interface-typed values, dynamic face
dispatch, and runtime face objects are not lowered yet. Transitive defaults are
flattened into the concrete class before native compilation.

Open work includes generated-code structure, runtime interfaces, memory
management, interface dispatch, escaping closures, exception lowering, debug information, compiler selection and flags,
standard-library linkage, platform support, optimization, broader expression
and statement lowering, and deterministic constraints beyond the implemented
numeric subset. Native compiler
invocation remains in the demo script rather than the `sagan` executable.
