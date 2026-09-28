---
title: Development setup
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Development setup
Current Windows development uses Git Bash with an MSYS2 UCRT64 toolchain.

```bash
git clone <repository-url>
cd sagan
bash scripts/test.sh
```

The test script expects `make`, `g++`, and `cygpath`, builds with C++23
warnings enabled as errors, runs tokenizer self-tests, and prints the current
version.

Useful commands:

```bash
make demo
make get-version
bin/sagan examples/tokenizer_error.sagan
```

For documentation:

```bash
bash scripts/docs.sh setup
bash scripts/docs.sh serve
bash scripts/docs.sh check
```

Generated compiler and documentation artifacts live under ignored build/output
directories and should not be committed.

## Development build versions

Sagan uses a commit-derived development version inspired by Schematic. The
format is `MAJOR.MINOR.PATCH+gREVISION`, with `.dirty` appended when tracked or
untracked workspace changes are present.

`MAJOR` and `MINOR` are selected deliberately. `PATCH` counts commits since the
base commit for that development line, while `REVISION` is the abbreviated Git
commit ID. This makes terminal output and bug reports traceable to their source
without treating every working build as an official release.

```bash
make get-version
make
bin/sagan --version
```

The version source is regenerated on every build. When starting a new major or
minor development line, update `VERSION_MAJOR`, `VERSION_MINOR`, and
`VERSION_BASE_COMMIT` together in the makefile. Public release tags and
source-compatibility guarantees will be specified before the first release.
