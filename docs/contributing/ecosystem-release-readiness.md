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
included in that release, the versioned documentation, and the VS Code
extension must agree on the same syntax and behavior and pass their respective
checks before the release is ready. Passing a compiler build alone is not a
release gate. An optional library that is not shipped must be explicitly listed
as excluded; it must not be presented in the docs or extension as available.

Before tagging or publishing, verify the exact candidate revision and assets:

1. Run the compiler and native execution suite with `bash scripts/test.sh`.
2. Run each included core-library test suite against that compiler revision.
3. Run `bash scripts/docs.sh check` and review examples and feature-status
   claims against executable behavior.
4. Run the VS Code extension tests in `editors/vscode-sagan` with `npm test`,
   and verify its grammar, capability negotiation, and packaged version against
   the candidate compiler.
5. Freeze and publish the matching artifact set together. If any component
   changes after verification, repeat the affected checks and reconfirm the
   full ecosystem agreement before publication.

This checklist describes the release gate, not an instruction to publish an
internal or work-in-progress documentation page. Human documentation audit and
the repository's separate release approval rules still apply.
