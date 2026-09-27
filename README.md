# Sagan
## Simulation Architecture for Geometry, Astrodynamics, and Numnerics

# Features
- Vectors
- Matrices
- Quaternions
- Strong typing
  - Minimal to no type coercion
- Composition over inheritence
- Multiple coordinate system 
- Low-level / core physics and rendering 
- Inherent built in math 
- Large number / scientidfic notation
- Javascript style .() notation as well as . chaining and list dot chaining specifically
- No top level expressions
- Lambdas are functions

# Purpose
- Cosmic physics simulation
- Rocket simulation (ascent, descent, landing, space flight, atmospheric flight)
- Multi-entity tracking and interaction
- 3D Rendering
- Math and science
- (Game engine)
- Typed enums
- Typed iterfaces

# Syntax
- Arrays: `[]`
- Vectors: `<>`
- Coordinates: `()`
- Mutability flag for functions: `!`
- Function dec: `fun`
- Class dec: `class`
- Dictionary: `{}`
- Exponents: `^`
- Declare: `=`
- Set and get result: `:=` (maybe)
- Compare: `==`
- Inline ternary: `bool ? if_true ; if_false`
- And and Or: `&` and `|`
- Inline function body (lambda, syntax option only): `=>`
- Constants: `VARNAME`
- Comments: `//`
- Loops:
  - For
  ```
  for it_name in list_name { ... }
  for it_name in 5.times { ... }
  ```
  - While `while bool_name { ... }`
  - Until `until bool_name { ... }`


- Functions:
```
fun function_name(p1, p2, p3) {
  // do stuff
  return x, y, z
}
```

```
let function_name = fun (p1, p2, p3) =>  p1 > p2
```
```
let function_name = fun (p1, p2, p3) =>  {
  // do stuff
  return p1 > p2
}
```
- Classes:
```
class Vector {
  // . means private

  fun public_name(p1, p2) { ... }
  fun .private_name(p1, p2_ { ... })
} 

Vector v = Vector(p1, p2)
v.public_name()
v.private_name() // error when called publicly

```
- Interfaces: 
```
face interface_name {
  fun method1_name()
  fun method2_name(p1)
  fun method3_name(p1, p2, p3)
}
```

# Notes
```
class ExplorerShip {
  String name = "Voyager"
  Engines engines = engineList[5]
  Scopes scopes = scopeList[13]
  FuelTanks fueltanks = tankList[8]
  Weapons weapons = weaponList[3]
}

ExplorerShip ship1 = ExplorerShip("Voyager")
```
Spread operator
Unpacking dicts
variable size parameters and returns