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
compiled-in tokenizer, parser, renderer, and semantic self-tests; exercises
every command-line output mode; verifies generated AST files; checks
representative CLI failures and diagnostics; and checks `--version`.

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
fail the coverage job so a missing report cannot appear successful. CI and the
release workflow also enforce a 90% line-coverage floor with
`scripts/coverage_threshold.sh`.

The live Codecov badge is authoritative for the current percentage. The suite
covers tokenizer lifecycle and defensive invariants; Unicode, emoji, escapes,
and malformed input; positive and negative parser and semantic cases; every AST
renderer; CLI success and failure paths; and generated output files.

Also inspect the full demonstrations and intentional error cases when changing
the front end:

```bash
make demo
bin/sagan --tokens examples/tokenizer_error.sagan
make parser-demo
bash scripts/parser_demo.sh
bash scripts/ast_demo.sh --no-open
bash scripts/semantic_demo.sh
bash scripts/type_demo.sh
bash scripts/entry_demo.sh
bash scripts/execution_demo.sh
```

Validate documentation metadata, links, navigation, Markdown, and rendering:

```bash
bash scripts/docs.sh check
```

The parser suite establishes syntactic correctness for the current grammar. The
semantic suite covers scopes, names, types, generics, classes, faces, modules,
units, ownership rules, and control flow. Coverage measures which implementation
lines the tests execute; it does not by itself prove that every language rule is
correct. Execution demos additionally prove that representative generated C++
compiles and runs natively.
