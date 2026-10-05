# Physics, rendering, and Space Game local extraction rehearsal

On October 5, 2026, disposable mirrors of the clean preparation branch at
`3c90a111f504279b456736bb0c858af993c98463` were filtered with the
pinned `git-filter-repo` v2.47.0 script and each exact component ownership
manifest. The intact shared checkout, transferred repository, and frozen
backup were not rewritten. No split GitHub repository was created.

## Physics

The default path-only filter silently omitted four manifest paths from the
solar Lagrange example, despite their presence at the source tip. An added
directory selector did not fix it. A diagnostic single-path filter showed
the files were available in source history. The corrected filter used
`--no-ff` with the exact path list; this preserved all **29** owned files,
with **11** surviving commits and filtered tip
`15d34e1e42e14cf4a63902a7bc930c7385e3b894`. The read-only verifier
`scripts/verify_filtered_component.py` confirmed exact tree parity and
`git fsck --full`. The historical solar-Lagrange addition retained its
author, timestamp, and subject. This is a **local path/history result**;
the package still lives under `libraries/physics/`, and standalone tests,
imports, docs, CI, and owner runbooks have not been relocated or validated.
Use `--no-ff` on the final physics extraction and verify the exact tree again;
do not assume the filter exit code proves file parity.

## Rendering

The default exact-path filter retained **21** owned files and **19** commits
at tip `e60ca2e95924775c4e6081b8fd2e506bdc1235ef`. Exact tree parity
and `git fsck --full` passed. The package and native bridge remain in
monorepo-relative directories; no independent window/backend build or
platform CI has run. This is path/history evidence only.

## Space Game

The default exact-path filter retained **4** owned files and **7** commits
at tip `91cfa605657c126e120f8ee1394b22a884fe8b26`. A separate local
filter relocated `sandbox/` to the candidate root, producing tip
`93be64700db529af5107d373bd2049a8fed93825` with exactly
`SPACE_GAME_DESIGN.md`, `sagan.toml`, `sagan.lock`, and `src/main.sagan`.
The earlier uncommitted seed freeze was stale; the refreshed source was
preserved separately and imported only into a disposable candidate. The
two-patch replay, result tree, hash, Git object check, and existing-toolchain
print-only demo passed; see `../patches/space-game/` and
`../audits/space-game-seed-refresh-2026-10-05.md`. The shared sandbox remains
dirty and untouched. An installed Sagan plus independent physics package,
game test suite, Linux/macOS CI, and owner design clarification remain.

All three filters used local disposable mirrors. Their historical language
release tags must **not** be pushed to new component repositories. Initial
publication must send only reviewed `dev` and `main` heads, with zero tags,
and pass `scripts/verify_initial_split_refs.sh` before any remote push.
