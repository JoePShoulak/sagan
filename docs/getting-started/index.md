---
title: Getting started
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Getting started
The current executable is a tokenizer demonstration, not a complete Sagan
compiler. It reads a source file and prints token names, source spans, text, and
selected literal values.

1. Complete the [installation](installation.md) prerequisites.
2. Build and test the tokenizer.
3. Run the [first program](first-program.md) through the token dumper.
4. Optionally install the early [VS Code extension](editor-support.md).

!!! note
    Tokenizing successfully proves that the input is lexically valid. It does
    not prove that the program has valid grammar, types, or runtime behavior.
