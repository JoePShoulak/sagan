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
The installed `sagan.exe` and `sagan-launch.exe` statically link their GCC and
C++ runtime support, including the MinGW threading runtime. They therefore
start normally from a terminal or File Explorer without requiring MSYS2's
`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, or `libwinpthread-1.dll` on the user's
`PATH`. The bundled toolchain remains available internally for
compiling generated C++ programs.

The installer is a self-contained offline package. Supported upgrades install
in place using the stable Sagan application identity and preserve the selected
location and integrations. Installing an older Sagan version over a newer one
is refused; uninstall the newer version first when an explicit downgrade is
necessary. Windows ARM64, Linux, and macOS are not supported by Sagan 1.0.

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
make check-windows-runtime
```

The resulting executable is `bin/sagan`. On Windows, the runtime check uses
`objdump` to reject local builds that import the MinGW GCC, C++, or threading
DLLs. Windows detection covers both an explicit `OS=Windows_NT` environment
and ordinary Git Bash `MINGW`/`MSYS` hosts, so a local build cannot silently
omit the static runtime flags.

Build the Windows installer locally with Inno Setup 6 installed:

```bash
bash scripts/windows/build_installer.sh
bash scripts/windows/verify_installer_artifact.sh
bash scripts/windows/test_installer.sh
```

The experimental `v0.88.0-rc.1` preview installer and planned initial 1.0.0
installer are explicitly disclosed unsigned exceptions and may show an
unknown-publisher warning. After the first post-1.0 documentation
audit, the signed follow-up and later public installers must be built with
`SAGAN_SIGNTOOL_COMMAND` set to an Inno
Setup-compatible Authenticode signing command and must pass:

```bash
bash scripts/windows/verify_installer_artifact.sh --release
```

Every build also produces a sibling `.exe.sha256` checksum. Signing credentials
must be supplied by protected release infrastructure and must never be stored in
the repository.

Public releases also provide a portable CLI archive. It changes no registry
keys, shortcuts, file associations, or PATH entries. From Git Bash, substitute
the release version and run:

```bash
gh release download v0.88.0-rc.1 --pattern 'sagan-0.88.0-windows-x64.zip'
mkdir -p "$HOME/.local/sagan"
unzip sagan-0.88.0-windows-x64.zip -d "$HOME/.local/sagan"
export PATH="$HOME/.local/sagan/bin:$PATH"
sagan --version
```

This downloads the experimental preview, not a stable 1.0 build. The `-rc.1`
tag identifies the preview; the archive currently uses the underlying numeric
version in its filename.

That `export` affects only the current shell. To keep it for future Git Bash
sessions, add the same export line to `~/.bashrc`. The graphical installer is
required for Start-menu entries and `.sagan` Explorer integration.

The smoke test installs into an isolated directory and deliberately removes
all UCRT64/MinGW runtime directories from `PATH`. It verifies direct CLI
startup, compilation and execution of a generated program, windowed Explorer
dispatch through `sagan-launch.exe`, file association, PATH registration, and
clean uninstallation. This prevents a developer or CI MSYS2 installation from
masking missing runtime dependencies in the packaged executables.

CI and the isolated-path smoke test establish implementation readiness. The
exact signed follow-up candidate must still be installed, run, upgraded,
associated with `.sagan`, and uninstalled on a clean Windows x64 computer or
VM. Until that evidence is recorded, Windows distribution acceptance remains
pending even after the initial 1.0.0 publication.

For documentation tooling, install Python 3 and run:

```bash
bash scripts/docs.sh setup
```

The documentation virtual environment and generated site live under the ignored
`build/` directory.
