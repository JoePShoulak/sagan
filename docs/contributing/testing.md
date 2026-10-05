---
title: Testing
status: review-needed
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
enforces the repository's line-coverage floor and uploads the report to Codecov
through GitHub OIDC. The owner approved a one-time v4.9.5 exception when the
external Codecov endpoint cannot be reached: the upload is still attempted and
warns on failure, while the measured report artifact and 90% floor remain
mandatory. Later versions require the upload to pass. Coverage shows which
implementation paths ran; it does not
prove that every language rule is correct.

## Branch and integration scope

On a focused change branch, run the smallest set of tests that fully exercises
the changed surfaces and refine until they pass. After the branch merges into
`dev`, rerun those relevant tests against the combined repository state; do not
run the complete suite merely because work entered `dev`. Promotion from `dev`
to `main`, and every release, is contingent on the complete compiler, coverage,
documentation, extension, installer, and other required platform gates passing.
See [Change and publication lifecycle](change-lifecycle.md) for the sequence.

## Documentation

For prose, navigation, or metadata changes that cannot affect executable
examples, validate documentation structure, links, navigation, Markdown, and
rendering without compiling every example:

```bash
bash scripts/docs.sh check-structure
```

When executable examples or their supporting compiler, package, tooling, or
harness behavior changes, test the examples directly:

```bash
bash scripts/docs.sh check-examples
```

Run the combined documentation gate when both surfaces are affected or when
preparing integration and publication evidence:

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
