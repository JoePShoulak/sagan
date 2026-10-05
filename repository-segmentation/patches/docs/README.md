# Independent official-docs aggregation patch

This is a **local-only rehearsal** for `sagan-docs`, not a published
repository. It applies with `git am` only to a disposable normal clone of
the docs-filtered repository at base
`47bdebf2cd576eab2d3bb6468daad13231c8b449`, itself filtered from
source preparation commit `72edf479f8820ed63699fa942354244f1f583e36`.
Do not apply it to the intact Sagan repository. The expected result is draft
commit `dbbaf69d6f79683ef56b0fb572e2c49f1683668e`, tree
`d2992da632fe7e6cec654bde9a8f35fd76a1f31a`.

The patch pins the primary Sagan source to public organization commit
`4132c8c37f99dc5f89c7141f5780b583b64d192e`, declares the six
site-owned paths, and adds an assembler that refuses wrong, dirty, or
wrong-origin source checkouts and existing outputs. It creates a new
generated MkDocs tree with source provenance, overlays only those six paths,
and never edits the source checkout. Draft root maintainer, technology,
new-chat, and agent guides record the boundary. Edited official-documentation
guide pages are marked `review-needed` and not publication-ready.

Five offline assembler tests passed on Windows. The public primary checkout
at the pinned SHA assembled successfully, and the generated tree passed a
strict MkDocs build using the existing pinned docs environment. The build
reported an already-existing unlisted contributor checkpoint page and exited
zero. A fresh normal clone replayed the patch with `git am`, matched the
expected tree, passed `git fsck --full`, and reran all five tests. No split
remote was created, no deployment ran,
and the live HP1 site was untouched.

This is the **first primary-source aggregation**, not full docs readiness.
Existing `docs.sh`, release/version scripts, and hosting automation still
assume monorepo layout. Future physics/rendering/extension/game docs need
explicit source/export locks and sections; review gates, component PR
previews, host cutover, rollback, clean-machine drill, Linux/macOS CI, and
new-organization permissions still require work. Re-filter from the final
reviewed `dev` and refresh exact pins before publication. Releases and
`main` promotion remain paused.
