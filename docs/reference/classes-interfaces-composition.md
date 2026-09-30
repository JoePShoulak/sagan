---
title: Classes, interfaces, and composition
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Classes, interfaces, and composition
Interfaces, spelled `face`, are Sagan's primary composition mechanism.
Interfaces may compose interfaces; classes declare conformance with `is` or
`has`; multiple interfaces are comma-separated; and `self` refers to the
current object.

This composition-first direction is **settled design**. Deep inheritance
hierarchies are not the intended reuse mechanism. A leading dot is intended to
mark a private member.

**Implemented in the parser:** `face` and `class` are top-level declarations.
Both accept `is` or `has` followed by comma-separated interface names. Faces
contain signature-only methods or block-bodied defaults. Classes contain `let`
fields and block-bodied methods. `self` is an expression, and a leading dot on
a class method is retained as private-member syntax in the AST.

**Implemented visibility subset:** a leading dot makes a class method private
to its declaring class. Methods of that class may call it through `self` or
another instance of the same class. Outside access is rejected, and a private
method cannot satisfy a face requirement. Fields do not yet have private syntax.

**Implemented default-method subset:** an unambiguous default is composed into
the class and may call other requirements from the same face through `self`.
An exact-signature class method overrides the default. If multiple directly
composed faces provide the same default signature, the class must provide an
explicit override. This is static composition; it does not create a superclass.

Faces may compose other faces using the same `is` or `has` spelling. Required
signatures and defaults flow transitively to the final class. A face declaration
may replace an inherited default with its own exact-signature declaration or
default. Cyclic face composition is invalid.

**Open questions:** object construction, storage layout, dynamic dispatch,
value/reference behavior, and whether limited
implementation inheritance will exist after 1.0; no implementation-inheritance
feature is part of the 1.0 hypercore contract.
