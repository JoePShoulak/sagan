---
title: Testing Sagan programs
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Testing Sagan programs

Sagan has a compiler-owned test model and a VS Code Test Explorer client.
Tests are explicit named declarations, not functions discovered by naming
convention:

```sagan
test "guidance/holds commanded heading" {
  let commanded = 42
  let measured = 42
  assert(commanded == measured, "guidance departed from the commanded heading")
}
```

The compiler parses and type-checks test bodies with the same language rules as
ordinary code. A test passes when its body completes. A false
`assert(condition)` or `assert(condition, message)` fails it. An unhandled
runtime error is reported as an errored test rather than a passed assertion.

Test names must be non-empty, non-interpolated string literals and must be
unique within a file. Slash-separated names such as
`"guidance/holds commanded heading"` form nested suite paths. A name cannot
start or end with `/` or contain an empty component such as `guidance//heading`.

## Run tests in VS Code

Install the matching compiler, language server, and extension, then open the
source directory or package root as a VS Code workspace.

1. Open the **Testing** view from the Activity Bar.
2. Refresh **Sagan Tests** if the workspace was already open when tools were
   installed.
3. Run a file, suite, individual test, or a multi-file selection with the normal
   Test Explorer controls.
4. Read assertion messages, captured output, duration, and error details in the
   test result.
5. Cancel from VS Code when a run should stop.

The extension refreshes an open Sagan document after open and save events. For
a workspace, it asks the compiler for project discovery. When project execution
is advertised, selections spanning multiple modules are sent as one checked
project run; older compatible servers fall back to document runs.

Test identity, package membership, module order, source ranges, selection, and
execution are determined by the compiler. The JavaScript extension does not
scan declarations or decide which project files belong together.

## Document and project behavior

- Document discovery and execution operate on one Sagan document.
- Project discovery resolves the package/module graph and can find tests across
  linked modules.
- Unsaved editor overlays take precedence over disk when the project snapshot
  is assembled.
- Selected test IDs are validated against the checked snapshot.
- Project tests run independently in resolved-module order.
- A stale project result publishes no test cases after a newer edit supersedes
  its snapshot.
- Output and structured diagnostics remain associated with the test result.

Ordinary execution does **not** run test declarations:

```bash
sagan path/to/program.sagan
```

That command runs the program's executable root and ignores test bodies. There
is not yet a public `sagan test` terminal command. Outside a test body,
`assert(false, "message")` instead prints the failure and terminates the program
with status 1.

## Current limits

- Test Explorer provides a **Run** profile, not a Sagan debug-test profile.
- The experimental debug adapter is not exposed by the released extension.
- Test execution does not currently produce per-test coverage information.
- Terminal-only users do not yet have a supported public test-runner command;
  the implemented execution transport is the versioned language-service/LSP
  contract used by the extension.

These limits must remain visible until their compiler, protocol, client, and
release gates all pass.

## Maintainer verification

The compiler and protocol test contracts are covered by the repository suite:

```bash
bash scripts/test.sh
```

The VS Code client has separate unit, bundle, and live Extension Host gates:

```bash
cd editors/vscode-sagan
npm ci
npm test
npm run test:bundle
npm run test:integration
```

The integration command requires compatible `bin/sagan` and `bin/sagan-lsp`
binaries, or explicit `SAGAN_COMPILER_PATH` and `SAGAN_LSP_PATH` values. See
[contributor testing](../contributing/testing.md) for the repository-wide test
layout and coverage command.
