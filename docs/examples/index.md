---
title: Examples
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Examples

The root `examples/` directory is intentionally small. It contains programs
worth reading, changing, and running by hand. Compiler regression inputs live
under `tests/fixtures/` instead, where their purpose is explicit and the test
suite owns them.

## Language showcase

`examples/showcase.sagan` is the broad end-to-end tour. It demonstrates Unicode
identifiers, collections, control flow, classes and faces, enums, exceptions,
dimensioned values, physical units, and native execution.

```bash
make run-demo
```

The command prints the source and result, then also runs the sample package.

## Geometry

`examples/geometry.sagan` demonstrates the distinction between affine points
and displacement vectors, plus native Cartesian and spherical literals.

```bash
make geometry-demo
```

## Units of measure

`examples/units.sagan` demonstrates conversions, affine temperatures,
unit-constrained callables, custom units, SI prefixes, and angular units.

```bash
make units-demo
```

## Modules and packages

`examples/package/` is a complete manifest-backed project. It uses qualified
modules in nested source directories and demonstrates package discovery,
imports, linked C++ generation, and native execution.

```bash
make package-demo
```

## Visual AST

`examples/ast.sagan` is a syntax-rich input for Sagan's tree renderers.

```bash
bash scripts/ast_demo.sh
```

This writes `build/ast-demo.html` and opens an interactive source-and-tree view
with zoom and pan controls. Add `--no-open` to generate it without opening a
browser.

## Executable documentation examples

Small examples embedded directly in these docs live under
`docs/examples/executable/`. The documentation check runs each `.sagan` file
and compares its output with the adjacent `.stdout` file.

```sagan
--8<-- "docs/examples/executable/hello.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/hello.stdout"
```

The compact [Fibonacci example](executable/fibonacci.sagan) demonstrates
parallel `let`, simultaneous reassignment, postfix increment, and a brace-free
one-statement loop. Run it with `sagan docs/examples/executable/fibonacci.sagan`;
its expected output is `144`.

## Regression fixtures

`tests/fixtures/` is grouped by compiler responsibility:

- `syntax/` contains tokenizer, parser, and recovery inputs.
- `semantic/` contains name, type, ownership, generic, and entry-point inputs.
- `modules/` contains multi-file resolver graphs and failure cases.
- `runtime/` contains code-generation and runtime behavior inputs.

The scripts in `tests/integration/` turn those fixtures into assertions. Run
them as part of the complete suite with `make test`, or separately with:

```bash
make integration-test
```

Fixtures are test data, not user-facing examples. A fixture may be deliberately
invalid and should not be treated as recommended Sagan style.
