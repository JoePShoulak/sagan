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

The file has been checked as tokenizer input. It is not executable because no
parser or later compiler stages exist.

## Parser and visual AST demonstration

`examples/parser_demo.sagan` exercises the parser features implemented so far,
including modules, imports, aliases, exports, precedence, right-associative exponentiation and assignment, the
conditional expression, indexing, ordinary and safe member access, function and
method calls, mutating method names, and deep postfix chains.

`examples/module_error.sagan` demonstrates the focused diagnostic produced when
a module declaration appears after another top-level declaration.
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

The first command prints the tree and confirms the intentional errors in
`examples/parser_error.sagan`, `examples/postfix_error.sagan`, and
`examples/string_error.sagan`, `examples/collection_error.sagan`,
`examples/block_error.sagan`, `examples/control_flow_error.sagan`, and
`examples/match_error.sagan`, `examples/exception_error.sagan`, and
`examples/type_error.sagan`, and `examples/lambda_error.sagan`. The second
opens a self-contained page showing the input source beside a colored tree. The
page supports button and mouse-wheel zoom plus drag-to-pan navigation. Use
`bash scripts/ast_demo.sh --no-open` to generate `build/ast-demo.html` without
launching a browser.

The string error fixture demonstrates the focused diagnostic for an empty
`${}` interpolation. The collection fixture demonstrates the minimum
two-element vector rule.
The block fixture demonstrates the focused diagnostic for a missing closing
brace. The control-flow fixture demonstrates the diagnostic for `break` outside
a loop. The match fixture demonstrates that `case else` must be the last branch.
The exception fixture demonstrates that `scream` requires a value.
The type fixture demonstrates that class methods require bodies.
The lambda fixture demonstrates that anonymous functions require `=>` before
their expression body.

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
