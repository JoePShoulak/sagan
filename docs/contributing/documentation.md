---
title: Documentation workflow
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation workflow

## Local commands

```bash
bash scripts/docs.sh setup
bash scripts/docs.sh serve
bash scripts/docs.sh check
bash scripts/docs_versions_test.sh
```

## Executable examples

Executable documentation examples live under `docs/examples/executable/` so
their source and expected output are retained in every published documentation
archive. Each `NAME.sagan` program must have a same-named `NAME.stdout` file
containing its exact expected standard output. Programs must exit successfully
and must not require interactive input.

`bash scripts/docs.sh check` builds the current Sagan compiler, runs every
archived executable example, and compares its output exactly after normalizing
platform line endings before building the site. A missing expected-output
file, an orphaned output file, a nonzero
exit status, output on standard error, or any output difference fails the
documentation build. Do not paste a second, independently maintained copy of
an executable example into a page; use the snippets extension to display the
archived source directly.

Add a new example with this layout:

```text
docs/examples/executable/example-name.sagan
docs/examples/executable/example-name.stdout
```

Then run:

```bash
bash scripts/docs.sh check
```

The unreleased documentation channel is named **experimental**. It is useful to
project contributors and to anyone building from a clone of the live repository.
Documentation changes on pull requests are validated on a GitHub-hosted runner.
After a matching change reaches `main`, the same validation must pass before the
experimental version is updated and the dedicated HP1 runner installs the
versioned site.

The workflow watches the documentation tree, MkDocs configuration and
requirements, documentation scripts, and HP1 deployment files. It can also be
started manually from GitHub Actions. See [Self-hosting](hosting.md) for runner
setup and deployment safeguards.

## Documentation version

The repository build defaults to documentation version **experimental**. A
release build supplies its immutable Sagan version through
`SAGAN_DOCS_VERSION` and sets `SAGAN_DOCS_CHANNEL=released`. Published versions
use the corresponding Sagan release number.

Mike stores each released version beside `experimental` and provides the
version selector. The `latest` alias and the site root point to the newest
released documentation. Before the first release exists, the site root may
temporarily point to `experimental`.

Every page starts with this metadata:

```yaml
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
```

To confirm a page as accurate and ready for publication, change all five fields together:

```yaml
status: publication-ready
publication_ready: true
verified_in: RELEASE_VERSION
verified_on: YYYY-MM-DD
verified_by: Reviewer name
```

`bash scripts/docs.sh check` rejects missing, incomplete, contradictory, or stale
ready-state metadata. In `experimental`, a ready page may be verified for an
upcoming semantic release so it can be reviewed before publication. A released
build accepts only pages verified for that exact release version. Previously
ready pages must therefore be reviewed and confirmed for each new release before
they can remain publication-ready in its archive.

Use `review-needed` when a draft is substantial enough for review but is not yet confirmed.

## Version publication

Every push to `main` refreshes only the `experimental` version. A manual
documentation workflow run with a release version performs the publication
gate, creates an immutable version, moves the `latest` alias to it, and makes
`latest` the default served at the site root. Existing release directories are
retained.

Release documentation must be built from the matching Sagan release ref. Never
overwrite an archived release merely to reflect the current repository; publish
a new release version instead.

The [documentation roadmaps](documentation-roadmaps.md) define the post-1.0
expansion plan and the ordered behavior-confirmation audit used to move pages
from internal drafts to publication-ready documentation.
