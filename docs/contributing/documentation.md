---
title: Documentation workflow
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Documentation workflow

## Writing for humans first

Sagan's documentation is also its main teaching tool. Assume the reader can
program a little but has never designed a language and does not yet know why a
feature matters. A page should help that reader use the feature before asking
them to understand its compiler terminology.

Use these rules throughout the site:

- introduce one idea at a time and build on ideas already explained;
- name the formal term, then immediately explain it in ordinary language;
- show the smallest useful Sagan example before giving edge cases;
- explain what the example does and, when useful, what it prints;
- use short executable snippets on reference pages and longer progressive
  programs in Getting Started and the tour;
- explain the practical consequences of major choices such as composition,
  ownership, exceptions, units, coordinates, and determinism;
- define unfamiliar words instead of assuming language-design knowledge;
- clearly distinguish implemented behavior from planned libraries or tooling;
- keep design history, open questions, and contributor procedure under Project
  Development rather than interrupting the learning path; and
- prefer direct sentences and concrete examples over compressed jargon.

Broad pages group related concepts under descriptive headings. Getting Started
and the tour are meant to be read in order. Reference pages should also work on
their own when someone arrives from search.

All pages remain work in progress until the project owner audits them. A Codex
content pass may correct, expand, and test a page, but it must leave this front
matter unchanged:

```yaml
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
```

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

## Sagan syntax highlighting

Use `sagan` on fenced Sagan source examples. The documentation build registers
the repository-owned lexer in `scripts/docs_sagan_lexer.py`, so these blocks are
highlighted with the same theme-aware token colors and copy controls as other
source code:

````markdown
```sagan
fun main() {
  print("Hello from Sagan!")
}
```
````

`bash scripts/docs.sh check` verifies both the lexer token categories and the
rendered HTML. When Sagan's lexical vocabulary changes, update the compiler,
the VS Code grammar, and this documentation lexer together.

GitHub README rendering is different: GitHub controls its own language list and
does not load repository CSS, JavaScript, or local Pygments lexers. Keep README
examples labeled `sagan` so their language is explicit and so they can gain
native highlighting if Sagan is added to GitHub Linguist later.

The unreleased documentation channel is named **experimental**. It is useful to
project contributors and to anyone building from a clone of the live repository.
The initial 1.0.0 release does not promote unaudited pages into a numbered
documentation archive. The page-by-page owner audit begins immediately after
that release and determines what needs correction before the first audited
documentation version is published.
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

`mike` stores each released version beside `experimental` and provides the
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

After the primary audit confirms a page as complete, change all five fields
together:

```yaml
status: complete
publication_ready: true
verified_in: RELEASE_VERSION
verified_on: YYYY-MM-DD
verified_by: Reviewer name
```

`bash scripts/docs.sh check` rejects missing, incomplete, contradictory, or stale
completion metadata. In `experimental`, a complete page may be verified for an
upcoming semantic release so it can be reviewed before publication. A released
build accepts only pages verified for that exact release version. Previously
complete pages must therefore be reviewed and confirmed for each new release before
they can remain complete in its archive.

Use `review-needed` only after the primary audit deliberately sends a page to
Zach. All unaudited pages remain `work-in-progress`.

## Version publication

Every documentation-relevant push to `main` refreshes only the `experimental`
version. A manual
documentation workflow run with a release version performs the publication
gate, creates an immutable version, moves the `latest` alias to it, and makes
`latest` the default served at the site root. Existing release directories are
retained.

Release documentation must be built from the matching Sagan release ref. Never
overwrite an archived release merely to reflect the current repository; publish
a new release version instead.

The [documentation audit roadmap](documentation-roadmaps.md) defines the first
post-1.0 task and the ordered behavior-confirmation process used to move pages
from unaudited drafts to complete documentation. Audit findings may require
changes to the language or tooling as well as to the prose; the published
1.0.0 tag and assets stay immutable.
