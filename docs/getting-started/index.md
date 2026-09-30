---
title: Getting started
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Getting started

This path takes you from no Sagan installation to a running native program.

1. [Install Sagan](installation.md) on Windows x64, or build it from source.
2. [Write your first program](first-program.md) and run it from Git Bash.
3. Follow the [language tour](../tour/index.md) to learn the main features.
4. Optionally install the early [VS Code extension](editor-support.md) for
   syntax coloring.

The simplest command is:

```bash
sagan program.sagan
```

Sagan reads the file, checks it, generates C++, compiles that C++, and runs the
resulting native program. If a lexical, syntax, name, type, module, or package
error is found, Sagan reports it before native compilation.
