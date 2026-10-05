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

## Primary-source aggregation candidate

A separate patch at `../patches/docs/` adds an exact source lock, explicitly
overlays the six docs-site-owned files, and assembles a fresh generated
MkDocs tree from the transferred primary repository at commit
`4132c8c37f99dc5f89c7141f5780b583b64d192e`. Its five offline tests
passed. The real pinned source assembled, and a strict MkDocs build passed
on Windows using the existing documentation environment. It reported the
pre-existing unlisted contributor checkpoint page but did not fail. The
source checkout, shared checkout, and live site were not modified.

The patch also supplies draft root maintainer, technology, and chat guides.
It does not wire the old monorepo `docs.sh`/deployment scripts to the
aggregate or add other component exports, independent hosted CI, human
publication review, or hosting rollback. No split remote was created or
pushed. A fresh normal clone replayed the patch with `git am`, matched tree
`d2992da632fe7e6cec654bde9a8f35fd76a1f31a`, passed `git fsck --full`,
and reran all five offline tests.

## Pinned component-mount rehearsal

A second replayable patch on the same filtered base, candidate commit
`3640c57c1dfe731350ede217b6263b673bb0c43a`, adds planned source mounts
for the extension, physics, and rendering sections. The canonical
`docs-sources.lock` leaves their nonexistent GitHub remotes inactive. An
ignored local fixture instead pinned the three clean extracted candidates
to their actual local origins and exact commits. The assembler copied their
four extension pages, physics page, rendering page, and three rendered image
assets into the official site's existing paths. `sources.json` recorded all
four source pins, and a strict Windows MkDocs build passed. The same
pre-existing unlisted contributor checkpoint warning remained.

Eight offline assembler tests passed; a clean replay of both patches matched
tree `6a4bc68abcf6eeadae6d0e2130796415a6c1b84f`, passed `git fsck
--full`, and reran all eight tests. This proves local aggregation with
ownership and lock checks. It does not prove hosted CI, component-PR preview,
publication review, version selection, deployment, or HP1 rollback. No
split remote was created or pushed.

A third patch adds a draft three-platform offline-test and primary-source
strict-build workflow. Its candidate and replay trees both equal
`cb43889503119e49cb454fd3d0334def3c9123e0`; `git fsck --full`
passed. The workflow is not a hosted CI pass and intentionally fails closed
if component mounts become active before its source-checkout step is
extended. No workflow was dispatched in a split destination.
