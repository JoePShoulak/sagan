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

## Remaining gates

This rehearsal intentionally covered only the extension subtree. The exact
`sagan-vscode` ownership manifest also assigns selected root workflows,
documentation, and release scripts. Their destination paths and required
rewrites are recorded in `../vscode-relocation.toml`; importing and testing
them together still remains before the real extraction.

`git filter-repo` remains unavailable, so this rehearsal used the slower
`git filter-branch` fallback. Production history extraction remains blocked on
the approved tool, backup, ref/tag policy, and rollback drill.

`npm ci` reported six high-severity dependency findings. Do not apply an
automatic breaking dependency rewrite; audit and resolve them in a dedicated
extension dependency request.
