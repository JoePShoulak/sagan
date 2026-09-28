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

`MAJOR.MINOR.PATCH` is calculated from Conventional Commit markers after the
baseline in `version.conf`, while `REVISION` is the abbreviated Git commit ID.
This makes terminal output and bug reports traceable to their source without
treating every working build as an official release.

```bash
make get-version
make
bin/sagan --version
```

The version source is regenerated on every build. Use `fix:` for a patch,
`feat:` for a minor change, and `!` or a `BREAKING CHANGE:` footer for a major
change. See the [versioning workflow](versioning.md) for badge preparation and
verification. Public release tags and source-compatibility guarantees will be
specified before the first release.
