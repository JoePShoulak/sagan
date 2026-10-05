---
title: Expressions
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Expressions
An expression produces a value. Literals such as `42`, names such as `answer`,
calls such as `calculate()`, and arithmetic such as `distance / time` are all
expressions.

Compound assignments are parsed as statements rather than expressions. Unlike
the value-producing `:=`, they do not participate in expression precedence or
chaining.

`^` means exponentiation; `and`, `or`, `not`, and
`!` are logical operators; `? ... ; ...` is the conditional expression; and
prefix/postfix increment return new/old values respectively.

`++` and `--` require an assignable numeric operand.
The prefix form mutates before producing its value; the postfix form produces
the old value and then mutates.

`^` is mathematical exponentiation, and `^=` assigns its result; neither is a
bitwise operation. Integer exponentiation requires a non-negative exponent and
raises a runtime error when the result overflows its checked type. Floating-
point bases permit negative exponents. A floating-point base with an integer
exponent keeps the base's floating-point type (`Float32 ^ Int` produces
`Float32`, and `Float64 ^ Int` produces `Float64`), without converting the
integer exponent to a float first. An integer base with a floating-point
exponent produces the floating-point type. This makes both `5 ^ 0.5` and
`PHI ^ n` valid when `PHI` is floating-point and `n` is an integer. `0 ^ 0`
is a runtime error for every numeric type; a floating-point zero raised to a
negative integer exponent raises `RuntimeError.division_by_zero`.

Exponentiation does not implicitly turn its floating-point result into an
integer return value. Use `Int.round(value)` for an explicit Float-to-Int
conversion. It rounds to the nearest integer, with exact halfway values away
from zero (`Int.round(2.5)` is `3`; `Int.round(-2.5)` is `-3`). It accepts a
`Float32` or `Float64` and returns `Int64`. NaN, infinity, and values whose
rounded result is outside `Int64` raise `RuntimeError.invalid_conversion`.

For example, Binet's Fibonacci formula can keep its integer return type:

```sagan
fun fibonacci_binet(n: Int): Int {
  const PHI = (1 + 5 ^ 0.5) / 2
  return Int.round((PHI ^ n - (-PHI) ^ (-n)) / 5 ^ 0.5)
}
```

Floating-point rounding can lose exactness for sufficiently large Fibonacci
indices; use an integer algorithm when exact results matter.

The native executable subset checks signed integer addition, subtraction,
multiplication, division, remainder, unary negation, increment, and decrement.
Overflow raises a runtime error. Division and remainder by zero raise runtime
errors for integer and floating-point operands. Compound assignments use the
same checks. Floating-point remainder follows `fmod` semantics. These failures
are catchable as the appropriate built-in `RuntimeError` case.

Anonymous lambdas use
`fun(parameters) => expression`, with optional parameter and return annotations.
Function types use `(Parameter, ...) => Result`; `() => Int` is therefore a
parameterless function returning `Int`. Typed lambdas can be stored, passed,
returned, called immediately, or called through variables and parameters.
Their arguments and results use the same lossless-conversion rules as named
functions.
When a lambda has trailing default parameters, a direct call or call through
an inferred binding may omit them. The compiler supplies those defaults at
each call site before invoking the stored function. A separately annotated
plain function type does not carry a default-argument promise.

Inside a class method, `super.Parent.method(arguments)` calls the named direct
parent implementation without dispatching back to an override. It is a method
call, not a general-purpose parent object or field-access expression.

Closures capture surrounding local variables and parameters by shared
reference. Captured storage remains alive while any closure refers to it, so an
escaping closure cannot retain a dangling stack reference. Copies of one
closure share its captured state, and multiple closures created in the same
scope observe mutations to the same captured variable. Untyped lambda
parameters remain parser/type-model input but are not native-emittable.

Closures that capture a method's contextual `self` are rejected rather than
being emitted with an unsafe object lifetime. Capturing `self` remains
deferred. Spread is supported in arrays and dictionaries; it is not a
general-purpose operator in other expressions.

Numeric and geometry literals accept unit suffixes. A simple suffix is a unit
name (`10 meter`); a composite suffix is parenthesized
(`<1.0, 0.0, 0.0> (meter / second)`). `value as unit` explicitly converts a
compatible value. Assignment, argument, and return contexts convert to their
declared concrete unit, while incompatible dimensions, named quantities, or
affine categories are type errors. `Delta<Celsius>` and `Δ<Celsius>` both name
an affine difference.

## Optional values and fallback

`Optional<T>` is the first implemented parameterized built-in type. `Some(value)`
constructs a present optional and `None` constructs an absent value when an
expected optional type supplies `T`. The right-associative `??` operator unwraps
its left operand when present and otherwise evaluates its right operand lazily:

`None` is Sagan's sole absence value. There is no general `null` value: ordinary
types remain guaranteed-present, and absence must be expressed as `Optional<T>`.

```sagan
let configured: Optional<Int> = None
let result = configured ?? 42
let chained = configured ?? cached ?? calculate_default()
```

`Optional<T> ?? T` produces `T`; `Optional<T> ?? Optional<T>` produces
`Optional<T>`. Safe access propagates absence through fields and method calls,
and may be chained before a fallback:

```sagan
let controller_name = possible_ship?.controller?.name ?? "No controller"
let result = possible_probe?.calculate() ?? 0
```

`match` can distinguish absence and bind a present payload. The binding exists
only inside its case:

```sagan
match possible_result {
  case Some(result) print(result)
  case None print("No result")
}
```

Payload-bearing enums use the same construction and binding shape:
`Success(42)` constructs a case and `case Success(value)` binds its typed
payload. Weak class fields use the optional model when read, including through
safe access and coalescing chains.

Dictionary entries are applied from
left to right. When an explicit entry or later spread repeats an existing key,
the later value replaces the earlier value. Each spread operand is evaluated
once.

## String expressions

Ordinary single- and double-quoted strings, raw strings, and triple-double-quoted
multiline strings are parsed as expressions. An interpolated string retains an
ordered sequence of decoded text and embedded expression nodes:

```sagan
let status = "mission ${mission.name}: altitude ${ship.position.altitude}"
let path = r"C:\simulation\${literal_text}"
```

Interpolation accepts a complete Sagan expression and therefore follows the
same precedence rules as an expression outside a string. `${}` is a syntax
error because every interpolation requires an expression. Raw strings never
interpolate; `${literal_text}` in the raw example is ordinary text.

## Collection expressions

The parser implements four collection forms:

```sagan
let values = [1, 2, ...additional_values,]
let metadata = {"name": "Voyager", active_key: true, ...defaults,}
let direction = <1.0, 0.0, 0.0,>
let position = (100.0, 200.0, 300.0,)
let radial_direction = s<2.0, 0.5, 1.0>
let radial_position = s(100.0, 0.5, 1.0)
```

Arrays and dictionaries may be empty. Dictionary keys accept any expression;
semantic analysis will determine whether a key's type is hashable.
An integer's `.times` property eagerly constructs an `Array<Int>` containing
the zero-based indices below that integer: `5.times` is `[0, 1, 2, 3, 4]`,
and `0.times` is `[]`. A negative count raises `RuntimeError.invalid_range`.
The count is evaluated once; this is an actual array, not a lazy sequence.

Cartesian vectors and points require at least two elements, so `<>`, `<1>`, and `(1,)`
are syntax errors. `(value)` remains a grouped expression. Spherical vectors
and points require exactly three components in the order shown. The `s` must be
adjacent to the delimiter: `s(...)` is a spherical point, while `s (...)` calls
an ordinary identifier named `s`. Inclination and azimuth are radians.

Cartesian and spherical vectors and points
retain distinct, fixed-size runtime types. All support construction, zero-based
indexing, iteration, equality, printing, and string interpolation. Cartesian
vectors and points additionally support same-family spreading. Spherical
literals reject spreads so their three-component representation stays explicit.
Invalid array, vector, or point indices raise
`RuntimeError.index_out_of_bounds`; absent dictionary keys raise
`RuntimeError.missing_key`.
Vectors of equal dimension support `+`, `-`, unary `+`/`-`, equality, scalar
`*` in either operand order, and vector/scalar `/`; the matching compound forms
are also supported. `.x`, `.y`, `.z`, and `.w` read or mutate components when
the value's dimension contains that component; requesting a component outside
the dimension is a compile-time error. Component arithmetic retains the scalar
overflow and zero-divisor checks. Cartesian points are affine locations: `point +
vector` and `point - vector` translate a point and produce a point;
`point - point` produces the displacement vector. Their dimensions
must agree, and component types widen only when the scalar conversion is
lossless. `point + point`, `vector + point`, point scaling,
and point negation are invalid. Vector-vector multiplication and ordered
comparison are undefined rather than implicitly meaning dot, cross, component
multiplication, or lexicographic ordering.

There is no implicit or context-free explicit conversion between points and
vectors: converting a point into a displacement requires an origin, and
converting a displacement into a point requires applying it to an origin.
Cartesian/spherical conversion is also explicit and is reserved for the first
core math geometry API after 1.0; it is not implemented in the hypercore.
Points and vectors use separate fixed-size wrappers with the same component
storage cost. Coordinate frames are not yet tracked, so values of the same
family, dimension, and compatible component type may interact regardless of
their conceptual frame. Future generic math APIs must state whether they accept
points, displacement vectors, or both rather than erasing this distinction.
Spherical values currently support access and storage but no arithmetic; callers
must not silently mix representations.

Calls and all collection forms permit trailing commas. `...value` spreads an
array into an array or a dictionary into a dictionary. A spread dictionary
entry does not use a colon. Unsupported spread contexts are compile-time
errors.

At the top level of a vector element, `>` closes the vector. Parentheses make a
greater-than comparison explicit inside a vector, as in
`<(left > right), true>`. Outside a vector-opening expression position, `<` and
`>` retain their comparison meanings.

## Implemented precedence

From highest to lowest:

1. calls, indexing, member access, safe member access, and postfix `++` and `--`;
2. right-associative `^`;
3. prefix `++`, `--`, `!`, `not`, `...`, unary `+`, and unary `-`;
4. `*`, `/`, `%`;
5. `+`, `-`;
6. `<`, `<=`, `>`, `>=`, `is`, `has`;
7. `==`, `!=`;
8. `and`;
9. `or`;
10. right-associative `? ... ; ...`;
11. right-associative `:=`.

Exponentiation binds more tightly than unary minus. Chained comparisons are a
syntax error and must be written as separate comparisons joined with `and`.

`is` and `has` in expressions inspect declared type relationships. For
example, `GunShip is Ship`, `GunShip has Weapons`, and
`Assault has Weapons` return `Bool`. Both operands must be declared class or
face names, with class/class operands for `is` and a face on the right of
`has`. These are not runtime object-instance tests.
Generic type-relationship expressions require a future specialization syntax;
the current form accepts non-generic declared names only.

Postfix forms chain from left to right. For example,
`fleet[index]?.navigator.course(origin).magnitude()` first indexes `fleet`,
safely accesses `navigator`, accesses and calls `course`, then accesses and
calls `magnitude`.
`and`, `or`, `??`, and the conditional expression evaluate lazily where their
result permits skipping a branch. Assignment targets, overloads, and implicit
conversions are checked before code generation.
