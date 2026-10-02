---
title: Lexical specification
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Lexical specification

Sagan source files use UTF-8. UTF-8 is the text encoding that lets one file
contain ordinary ASCII, non-English scripts, mathematical symbols, and emoji.
Malformed UTF-8 is a lexical error. The compiler retains each token's original
byte range so diagnostics can point back to the source.

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
later compiler processing. In plain language, canonically equivalent Unicode
spellings are treated as the same name even when their bytes differ. Source
spans still reference the original bytes. Standalone join controls and
incomplete emoji ZWJ sequences are invalid. ASCII punctuation sequences such
as `:)` are not identifiers.

Identifiers are case-sensitive. Names beginning with `__` are reserved for the
compiler. A trailing `!` may be included in a method identifier as Sagan's
mutating-counterpart naming convention.

`const` is a keyword, not an identifier. A constant's name is deliberately
stricter than an ordinary identifier: it must match ASCII
`[A-Z][A-Z0-9_]*`. Unicode and emoji remain valid for ordinary Sagan names,
but do not yet have a capitalization rule for constants. A `let` name matching
this reserved form is rejected rather than silently becoming a constant.

## Numbers

Sagan supports decimal integers, decimal floating-point values, scientific
notation, and `_` separators placed strictly between digits.

```sagan
let count = 10_000
let gravity = 6.674_30e-11
```

A decimal point requires a digit on both sides. Consequently `.5`, `5.`,
`.5e2`, and `5.e2` are lexical errors. Binary, octal, and hexadecimal numeric
literals are not supported. A numeric or geometry literal
may be followed by a separately tokenized unit name; the parser, rather than the
lexer, forms the measured expression.
An integer immediately followed by `.` and an identifier is tokenized as
member access, so `5.times` is valid while a bare `5.` remains invalid.

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
let const weak fun test new class face enum dimension quantity unit affine
if else match case for in while until break continue return yield
import from as module export
hope unless finally scream
and or not self is has
true false inf nan
```

Documentation comments must be followed by a logical newline. The parser
attaches them to the next declaration. Ordinary blank lines are not represented
in the token stream, so attachment does not depend on the number of physical
blank lines between the comment and declaration.

## Punctuation and operators

```text
( ) [ ] { } < > , : . ? ;
+ - * / % ^ = !
... ?. ?? := => == != <= >= ++ --
+= -= *= /= %= ^=
```

The longest valid token wins, so `>=` is one token rather than `>` followed by
`=`. Standalone `@` and backticks are errors.

## Token categories

The token vocabulary also includes newline; ordinary and method identifiers;
integer and float; whole/raw strings; string begin, segment, end, and
interpolation boundaries; and documentation comments. Tokens carry source
spans and text, with decoded numeric or string values where appropriate.
