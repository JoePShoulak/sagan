# Sagan

**Simulation Architecture for Geometry, Astrodynamics, and Numerics**

Sagan is an experimental, strongly typed programming language for scientific and real-time simulation. It is designed to make geometry, astrodynamics, physics, rendering, and multi-entity systems natural to express while retaining predictable behavior and practical performance.

Sagan is currently in the language-design stage. The syntax shown here expresses the intended direction, but the language is not yet ready for general use.

## Design philosophy

### Simulation first

Vectors, matrices, quaternions, coordinates, scientific notation, and large numerical workloads are central concerns rather than afterthoughts. Sagan is intended for orbital and cosmic simulations, rocket flight, interacting entities, scientific computing, rendering, and potentially game-engine development.

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
- Method chaining and planned element-wise collection operations
- Exceptions with Sagan-specific vocabulary
- String interpolation, raw strings, and multiline strings
- Modules and imports
- No arbitrary executable expressions at the top level
- Initial C++ source backend targeting Windows, Linux, and macOS

Physical units and coordinate frames are not encoded into the initial type system. Libraries and user-defined types may represent units, and vectors of the same dimension are initially compatible regardless of their conceptual frame.

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
- Identifiers use Unicode `XID_Start` and `XID_Continue`, with `_` additionally permitted.
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

## Project status

The lexical design is sufficiently defined to replace the inherited token vocabulary and implement the tokenizer. Parser, semantic-analysis, runtime, and standard-library decisions remain, including:

- complete type inference and declaration rules;
- reference-count cycle handling and value/reference semantics;
- interface defaults and conflict resolution;
- generics and possible sum types;
- constructor, enum, and collection semantics;
- exception matching and propagation;
- entry-point forms;
- deterministic numeric and runtime requirements; and
- the boundary between language features and the standard library.

See [ROADMAP.md](ROADMAP.md) for the planned implementation stages.

## Origins and attribution

Sagan is based on Zachary Westerman's [Schematic](https://github.com/ZacharyWesterman/schematic), a work-in-progress compiler for a node-based language. Schematic supplied the starting tokenizer, lexer, parser utilities, diagnostic infrastructure, AST foundation, and build structure.

The current repository still contains Schematic-derived parser support code and an inherited lexer that has not yet been replaced with the Sagan token set described above. Schematic is published under the GNU General Public License v3. Sagan must preserve the applicable license and attribution requirements before redistribution.
