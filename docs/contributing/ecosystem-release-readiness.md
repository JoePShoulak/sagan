---
title: Ecosystem release readiness
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Ecosystem release readiness

A Sagan release is one coordinated freeze. The compiler/language, every library
included in that release, the documentation snapshot, and the VS Code
extension must agree on the same syntax and behavior and pass their respective
automated checks before the release is ready. Passing a compiler build alone
is not a release gate. An optional library that is not shipped must be listed
as excluded; it must not be presented in the docs or extension as available.

The initial 1.0.0 release is a disclosed exception to the usual documentation
publication gate: its documentation remains `experimental` and visibly
work-in-progress. The owner performs the page-by-page audit as the **first
post-1.0 task**, then decides whether the evidence calls for documentation,
compiler, or language changes. Do not describe the 1.0.0 pages as audited or
publish a numbered 1.0.0 documentation archive afterward.

The main-push workflow tags a new stable version before running the release
gates. Before approving publication, verify the exact tagged revision and
assets:

1. Run the compiler and native execution suite with `bash scripts/test.sh`.
2. Run each included core-library test suite against that compiler revision.
3. Run `bash scripts/docs.sh check` and verify executable examples and
   feature-status claims. The separate human page audit is deferred only for
   the initial 1.0.0 release.
4. Run the VS Code extension tests in `editors/vscode-sagan` with `npm test`,
   and verify its grammar, capability negotiation, and packaged version against
   the candidate compiler.
5. Freeze and publish the matching artifact set together. If any component
   changes after verification, create a new version and repeat the affected
   checks; do not move or rebuild the existing tag or assets.

This checklist describes the release gate, not an instruction to publish an
internal or work-in-progress documentation page. After the audit, release
documentation requires the normal page metadata and `release-check` gate.
Joe's separate stable-publication approval applies to every stable release.
