# History-preserving extraction and rollback draft

This procedure is a draft preparation artifact. The owner chose public
visibility for all six split repositories on October 5, 2026. Do not run its
push steps until destination permissions, protections, exact file ownership,
and rollback have been approved. Always operate on a temporary mirror, never
the working checkout.

## Prerequisites

- A clean, reviewed `dev` commit recorded in `readiness.toml`.
- A verified offline mirror backup and a recorded ref inventory.
- An empty destination repository with no generated starter commit.
- Working GitHub authentication with organization repository permissions.
- `git filter-repo` installed and its version recorded.
- An approved exact include/rename list derived from `inventory.tsv`.

An official upstream `git-filter-repo` v2.47.0 copy is pinned by commit and
file hash in `rehearsals/vscode.md`; it ran from a disposable checkout, not a
global installation. GitHub CLI authentication works for an active
`Sagan-Shoulak` admin. The
intact primary repository transfer and its frozen-baseline local backup passed;
see `audits/primary-transfer-result-2026-10-05.md`. Destination permissions,
exact ownership, approved extraction tooling, and rollback rehearsal still
precede any split repository.

The local fallback rehearsal script is
`scripts/rehearse_vscode_extraction.sh`. It uses `git filter-branch` only to
prove that the extension subtree has usable independent history and can build
and test after relocation. It is not the approved production extraction tool.

## Preserve the source

From the parent directory of the normal checkout:

```bash
git clone --mirror sagan sagan-segmentation-backup.git
git -C sagan-segmentation-backup.git show-ref > sagan-segmentation-backup.refs
git -C sagan-segmentation-backup.git fsck --full
```

Copy the mirror and ref inventory to approved backup storage before filtering.
Record their hashes and restore location outside the source repository.

## Rehearse one extraction

Create another temporary mirror from the preserved source mirror. For example,
the VS Code extraction will eventually retain the extension subtree and move it
to the new repository root:

```bash
git clone --mirror sagan-segmentation-backup.git sagan-vscode-filter.git
git -C sagan-vscode-filter.git filter-repo \
  --path editors/vscode-sagan/ \
  --path-rename editors/vscode-sagan/:
git -C sagan-vscode-filter.git fsck --full
```

The final filter command must also retain deliberately shared root history such
as security, licensing, maintainer, technology, and onboarding files when the
approved extraction manifest requires them. Generate those files before the
destination's first reviewed commit rather than silently losing the contract.
For the VS Code repository, `vscode-relocation.toml` lists each owned file
outside the extension subtree and its destination path. Those files need an
additional history-preserving import and the listed rewrites before parity can
be claimed.

## Verification before any push

```bash
git -C sagan-vscode-filter.git log --all --oneline --decorate
git -C sagan-vscode-filter.git ls-tree -r --name-only HEAD
git -C sagan-vscode-filter.git fsck --full
```

Compare the filtered tree to the approved ownership manifest, build and test a
normal clone of the filtered mirror, and verify author, timestamp, tag, and
relevant rename history. A successful filter alone is not extraction parity.

## Publish and rollback boundary

Only after the filtered clone passes its extraction gate may a maintainer add
the empty destination remote and push the reviewed `dev` and `main` heads
with explicit branch refspecs. The VS Code history retains nine unrelated
Sagan language release tags: do **not** use `--mirror`, `--all`, `--tags`,
or `--follow-tags` for its first push. Keep all tags unpublished until the
owner approves an extension-specific release-tag policy after releases resume.
Do not delete or rewrite the monorepo copy at this stage. If verification, CI,
documentation aggregation, package consumption, or workspace integration
fails, abandon the destination candidate and restore from the untouched mirror.

Remove the monorepo copy only in a later reviewed request after the destination
release, exact workspace lock, documentation aggregation, redirects, and
rollback drill all pass.

## Recorded local rehearsal

The October 5, 2026 VS Code rehearsal passed from source commit `33d258e`.
The extracted subtree also passed its compiler-demo check and live Extension
Development Host test on that date. Details and remaining gaps are recorded in
`rehearsals/vscode.md`. This proves the extension subtree can stand alone; it
does not approve repository creation, shared-file relocation, pushing, or
deletion from the monorepo.
