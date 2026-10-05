---
title: Code signing policy
status: review-needed
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Code signing policy

Sagan's initial 1.0.0 Windows installer was published unsigned, with a warning
on its release page. On October 5, 2026, the owner deferred further signing
work because of its cost and paused all release publication for now. The
SignPath design below is historical and conditional, not an active plan or a
completed capability. Do not describe an artifact as signed merely because it
came from GitHub Releases or has a SHA-256 checksum.

Free code signing provided by [SignPath.io](https://signpath.io/), certificate
by [SignPath Foundation](https://signpath.org/). The Authenticode publisher on
an accepted signed artifact will be **SignPath Foundation**, not Joe P.
Shoulak. A valid signature establishes publisher identity and file integrity;
it does not guarantee that Windows SmartScreen will never show a warning.

## People and approval

- Author/maintainer: [Joe P. Shoulak](https://github.com/JoePShoulak).
- Reviewer of contributions from other authors: Joe P. Shoulak.
- Approver of every release signing request: Joe P. Shoulak.

The signing approver checks the exact source revision, automated build results,
artifact identity, and release version before approving. Signing requests are
not auto-approved. GitHub and SignPath accounts used by the team must have
multi-factor authentication enabled.

Only Sagan's own executables and installer may be signed under this project.
The bundled MSYS2/GCC toolchain is third-party software and must not be signed
as if Sagan authored it. The build records included licenses and an SBOM.
Signed artifacts must come from the public repository's GitHub-hosted release
workflow, not an uploaded local build. Release assets and their checksums are
calculated from the **final signed bytes** and are never replaced in place.

## Privacy and system changes

The Sagan compiler and installer do not send source files or usage telemetry
to Sagan-operated services or check for updates automatically. Downloading
Sagan from GitHub or the HP1 mirror uses those sites' normal network services;
running a user program may have behavior chosen by its author. The installer
shows optional PATH and `.sagan` file-association changes, offers current-user
or elevated installation, and includes an uninstaller. See the
[installation guide](../getting-started/installation.md) for those choices.

## Acceptance gate

Before a signed release is published, verify trusted SHA-256 Authenticode
signatures on `sagan.exe`, `sagan-launch.exe`, the embedded uninstaller, and
the installer; verify the timestamp and checksum; and test the exact installer
on a clean Windows x64 machine or VM. Signing setup and clean-machine acceptance
are still pending. See the [Windows installer release gate](windows-installer-release.md)
and [release lifecycle](release-lifecycle.md).
