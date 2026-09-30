<p align="center">
  <img src="docs/assets/images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="280">
</p>

# Sagan

[![Development version 0.76.0](https://img.shields.io/badge/development-0.76.0-2563eb)](docs/contributing/versioning.md)
[![Documentation](https://github.com/JoePShoulak/sagan/actions/workflows/documentation.yml/badge.svg)](https://github.com/JoePShoulak/sagan/actions/workflows/documentation.yml)
[![Codecov](https://codecov.io/gh/JoePShoulak/sagan/graph/badge.svg)](https://codecov.io/gh/JoePShoulak/sagan)

Sagan is a strongly typed programming language for simulations: orbits,
geometry, physical units, moving bodies, and—eventually—whole worlds you can
see and interact with.

It aims to make scientific code feel natural without making it ambiguous. A
meter cannot quietly become a second. A point is not a direction. A missing
value must say that it might be missing. An emoji, however, is a perfectly good
function name.

> [!WARNING]
> Sagan is approaching its first release. It can compile and run real programs,
> but it is not ready for production software yet.

## Hello, universe

```sagan
fun 🚀(name: String): String => "Hello, ${name}!"

fun main(): Int {
  print(🚀("universe"))
  return 0
}
```

Save that as `hello.sagan`, then run:

```bash
sagan hello.sagan
```

## Why Sagan feels different

### Units are part of the type

Sagan checks physical dimensions before your program runs. Compatible units
can convert; incompatible units are an error.

```sagan
fun speed(
  distance: Float64<meter>,
  time: Float64<second>
): Float64<meter / second> => distance / time

let launch_speed = speed(1 kilometer, 20 second)
```

### Geometry says what it means

Points describe places. Vectors describe directions or displacement. Sagan
keeps them distinct, supports Cartesian and spherical notation, and rejects
geometry that does not make mathematical sense.

```sagan
let launch_pad = (0.0, 0.0, 0.0) meter
let movement = <10.0, 2.0, 0.0> meter
let spacecraft = launch_pad + movement

let line_of_sight = s<100.0, 0.5, 1.2> meter
```

### Constants say so explicitly

`const` makes a binding read-only; uppercase spelling alone never does.

```sagan
const STANDARD_GRAVITY: Float64<meter / second^2> = 9.80665 (meter / second^2)
```

### Composition comes first

Sagan calls interfaces `face`s. Classes build behavior from small faces and
their defaults instead of growing deep inheritance trees.

```sagan
face Named {
  fun name(): String
  fun greeting(): String => "Hello, ${self.name()}"
}
```

### Absence and failure are explicit

`Optional<T>`, `Some`, `None`, safe access `?.`, and fallback `??` handle
missing values. `hope`, `unless`, `finally`, and `scream` handle failures.
Sagan also supports checked arithmetic, generics, payload enums, collections,
pattern matching, reference-counted classes, modules, and packages.

## Install Sagan

Windows x64 is the first supported platform. Download the available installers
and portable archives from the **[Sagan download mirror](https://sagan.shoulak.org/downloads/)**.
GitHub Releases remains the canonical release source.

The installer includes the compiler toolchain, adds `sagan` to `PATH`, and can
associate `.sagan` files with Sagan. After installation, open a new terminal:

```bash
sagan --version
sagan hello.sagan
```

See the [installation guide](docs/getting-started/installation.md) for portable
archives, file associations, windowed programs, upgrades, and building from
source. Windows ARM64, Linux, and macOS remain future targets.

## Get started

1. [Install Sagan](docs/getting-started/installation.md).
2. [Write and run your first program](docs/getting-started/first-program.md).
3. [Take the language tour](docs/tour/index.md).
4. [Try the tested examples](docs/examples/index.md).
5. Keep the [language reference](docs/reference/index.md) nearby for exact
   rules.

Working from a source checkout instead? Git Bash and an MSYS2 UCRT64 toolchain
are currently required:

```bash
bash scripts/test.sh
bin/sagan docs/examples/executable/hello.sagan
```

Curious about what the compiler sees? It can display tokens, symbols, inferred
types, generated C++, and syntax trees—including SVG and zoomable HTML trees.
Start with the [compiler tooling guide](docs/tooling/compiler.md).

## Where Sagan is going

Sagan's planned simulation stack has three closely related layers:

- **Math** is built in and automatically available.
- **Rendering** will make simulations visible and debuggable.
- **Physics** will grow from real simulation needs and share the same math,
  geometry, units, and rendering conventions.

Rendering and physics will be first-party libraries, but they will require
explicit imports so lightweight command-line programs stay lightweight.

For the honest line between implemented, planned, and deliberately deferred
work, see [project status](docs/design/status.md). The full documentation is
also hosted at **[sagan.shoulak.org](https://sagan.shoulak.org/)** and remains
visibly work-in-progress until its human audit is complete.

## Origins and license

Sagan began with compiler infrastructure from Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic). Sagan preserves the
applicable attribution and GNU GPLv3 obligations of that work.

Read the [project history](docs/about/project-history.md) and
[license and attribution](docs/about/license-and-attribution.md) for the full
record.
