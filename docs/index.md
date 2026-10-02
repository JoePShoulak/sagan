---
title: Sagan Documentation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Sagan Documentation

<p align="center">
  <img src="assets/images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="240">
</p>

Sagan is a strongly typed programming language for geometry, astrodynamics,
numerical work, and real-time simulation. A Sagan program is checked, translated
to C++, compiled, and run as a native program.

You do not need to understand compilers or language design to begin. Start with
[Install Sagan](getting-started/installation.md), then [write and run your first
program](getting-started/first-program.md). The [language tour](tour/index.md)
builds from variables and functions to classes, composition, and errors.

## Choose what you need

- **I want to learn Sagan.** Follow [Getting Started](getting-started/index.md)
  and then the [language tour](tour/index.md).
- **I need to look up a rule.** Use the [language reference](reference/index.md).
- **I want runnable examples.** Visit the [examples](examples/index.md); archived
  documentation examples are executed during every documentation build.
- **I want to understand a design choice.** Read
  [Why Sagan works this way](design/index.md).
- **I want to work on Sagan itself.** Open [Project Development](contributing/index.md)
  and the [implementation overview](implementation/index.md).

## What works today

The compiler tokenizes, parses, resolves, type-checks, generates C++, and runs
the implemented Sagan language. That includes modules and packages, functions
and closures, collections, classes and faces, reference-counted objects,
optionals, enums and payload enums, generics, exceptions and catchable runtime
errors, geometry values, and native units of measure.

The language server already powers a usable VS Code extension, including
diagnostics, navigation, formatting, safe rename, build/run commands, and Test
Explorer. Complete external-package completion and a supported debugger are
still in development. The math library beyond the built-in language
foundation, rendering, physics, and non-Windows installers are later work.
Pages say clearly when they describe planned areas.

## Documentation status

This is the **experimental** documentation from the current repository. Every
page remains work in progress until the project owner audits it. A page may be
accurate and tested without yet being approved for the frozen 1.0 archive.
The current development branch also contains changes made after the published
1.0 release; use the matching compiler and extension when following its examples.
