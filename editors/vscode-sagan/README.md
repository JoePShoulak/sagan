# Sagan Language for VS Code

<p align="center">
  <img src="images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="180">
</p>

VS Code language support for the experimental Sagan programming language. Version 0.1.2 tracks the tokenizer and parser grammar in this repository, including weak fields, optional fallback, and generic declarations.

The extension associates `.sagan` files with Sagan and provides TextMate highlighting derived from the repository's current tokenizer. It covers declarations, keywords, types, literals, operators, punctuation, comments, documentation comments, strings, interpolation, Unicode identifiers, private members, and mutating method names.

The extension currently provides syntax-based TextMate highlighting only. The compiler now exposes the first versioned language-service foundations, but it does not yet advertise the recovery, document-overlay, or language-server capabilities required for live semantic editor features.

## Local development

1. Open the Sagan repository in VS Code.
2. Press `F5` and choose **Run Sagan Extension** if prompted.
3. The Extension Development Host opens `examples/demo.sagan` from this extension.
4. Confirm the status bar identifies the file as **Sagan**.

No compiler build is required for development-host testing. TextMate grammar changes require restarting the Extension Development Host.

## Test

Install the pinned development dependencies and run the grammar against focused fixtures plus the repository's comprehensive parser demo:

```bash
npm ci
npm test
```

The tests use VS Code's TextMate and Oniguruma engines to verify scopes for the current vocabulary, Unicode and emoji identifiers, strings and interpolation, nested and documentation comments, enum documentation, yields, exceptions, operators, and retired Schematic constructs.

## Package

From this directory:

```bash
npm run package
```

The resulting `.vsix` can be installed from VS Code's Extensions view or with the `code --install-extension` command.

## Grammar limits

TextMate highlighting is lexical and cannot fully resolve context-sensitive constructs. In particular, braces may delimit blocks or dictionaries, and angle brackets may delimit vectors or serve as comparison operators. The grammar gives their tokens stable scopes while leaving those grammatical distinctions and all semantic validation to the compiler or a future parser-backed language service.
