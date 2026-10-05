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

## Exact-lock bootstrap candidate

A later, separate local candidate on that same filtered base is captured as
the replayable patch in `../patches/workspace/`. It moves `workspace.toml` to
the root, activates only the transferred primary repository, and adds an exact
`workspace.lock`, Bash bootstrap/status entry points, offline safety tests,
and draft root maintainer/technology/chat guides. The seven offline tests
passed on Windows. `bash scripts/bootstrap.sh` cloned the public primary
repository into an ignored child checkout at the pinned commit, and
`bash scripts/status.sh` reported it clean. A fresh-clone `git am` replay
matched candidate tree `793efd2316744e0ed7a935084437cf76344fe2db`,
passed `git fsck --full`, and reran all seven tests.

A second patch adds a conservative `restore-lock` command. It moves only
clean, correct-origin children to the exact pin, fetching a missing commit
without tags and refusing ignored-file collisions. Three more offline tests
passed for fetching a changed lock, dirty-checkout refusal, and ignored-file
preservation; wrong-origin refusal was also asserted. The candidate's real
primary checkout was already at its pin and `bash scripts/restore-lock.sh`
reported that without changing it. A fresh normal clone replayed both patches
with `git am`, matched tree `7139252cc1351188be94ef0d0548ad0b1bc39d6f`,
passed `git fsck --full`, and reran all ten offline tests.

At this two-patch stage, this was **partial functionality**, not destination
readiness. The candidate still contained monorepo-only audit scripts and
archived manifest paths; update/build/test/lock-refresh, multi-root editor
generation, package-index generation, cross-repository demos, independent CI,
Linux/macOS checks, and the owner clean-machine drill remained. No split
remote was created or pushed.

## Combined package catalog and cross-repository smoke rehearsal

A third patch on the same local candidate, commit
`711d235d491046e9add6b259217c7706c152c806`, generated one index from
the extracted physics and rendering catalogs without activating their
planned remotes. It validated relative manifest containment, package
identity, duplicate versions, and safe replacement. Fifteen offline tests
passed. With the existing Windows development compiler and that combined
index, the extracted game printed its six-body summaries, all three
headless physics tests passed, and the two auto-closing Windows native
render tests passed. This checked package resolution across separate local
repositories, not hosted destination CI or cross-platform native rendering.

The replay checkout applied the third patch with `git am`; its tree
`e7257c6364f38a66d03e2992fbaf9e494c6ba766` exactly matched the
functional candidate, `git fsck --full` passed, and all fifteen offline
tests passed again. `--from-lock` intentionally refuses the current lock
because physics/rendering remain planned. The combined index was generated
under ignored `build/` and is not a public package catalog.

A fourth patch drafts a three-platform GitHub Actions workflow for the
offline workspace contract tests, without trying to fetch planned split
repositories. It replayed cleanly; the candidate and replay trees both
equal `e5347f489c8a8ec4e1932f045259eb817bc9054e`, and the fifteen
offline tests passed again. The workflow has **not** run in an independent
GitHub destination and does not establish hosted CI readiness.

A fifth patch generates a VS Code multi-root file under ignored `build/`
from the exact active manifest/lock set. It refuses missing, wrong, or dirty
checkouts, omits planned remotes, and preserves a differing output unless
`--force` is explicit. The real locked primary checkout produced a file
containing the coordinator and `sagan` roots. Eighteen offline tests passed
in both candidate and fresh replay; both trees equal
`25f0262c137d590f4df728b32f78859949407b5c`, and `git fsck --full`
passed. Bash syntax passed. This does not establish hosted CI or the still
missing update/build/test/lock-refresh commands.

A sixth patch adds fetch-only `update` and preview-first `lock` commands.
An exact-source fetch in the real local workspace left HEAD and lock pinned
at `4132c8c`; lock preview reported no change. Fixture tests exercised a
new remote tip, preview without mutation, explicit atomic write, and refusal
of dirty or extended-field locks. Twenty-one offline tests passed in both
candidate and fresh replay, tree equality was
`b961e28053df3cb578ca69dbab42d469a47a7be3`, `git fsck --full`
passed, and Bash syntax passed. Build/test orchestration and independent
hosted CI remain unverified.
