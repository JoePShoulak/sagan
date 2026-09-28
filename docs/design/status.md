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
| Semantic analysis | **Executable-subset foundation implemented** | scopes, names, types, lossless widening, calls/returns, literal collections, dimensions, definite initialization/returns, unreachable code, entry points, demos |
| Runtime and memory model | **Provisional/planned** | design intent only |
| Standard/core libraries | **Model settled; APIs open** | math is automatic; physics and rendering are explicit first-party imports |
| C++ code generation | **Initial executable subset implemented** | `--emit-cpp`, output/interpolation, typed collections and collection spreads, loops, expression-pattern matching, native execution demo |
| Deterministic execution | **Goal; contract open** | design intent only |

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
cover every implemented AST node. Blocks, ordinary and compound assignment,
expression statements, and `if`/`else if`/`else` control flow are also verified. The parser
enforces declaration-only program roots and provides named, typed, block-bodied
functions as statement containers. `for`/`in`, `while`, and `until` loops,
unlabeled `break` and `continue`, and bare or value-bearing `return` statements
are implemented and rendered in every AST format. Block-bodied `match`/`case`
supports expression-shaped patterns plus a unique final `case else`; pattern
meaning and exhaustiveness remain semantic work. The parser demo includes
`hope`/`unless`/`finally` and value-bearing `scream`; exception matching and
propagation remain semantic/runtime work. Successful source plus focused
expression, collection, control-flow, matching, exception, and
unterminated-block errors are demonstrated. Faces, classes, simple enums,
interface composition, class fields and methods, private method spelling, and
`self` are parsed and rendered; their conformance and object semantics remain
future work. Named functions and methods accept block or `=>` expression bodies,
and anonymous typed lambdas are represented as expressions; capture and callable
semantics remain future analysis.
Module declarations, import sources and aliases, and standalone exports are
parsed and rendered. Import resolution, visibility, initialization, and package
behavior remain semantic and module-loader work.
Documentation comments attach to supported declarations with retained text and
source spans and appear in every AST renderer. Focused errors cover orphaned,
same-line, executable-statement, and enum-member placements.
Bare and value-bearing `yield` statements and documented enum members complete
the current parser grammar. Generator behavior and enum meaning remain semantic
and runtime work.

## Major open language questions

Generic annotations, member inference, value/reference behavior, reference-count cycles,
interface defaults and conflict resolution, generics and possible sum types,
constructors, enum semantics, exception propagation, entry points, module
resolution and packages, and the concrete math, physics, and rendering APIs
remain unresolved. Math's automatic availability and the explicit-import
status of the first-party physics and rendering libraries are settled.
