# Documentation repository extraction rehearsal

On October 5, 2026, a disposable local mirror of the clean preparation
branch at `72edf47` was filtered with the pinned `git-filter-repo`
v2.47.0 script using exactly
`repository-segmentation/file-manifests/sagan-docs.txt`. Neither the
intact repository nor its transfer backup was rewritten.

The filtered candidate tip is
`47bdebf2cd576eab2d3bb6468daad13231c8b449`. It contains exactly
**27 files**, matching the approved documentation ownership manifest
with no missing or extra paths. Its surviving history has 50 commits,
and `git fsck --full` passed. The filtered `main` snapshot is
`a5d11959634d2397ca4f29db9b2d23364b66208c`.

A separate disposable bare repository received only the filtered
candidate as `dev` and the filtered `main` snapshot as `main`.
Its default HEAD is `dev`; `scripts/verify_initial_split_refs.sh`
confirmed exactly two heads, zero tags, and valid Git objects. The
filtered source still contains nine old Sagan language release tags,
none of which were pushed to the candidate. No GitHub repository was
created or changed.

This is a **history/path rehearsal only**. The current `mkdocs.yml`
and site navigation refer to component-authored pages outside this
27-file tree. The docs repository cannot publish a complete official
site until the documentation-export manifests, exact component locks,
aggregation/build workflow, human-review gate, and host rollback are
implemented and tested. The public HP1 site remains on the intact
repository's documentation workflow until a separately reviewed
cutover. Re-extract from final reviewed `dev` before publication.
