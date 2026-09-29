---
title: Documentation status
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation status
## Roadmap snapshot

| Area | Status | Evidence |
| --- | --- | --- |
| Language direction | **Settled enough for early work** | README design and lexical rules |
| Token vocabulary | **Implemented** | `tokens.hpp`, `tokens.cpp` |
| Tokenizer | **Complete for the current lexical specification** | Unicode-aware lexer, comprehensive self-tests, examples |
| Parser and Sagan AST | **Complete for the current syntax specification** | modules, imports, exports, declarations, functions, types, composition, expressions, collections, control flow, matching, exceptions, documentation, AST renderers, parser demos |
| Semantic analysis | **Executable-subset foundation implemented** | scopes, names, types, lossless widening, calls, nominal enums, class fields and `self`, structural face conformance and defaults, collections, dimensions, definite initialization/returns, unreachable code, entry points, demos |
| Runtime and memory model | **Provisional/planned** | design intent only |
| Standard/core libraries | **Model settled; APIs open** | math is automatic; physics and rendering are explicit first-party imports |
| C++ code generation | **Initial executable subset implemented** | `--emit-cpp`, default-constructed classes with public/private methods, local captured/immediate lambdas, checked arithmetic, typed collections, dimensioned values, loops, matching, native execution demo |
| Deterministic execution | **Numeric foundation implemented** | checked integer arithmetic, division/modulo guards, focused runtime-error fixtures |

## Tokenizer verification

The tokenizer build verifies Unicode 17 XID and emoji identifiers, malformed
UTF-8 rejection, NFC normalization, every current keyword and operator,
focused error cases, and randomized byte-input robustness. Interpretation of
newlines inside ambiguous `<...>` and `{...}` constructs belongs to the parser
and is not unfinished tokenizer behavior.

## What can run today

The `bin/sagan` front-end can print tokens, parse the current grammar, emit text,
DOT, SVG, or interactive HTML ASTs, and print the initial semantic model. The
HTML renderer supports zooming and panning. Parser and semantic demonstrations
provide broad successful source files plus focused malformed examples. The
semantic mode validates names and scopes. Type mode additionally validates the
implemented scalar and function rules. The initial backend can emit C++ for a
validated scalar/function/control-flow subset, and the execution demo compiles
that output with `g++` and runs it.
Entry mode performs the complete implemented checks and requires exactly one
parameterless `main` returning `Int` or `Void`.

The live Codecov report tracks tokenizer lifecycle behavior, Unicode and emoji
edge cases, string escapes, malformed input, parser and semantic errors, all AST
renderers, CLI behavior, and defensive invariants.

## Parser verification

The parser currently verifies `let` declarations, core primary expressions,
the settled operator-precedence table, chained calls/indexing/member access,
ordinary and safe access, mutating method calls, ordinary/raw/multiline/
interpolated strings, and array/dictionary/vector/coordinate expressions with
spreads and trailing commas. Text, DOT, SVG, and interactive HTML tree renderers
cover every implemented AST node. Blocks, same-line single-statement bodies,
ordinary and compound assignment, expression statements, and
`if`/`else if`/`else` control flow are also verified. The parser
enforces declaration-only program roots and provides named, typed, block-bodied
functions as statement containers. `for`/`in`, `while`, and `until` loops,
unlabeled `break` and `continue`, and bare or value-bearing `return` statements
are implemented and rendered in every AST format. Braced or same-line
single-statement `match`/`case`
supports expression-shaped patterns plus a unique final `case else`; pattern
meaning and exhaustiveness remain semantic work. The parser demo includes
`hope`/`unless`/`finally` and value-bearing `scream`; exception matching and
propagation remain semantic/runtime work. Successful source plus focused
expression, collection, control-flow, matching, exception, and
unterminated-block errors are demonstrated. Classes now execute with typed
defaulted fields, zero-argument construction, `self`, field access/mutation, and
ordinary or `!`-suffixed methods. `is` and `has` composition require every face
signature to have an exact class implementation or an unambiguous default. Simple
nominal enums now execute with `Type.member` selection, equality, matching,
interpolation, and readable printing. Face defaults execute, may call other face
requirements through `self`, and require an explicit class override when two
composed faces provide the same signature. Interface-typed values, transitive
face composition, custom constructors, and reference ownership remain future
work. Named functions and methods accept block or `=>` expression bodies.
Typed expression lambdas are callable, may capture local lexical state, and can
be stored in local variables or invoked immediately. Escaping closures and
function-type annotations remain future runtime and type-system work.
Module declarations, import sources and aliases, and standalone exports are
parsed and rendered. Import resolution, visibility, initialization, and package
behavior remain semantic and module-loader work.
Documentation comments attach to supported declarations with retained text and
source spans and appear in every AST renderer. Focused errors cover orphaned,
same-line, executable-statement, and enum-member placements.
Bare and value-bearing `yield` statements and documented enum members complete
the current parser grammar. Payload-bearing enum cases and explicit enum values
remain future semantic and runtime work.

## Major open language questions

Generic annotations, value/reference behavior, reference-count cycles,
interface-typed values, transitive face composition, generics and possible sum types,
custom constructors, payload-bearing enums and explicit enum values, exception propagation, module
resolution and packages, and the concrete math, physics, and rendering APIs
remain unresolved. Math's automatic availability and the explicit-import
status of the first-party physics and rendering libraries are settled.
