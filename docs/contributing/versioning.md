---
title: Versioning
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Versioning

Sagan combines semantic version impact with a Git-derived build identity. The
numeric version is calculated by applying Conventional Commit markers after the
baseline recorded in `version.conf`. The full form is
`MAJOR.MINOR.PATCH+gREVISION[.dirty]`.

## Declaring a commit's impact

Use the commit subject or footer to declare the change:

| Impact | Declaration | Example |
| --- | --- | --- |
| Patch | `fix:` | `fix:reject-empty-interpolation` |
| Minor | `feat:` | `feat:add-collection-literals` |
| Major | `!` after the type or scope | `feat(parser)!:replace-vector-syntax` |
| Major | `BREAKING CHANGE:` footer | `BREAKING CHANGE: vectors now require dimensions` |
| None | any other type | `docs:explain-collection-parsing` |

Scopes are optional, so `fix(parser): ...` and `feat(renderer): ...` work as
expected. If a commit contains a breaking marker, major takes precedence over
every other marker.

## Preparing a change

The README badge is part of the commit and must show the resulting numeric
version. Prepare it before committing:

```bash
bash scripts/version.sh prepare patch
bash scripts/version.sh prepare minor
bash scripts/version.sh prepare major
```

The helper changes only the README badge. It does not stage files, create a
commit, or push. Documentation-only and maintenance commits have no version
impact and do not require a badge update.

Routine commits and pushes go directly to `dev`; the project does not require
per-feature branches. When the integrated `dev` history is ready for a release,
promote it deliberately to `main` without adding unrelated publication-only
changes.

Every push to `main` runs the release-preparation workflow. It checks the
history-derived version and README badge. For a new 0.x version, CI signs an
`-rc.1` preview tag; for a new stable version from 1.0 onward, CI signs the
stable tag. Both run release gates. Passing preview gates publishes a marked
prerelease; passing stable gates creates a draft whose publication remains
behind the project owner's approval. A documentation, test, or maintenance
commit that does not change the version still runs CI but does not create a
duplicate tag.

The private CI signing key belongs in the `SAGAN_RELEASE_TAG_SSH_PRIVATE_KEY`
GitHub Actions secret, never in the repository. Its public key must be
registered as a GitHub SSH signing key and listed in
`.github/allowed_signers`. GitHub's `GITHUB_TOKEN` does not trigger a workflow
from its own tag push, so the main-push workflow explicitly dispatches the
release workflow at the signed tag.

The repository convention uses the compact `type:description` subject style
shown above. Before committing, propose escalation if a change appears to need
a higher impact than originally expected; then prepare the approved impact.

After committing, verify the result:

```bash
bash scripts/version.sh current
bash scripts/version.sh check-badge
```

Additional inspection commands are available:

```bash
bash scripts/version.sh numeric
bash scripts/version.sh next minor
bash scripts/version.sh impact HEAD
```

`make get-version` uses the same calculator. Builds regenerate the compiled-in
version unconditionally, and `.dirty` records any tracked or untracked workspace
change.

## Baseline changes

`version.conf` preserves the migration baseline. It should change only when the
project deliberately adopts a new version-history baseline, not during an
ordinary major, minor, or patch commit. Normal version progression comes from
commit declarations.
