# Independent VS Code extension relocation patches

These patches are a **local rehearsal**, not a published extension repository.
They apply in order after filtering the organization repository at source
`dev` commit `4132c8c37f99dc5f89c7141f5780b583b64d192e` with
`vscode-relocation.toml`. The expected filtered base is
`5a8d156f18785d12ba6603f751f9a5533c2ea1b9`. Verify both hashes before
applying the four numbered patches with `git am`; do not apply them to the
intact Sagan repository. A fresh-clone replay of all four patches produced
tree `9590c0b30ef36edea5933ecab9886b34af2965ec`, identical to the
draft branch.

The workflow checks out the public `Sagan-Shoulak/sagan` repository at the
exact commit in `sagan-source-commit.txt`, builds `sagan` and `sagan-lsp` on
each CI platform, and tests the extension from its own checkout. The source
checkout is a sibling directory so native source and build outputs cannot be
accidentally packaged in the VSIX. Docs-only changes do not trigger the
runtime test matrix.

The local Windows rehearsal passed extension unit tests, bundle integrity,
live Extension Development Host integration, VSIX creation and checksum,
isolated VSIX installation, and activation with existing native binaries.
The pinned source itself has **not** yet been built in the independent
workflow, and Linux/macOS jobs have **not** run. Do not publish a split
repository or claim independent CI parity on this evidence alone.

The patches include draft maintainer, technology, and new-chat guides. They
do not complete cross-repository documentation links, an owner recovery
drill, independent hosted CI, extension release-tag naming, or destination
governance. All imported docs remain `publication_ready: false`
and require human review after their links and ownership are rewritten.
The primary transfer gate remains provisional while Codecov's upstream
October 5 TLS outage prevents a successful post-transfer upload.

After the primary transfer gate clears, re-run the extraction from the
then-current reviewed `dev` commit. Recompute this patch series if its base
changed, refresh the exact compiler-source lock, complete the remaining
rewrites, and run all three hosted extension jobs before creating or
publishing a release.
