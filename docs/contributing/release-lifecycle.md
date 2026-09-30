---
title: Release lifecycle
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Release lifecycle

Sagan releases are on demand. The project has stable and preview channels;
preview versions use SemVer identifiers such as `1.0.0-rc.1`. Only Joe P.
Shoulak authorizes stable publication. Support is best effort, with no
response-time SLA, and covers only the latest stable release.

## Identity and compatibility

Every public release starts from a verified signed annotated `vVERSION` tag on
`main` and has one matching GitHub Release. Stable 1.x releases preserve source
language, package-manifest, and public CLI compatibility. Compatible additions
are minor releases, fixes are patch releases, and breaking changes require the
next major version. Deprecated 1.x behavior remains supported until that major
boundary.

Published tags, artifacts, checksums, manifests, SBOMs, and numbered
documentation are immutable. A failed candidate receives the next identifier;
an urgent correction uses the ordinary patch pipeline rather than replacing or
skipping gates.

## Automated gates

Tag automation verifies the signature, main ancestry, and history-derived
version before running:

- the complete compiler and CLI tests;
- the enforced 90% line-coverage floor;
- strict documentation and release-readiness checks;
- one canonical Windows staging build;
- Authenticode signing and trust verification;
- installer and portable-archive checksums;
- isolated-PATH installer and portable compilation/execution tests;
- in-place upgrade and downgrade-refusal tests;
- a machine-readable release manifest and SPDX SBOM; and
- high/critical vulnerability scanning of shipped assets.

A relevant scanner finding blocks publication unless Joe explicitly records and
approves an exception. This is a lightweight safety gate, not a response-time or
formal incident-management commitment.

## Windows assets

Every Windows release contains:

- `sagan-VERSION-windows-x64.exe`, the signed graphical installer;
- `sagan-VERSION-windows-x64.zip`, a portable CLI archive;
- SHA-256 sidecars for both packages;
- `sagan-VERSION-release-manifest.json`;
- `sagan-VERSION-sbom.spdx.json`; and
- reviewed release notes plus GitHub's source archives.

The installer, embedded uninstaller, `sagan.exe`, and `sagan-launch.exe` must
pass Authenticode verification. Unsigned CI artifacts are development builds
and cannot be published as previews or stable releases.

The portable ZIP reuses the installer build's `sagan.exe`, compiler toolchain,
licenses, and version metadata. Its root contains `bin/`, `toolchain/`,
`licenses/`, and `VERSION`. It deliberately omits `sagan-launch.exe`, registry
changes, file associations, shortcuts, and automatic PATH changes.

## Preview and stable publication

Signed preview tags publish automatically as GitHub prereleases after automated
gates. Previews are best effort, may change incompatibly before stable release,
and receive no backported fixes.

A stable tag creates an unpublished GitHub draft containing the signed assets
and generated release-note draft. The exact attached installer must then pass a
clean Windows x64 computer or VM test. The tester records their identity, date,
Windows version, installer SHA-256, and result in the draft. Publication remains
behind the protected `stable-release` environment and requires Joe's approval.

Publishing promotes versioned documentation from the same tag and makes it the
`latest` documentation version. GitHub Releases is canonical. HP1 mirrors and
verifies stable assets afterward; mirror failure is reported and retried but
does not invalidate or block the canonical release.

## Withdrawal and security

A faulty stable release is preserved, marked withdrawn, and removed from the
recommended/latest position. The prior good release becomes the recommendation
while a new patch version passes the full pipeline. Assets and tags are never
replaced in place.

Security reports use GitHub private vulnerability reporting. When practical,
the issue is validated privately and the patch release and advisory are
published together. Only the latest stable release is supported; there are no
guaranteed backports, deadlines, or service levels.

## External 1.0 prerequisites

The repository intentionally fails closed until release infrastructure has:

- a trusted code-signing identity configured in the protected
  `release-signing` environment;
- `SAGAN_SIGNTOOL_COMMAND` using SHA-256 Authenticode and an RFC 3161 SHA-256
  timestamp;
- Joe as the required reviewer for `stable-release`; and
- recorded clean-machine evidence for the exact signed 1.0.0 installer.

The full VS Code extension and the math, rendering, and physics libraries remain
separate post-1.0 work.
