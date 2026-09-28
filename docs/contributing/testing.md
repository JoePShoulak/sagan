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
compiled-in tokenizer self-tests, and checks `--version`.

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
