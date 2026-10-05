# Independent headless physics candidate patch

This is a **local-only rehearsal**, not a published `sagan-physics` repo.
Filter a disposable mirror of source preparation commit
`3c90a111f504279b456736bb0c858af993c98463` with pinned
`git-filter-repo` v2.47.0, `--no-ff`, and the exact
`file-manifests/sagan-physics.txt` paths. The expected filtered base is
`15d34e1e42e14cf4a63902a7bc930c7385e3b894`, with 29 owned files and
11 surviving commits. Default filtering omitted four solar-Lagrange files;
do not use it. Verify the filtered tree and Git objects before replay.

Apply the numbered patch with `git am` only in a disposable normal clone of
that base. Expected result: commit
`f9d24d04d628574ce2c8627c0172ba6effc232da`, tree
`4d53d27daf31481eac74bcd3bf3ad993d79d1f1f`.
The patch adds a candidate-local package index, changes six numeric
demo/test scripts to use `SAGAN_EXECUTABLE` or installed `sagan`, and adds
draft maintainer, technology, new-chat, and agent guides. New documentation
guides are `review-needed`, not publication-ready.

On Windows, all three headless integration scripts passed against this
candidate package with the existing Sagan 4.9.5 development executable.
The orbit suite also checked invalid mass, step, and overlapping positions.
A fresh-clone `git am` replay matched the expected tree, passed `git fsck
--full`, and reran all three tests. No split remote was created or changed.

The package remains under `libraries/physics/` in this first candidate.
Final root relocation, independent Linux/macOS/Windows hosted CI, package
distribution, compatibility promises, official-docs aggregation, and the
owner clean-machine drill remain. Re-filter from final reviewed `dev` and
refresh exact pins before publication. Releases and `main` promotion remain
on hold.
