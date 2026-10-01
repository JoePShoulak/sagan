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
An unsigned artifact is normally a development artifact. The experimental
`v0.87.1-rc.1` preview and planned initial 1.0.0 release are the two narrowly
disclosed exceptions.

## Public-release signing gate

Other public releases require trusted
Authenticode signatures on the installer, embedded uninstaller, and Sagan
executables. Release infrastructure will supply an Inno Setup sign-tool command
through `SAGAN_SIGNTOOL_COMMAND`; the command must contain Inno's `$f` file
placeholder. Certificate material and credentials must remain in protected
release infrastructure, never in source control.

For a signed candidate, run before publication:

```bash
bash scripts/windows/verify_installer_artifact.sh --release
```

This verifies both the checksum and Windows trust validation. Verified signed
tags trigger release builds; the signing provider and credentials must be
configured in the protected `release-signing` environment for the signed
post-1.0 patch and later releases.

The initial 1.0.0 release is an explicit exception: its unsigned installer may
be published after automated package tests and project-owner approval, with a
prominent unknown-publisher warning in its release notes. The page-by-page
documentation audit comes first after 1.0.0; signing is subsequent acceptance
work. The signed-release gate must be restored and verified before claiming a
later public installer meets this policy.

## Clean-machine acceptance gate

Automation cannot prove that a developer workstation is not masking an
installer defect. The exact signed release candidate must therefore be tested
on a clean Windows x64 computer or VM. Record the tester, date, Windows version,
installer SHA-256, and results for interactive install, CLI execution, `.sagan`
double-click and context-menu behavior, in-place upgrade, downgrade refusal,
uninstall, and post-uninstall cleanup.

This manual gate is currently **pending**. It follows the initial 1.0.0
publication and documentation audit, and becomes mandatory for the signed
follow-up release that completes Windows distribution acceptance.
