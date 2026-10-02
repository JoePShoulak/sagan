---
title: Change and publication lifecycle
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Change and publication lifecycle

This page is the entry point for taking a change from an idea to a published
Sagan release. It defines the branch and merge policy. The linked pages remain
authoritative for detailed test, version, documentation, and release mechanics.

## Branch roles

| Branch | Purpose | Publication effect |
| --- | --- | --- |
| `type/short-name` | One focused feature, fix, documentation, test, or maintenance change | None |
| `dev` | Integration branch for the next experimental version | Refreshes experimental documentation after all required checks pass |
| `main` | Exact source history for stable releases | Starts stable release preparation |

Use a short-lived branch based on current `dev` for every change. Recommended
prefixes are `feat/`, `fix/`, `docs/`, `test/`, `refactor/`, and `chore/`.
Automation-created branches may use their tool's required prefix. Do not develop
directly on `main`, and do not use `main` merely to see whether a change passes.

## 1. Start a focused branch

Begin from an up-to-date `dev` branch:

```bash
git switch dev
git pull --ff-only origin dev
git switch -c type/short-name
```

Keep the branch limited to one coherent change. Check the
[technology stack and change map](../implementation/technology-stack.md) before
editing so compiler, language-server, extension, tests, examples, documentation,
packaging, and release effects are not missed.

## 2. Validate the change in isolation

Run the smallest checks that fully cover the surfaces changed. Examples include:

```bash
bash scripts/test.sh
bash scripts/docs.sh check
make integration-test
make coverage
```

Extension changes additionally use the commands in [Testing](testing.md). A
branch does not need to run unrelated packaging or publication jobs, but its
pull request must state what changed and list the checks that were run. New or
changed behavior needs focused tests; documentation examples must remain
executable and match their expected output.

## 3. Declare the version impact and commit

Use the repository's compact Conventional Commit form. `fix:` has patch impact,
`feat:` has minor impact, and `!` or a `BREAKING CHANGE:` footer has major impact.
Documentation, tests, refactoring, and maintenance have no release impact unless
they intentionally carry a breaking marker. See [Versioning](versioning.md) for
the complete rules and README badge preparation commands.

Before pushing, verify the resulting repository identity:

```bash
bash scripts/version.sh current
bash scripts/version.sh check-badge
git status --short
```

Then publish only the change branch:

```bash
git push -u origin type/short-name
```

## 4. Merge into `dev`

Open a pull request from the change branch into `dev`. Path-relevant pull-request
workflows validate the affected subsystem. Review must confirm the declared
semantic-version impact, test evidence, documentation impact, and absence of
unrelated files.

After merge, CI retests the integrated `dev` state rather than trusting the
branch result alone. The integration gates are:

- the complete compiler and CLI suite;
- the enforced coverage floor;
- strict documentation and executable-example checks;
- Linux, macOS, and Windows extension-host checks; and
- the Windows installer build and smoke test.

These checks deliberately overlap some branch checks. Their purpose is to find
interactions between independently valid changes. A failed `dev` check blocks
promotion; fix it through another change branch. Once documentation validation
passes, the versioned-docs workflow publishes the integrated `dev` snapshot as
the selectable **experimental** documentation channel.

## 5. Promote `dev` to `main`

When the integrated commit is intended to become a stable release, open a pull
request from `dev` to `main`. The candidate must have green `dev` integration
checks, a correct version badge, and the required release evidence. Configure
branch protection so `dev` changes require pull requests and integration checks,
and so `main` accepts only the protected `dev` promotion path with required
checks and approval.

Merging that pull request is a publication action. A push to `main` runs release
preparation, calculates the Git-derived semantic version, creates and verifies a
signed immutable tag when that version is new, and dispatches the complete
release gates. A no-impact commit whose version is already tagged runs the
preparation check but does not create a duplicate release.

## 6. Publish and archive the stable release

The signed tag pipeline runs the compiler, coverage, documentation, Windows
packaging, checksum, installation, upgrade, SBOM, and vulnerability gates. A
passing stable candidate becomes a draft GitHub Release. It is not public until
the protected `stable-release` environment receives project-owner approval.

After approval, GitHub Releases is canonical. CI publishes numbered
documentation from the exact release tag, updates the documentation version
selector and `latest` alias, and mirrors verified release assets to HP1. Released
tags, assets, checksums, manifests, SBOMs, and numbered documentation are never
rewritten. Corrections use a new patch release.

For the exact gates, exceptions, withdrawal process, and ownership rules, read
[Release lifecycle](release-lifecycle.md). For the documentation channels and
HP1 deployment process, read [Documentation workflow](documentation.md) and
[Self-hosting](hosting.md).

## Required repository settings

The repository configuration should enforce the policy documented here:

- prohibit direct pushes and force pushes to `dev` and `main`;
- require pull requests, successful checks, and resolved conversations;
- require the integrated checks on `dev`;
- require a current green `dev` candidate and approval before merging to `main`;
- protect the `stable-release`, `release-signing`, and `documentation`
  environments with their documented reviewers and secrets; and
- retain immutable releases and signed tags.

Repository-host settings are part of the release system even though they are not
stored in Git. Audit them whenever workflow names or required checks change.
