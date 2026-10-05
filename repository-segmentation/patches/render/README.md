# Independent Windows renderer candidate patch

This is a **local-only rehearsal**, not a published `sagan-render` repo.
Filter a disposable mirror of source preparation commit
`3c90a111f504279b456736bb0c858af993c98463` with pinned
`git-filter-repo` v2.47.0 and exact
`file-manifests/sagan-render.txt` paths. The expected filtered base is
`e60ca2e95924775c4e6081b8fd2e506bdc1235ef`, with 21 owned files and
19 surviving commits. Verify tree parity and Git objects before replay.

Apply the numbered patch with `git am` only in a disposable normal clone of
that base. Expected result: commit
`99a8b97d2f9ef4dfc3412e4a4baeab7d8b9d9b3e`, tree
`f3bee08e2c9a6fea6138fb508ce507068c9c9c0a`.
The patch adds a candidate-local package index, lets demo scripts use
`SAGAN_EXECUTABLE` or installed `sagan`, removes the monorepo's launcher
resource object from native linking, and supplies a stock Windows icon
fallback when an executable has no custom icon. It also adds draft root
maintainer, technology, new-chat, and agent guides. New documentation guide
pages are `review-needed` and not publication-ready.

The first local window run failed because the bridge still required the
custom icon resource. A subsequent compiler attempt exposed a wide/narrow
Win32 icon-identifier mismatch; both were corrected in the candidate. The
short auto-closing window test and 960×540 shape/text BMP capture then passed
on Windows with the extracted renderer and existing Sagan 4.9.5 executable.
A fresh-clone `git am` replay matched the expected tree, passed `git fsck
--full`, and reran both tests. No split remote was created or changed.

The package remains under `libraries/render/` in this first candidate.
Linux/macOS rendering is **not** implemented; the native bridge explicitly
reports Windows-only support. Final root relocation, hosted CI, native
packaging, official-docs aggregation, and the owner clean-machine drill
remain. Re-filter from final reviewed `dev` and refresh exact pins before
publication. Releases and `main` promotion remain on hold.
