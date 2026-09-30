<p align="center">
  <img src="docs/assets/images/sagan-logo.png" alt="Sagan logo: a slice of pie filled with a spiral galaxy" width="280">
</p>

# Sagan

[![Development version 0.71.7](https://img.shields.io/badge/development-0.71.7-2563eb)](docs/contributing/versioning.md)
[![Documentation](https://github.com/JoePShoulak/sagan/actions/workflows/documentation.yml/badge.svg)](https://github.com/JoePShoulak/sagan/actions/workflows/documentation.yml)
[![Codecov](https://codecov.io/gh/JoePShoulak/sagan/graph/badge.svg)](https://codecov.io/gh/JoePShoulak/sagan)

Sagan is a strongly typed programming language for simulations—the kind with
orbits, moving bodies, physical units, geometry, and eventually a whole lot of
things flying around on screen.

The goal is to make scientific code feel direct without making it vague. A
point should not quietly become a direction. Meters should not accidentally be
added to seconds. Failure should be catchable. An emoji should be a perfectly
respectable function name if that makes the program more fun.

> [!WARNING]
> Sagan is a young language approaching its first release. It is a real,
> executable compiler project, but it is not yet a safe choice for production
> software.

## A taste of Sagan

```sagan
fun travel(
  distance: Float64<meter>,
  duration: Float64<second>
): Float64<meter / second> => distance / duration

fun 🚀(
  velocity: Vector3<Float64, meter / second>,
  duration: Float64<second>
): Vector3<Float64, meter> =>
  velocity * duration

fun main(): Int {
  let distance: Float64<meter> = 1 kilometer
  let speed = travel(distance, 2 second)

  let origin = (0.0, 0.0, 0.0) meter
  let velocity = <10.0, 2.0, 0.0> (meter / second)
  let destination = origin + 🚀(velocity, 5 second)

  print("Speed: ${speed}")
  print("Destination: ${destination}")
  return 0
}
```

Sagan understands the difference between the point `origin` and the vector
`velocity`. It checks the units passed into both functions, converts compatible
units, and erases the unit metadata when it generates native code.

## What makes it Sagan?

- **Units belong in the type system.** Scalars, vectors, points, parameters,
  fields, generics, and interfaces can require real scientific units. The
  built-in catalog includes SI units and prefixes, affine temperatures,
  astronomical units, and common laboratory and engineering units.
- **Points are not vectors.** Points represent locations; vectors represent
  directions or displacement. Translating a point is valid. Adding two points
  is not.
- **Cartesian and spherical values both look native.** Write `<x, y, z>` and
  `(x, y, z)` for Cartesian vectors and points, or `s<magnitude, inclination,
  azimuth>` and `s(radius, inclination, azimuth)` for spherical forms.
- **Composition comes before inheritance.** Interfaces are called `face`s.
  Classes compose small faces, inherit useful default behavior, and resolve
  conflicts explicitly instead of building deep class hierarchies.
- **Errors are values you can catch.** `hope`, `unless`, `finally`, and
  `scream` provide exception handling, including checked arithmetic and
  collection failures through `RuntimeError`.
- **Absence is explicit.** `Optional<T>`, `Some`, `None`, safe access `?.`, and
  lazy fallback `??` handle values that may not exist. There is no general
  `null` hiding in every type.
- **Unicode is ordinary source code.** Identifiers may use international text,
  mathematical symbols, and emoji. Sagan normalizes names safely while keeping
  accurate source locations.
- **The exact-value core is deterministic.** Checked integers, source-ordered
  control flow, collections, matching, and module resolution have predictable
  language-level results. Floating-point and future library guarantees are
  documented separately rather than overpromised.

## What works today?

Sagan can tokenize, parse, resolve, type-check, generate C++, compile, and run
programs. The implemented language includes:

- functions, lambdas, overloads, generics, and checked conversions;
- arrays, dictionaries, vectors, points, optionals, and payload enums;
- classes, constructors, private members, faces, defaults, and dynamic
  dispatch;
- reference-counted objects, weak fields, and escaping closures;
- matching, loops, one-line control-flow bodies, and exceptions;
- modules, exports, imports, packages, and `sagan.toml`; and
- native units, checked arithmetic, Unicode names, and string interpolation.

The compiler can also display tokens, semantic and type models, and syntax
trees as text, DOT, SVG, or a zoomable interactive HTML page.

The concise [project status](docs/design/status.md) tracks what is implemented
and what is deliberately deferred. The [language reference](docs/reference/index.md)
is the authority for exact behavior.

## Where are we going?

Sagan is meant to grow into a coherent simulation environment, not merely a
collection of unrelated language features.

1. **Math** will be automatically available and will define the shared
   numerical and geometric vocabulary.
2. **Rendering** will give simulations a visible, debuggable world.
3. **Physics** will follow real simulation needs and use the same math, units,
   geometry, and rendering conventions.

Rendering and physics will be tightly integrated first-party libraries, but
they will require explicit imports so a small command-line program does not
carry the entire simulation stack. Cartesian/spherical conversion and reference
frames will be designed carefully with those real library needs in view.

Editor tooling is following the same principle: the compiler already exposes
reusable document, diagnostic, recovery, workspace, and semantic-index APIs so
future VS Code and other editor integrations do not reimplement the language.

## Try it

Windows x64 is the first supported platform. From a development checkout using
Git Bash and an MSYS2 UCRT64 toolchain:

```bash
bash scripts/test.sh
make run-demo
make geometry-demo
make units-demo
bash scripts/ast_demo.sh
```

Run a source file directly:

```bash
bin/sagan examples/showcase.sagan
```

Then explore the friendly path through the documentation:

- [Install Sagan](docs/getting-started/installation.md)
- [Write your first program](docs/getting-started/first-program.md)
- [Take the language tour](docs/tour/index.md)
- [Browse the examples](docs/examples/index.md)
- [Look up an exact language rule](docs/reference/index.md)
- [Explore the compiler and its visual tools](docs/tooling/index.md)

The documentation site is available at
**[sagan.shoulak.org](https://sagan.shoulak.org/)**. Its pages remain visibly
work-in-progress until they receive the project's human audit.

## Project status and releases

Sagan is preparing its `1.0.0` release. Automated compiler, coverage,
packaging, checksum, SBOM, and vulnerability gates protect that release. The
detailed lifecycle and remaining follow-up work live in the project
documentation rather than taking over this introduction.

See the [release lifecycle](docs/contributing/release-lifecycle.md),
[versioning policy](docs/contributing/versioning.md), and
[contributor guide](docs/contributing/index.md) for the detailed machinery.

## Origins and license

Sagan began with compiler infrastructure from Zachary Westerman's
[Schematic](https://github.com/ZacharyWesterman/schematic). Sagan preserves the
applicable attribution and GNU GPLv3 obligations of that work.

See [project history](docs/about/project-history.md) and
[license and attribution](docs/about/license-and-attribution.md) for the full
record.
