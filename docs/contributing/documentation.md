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
```

The documentation is internal-only, but it has a deployment workflow for the
privately hosted preview. Documentation changes on pull requests are validated
on a GitHub-hosted runner. After a matching change reaches `main`, the same
validation must pass before the dedicated HP1 runner installs a new origin
release.

The workflow watches the documentation tree, MkDocs configuration and
requirements, documentation scripts, and HP1 deployment files. It can also be
started manually from GitHub Actions. See [Self-hosting](hosting.md) for runner
setup and deployment safeguards.

## Documentation version

The current documentation release is defined once at `extra.documentation.version` in
`mkdocs.yml`. Internal builds use semantic versions with an internal build suffix, such as
`0.1.0-internal.1`.

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
verified_in: 0.1.0-internal.1
verified_on: YYYY-MM-DD
verified_by: Reviewer name
```

`bash scripts/docs.sh check` rejects missing, incomplete, contradictory, or stale ready-state
metadata. When the site documentation version changes, previously ready pages must be reviewed
and confirmed for the new version before they can remain publication-ready.

Use `review-needed` when a draft is substantial enough for review but is not yet confirmed.
