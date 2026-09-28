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
| Patch | `fix:` | `fix: reject empty interpolation` |
| Minor | `feat:` | `feat: add collection literals` |
| Major | `!` after the type or scope | `feat(parser)!: replace vector syntax` |
| Major | `BREAKING CHANGE:` footer | `BREAKING CHANGE: vectors now require dimensions` |
| None | any other type | `docs: explain collection parsing` |

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
