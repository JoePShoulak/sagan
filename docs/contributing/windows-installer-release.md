---
title: Windows installer release gate
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Windows installer release gate

Sagan 1.0 supports Windows x64 through one self-contained offline installer.
Windows ARM64, Linux, macOS, network/bootstrap installers, and package-manager
distribution remain future work.

## Automated implementation gate

A candidate is implementation-ready only when all of these checks pass:

- the compiler and Explorer launcher have no MinGW runtime DLL dependency;
- the installer builds from a clean checkout and includes the UCRT64 toolchain;
- its SHA-256 sidecar verifies;
- silent current-user installation succeeds with compiler-runtime directories
  absent from `PATH`;
- direct CLI compilation, generated-program execution, and windowed launch work;
- installing the same or a newer version in place succeeds;
- installing an older version over a newer version is refused;
- `.sagan` association, PATH registration, and clean uninstall work; and
- the normal compiler, CLI, documentation, and installer-policy tests pass.

Run the local portions with:

```bash
bash scripts/windows/build_installer.sh
bash scripts/windows/verify_installer_artifact.sh
bash scripts/windows/test_installer.sh
```

The Windows Installer workflow performs the same package build and smoke test.
An unsigned artifact is a development artifact regardless of version text.

## Public-release signing gate

Public installers and their embedded uninstallers require a trusted
Authenticode signature. Release infrastructure supplies an Inno Setup sign-tool
command through `SAGAN_SIGNTOOL_COMMAND`; the command must contain Inno's `$f`
file placeholder. Certificate material and credentials must remain in protected
release infrastructure, never in source control.

Before publication, run:

```bash
bash scripts/windows/verify_installer_artifact.sh --release
```

This verifies both the checksum and Windows trust validation. The lifecycle
review will decide the credential provider and release trigger before 1.0.

## Clean-machine acceptance gate

Automation cannot prove that a developer workstation is not masking an
installer defect. The exact signed release candidate must therefore be tested
on a clean Windows x64 computer or VM. Record the tester, date, Windows version,
installer SHA-256, and results for interactive install, CLI execution, `.sagan`
double-click and context-menu behavior, in-place upgrade, downgrade refusal,
uninstall, and post-uninstall cleanup.

This manual gate is currently **pending**. It may be deferred during development,
but it blocks publishing Sagan 1.0.
