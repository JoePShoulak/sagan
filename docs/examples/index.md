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

```sagan
--8<-- "examples/showcase.sagan"
```

```bash
make run-demo
```

The command prints the source and result, then also runs the sample package.

## Geometry

`examples/geometry.sagan` demonstrates the distinction between affine points
and displacement vectors, plus native Cartesian and spherical literals.

```sagan
--8<-- "examples/geometry.sagan"
```

```bash
make geometry-demo
```

## Units of measure

`examples/units.sagan` demonstrates conversions, affine temperatures,
unit-constrained callables, custom units, SI prefixes, and angular units.

```sagan
--8<-- "examples/units.sagan"
```

```bash
make units-demo
```

## Minimal orbital math

`examples/orbit_math.sagan` demonstrates M0's automatically available square
root, vector length, squared length, dot product, normalization, and explicit
physical-to-display coordinate conversion while preserving native units.

```sagan
--8<-- "examples/orbit_math.sagan"
```

```bash
make orbit-math-demo
```

## Modules and packages

`examples/package/` is a complete manifest-backed project. It uses qualified
modules in nested source directories and demonstrates package discovery,
imports, linked C++ generation, and native execution.

=== "Manifest"

    ```toml
    --8<-- "examples/package/sagan.toml"
    ```

=== "Entry module"

    ```sagan
    --8<-- "examples/package/src/main.sagan"
    ```

=== "Guidance module"

    ```sagan
    --8<-- "examples/package/src/navigation/guidance.sagan"
    ```

=== "Telemetry module"

    ```sagan
    --8<-- "examples/package/src/telemetry/flight.sagan"
    ```

```bash
make package-demo
```

## Visual AST

`examples/ast.sagan` is a syntax-rich input for Sagan's tree renderers.

```sagan
--8<-- "examples/ast.sagan"
```

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

```sagan
--8<-- "docs/examples/executable/fibonacci.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/fibonacci.stdout"
```

The classes-and-faces learning path uses four complete programs. Read them in
this order:

1. [Class state and methods](executable/class_fuel_tank.sagan) demonstrates
   construction, private storage, observation, and mutation without a face.
2. [One face contract](executable/face_contract.sagan) demonstrates explicit
   class conformance and a function that accepts the face instead of the class.
3. [Composed faces](executable/face_composition.sagan) demonstrates transitive
   requirements and a default method that dispatches to the class.
4. [Shared face identity](executable/composition.sagan) demonstrates conversion
   to a face value and mutation observed through that shared view.

The [Classes and faces tour](../tour/classes-and-interfaces.md) walks through
the programs and explains how each class and face interacts. Every adjacent
`.stdout` file is checked with the rest of the executable documentation.

### Class state and methods

```sagan
--8<-- "docs/examples/executable/class_fuel_tank.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/class_fuel_tank.stdout"
```

### One face contract

```sagan
--8<-- "docs/examples/executable/face_contract.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/face_contract.stdout"
```

### Composed faces

```sagan
--8<-- "docs/examples/executable/face_composition.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/face_composition.stdout"
```

### Shared face identity

```sagan
--8<-- "docs/examples/executable/composition.sagan"
```

Expected output:

```text
--8<-- "docs/examples/executable/composition.stdout"
```

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
