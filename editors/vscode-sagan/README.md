# Sagan Language for VS Code

VS Code language support for the experimental Sagan programming language.

The extension associates `.sagan` files with Sagan and provides TextMate highlighting derived from the repository's current tokenizer. It covers declarations, keywords, types, literals, operators, punctuation, comments, documentation comments, strings, interpolation, Unicode identifiers, private members, and mutating method names.

Sagan is still in language design. This extension provides lexical highlighting only; it does not provide IntelliSense, parsing, semantic analysis, type checking, diagnostics, formatting, or a language server.

## Local development

1. Open the Sagan repository in VS Code.
2. Press `F5` and choose **Run Sagan Extension** if prompted.
3. The Extension Development Host opens `examples/demo.sagan` from this extension.
4. Confirm the status bar identifies the file as **Sagan**.

No compilation is required for development-host testing. TextMate grammar changes require restarting the Extension Development Host.

## Package

From this directory:

```bash
npx --yes @vscode/vsce package
```

The resulting `.vsix` can be installed from VS Code's Extensions view or with the `code --install-extension` command.

## Grammar limits

TextMate highlighting is lexical and cannot fully resolve context-sensitive constructs. In particular, braces may delimit blocks or dictionaries, and angle brackets may delimit vectors or serve as comparison operators. The grammar gives their tokens stable scopes while leaving those semantic distinctions to a future parser-backed language service.
