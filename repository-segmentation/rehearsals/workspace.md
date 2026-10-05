# Workspace repository extraction rehearsal

On October 5, 2026, a disposable local mirror of the clean
`codex/segmentation-public-visibility` preparation branch at
`68d55b003709e28b4e96bdcbd22d8341018c2253` was filtered with the
pinned `git-filter-repo` v2.47.0 script documented in
`vscode.md`. The include list was the exact
`file-manifests/sagan-workspace.txt` at that source commit. The intact
source checkout and frozen transfer backup were not filtered.

The filtered preparation tip is
`47a72ef3a23421248dc2f983ead8d5548a5d473d`. Its tree has **69 files**,
exactly matching the ownership manifest with no missing or extra paths,
and its surviving history has 75 commits. `git fsck --full` passed. A
representative rewritten commit retained its author, committer,
timestamps, and subject.

A second disposable bare repository received only that reviewed candidate
as `dev` and the filtered `main` snapshot
`f2962c9fb948e102c2824e85c803d1b2ce318ef9` as `main`. Its default
HEAD was set to `dev`; `scripts/verify_initial_split_refs.sh` confirmed
exactly those two heads, zero tags, and valid Git objects. A historical
tag survived in the filtered source but was **not** published to the
candidate. Neither local rehearsal contacted GitHub or created a split
repository.

This proves only path/history and initial-ref safety. It does **not**
establish an independently functional `sagan-workspace`: integration
scripts, `repository-segmentation` contracts, documentation links,
workspace locks, and source paths still assume the monorepo. Before
publication, rewrite and test those contracts against separately
versioned component checkouts, add specialized maintainer/technology/chat
guides, complete the owner recovery drill, and rerun the extraction from
the final reviewed `dev` commit.
