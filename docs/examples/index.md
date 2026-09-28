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
including precedence, right-associative exponentiation and assignment, the
conditional expression, indexing, ordinary and safe member access, function and
method calls, mutating method names, and deep postfix chains.
It also includes ordinary, raw, multiline, and interpolated strings, including
interpolations containing member access and arithmetic expressions.

```bash
bash scripts/parser_demo.sh
bash scripts/ast_demo.sh
```

The first command prints the tree and confirms the intentional errors in
`examples/parser_error.sagan`, `examples/postfix_error.sagan`, and
`examples/string_error.sagan`. The second
opens a self-contained page showing the input source beside a colored tree. The
page supports button and mouse-wheel zoom plus drag-to-pan navigation. Use
`bash scripts/ast_demo.sh --no-open` to generate `build/ast-demo.html` without
launching a browser.

The string error fixture demonstrates the focused diagnostic for an empty
`${}` interpolation.

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
