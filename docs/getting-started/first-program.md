---
title: First program
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Write your first program

Create a file named `hello.sagan` with this program:

```sagan
--8<-- "docs/examples/executable/hello.sagan"
```

The file you run is the entry point. Sagan executes its top-level statements
in order, so this one-line program needs no `main` function or braces. `print`
writes one line. If the file finishes normally, its process status is `0`
(success). A command-line program can call `exit(code)` to choose another
status explicitly.

Run an installed compiler from Git Bash:

```bash
sagan hello.sagan
```

When working inside the Sagan repository, use the locally built compiler:

```bash
make all
bin/sagan hello.sagan
```

The result is:

```text
--8<-- "docs/examples/executable/hello.stdout"
```

This example is not just illustrative: the documentation build runs the
archived source and compares its output with the text above.

## See what the compiler understood

You can inspect the same source at different stages:

```bash
bin/sagan --tokens hello.sagan
bin/sagan --ast hello.sagan
bin/sagan --symbols hello.sagan
bin/sagan --types hello.sagan
bin/sagan --emit-cpp hello.sagan build/hello.cpp
```

Tokens are the smallest pieces of source, such as `print`, `(`, and the string. The
AST is the **abstract syntax tree**: a structured view of how those pieces form
declarations and expressions. Symbols show declared names and scopes. Type
checking confirms that operations use compatible values. `--emit-cpp` lets you
inspect the generated C++ without running it.

Next, continue with [Values and variables](../tour/values-and-variables.md).
