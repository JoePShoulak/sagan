# VS Code repository extraction rehearsal

## Result

Passed locally on October 5, 2026. No remote repository was created or changed,
and the segmentation branch was not merged into `dev`.

- Source branch: `codex/repo-segmentation-manifests`
- Source commit: `33d258e42243372b9ff068fb8ad0f40a30a9d0db`
- Operation: clone, retain `editors/vscode-sagan` history, and relocate it to
  the candidate repository root
- Extracted commit: `b82d0c1231c7d2edcc75bdb2ae34b37dbc14bd0b`
- Extracted history: 42 commits
- Extracted tree: 52 files
- Repository integrity: `git fsck --full` passed
- Dependency installation: `npm ci` passed
- Focused extension tests: `npm test` passed
- Bundle integrity: `npm run test:bundle` passed

The extension grammar tests initially depended on Sagan compiler-repository
fixtures. Those cases now use extension-owned fixtures, allowing the extracted
repository to test itself.

The integration runner now resolves its packaged demo relative to the
extension root. An initial direct monorepo live-host attempt exited with code
1 after launch without a test assertion. A later attempt with normal filesystem
access passed in the monorepo on October 5.

The updated subtree extraction passed again on October 5 from source commit
`871927a44fd0fc873a186200c478077d07bd3250` (extracted commit
`2676765c64731551d52672970a885e78809a9f02`). It preserved 43 extension
commits and 52 files. `git fsck --full`, `npm ci`, `npm test`, and
`npm run test:bundle` passed. The supplied `bin/sagan.exe` reported a complete
diagnostic-free result for `examples/demo.sagan` inside the extracted tree.

A fresh temporary extraction from source commit `8cafb34` retained the same
43-commit extension history at extracted commit
`2676765c64731551d52672970a885e78809a9f02`. `npm ci`, `npm test`, and
`npm run test:bundle` passed. The extracted demo was diagnostic-free with the
current Sagan compiler, and `npm run test:integration` passed against explicit
absolute compiler and language-server paths. The first attempt to run the
rehearsal wrapper stopped because a relative Windows tool path was supplied;
the corrected absolute paths passed in the same extracted clone. No remote
repository was changed.

## Pinned filter-repo rehearsal after the organization transfer

On October 5, 2026, a separate disposable mirror was cloned from
`Sagan-Shoulak/sagan` at source `dev`
`4132c8c37f99dc5f89c7141f5780b583b64d192e`. A pinned copy of
[upstream git-filter-repo](https://github.com/newren/git-filter-repo/releases/tag/v2.47.0)
was taken from tag `v2.47.0`, which resolves to commit
`6f79afc8c90c592a3052e6cc53c2ca8907515bca`. The single-file script's
SHA-256 was
`67447413e273fc76809289111748870b6f6072f08b17efe94863a92d810b7d94`.
The upstream tag did not carry a verifiable signature; the commit and file
hash are recorded for repeatability. The script ran directly with the local
Python interpreter, without modifying Git's global installation.

The filter retained `editors/vscode-sagan/` and all nine shared paths in
`vscode-relocation.toml`, then relocated the extension subtree to the root.
The filtered `dev` tree has **exactly 61 files**, matching the ownership
manifest after relocation: no missing or extra paths. The filtered `dev`
history has 99 commits and tip
`5a8d156f18785d12ba6603f751f9a5533c2ea1b9`; `main` has 98 commits.
`git fsck --full` passed, and a representative rewritten commit preserved
its source author, committer, timestamps, and subject.

A fresh normal clone of that filtered mirror passed `npm ci`, `npm test`,
`npm run test:bundle`, and the live `npm run test:integration` with explicit
existing compiler and language-server paths. This was a **local-only**
rehearsal: no split repository was created or pushed, and neither the source
repository nor its transfer backup was filtered.

## Remaining gates

The full path set and its history now pass a pinned filter-repo rehearsal.
The first standalone source-build and release-script rewrite is preserved
as an ordered local patch series in `../patches/vscode/`. Its Windows
rehearsal passed `npm test`, `npm run test:bundle`, live
`npm run test:integration`, standalone VSIX packaging/checksum, isolated
VSIX installation, and matching-native-tool activation. The integration
and package checks used the existing local native compiler/server, not
a fresh CI build of the exact pinned source. The extracted CI workflow has
not yet run on GitHub or on Linux/macOS. `npm ci` continued to report six
high-severity dependency findings; no automatic dependency rewrite was
performed.

Imported documentation and remaining release integration still contain
monorepo-relative behavior beyond this first patch. Apply the rewrites in
`../vscode-relocation.toml` and test the resulting independent repository
before creation. The owner approved a temporary CI contract that checks out
an exact pinned Sagan source commit and builds compiler/LSP on each platform;
the exact lock and cross-platform jobs still need implementation and
verification. The filtered mirror retained nine historical Sagan release
tags, which must **not** be published as VS Code extension releases by
accident; approve a branch/tag policy first. Complete the destination
governance and owner-survivability/rollback drills before publishing.

`npm ci` reported six high-severity dependency findings. Do not apply an
automatic breaking dependency rewrite; audit and resolve them in a dedicated
extension dependency request.
