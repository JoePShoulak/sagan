---
title: Tokenizer
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Tokenizer

The tokenizer stage is complete for Sagan's current lexical specification. It
converts UTF-8 source into tokens carrying byte spans into the original source
and reports focused lexical errors before parsing begins.

## Implemented behavior

- The complete current keyword, operator, and punctuation vocabulary
- Decimal integers, floating-point numbers, scientific notation, and digit
  separators
- Unicode XID identifiers plus emoji sequences
- NFC normalization of identifier token text
- Strict rejection of malformed UTF-8
- Ordinary, raw, multiline, and interpolated strings
- Nested expressions inside string interpolation
- Line comments, nested block comments, and documentation comments
- Logical newline tokens and continuation inside parentheses, brackets, or
  after incomplete operators
- Source spans and command-line lexical diagnostics

Source spans use UTF-8 byte offsets so they can slice the original source
without conversion. Diagnostic columns count decoded Unicode code points.
Normalized identifier text is stored separately in the token, so normalization
does not invalidate the original span.

## Unicode implementation

Identifier classification uses generated Unicode 17 `XID_Start`,
`XID_Continue`, `Extended_Pictographic`, and `Emoji_Modifier` tables. UniAlgo is
vendored for portable NFC normalization. Join controls are accepted only as
part of a recognized emoji sequence, preventing incomplete zero-width-joiner
sequences from silently becoming identifiers.

Maintainers can regenerate the checked-in property tables from the official
Unicode Character Database:

```bash
bash scripts/update_unicode_tables.sh
```

## Parser boundary

The tokenizer does not guess whether `{...}` is a dictionary or code block, or
whether `<...>` is a vector or a pair of comparison operators. It emits the
same delimiter and newline tokens in either case. The parser has the grammar
context required to interpret them and decide whether a newline terminates a
statement.

## Verification

`bash scripts/test.sh` checks tokenizer construction, iteration, reset, and
end-of-input lifecycle behavior; every keyword and operator; representative
valid fragments; Unicode normalization and edge cases; emoji sequences;
malformed numbers, strings, comments, UTF-8, and escapes; and randomized byte
input. Randomized and defensive tests verify that arbitrary or invalid input
either tokenizes or produces a controlled lexical error rather than an
unexpected exception.
See the [lexical specification](../reference/lexical-specification.md).
