---
title: Development setup
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Development setup

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
