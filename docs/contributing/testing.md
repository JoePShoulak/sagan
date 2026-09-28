---
title: Testing
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Testing
Run the complete current compiler check from Git Bash:

```bash
bash scripts/test.sh
```

This performs a clean C++ build, calculates the development version, runs the
compiled-in tokenizer and parser self-tests, exercises every command-line output
mode, verifies generated AST files, checks representative CLI failures and
diagnostics, and checks `--version`.

## Coverage

Run an instrumented build from Bash with:

```bash
make coverage
```

The coverage script performs a clean build with GCC's `--coverage`
instrumentation and runs the complete front-end suite. If `lcov` is installed,
the portable report is written to `build/coverage.info`. Instrumented objects
are always removed when the script exits, including after a failure, so a later
ordinary build never tries to link coverage objects without the gcov runtime.

`.github/workflows/coverage.yml` repeats this process on Ubuntu for every push
and pull request, then uploads `build/coverage.info` to Codecov. Authentication
uses GitHub OIDC rather than a stored `CODECOV_TOKEN`. Codecov upload failures
fail the coverage job so a missing report cannot appear successful.

The self-tests cover representative valid tokens and expected lexical failures.
Also inspect the full demo and intentional error case when changing token output:

```bash
make demo
bin/sagan examples/tokenizer_error.sagan
```

Validate documentation metadata, links, navigation, Markdown, and rendering:

```bash
bash scripts/docs.sh check
```

Future parser and semantic work will require separate positive and negative test
suites. Tokenizer tests cannot establish grammatical or semantic correctness.
