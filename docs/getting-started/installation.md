---
title: Installation
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Installation
## Windows installer

Windows x64 is the initial supported installation platform. The generated Inno
Setup installer provides per-user or administrator-selected installation,
location and task selection, progress, Start-menu and PATH integration, clean
uninstallation, a `.sagan` file association, and Explorer launch overrides. It
bundles the UCRT64 compiler toolchain used by the current C++ backend, so an
installed copy does not require a separate MSYS2 or `g++` installation.

After installation, open a new terminal and run:

```bash
sagan --version
sagan path/to/program.sagan
```

Double-clicking a loose `.sagan` file opens a terminal by default. A package may
select terminal-free launch behavior in `sagan.toml`:

```toml
[application]
mode = "windowed"
```

Use `mode = "console"` to select a terminal explicitly. Explorer's **Run in
Terminal** and **Run Without Terminal** actions override the configured choice
for one launch. Windowed failures are recorded at
`%LOCALAPPDATA%\Sagan\logs\latest-launch.log` and reported with a native dialog.

Windows installer artifacts are built and smoke-tested by the Windows Installer
workflow. macOS and Linux installers remain planned targets; their future
support must preserve this manifest contract without pretending they are
currently supported.

## Prerequisites

Building Sagan from source is currently documented for **Git Bash on Windows** with an
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

Build the Windows installer locally with Inno Setup 6 installed:

```bash
bash scripts/windows/build_installer.sh
bash scripts/windows/test_installer.sh
```

For documentation tooling, install Python 3 and run:

```bash
bash scripts/docs.sh setup
```

The documentation virtual environment and generated site live under the ignored
`build/` directory.
