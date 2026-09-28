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
