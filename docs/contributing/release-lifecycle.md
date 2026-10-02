---
title: Release lifecycle
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Release lifecycle

Development occurs on focused branches created from `dev`. Changes merge into
`dev` through pull requests, where the combined state receives the full
integration suite. The project owner promotes validated `dev` history to `main`
only when intentionally preparing a release; ordinary feature and fix work does
not land directly on either protected branch. See the
[change and publication lifecycle](change-lifecycle.md) for the complete path.

Every push to `main` runs release-preparation CI. A new history-derived 0.x
version gets a signed `-rc.1` preview tag; a new 1.x version gets a signed
stable tag. A docs, test, or maintenance commit with no version change does
not create a duplicate release. Passing stable gates prepares a draft,
**not** a public release. Only Joe P. Shoulak authorizes stable publication.
Previews are automatically published after their gates pass. Support is best effort,
with no response-time SLA, and covers only the latest stable release.

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

Main-driven stable tags use a dedicated CI SSH signing key registered with
GitHub; an authorized maintainer may also sign a preview tag. Both trusted
public keys are in `.github/allowed_signers`. A checkout can verify a
candidate independently from GitHub with:

```bash
git -c gpg.ssh.allowedSignersFile=.github/allowed_signers verify-tag vVERSION
```

The release workflow requires that local verification and GitHub's verified-tag
result both succeed.

## Automated gates

Tag automation verifies the signature, main ancestry, and history-derived
version before running:

- the complete compiler and CLI tests;
- the enforced 90% line-coverage floor;
- strict documentation build checks;
- one canonical Windows staging build;
- installer checksum and isolated-environment verification;
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

- `sagan-VERSION-windows-x64.exe`, the graphical installer;
- `sagan-VERSION-windows-x64.zip`, a portable CLI archive;
- `sagan-language-EXTENSION_VERSION.vsix`, the compatible VS Code extension;
- SHA-256 sidecars for all three packages;
- `sagan-VERSION-release-manifest.json`;
- `sagan-VERSION-sbom.spdx.json`; and
- reviewed release notes plus GitHub's source archives.

The `v0.88.0-rc.1` experimental prerelease is a one-time unsigned-installer
exception; Windows may show an unknown-publisher warning. The initial 1.0.0
release also has a separately documented unsigned exception. Other release
installers, embedded uninstallers, `sagan.exe`, and `sagan-launch.exe` must
pass the applicable Authenticode verification gate.

The portable ZIP reuses the installer build's `sagan.exe`, compiler toolchain,
licenses, and version metadata. Its root contains `bin/`, `toolchain/`,
`licenses/`, and `VERSION`. It deliberately omits `sagan-launch.exe`, registry
changes, file associations, shortcuts, and automatic PATH changes.

## Preview and stable publication

Signed preview tags publish automatically as GitHub prereleases after automated
gates. Previews are best effort, may change incompatibly before stable release,
and receive no backported fixes.

For the single `v0.88.0-rc.1` preview, the page-by-page documentation audit
remains open, and the docs remain visibly experimental. The preview still
requires strict docs-build, compiler, coverage, installer/portable, checksum,
and vulnerability-scan gates. Its unsigned installer is not the accepted
final Windows distribution; do not present this preview as Sagan 1.0.

A stable tag creates an unpublished GitHub draft containing the release assets
and generated release-note draft. Publication remains behind the protected
`stable-release` environment and requires Joe's approval.

For the initial 1.0.0 publication, the documentation audit, Authenticode
signing, and clean-machine acceptance are explicit post-publication work. The
release notes must say that the installer is unsigned, may trigger an
unknown-publisher warning, and has not completed clean-machine acceptance. The
documentation remains on the experimental channel. **The page-by-page
documentation audit is the first task after 1.0.0.** It may identify language,
tooling, or documentation corrections; decide those from the evidence and use
the normal versioning rules for any follow-up release. Published 1.0.0 assets
and tags remain immutable.

After the documentation audit is complete, a later release promotes versioned
documentation from its matching tag and makes it the `latest`
documentation version. GitHub Releases is canonical. HP1 mirrors and verifies
stable assets afterward; mirror failure is reported and retried but does not
invalidate or block the canonical release.

## Withdrawal and security

A faulty stable release is preserved, marked withdrawn, and removed from the
recommended/latest position. The prior good release becomes the recommendation
while a new patch version passes the full pipeline. Assets and tags are never
replaced in place.

Security reports use GitHub private vulnerability reporting. When practical,
the issue is validated privately and the patch release and advisory are
published together. Only the latest stable release is supported; there are no
guaranteed backports, deadlines, or service levels.

## Post-publication 1.0 follow-ups

After the initial 1.0.0 publication, audit the documentation first. Then
complete the remaining distribution work before calling the Windows package
fully accepted:

- a trusted code-signing identity configured in the protected
  `release-signing` environment;
- `SAGAN_SIGNTOOL_COMMAND` using SHA-256 Authenticode and an RFC 3161 SHA-256
  timestamp;
- versioned documentation published from the matching follow-up release tag; and
- recorded clean-machine evidence for the exact signed patch installer.

Joe remains the required reviewer for every `stable-release` publication,
including the initial 1.0.0 release.

Visual Studio Marketplace publication, further editor integrations, the
debugger and test protocols, and the math, rendering, and physics libraries
remain separate post-1.0 work.
