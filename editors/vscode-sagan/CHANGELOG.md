# Changelog

## 0.1.1

- Align syntax scopes and the demonstration program with Sagan 0.13.0's complete tokenizer and parser grammar.
- Add contextual module, import, export, enum-member, property, parameter, and mutating-method scopes.
- Tighten Unicode identifier highlighting while retaining emoji identifiers and sequences.
- Add indentation behavior for brace-delimited bodies.
- Add TextMate tokenization tests covering the current vocabulary, strings, comments, Unicode, parser demo, and removed Schematic constructs.

## 0.1.0

- Replace the JavaScript fallback with a tokenizer-aligned Sagan TextMate grammar.
- Add Sagan declarations, keywords, literals, operators, punctuation, comments, and identifier scopes.
- Add ordinary, raw, multiline, and interpolated strings with nested Sagan expressions.
- Add recursive nested block-comment highlighting.
- Add Unicode, emoji, private-member, and mutating-method highlighting.
- Expand and package the comprehensive syntax demonstration file.

## 0.0.1

- Register the Sagan language and `.sagan` source-file extension.
- Add comment, bracket, auto-closing, surrounding-pair, and folding behavior.
- Highlight the current draft Sagan keywords over VS Code's JavaScript TextMate grammar baseline.
