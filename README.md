# Sagan

**Simulation Architecture for Geometry, Astrodynamics, and Numerics**

Sagan is an experimental, strongly typed programming language for scientific and real-time simulation. It is designed to make geometry, astrodynamics, physics, rendering, and multi-entity systems natural to express while retaining predictable behavior and practical performance.

Sagan is currently in the language-design stage. The syntax shown here expresses the intended direction, but the language is not yet ready for general use.

## Design philosophy

### Simulation first

Vectors, matrices, quaternions, coordinates, scientific notation, and large numerical workloads are central concerns rather than afterthoughts. Sagan is intended for orbital and cosmic simulations, rocket flight, interacting entities, scientific computing, rendering, and potentially game-engine development.

Sagan's core-library design reflects that purpose. Mathematics is built in and
automatically available to every program. Physics and rendering are tightly
coupled, first-party core libraries designed alongside the language and its math
types, but they must be imported explicitly so lightweight programs do not pay
for systems they do not use.

### Strong and explicit types

Sagan minimizes implicit coercion. A conversion is allowed only when it is lossless, including integer-to-floating-point conversion only when the compiler can prove that no information is lost. Arithmetic overflow raises a runtime error.

### Interface-based composition

Interfaces—not deep inheritance hierarchies—are the primary tools for abstraction, reuse, and polymorphism. Classes implement and combine small interfaces, and interfaces can compose other interfaces. Whether Sagan will permit any limited implementation inheritance remains undecided.

### Visible mutation

Variables are mutable by default. A method may use a Ruby-style trailing `!` to identify a mutating counterpart:

```sagan
let normalized = vector.normalize()
vector.normalize!()
```

The `!` is part of the method name and is a convention rather than an independently enforced effect system.

### Safety before unchecked speed

Sagan favors safe behavior by default and may provide explicit unsafe escape hatches where low-level control is necessary. Memory is intended to use reference counting. Parallel execution is deferred; when introduced, two threads will not be allowed to mutate the same data concurrently.

### Predictable simulation

Identical inputs are intended to produce identical results across supported platforms. Because Sagan initially transpiles to C++, this guarantee will require controlled numeric semantics, generated code, compiler options, libraries, and scheduling rather than relying on the backend alone.

## Language at a glance

- Strong static typing with limited, lossless implicit conversion
- Mutable variables and explicit mutating-method naming
- Functions, lambdas, overloading, and multiple return values
- Classes with interface-based composition
- Typed interfaces and enums
- Arrays, dictionaries, vectors, matrices, quaternions, and coordinates
- Automatically available core mathematics
- Explicit, tightly integrated core physics and rendering libraries
- Method chaining and planned element-wise collection operations
- Exceptions with Sagan-specific vocabulary
- String interpolation, raw strings, and multiline strings
- Modules and imports
- No arbitrary executable expressions at the top level
- Initial C++ source backend targeting Windows, Linux, and macOS

Physical units and coordinate frames are not encoded into the initial type system. Libraries and user-defined types may represent units, and vectors of the same dimension are initially compatible regardless of their conceptual frame.

## Core-library model

Three libraries define Sagan's intended core ecosystem and its levels of
coupling:

1. **Math** is built into Sagan and automatically available. Its scalar,
   vector, matrix, quaternion, coordinate, and related numerical facilities form
   the common vocabulary of the language and the other core libraries.
2. **Physics** is a first-party core library with direct interoperability with
   Sagan's mathematical types. It is not automatically included and must be
   imported by programs that need simulation facilities.
3. **Rendering** is likewise a first-party, tightly integrated core library
   built around the same math and simulation vocabulary. It is also an explicit
   import rather than part of every executable.

This arrangement makes physics and rendering feel native when used together
without forcing their compile-time, binary-size, initialization, or conceptual
costs on lightweight numerical or general-purpose programs. Exact module names,
package layout, and import granularity will be settled with the module system
and standard-library implementation.

## Key syntax

The examples below are a design sketch, not yet a formal grammar.

### Variables and assignment

`let` declares a variable. Variables are mutable, and declaration, initialization, and reassignment all use `=`.

```sagan
let altitude: Float = 125_000.0
altitude = altitude + 500.0
```

`:=` assigns a value and evaluates to the value assigned:

```sagan
let current = altitude := calculate_altitude()
```

`UPPER_SNAKE_CASE` identifies constants by convention; it is not a distinct token class or compiler-enforced rule.

### Numbers

Sagan supports decimal integers and floating-point numbers, scientific notation, and `_` digit separators:

```sagan
let entities = 10_000
let gravity = 6.674_30e-11
let distance = 1E+9
```

A decimal point requires digits on both sides. `.5`, `5.`, `.5e2`, and `5.e2` are invalid. Binary, octal, hexadecimal, numeric suffixes, and unit suffixes are not part of the initial language. `inf` and `nan` are reserved floating-point values.

### Collections and mathematical values

```sagan
let values = [1, 2, 3]
let metadata = {"name": "Voyager", "active": true}
let direction = <1.0, 0.0, 0.0>
let position = (100.0, 200.0, 300.0)
```

- `[]` creates and indexes arrays.
- `{key: value}` creates dictionaries; context distinguishes them from code blocks.
- `<>` creates vectors; `<` and `>` also perform comparisons.
- `(x, y, ...)` creates coordinates with at least two elements.
- `(value)` is grouping, and Sagan does not have tuple literals.
- Generic type arguments use parentheses, such as `Array(Float)`, although generic semantics remain under design.

Spread uses `...value`, and safe member access uses `?.`.

### Functions and lambdas

```sagan
fun distance(a: Vector, b: Vector): Float {
  return (a - b).magnitude()
}
```

Expression-bodied functions and lambdas use `=>`:

```sagan
let greater = fun(a: Float, b: Float) => a > b
```

Functions may be overloaded by parameter types. Multiple returns, destructuring, variadic parameters, and yielding are planned but still need complete parser and semantic rules.

### Classes and interfaces

A class declares interface conformance with either `is` or `has`. The two words are interchangeable in a conformance declaration, and multiple interfaces are comma-separated.

```sagan
face Renderable {
  fun render()
}

face Spacecraft is Renderable, Movable {
  fun trajectory(): Vector
}

class ExplorerShip has Spacecraft, Trackable {
  fun render() {
    // ...
  }

  fun .calculate_internal_state(): Vector {
    // A leading dot marks a private member.
  }
}
```

`self` refers to the current object. `is` also performs type or interface-conformance tests:

```sagan
if object is Renderable {
  object.render()
}
```

Interface default methods, conflict resolution, structural versus explicit conformance, and value-versus-reference type behavior remain under design.

### Control flow

```sagan
if condition {
  // ...
} else {
  // ...
}

for item in items {
  // ...
}

while condition {
  // ...
}

until condition {
  // ...
}
```

Sagan also reserves `match`, `case`, `break`, `continue`, `return`, and `yield`.

The conditional expression uses `?` and `;`:

```sagan
let status = active ? "running" ; "stopped"
```

`;` does not terminate ordinary statements. Newlines terminate statements, except inside expression delimiters or after a token that leaves an expression incomplete.

### Increment and compound assignment

Both prefix and postfix increment and decrement are supported:

```sagan
let new_value = ++count
let old_value = count++
```

Prefix returns the new value; postfix returns the old value. Compound assignment includes:

```text
++  --  +=  -=  *=  /=  %=  ^=
```

`^` is exponentiation. Sagan does not initially provide bitwise operators.

### Logical operators

```sagan
if ready and not failed {
  // ...
}

if !failed or retrying {
  // ...
}
```

`not` and `!` are equivalent prefix-negation operators. `and` and `or` are the logical conjunction and disjunction operators.

### Exceptions

Sagan uses its own exception vocabulary:

- `hope` begins protected code;
- `unless` introduces an exception handler;
- `finally` introduces unconditional cleanup; and
- `scream` raises an exception.

The detailed grammar for exception matching and propagation remains to be defined.

### Strings

Both single and double quotes create strings; Sagan does not have a separate character-literal type.

```sagan
let name = "Voyager"
let message = "speed: ${distance / time}"
let path = r"C:\simulation\data"
```

Every non-raw string supports `${expression}` interpolation with a complete Sagan expression. Raw strings use `r"..."` or `r'...'` and process neither escapes nor interpolation.

Multiline strings use triple double quotes and preserve their contents exactly:

```sagan
let description = """
First line
  Indentation is retained.
"""

let raw_data = r"""
\n and ${value} remain literal.
"""
```

The initial escapes are `\\`, `\"`, `\'`, `\n`, `\r`, `\t`, `\0`, and `\u{...}`.

### Modules

Sagan reserves `module`, `export`, `import`, `from`, and `as`. Exact module and package semantics remain under design.

## Source and lexical conventions

- Source files are UTF-8.
- Identifiers are case-sensitive and normalized to Unicode NFC.
- Identifiers use Unicode `XID_Start` and `XID_Continue`, with `_` and Unicode emoji
  sequences additionally permitted. An identifier may begin with an emoji, so names such
  as `🚀`, `🌌distance`, and `calculate🪐` are valid for variables and functions.
- ASCII punctuation does not become part of an identifier merely because it resembles an
  emoticon; for example, `:)` remains punctuation and is not a valid name.
- Names beginning with `__` are reserved for the compiler.
- Keywords are lowercase and receive dedicated token types.
- `LF` and `CRLF` each represent one logical newline.
- `//` begins a line comment.
- Nested `/* ... */` comments are supported.
- `/// ...` and `/** ... */` produce documentation-comment tokens.
- Ordinary comments and non-significant whitespace are discarded initially.
- Unknown characters and malformed or unterminated literals produce focused tokenizer errors.
- When tokens share a prefix, the longest valid token wins.

Schematic's `@tag`, special `!event`, and backtick code-block tokens are not part of Sagan. `!event` is ordinary logical negation of an identifier, while standalone `@` and backticks are invalid.

## Execution model

The initial compiler will translate Sagan into C++, then use a C++ compiler to produce native code. The first platform targets are Windows, Linux, and macOS.

Reference counting is the intended memory-management model. Exceptions are the primary error mechanism. C and C++ interoperability may be added later but is not required for the first compiler.

## Building the tokenizer

The current bootstrap executable reads Sagan source and prints the resulting token stream. From Bash with an MSYS2 UCRT64 toolchain available:

```bash
bash scripts/test.sh
make demo
```

`bash scripts/test.sh` builds the executable and runs the tokenizer self-tests. `make demo` tokenizes `examples/tokenizer_demo.sagan`. To inspect another source file:

```bash
bin/sagan path/to/program.sagan
```

`examples/tokenizer_error.sagan` intentionally contains a malformed number and demonstrates focused lexical-error reporting.

## Development versions

Sagan identifies development builds with a Git-derived version such as
`0.1.3+g1a2b3c4d`:

- `0.1` is the manually selected major and minor language-development line;
- `3` is the number of commits since that line's base commit;
- `g1a2b3c4d` identifies the exact Git revision; and
- `.dirty` is appended when the build includes uncommitted or untracked changes.

This system is adapted from Schematic's dynamic in-app version numbering. Sagan
regenerates the version during every build so it cannot remain stale after a new
commit. Inspect the version without building or run the compiled executable with:

```bash
make get-version
bin/sagan --version
```

The generated version is a development-build identity, not yet a promise of
stable-language compatibility. When a major or minor version changes, its base
commit is updated and the patch counter begins again at zero. Tagged releases
and source-language compatibility policy will be defined before Sagan's first
public release.

This is a bootstrap implementation. It accepts non-ASCII UTF-8 identifier bytes, including emoji, but full Unicode `XID_Start`/`XID_Continue` and emoji-sequence validation plus NFC normalization still require a Unicode library. Context-sensitive newline suppression inside vector and dictionary literals will be finalized with the parser because `<...>` and `{...}` also represent comparisons and code blocks.

## Project status

The bootstrap tokenizer now recognizes Sagan's keywords, operators, punctuation, identifiers, numbers, comments, documentation comments, strings, raw and multiline strings, and nested string interpolation. It includes a token-dump CLI, self-tests, a representative demo, and focused lexical errors. Parser, semantic-analysis, runtime, and standard-library decisions remain, including:

- complete type inference and declaration rules;
- reference-count cycle handling and value/reference semantics;
- interface defaults and conflict resolution;
- generics and possible sum types;
- constructor, enum, and collection semantics;
- exception matching and propagation;
- entry-point forms;
- deterministic numeric and runtime requirements; and
- the exact API boundary between built-in math and the explicitly imported
  physics and rendering core libraries.

## Roadmap

### 1. Language definition — sufficiently defined

Define what Sagan is for and establish its design philosophy, core features,
and lexical rules. The language is sufficiently defined to support tokenizer
work, while parser and semantic decisions will continue to be refined when
their implementation makes the tradeoffs concrete.

### 2. Tokenizer — bootstrap complete

Define Sagan's token IDs, keywords, operators, punctuation, identifiers, and
literals. Convert UTF-8 source text into tokens with source spans, report
focused lexical errors, and provide token-dump demonstrations and automated
checks.

The initial tokenizer is working. Remaining hardening includes complete Unicode
XID and emoji-sequence validation, NFC normalization, malformed UTF-8 rejection,
parser-informed newline handling for ambiguous delimiters, and broader
regression and fuzz testing.

### 3. Parser and syntax tree — next

Turn the token stream into a structured syntax tree representing declarations,
expressions, statements, control flow, types, classes, interfaces, and
composition. This stage establishes the concrete grammar, operator precedence,
and syntax-error diagnostics.

### 4. Semantic analysis — planned

Walk the syntax tree and determine whether syntactically valid programs are
meaningful. This includes name resolution, scope checking, duplicate-name
detection, type inference and checking, interface conformance, mutation rules,
and other semantic diagnostics.

### 5. Code generation — planned

Translate the validated program into C++ and invoke a C++ compiler to produce a
native executable. Later work will define optimization, runtime integration,
debug information, platform support, and the boundary between generated code
and Sagan's standard library.

## Origins and attribution

Sagan is based on Zachary Westerman's [Schematic](https://github.com/ZacharyWesterman/schematic), a work-in-progress compiler for a node-based language. Schematic supplied the starting tokenizer, lexer, parser utilities, diagnostic infrastructure, AST foundation, and build structure.

The repository retains Schematic-derived tokenizer state, span, diagnostic, and parser-support foundations, while the inherited node-language token table and lexer have now been replaced by Sagan's initial tokenizer. Schematic is published under the GNU General Public License v3. Sagan must preserve the applicable license and attribution requirements before redistribution.
