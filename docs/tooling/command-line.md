---
title: Command line
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Command line
## Executable

```bash
bin/sagan path/to/source.sagan
bin/sagan --self-test
bin/sagan --version
bin/sagan --ast path/to/source.sagan
bin/sagan --ast-dot path/to/source.sagan
bin/sagan --ast-svg path/to/source.sagan build/tree.svg
bin/sagan --ast-html path/to/source.sagan build/tree.html
```

With no valid source path, the program reports command usage or a file error.
`--ast` prints a readable tree. `--ast-dot` prints Graphviz DOT without requiring
Graphviz itself. SVG and HTML modes write their final argument; HTML includes
both the original input and an embedded visual tree.

The generated HTML viewer offers zoom-in, zoom-out, fit, and 100% controls.
Mouse-wheel zoom follows the pointer, and the tree can be dragged to pan across
large syntax trees.

## Make targets

```bash
make all
make test
make demo
make parser-demo
make ast-demo
make get-version
make clean
```

`all` builds `bin/sagan`; `test` runs self-tests; `demo` tokenizes the
repository example; `parser-demo` prints the parser example's AST; `ast-demo`
writes the visual source-and-tree page to `build/ast-demo.html`; `get-version`
prints the calculated build identity; and `clean` removes compiler objects and
the binary.

Development versions have the form
`MAJOR.MINOR.PATCH+gREVISION[.dirty]`. Conventional Commit markers after the
configured baseline determine the numeric version, and `.dirty` records tracked
or untracked working-tree changes. Use `bash scripts/version.sh prepare TYPE`
before a patch, minor, or major commit and `bash scripts/version.sh check-badge`
afterward. The complete workflow is documented in
[Versioning](../contributing/versioning.md).
