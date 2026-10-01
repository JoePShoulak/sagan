---
title: Editor support
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Editor support

Sagan provides a VS Code extension that associates `.sagan` files with the
language, supplies tokenizer-aligned TextMate highlighting, and discovers a
compatible Sagan compiler and language server. The extension has its own version number; the VSIX
attached to a Sagan release is built from that same tagged source and is the
recommended match for the release.

There is no Visual Studio Marketplace listing yet. Install the extension from a
release asset or build it from source.

## Install from a Sagan release

1. Open the [HP1 download mirror](https://sagan.shoulak.org/downloads/) or the
   canonical [GitHub Releases](https://github.com/JoePShoulak/sagan/releases)
   page.
2. Select the same Sagan release as the compiler you installed.
3. Download `sagan-language-EXTENSION_VERSION.vsix` and its `.sha256` file.
4. In VS Code, open **Extensions**, select the `...` menu, choose
   **Install from VSIX...**, and select the downloaded file.

Alternatively, install the downloaded asset from Bash:

```bash
code --install-extension sagan-language-VERSION.vsix
```

## Build and install from a repository clone

Use this route when working with Sagan's experimental branch:

```bash
cd editors/vscode-sagan
npm ci
npm test
npm run test:bundle
npm run package
code --install-extension sagan-language-*.vsix
```

The repository's extension version is currently 0.3.1. Its version is
independent of the compiler version.

## Verify the installation

1. Open a `.sagan` file and confirm VS Code identifies it as **Sagan**.
2. Confirm comments, declarations, strings, interpolation, keywords, and types
   receive syntax coloring.
3. Run **Sagan: Show Tooling Status** from the Command Palette.
4. Confirm the **Sagan** output channel reports that the language server started.
5. Try completion, hover, go to definition, diagnostics, formatting, and rename.
6. If discovery fails, set `sagan.compiler.path` and `sagan.server.path` to the
   matching executables. Relevant setting changes restart the server automatically.

The extension locates the compiler and [language server](../tooling/language-server.md),
then enables only the capabilities advertised during LSP initialization.
TextMate coloring remains lexical and is not proof that code parses or
type-checks. The VSIX does not embed the native language-server executable; a
matching Sagan installation must provide `sagan-lsp` on `PATH`, beside the
configured compiler, in the repository `bin` directory, or through
`sagan.server.path`.

## Development-host testing

Open `editors/vscode-sagan/` in VS Code, press `F5`, and choose **Run Sagan
Extension** if prompted. The Extension Development Host opens the bundled
demonstration file. This is intended for extension development, not ordinary
installation.
