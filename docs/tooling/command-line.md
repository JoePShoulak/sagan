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
```

With no valid source path, the program reports command usage or a file error.

## Make targets

```bash
make all
make test
make demo
make get-version
make clean
```

`all` builds `bin/sagan`; `test` runs self-tests; `demo` tokenizes the
repository example; `get-version` prints the calculated build identity; and
`clean` removes compiler objects and the binary.

Development versions have the form
`MAJOR.MINOR.COMMITS+gREVISION[.dirty]`. The commit count is measured from the
configured base commit, and `.dirty` records tracked or untracked working-tree
changes.
