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
conditional expressions, `if`/`else`, `while`/`until`, loop control, and
returns. Homogeneous array literals preserve the element width inferred by the
type checker, array indexing is bounds-checked, and `for … in` iterates values
by local copy. Array spreads are not lowered yet. The built-in `print(value)`
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

Open work includes generated-code structure, runtime interfaces, memory
management, exception lowering, debug information, compiler selection and flags,
standard-library linkage, platform support, optimization, broader expression
and statement lowering, and deterministic numeric constraints. Native compiler
invocation remains in the demo script rather than the `sagan` executable.
