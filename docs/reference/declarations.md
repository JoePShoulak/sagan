---
title: Declarations
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Declarations
Declarations introduce names. `let` declares mutable variables; `const`
declares immutable bindings; `fun` introduces
functions; `test` introduces an explicitly named test body; `face`, `class`,
and `enum` introduce named types; and
`dimension`, `quantity`, `unit`, and `affine unit` introduce compile-time
measurement metadata. Measurement declarations are checked for duplicate names,
inconsistent dimensions, unresolved references, and dependency cycles. See the
[native units contract](../design/units-of-measure.md).
`module`, `import`, and `export` participate in modular source.

An explicit `test "name" { ... }` declaration is now recognized and
type-checked. Its quoted name may contain Unicode but cannot be empty,
interpolated, or duplicated in the same file. Slash-separated names, such as
`"orbits/elliptical"`, create suite paths; leading, trailing, or doubled
slashes are invalid. `assert(condition[, message])` requires a Bool and an
optional String. A false assertion fails a test; outside a test it ends the
program with status 1 and prints the failure message. Test bodies are not run
by ordinary `sagan file.sagan` execution. Compiler-owned document and
resolved-project discovery and selected test execution are available through
the language-service and LSP APIs. The VS Code extension presents them through
Test Explorer, including multi-module project selections, captured output,
structured outcomes, and cancellation. There is not yet a public `sagan test`
terminal command or a debug-test profile. See
[Testing Sagan programs](../tooling/testing-sagan-programs.md).

The parser accepts a single optional leading `module`
declaration, imports with optional `from` and `as` clauses, and standalone
exports with an optional alias. See [Modules](modules.md) for the accepted
forms.

`///` and `/** ... */` documentation comments attach to the declaration that
immediately follows them. The AST preserves each comment separately with its
source span and text. Supported targets are modules, imports, exports, `let`, `const`,
`fun`, `face`, `class`, and `enum` declarations, including fields, methods, and
local variables and individual enum members.

```sagan
let altitude: Float = 125_000.0
const MAX_RETRIES: Int = 5
```

To introduce several mutable variables at once, use a parallel `let` with one
initializer per name:

```sagan
let i, a, b = 0, 0, 1
```

All right-hand expressions are evaluated from left to right before any of the
new names enters scope. An unannotated integer `let` variable uses `Int64`,
including each name in this group; annotate a name to request a narrower type.
Parallel `let` works at the executable root or inside a function; class fields
cannot use the grouped form. Reassignment of existing names uses
`a, b = b, a + b` instead.

`const` always requires an initializer and an ASCII `SCREAMING_SNAKE_CASE`
name (`[A-Z][A-Z0-9_]*`). The keyword creates the immutable binding;
capitalization alone never does. Consequently `let MAX_RETRIES = 5` is an
error, not a constant. Constants may infer a type or use an annotation and may
be local, module-level, exported, or imported. Reassignment, increment,
compound assignment, indexed or field mutation rooted at the constant, and a
mutating `!` method call through it are rejected. A mutable alias to the same
referenced object can still mutate that object: this is a const view, not a
transitive deep freeze or general compile-time expression evaluator.
The compiler exposes the same naming check for rename tooling; a safe local
rename cannot give a constant a lowercase name or a mutable variable the
reserved constant spelling. Cross-file and exported-symbol rename remain
unavailable.

Top-level declarations may define mutable variables, functions, faces, classes,
and enums. Functions may use a block body or a short expression body:

```sagan
fun double(value: Int): Int => value * 2
```

Trailing parameters may provide defaults in functions, methods, constructors,
and lambdas. A caller may omit those arguments, and the defaults are evaluated
for that call:

```sagan
fun greet(name: String = "friend"): String => name
let double = fun(value: Int, factor: Int = 2): Int => value * factor
print(greet())
print(double(21))
```

Required parameters must come first. A default requires an explicit parameter
type and currently must be a self-contained value (literals and expressions
made from them); it cannot capture another parameter, a local variable, or
`self`. Calls through a separately declared function type do not inherit
defaults from the original function or lambda.

Faces accept required method signatures and methods with default bodies.
Classes accept fields, constructors, and methods. Enum members may be plain
names or may carry typed payloads. A plain enum value is written with its type,
such as `Direction.north`; payload cases such as `Success(42)` act as
constructors.

Class fields require explicit types and may
have default initializers. A class may declare overloaded `new(...)` constructors;
`const` fields require an explicit type and a declaration-site initializer.
Constructor-only initialization of const fields is deferred; a const field
cannot be assigned again, even in `new`.
Calling `ClassName(arguments...)` selects exactly one compatible constructor.
Every field without a declaration-site default must be assigned on every
constructor path. With no declared constructor, `ClassName()` is available only
when every field has a default. `new` has no source-level return type and cannot
use `return`, while
`init` remains an ordinary identifier for possible separate lifecycle APIs.
For inherited classes, `new(...) is Parent(args), OtherParent(args)` forwards
arguments to direct parent constructors in the same order as the class's `is`
list. Unlisted parents must be default-constructible. Parent initializer
arguments cannot refer to `self` before construction is complete.
`self` resolves to the current instance. Fields can be read or mutated,
and block- or expression-bodied methods execute. A trailing `!` is allowed only
on methods and conventionally identifies a mutating alternative; it does not by
itself change dispatch or mutation rules. A leading dot makes a method private
to its declaring class; it remains callable from other methods of that class
but cannot be accessed externally or used to satisfy a face requirement.
A leading dot on a field, as in `let .value: Int`, likewise restricts access to
constructors and methods of the declaring class.

`weak let target: Probe` declares a non-owning class field. Weak fields require
an explicit class or face type, begin empty, and cannot declare an initializer.
Assigning a normal strong value stores a weak reference; reading the field
produces `Optional<Probe>`, so callers use matching, `?.`, or `??` to handle an
expired target. Weak fields may also be private with `weak let .target: Probe`.

Reference counting is the 1.0 ownership model; there is no tracing cycle
collector. The type checker rejects declaration-level cycles made entirely of
strong class fields, including class references nested in optionals or generic
type annotations. At least one edge must use `weak let`. A face-typed field must also
be weak because its concrete class target is selected dynamically and cannot be
proven acyclic. The diagnostic reports the strong field path that forms a cycle.

On classes, `is` introduces one or more parent classes and `has` introduces
one or more adopted faces. The `is` clause comes first, followed by `, has`
when both are present. A class composing a face must satisfy each required
method with an exact class implementation or unambiguous default. Missing or
incompatible methods are compile-time errors.

An unambiguous face default is composed into the class, an exact
class method overrides it, and competing defaults with the same signature
require an explicit class override. Defaults may call other requirements from
their own face through `self`.
Face composition is transitive: inherited requirements and defaults flow into
the composing face and ultimately into its classes. Cycles are rejected.
Face names may be used as value, parameter, and return annotations. A class
value converts to a face only when its declaration explicitly conforms through
`has`, including transitive face composition. Calls through the face
dispatch to the concrete class while retaining shared reference identity.

Enum cases may carry one or more typed payload values, such as
`Success(Int)` or `Position(Float, Float)`. Payload cases act as constructors in
the surrounding declaration namespace. Match patterns use the case name and
bind one name per payload value; covering every case makes the match exhaustive.
Payload-constructor names must currently be unique within that namespace.

Every enum case has a unique signed 64-bit numeric tag. The first implicit tag
is zero, and each later implicit tag is one greater than the preceding case.
An explicit assignment such as `Success(Int) = 200` sets that case's tag and
resets the sequence used by following implicit cases. Numeric separators are
accepted. Duplicate tags, values outside the signed 64-bit range, and an
implicit increment beyond that range are compile-time errors. These tags are
stable representation metadata; enum equality and matching remain nominal, and
enum values do not implicitly participate in integer arithmetic.

Enums may declare type parameters: `enum Result<T, E>`. An expected type such
as `let result: Result<Int, String> = Failure("problem")` supplies generic
arguments that the selected case cannot infer. Explicit qualification such as
`let result = Result<Int, String>.Failure("problem")` supplies those arguments
without an expected type. Both forms are type-checked and execute natively.

Top-level functions may declare type parameters after their name:
`fun identity<T>(value: T): T => value`. Calls infer each type argument from
the corresponding argument type, including type parameters nested inside a
generic annotation. Every declared type parameter must be inferable from the
call unless it is supplied explicitly, as in `identity<String>("signal")`.
A parameter may require structural face conformance with
`fun preserve<T is Readable<Int8>>(value: T): T => value`; inferred and explicit
arguments are both checked against the specialized face.

Classes and faces may also declare type parameters. Constructor calls infer a
generic class specialization from constructor arguments, so `Box(42)` produces
the corresponding `Box<Int8>` under normal literal inference. An expected
annotation may supply otherwise uninferable parameters. Fields and methods
substitute the specialization consistently. Composition accepts specialized
faces, as in `class Box<T> has Readable<T>`, and conversions preserve invariant
type arguments. A generic face may provide ordinary default methods that use
its type parameters and dispatch through `self`. Class methods may introduce
their own inferred parameters, such as `fun echo<U>(value: U): U`, and callers
may instead write `box.echo<String>("signal")`. Constructors likewise accept
explicit specialization with `Box<Int8>(42)`. Generic class parameters may use
the same `is Face` constraint syntax and are checked when constructed. A face
method cannot introduce method-specific parameters because virtual generic
methods are intentionally unsupported.

Duplicate declarations, unresolved names, incompatible overloads, invalid
visibility access, and module export violations are compile-time errors. Generic
types are invariant. Borrowing, variance, and virtual methods with their own
method-level generic parameters are not part of the current language.
