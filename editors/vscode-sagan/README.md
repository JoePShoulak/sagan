# Sagan Language for VS Code

Early VS Code language support for Sagan.

This first working version associates `.sagan` files with the Sagan language, highlights the current draft keyword set, and temporarily delegates the remaining token coloring to VS Code's JavaScript TextMate grammar. That gives the evolving language useful highlighting for comments, strings, numbers, operators, delimiters, and JavaScript-like constructs without prematurely encoding a complete Sagan grammar.

## Try it locally

1. Open the Sagan repository in VS Code.
2. Press `F5` and choose **Run Sagan Extension** if VS Code asks for a launch configuration.
3. The Extension Development Host opens the persistent `examples` test workspace and `demo.sagan`.
4. Confirm the status bar identifies the file as **Sagan**.

No dependency installation or compilation is required.

## Next grammar step

Replace the `source.js` include in `syntaxes/sagan.tmLanguage.json` with additional Sagan-specific TextMate patterns as the lexical rules stabilize. The language registration and editor configuration can remain in place.
