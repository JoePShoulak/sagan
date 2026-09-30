---
title: Built-in unit catalog
status: work-in-progress
publication_ready: false
verified_in: null
verified_on: null
verified_by: null
---

# Built-in unit catalog

Sagan ships a compiler-owned scientific unit catalog. Unit names below are
source spellings; symbols are documentation metadata and are not alternate
literal spellings unless explicitly listed as aliases.

The catalog follows the BIPM SI Brochure, 9th edition version 4.01, for SI base,
derived, prefix, and listed non-SI definitions. Selected customary units use
exact international definitions. Sagan gives plane angle its own static
dimension—even though SI treats radians as the unit one—because silently mixing
angles with arbitrary ratios is unsafe for simulation code.

## Dimensions and quantities

Built-in dimensions are `Length`, `Time`, `Mass`, `ElectricCurrent`,
`Temperature`, `AmountOfSubstance`, `LuminousIntensity`, and `Angle`.

Named quantities include:

| Domain | Quantities |
| --- | --- |
| Geometry and mechanics | `Area`, `Volume`, `Angle`, `SolidAngle`, `Frequency`, `Speed`, `Acceleration`, `Wavenumber`, `Density`, `Momentum`, `Force`, `Pressure`, `Energy`, `Torque`, `Power`, `DynamicViscosity`, `KinematicViscosity` |
| Electromagnetism | `ElectricCharge`, `Voltage`, `Capacitance`, `Resistance`, `Conductance`, `MagneticFlux`, `MagneticFluxDensity`, `Inductance` |
| Light, radiation, chemistry | `LuminousFlux`, `Illuminance`, `Radioactivity`, `AbsorbedDose`, `EquivalentDose`, `CatalyticActivity` |

`Energy` and `Torque`, `Frequency` and `Radioactivity`, and `AbsorbedDose` and
`EquivalentDose` deliberately remain different named quantities even when their
dimension exponents are identical.

## SI units

| Family | Source spellings |
| --- | --- |
| Base and angle | `meter`, `second`, `gram`, `kilogram`, `ampere`, `kelvin`, `mole`, `candela`, `radian`, `steradian` |
| Named derived | `hertz`, `newton`, `pascal`, `joule`, `newtonMeterTorque`, `watt`, `coulomb`, `volt`, `farad`, `ohm`, `siemens`, `weber`, `tesla`, `henry`, `lumen`, `lux`, `becquerel`, `gray`, `sievert`, `katal` |

The distinct spelling `newtonMeterTorque` preserves torque identity instead of
silently treating it as energy.

Every current SI decimal prefix is recognized on prefixable SI units:
`quecto`, `ronto`, `yocto`, `zepto`, `atto`, `femto`, `pico`, `nano`, `micro`,
`milli`, `centi`, `deci`, `deca`, `hecto`, `kilo`, `mega`, `giga`, `tera`,
`peta`, `exa`, `zetta`, `yotta`, `ronna`, and `quetta`. Direct built-in names
win before prefix splitting, so `kilogram` retains its standard meaning.

## Common scientific and engineering units

| Family | Source spellings |
| --- | --- |
| Time and angle | `minute`, `hour`, `day`, `degree`, `arcminute`, `arcsecond` |
| Area, volume, mass | `liter`, `tonne`, `hectare`, `barn` |
| Distance and astronomy | `angstrom`, `astronomicalUnit`, `lightYear`, `parsec`, `nauticalMile` |
| Speed and acceleration | `knot`, `gal` |
| Pressure | `bar`, `standardAtmosphere`, `torr` |
| Energy | `electronvolt`, `calorie`, `kilowattHour` |
| CGS laboratory units | `dyne`, `erg`, `gauss`, `poise`, `stokes` |
| International customary | `inch`, `foot`, `yard`, `mile`, `poundMass`, `poundForce` |
| Affine temperature | `Kelvin`, `Celsius`, `Fahrenheit`; differences use `Delta<...>` or `Δ<...>` |

Dynamic units, logarithmic units such as decibels, measurement uncertainty,
calendar-dependent durations, fractional dimension exponents, and external unit
catalog packages are intentionally deferred.

## Examples

```sagan
fun impulse(force: Float64<newton>, time: Float64<second>): Float64<newton * second> =>
  force * time

let orbit: Float64<meter> = 1 astronomicalUnit
let bearing: Float64<radian> = 90 degree
let chamber: Float64<pascal> = 1 standardAtmosphere
let warmer = 20 Celsius + 5 Δ<Celsius>
```

Run the executable catalog and conversion demonstration with:

```bash
make units-demo
```

Sources: [BIPM SI Brochure](https://www.bipm.org/en/publications/si-brochure)
and [BIPM SI Brochure 9th edition, version 4.01](https://www.bipm.org/documents/20126/41483022/SI-Brochure-9.pdf/fcf090b2-04e6-88cc-1149-c3e029ad8232).
