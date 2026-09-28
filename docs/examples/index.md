---
title: Examples
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Examples
## Tokenizer demonstration

`examples/tokenizer_demo.sagan` is the representative source fixture. It
covers modules and imports, interfaces and classes, function forms, mutating
method names, typed declarations, numeric separators and scientific notation,
vectors and coordinates, dictionaries, raw and multiline strings, nested
interpolation, exception keywords, increment, and an emoji identifier.

```bash
make demo
```

The file is checked as tokenizer input. It is not an executable program and is
not evidence of successful parsing, semantic validation, or execution.

## Parser and visual AST demonstration

`examples/parser_demo.sagan` exercises the parser features implemented so far,
including modules, imports, aliases, exports, precedence, right-associative
exponentiation and assignment, compound assignment statements, the
conditional expression, indexing, ordinary and safe member access, function and
method calls, mutating method names, and deep postfix chains.

`examples/module_error.sagan` demonstrates the focused diagnostic produced when
a module declaration appears after another top-level declaration.

`examples/compound_assignment_error.sagan` demonstrates a missing right-hand
value after a compound-assignment operator.

The parser demo includes line and block documentation comments on a module,
face, class, field, method, function, and local variable.
`examples/documentation_error.sagan` demonstrates rejection of a documentation
comment placed before executable control flow.

The success demo also includes a documented enum member and a generator-shaped
function using `yield`, so both appear in text and interactive visual AST output.
It also includes ordinary, raw, multiline, and interpolated strings, including
interpolations containing member access and arithmetic expressions.
The collection section demonstrates arrays, dictionaries with expression keys,
vectors, coordinates, spreads, trailing commas, and multiline formatting.
Its control-flow section adds nested blocks, declarations, ordinary assignment,
expression statements, an `if`/`else if`/`else` chain, all three loop forms,
`break`, `continue`, both return forms, and `match`/`case` with a fallback inside
a typed, block-bodied function. It also demonstrates multiple `unless` handlers,
`scream`, and `finally` cleanup around a protected `hope` block.
The top of the fixture demonstrates composed faces, signatures and default
methods, a class with fields, public and private methods, `self`, and a simple
enum. It also contains a multiline expression-bodied function and an anonymous
typed lambda.

```bash
bash scripts/parser_demo.sh
bash scripts/ast_demo.sh
```

The first command prints the successful tree and confirms that every focused
error fixture fails as intended. Those fixtures cover general parse errors,
postfix/member access, strings and interpolation, collections, blocks, control
flow, matching, exceptions, types and declarations, lambdas, module placement,
compound assignment, and documentation-comment placement. The second opens a
self-contained page showing the input source beside a colored tree. The page
supports button and mouse-wheel zoom plus drag-to-pan navigation. Use
`bash scripts/ast_demo.sh --no-open` to generate `build/ast-demo.html` without
launching a browser.

## Semantic-analysis demonstration

`examples/semantic_demo.sagan` exercises program, type, function, block, and
branch scopes; built-in and user-defined type names; parameters; local and
top-level variables; functions; composition; `self`; and resolved references.

```bash
bash scripts/semantic_demo.sh
```

The script prints the input's scope/symbol/resolution model, then confirms that
`examples/semantic_undefined_error.sagan` and
`examples/semantic_duplicate_error.sagan` fail with focused diagnostics. This
demonstrates the first semantic pass only; it does not type-check or execute the
source.

## Type-checking demonstration

`examples/type_demo.sagan` demonstrates scalar inference, integer-width
selection, annotations, lossless numeric widening, calls, overloads,
conditions, returns, homogeneous arrays and dictionaries, indexing, and
dimensioned vector and coordinate literals.

```bash
bash scripts/type_demo.sh
```

The script prints a successful `TypeModel`, then confirms focused failures for
an incorrect return type, an uninferable variable, heterogeneous collections,
and an invalid vector component. This is not an execution demo, and `Unknown`
still marks deferred member and user-defined-type semantics.

## Executable-entry demonstration

`examples/entry_demo.sagan` is the first source fixture shaped like an
executable program. It has a parameterless `main(): Int`, local arithmetic,
conditional control flow, and guaranteed returns.

```bash
bash scripts/entry_demo.sh
```

The script confirms the valid entry point and demonstrates missing-entry,
uninitialized-read, and unreachable-statement diagnostics. It still does not
generate or execute native code.

## Native execution demonstration

`examples/execution_demo.sagan` is the first end-to-end executable fixture. It
calls an emoji-named function to compute `40 + 2`, builds and iterates a typed
countdown array using a spread, reads a checked array index, formats interpolated
strings,
constructs a typed dictionary using a spread and later-key override, reads a
checked key, demonstrates checked powers and prefix/postfix increment values,
constructs a vector with a spread, constructs a coordinate, prints and indexes
both dimensioned values, iterates the vector's components, and mutates state through
`while` and `until` loops, selects an ordered `match` case with a fallback,
tests the result, and returns success from `main`. Its `match` cases, `for`
loop, and final `if` branches demonstrate same-line bodies without braces.

```bash
bash scripts/execution_demo.sh
```

The script prints the Sagan input, emits and prints C++, compiles it with
`g++`, runs the native program, displays the program's own `print` output, and
reports its exit code. It also compiles and runs focused fixtures proving that
negative integer exponents, `0 ^ 0`, fixed-width integer power overflow,
checked addition, subtraction, multiplication, division, remainder, negation,
increment, and decrement failures
raise runtime errors. This demonstrates only the documented initial backend
subset, not the entire parsed language.

The string error fixture demonstrates the focused diagnostic for an empty
`${}` interpolation. The collection fixture demonstrates the minimum
two-element vector rule.
The block fixture demonstrates the focused diagnostic for a missing closing
brace. The control-flow fixture demonstrates the diagnostic for `break` outside
a loop. The match fixture demonstrates that `case else` must be the last branch.
The exception fixture demonstrates that `scream` requires a value.
The type fixture demonstrates that class methods require bodies.
The lambda fixture demonstrates that anonymous functions require `=>` before
their expression body. Additional fixtures cover a late module declaration, a
missing compound-assignment value, and a documentation comment before an
executable statement.

## Intentional lexical error

`examples/tokenizer_error.sagan` contains:

```sagan
let malformed_number = 1e
```

```bash
bin/sagan examples/tokenizer_error.sagan
```

The tokenizer rejects it because the exponent has no digits.

When adding examples, say whether they are tokenizer fixtures, intended syntax,
or eventually executable programs. Never imply execution solely from successful
tokenization.
