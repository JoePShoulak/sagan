---
title: Installation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Installation
## Prerequisites

Development is currently documented for **Git Bash on Windows** with an
**MSYS2 UCRT64** C++ toolchain. The build uses C++23 and expects `g++`,
`make`, `bash`, and `cygpath`.

From the repository root:

```bash
bash scripts/test.sh
```

That script adds the UCRT64 and Unix tool directories to `PATH`, creates a
repository-local temporary directory, performs a clean build, prints the
Git-derived version, runs the tokenizer, parser, renderer, and CLI test suites,
and verifies `--version`.

To build without running the test script:

```bash
make all
```

The resulting executable is `bin/sagan`.

For documentation tooling, install Python 3 and run:

```bash
bash scripts/docs.sh setup
```

The documentation virtual environment and generated site live under the ignored
`build/` directory.
