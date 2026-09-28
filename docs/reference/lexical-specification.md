---
title: Lexical specification
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Lexical specification

Sagan source is UTF-8. Malformed UTF-8 is a lexical error. Tokens retain byte
spans into the original source.

## Identifiers

Textual identifiers follow Unicode 17 `XID_Start` and `XID_Continue`, with `_`
also accepted in either position. Emoji sequences may begin or continue an
identifier. This permits names such as:

```sagan
let Δx = 1.0
let 変数 = Δx
let 🌌distance = 42

fun 👩🏽‍🚀(destination) {
  // ...
}
```

Identifier spellings are normalized to Unicode NFC before keyword lookup and
later compiler processing. Source spans continue to reference the original
bytes. Standalone join controls and incomplete emoji ZWJ sequences are invalid.
ASCII punctuation sequences such as `:)` are not identifiers.

Identifiers are case-sensitive. Names beginning with `__` are reserved for the
compiler. A trailing `!` may be included in a method identifier as Sagan's
mutating-counterpart naming convention.

## Numbers

The initial language supports decimal integers, decimal floating-point values,
scientific notation, and `_` separators placed strictly between digits.

```sagan
let count = 10_000
let gravity = 6.674_30e-11
```

A decimal point requires a digit on both sides. Consequently `.5`, `5.`,
`.5e2`, and `5.e2` are lexical errors. Binary, octal, hexadecimal, unit
suffixes, and numeric type suffixes are not currently supported.

## Strings

Single and double quotes create strings. Triple double quotes create multiline
strings. Non-raw strings support escapes and `${...}` interpolation; raw
strings use an `r` prefix and process neither feature.

The supported escapes are `\\`, `\"`, `\'`, `\n`, `\r`, `\t`, `\0`, and
`\u{...}`. Unicode escapes must identify a Unicode scalar value and contain no
more than six hexadecimal digits.

## Comments

- `//` begins a line comment.
- `/* ... */` is a nestable block comment.
- `///` and `/** ... */` produce documentation-comment tokens.
- Other comments are discarded by the tokenizer.

## Newlines

`LF` and `CRLF` each represent one logical newline. A lone carriage return is a
lexical error. Newlines are suppressed inside parentheses and brackets and
after tokens that leave an expression incomplete. Ambiguous brace and angle
delimiter contexts are preserved for the parser to interpret.

## Keywords

```text
let fun class face enum
if else match case for in while until break continue return yield
import from as module export
hope unless finally scream
and or not self is has
true false inf nan
```

## Literals

Decimal integers and floating-point literals support `_` only between digits.
Floating-point forms may contain a decimal point with digits on both sides and
an `e` or `E` exponent with optional sign. `.5`, `5.`, malformed
separators, incomplete exponents, and letters directly following a number are
not accepted as one valid numeric literal.

Single- and double-quoted strings are recognized. Triple double quotes create
multiline strings. Non-raw strings process `\\`, `\"`, `\'`, `\n`,
`\r`, `\t`, `\0`, and `\u{...}`; they tokenize `${...}`
interpolation, including nested braces. Raw strings begin with `r` and do not
process escapes or interpolation.

## Comments and whitespace

`//` starts a line comment. Block comments `/* ... */` nest.
`///` and `/** ... */` produce `DOC_COMMENT`; ordinary comments are
discarded. Spaces, tabs, form feeds, and vertical tabs are non-significant.

LF and CRLF each count as one logical newline. Blank and comment-only lines do
not emit newline tokens. Newlines are suppressed inside parentheses and
brackets and after a continuation token. Brace and angle-bracket newlines are
preserved so the parser can interpret them with grammar context.

## Punctuation and operators

```text
( ) [ ] { } < > , : . ? ;
+ - * / % ^ = !
... ?. := => == != <= >= ++ --
+= -= *= /= %= ^=
```

Longest valid token wins. Standalone `@` and backticks are errors; Schematic's
special tag, event, and code-block tokens are not part of Sagan.

## Token categories

The token vocabulary also includes newline; ordinary and method identifiers;
integer and float; whole/raw strings; string begin, segment, end, and
interpolation boundaries; and documentation comments. Tokens carry source
spans and text, with decoded numeric or string values where appropriate.
