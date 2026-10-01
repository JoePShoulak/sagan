---
title: Downloads
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Downloads

When stable Windows releases are published, they are available from two
locations:

- **[HP1 download mirror](https://sagan.shoulak.org/downloads/)** — installers,
  portable ZIP archives, VS Code extension packages, checksums, release
  manifests, and software bills of materials hosted beside this documentation.
- **[GitHub Releases](https://github.com/JoePShoulak/sagan/releases)** — the
  canonical source and release record.

The HP1 copy is a convenience mirror. Stable releases and public release
candidates are downloaded from GitHub after publication. Publisher-supplied
SHA-256 files are verified before listing; older releases without one receive a
mirror-generated checksum alongside the unchanged asset. Release directories
are immutable, so corrections receive a new version rather than replacing
existing files.

The experimental `v0.87.2-rc.1` preview and planned first 1.0.0 installer are
approved unsigned exceptions and may show a Windows unknown-publisher warning.
Check the SHA-256 sidecar and read the release notes before running either.
Later signed installers and clean-machine
acceptance are post-1.0 work, after the documentation audit.

Windows is the only supported installation platform today. Start with the
[installation guide](installation.md) for the installer, portable CLI setup,
and Windows warning details. The matching VS Code extension is distributed as
a `.vsix` asset with each release; follow the [editor-support guide](editor-support.md)
to install and verify it.
