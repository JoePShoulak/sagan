---
title: Testing
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Testing

Run the complete compiler check from Git Bash:

```bash
bash scripts/test.sh
```

This performs a clean C++ build; runs the compiled-in unit tests; exercises the
CLI, diagnostics, renderers, semantic index, workspace overlays, and source
mapping; and runs the integration suite against focused Sagan fixtures.

The test material has three distinct homes:

- C++ library and component tests live in `tests/*.cpp`.
- Sagan source inputs live in `tests/fixtures/`, grouped by compiler phase.
- End-to-end shell assertions live in `tests/integration/`.

Run only the integration layer with:

```bash
make integration-test
```

Human-facing programs belong in `examples/`; they are not substitutes for
assertions. The curated visual and executable demonstrations are:

```bash
make run-demo
make geometry-demo
make units-demo
make orbit-math-demo
make package-demo
bash scripts/ast_demo.sh --no-open
```

## Coverage

Run an instrumented build with:

```bash
make coverage
```

The coverage script uses GCC's `--coverage` instrumentation and runs the full
suite. When `lcov` is available it writes `build/coverage.info`. Instrumented
objects are removed even after failure so later ordinary builds do not inherit
coverage linkage.

`.github/workflows/coverage.yml` repeats this on Ubuntu for pull requests that
affect the implementation and for every integrated push to `dev` or `main`. It
enforces the repository's line-coverage floor on Ubuntu 26.04, then passes the
measured report as a CI artifact to a separate Ubuntu 24.04 job for Codecov
upload through GitHub OIDC. Both jobs must pass for the coverage workflow to
be green. Coverage shows which implementation paths ran; it does not
prove that every language rule is correct.

## Branch and integration scope

On a focused change branch, run the smallest set of tests that fully exercises
the changed surfaces. After the branch merges into `dev`, CI reruns the complete
compiler, coverage, documentation, extension, and installer gates against the
combined repository state. Promotion to `main` then starts the stable-release
pipeline. See [Change and publication lifecycle](change-lifecycle.md) for the
required sequence.

## Documentation

Validate documentation metadata, links, navigation, Markdown, executable
examples, and rendering with:

```bash
bash scripts/docs.sh check
```

## VS Code extension

Run the extension's lexical, capability, lifecycle, operations, Test Explorer,
bundle, and live Extension Host checks separately from the C++ suite. Build the
matching compiler and language server first:

```bash
make all bin/sagan-lsp
cd editors/vscode-sagan
npm ci
npm test
npm run test:bundle
npm run test:integration
```

The unit tests are offline. The integration command downloads or reuses the
supported VS Code test runtime, starts a real Extension Development Host, and
checks the extension against `bin/sagan-lsp`. CI repeats the extension gate on
Linux, macOS, and Windows; Linux supplies Xvfb for the graphical host.

These commands test the extension implementation. To write and run tests *in a
Sagan program*, follow [Testing Sagan programs](../tooling/testing-sagan-programs.md).
