---
title: Declarations
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Declarations
Declarations introduce names. `let` declares mutable variables; `fun` introduces
functions; `face`, `class`, and `enum` introduce named types; and
`dimension`, `quantity`, `unit`, and `affine unit` introduce compile-time
measurement metadata. Measurement declarations are checked for duplicate names,
inconsistent dimensions, unresolved references, and dependency cycles. See the
[native units contract](../design/units-of-measure.md).
`module`, `import`, and `export` participate in modular source.

The parser accepts a single optional leading `module`
declaration, imports with optional `from` and `as` clauses, and standalone
exports with an optional alias. See [Modules](modules.md) for the accepted
forms.

`///` and `/** ... */` documentation comments attach to the declaration that
immediately follows them. The AST preserves each comment separately with its
source span and text. Supported targets are modules, imports, exports, `let`,
`fun`, `face`, `class`, and `enum` declarations, including fields, methods, and
local variables and individual enum members.

```sagan
let altitude: Float = 125_000.0
```

Top-level declarations may define mutable variables, functions, faces, classes,
and enums. Functions may use a block body or a short expression body:

```sagan
fun double(value: Int): Int => value * 2
```

Faces accept required method signatures and methods with default bodies.
Classes accept fields, constructors, and methods. Enum members may be plain
names or may carry typed payloads. A plain enum value is written with its type,
such as `Direction.north`; payload cases such as `Success(42)` act as
constructors.

Class fields require explicit types and may
have default initializers. A class may declare overloaded `new(...)` constructors;
calling `ClassName(arguments...)` selects exactly one compatible constructor.
Every field without a declaration-site default must be assigned on every
constructor path. With no declared constructor, `ClassName()` is available only
when every field has a default. `new` has no source-level return type and cannot
use `return`, while
`init` remains an ordinary identifier for possible separate lifecycle APIs.
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

`is` and `has` are interchangeable and do
not denote inheritance. A class composing a face must satisfy each required
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
`is` or `has`, including transitive face composition. Calls through the face
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
faces, as in `class Box<T> is Readable<T>`, and conversions preserve invariant
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
