# Space Game Design Document

## Concept

Space Game is a space-based resource-management and exploration game. It begins
with a doomed Earth and a sizable human presence in space sometime in the near
future. Humanity must move its production capabilities off-planet and prepare
to live away from Earth until a new home can be found.

The player will create outposts, extract and refine resources, establish
additional outposts, and build a fleet capable of temporarily housing humanity
while searching for a new home among the stars.

## Entities

### Natural Bodies

Natural bodies include Sol, the planets, their major moons, the dwarf planets,
some comets, and many asteroids. An outpost built on a natural body is called a
**base**. Natural bodies will have realistic resource distributions so that
building multiple bases on the same planet can serve a purpose.

### Vehicles

Vehicles are central to the game and include several subtypes.

#### Ships

Ships may be manned or unmanned and have the following systems:

- Chassis
- Navigation
- Communication
- Sensors
- Propulsion
  - Engines
  - Stability Augmentation System (SAS)
  - Reaction Control System (RCS)
- Life support, if manned
- Storage
  - Fuel
  - Life-support supplies, if manned
  - Ammunition, if armed

The chassis is the core of a ship and determines what the ship can become. It
has slots for required systems and may provide additional slots for
customization.

A chassis may have armor slots that allow armor to be installed. It may also
have weapon slots that allow weapons to be installed. Other non-mandatory
chassis slots are called **machine slots** and hold machines. Machines are the
category for miscellaneous systems, including systems used for research,
resource extraction, and resource refinement.

#### Trucks

**Truck** is a catchall term for any vehicle intended to operate on the surface
of a body rather than in space. Like ships, trucks have chassis and slots that
allow their implementation and capabilities to be customized.

#### Satellites

Although *satellite* technically means anything orbiting another body, this
document uses the term for artificial, human-made machines placed in space to
perform a task. While they are not vehicles in the traditional sense,
satellites share many systems with ships. Consequently, they have chassis and
slots like ships do.

### Outposts

Outposts are the counterpart to vehicles and have a major role in the game.
They may be built on a surface, where they are called **bases**, or freestanding
in space, where they are called **stations**. Like vehicles, outposts have
chassis composed of slots that allow a base or station to be customized.

### Drones

Drones are the final type of entity and the only entity type that is not
first-class. Planetary bodies have no agent because their behavior is the result
of predetermined calculations. Unmanned vehicles may require simple agents.
Outposts do not themselves require agents, but drones support vehicles and
outposts by handling tasks that need the same level of agency as an unmanned
ship without being independent, self-existing entities.

For example, a station may have a machine that provides repair drones capable
of servicing visiting ships. These repair drones need some agency to perform
their tasks, but they are not considered ships because their activities are
closer to animations than independent actions.

This distinction is clearer when compared with a ship running a resource route
and transporting materials between planets. That ship should not be assigned
the same level of complexity or implied identity as a repair drone, even though
the drone may share many systems with ships.

## Narrative

*To be defined.*

## Gameplay

### Overview

*To be defined.*

### Mechanics

#### Resources

*To be defined.*

##### Extraction

*To be defined.*

##### Refinement

*To be defined.*

##### Transportation

*To be defined.*

##### Consumption

*To be defined.*

#### Orbital Decay

*To be defined.*

#### Ships

*To be defined.*

##### Creation

*To be defined.*

##### Customization

*To be defined.*

#### Mechanical Systems

*To be defined.*

##### Degradation

*To be defined.*

##### Repair

*To be defined.*

#### Life Support

*To be defined.*

##### Food

*To be defined.*

##### Water

*To be defined.*

##### Oxygen

*To be defined.*

##### Sleep

*To be defined.*

##### Comfort

*To be defined.*

#### Population

*To be defined.*

##### Management

*To be defined.*

##### Housing

*To be defined.*

##### Life Support

*To be defined.*

##### Transportation

*To be defined.*

#### Terraforming

*To be defined.*

#### Exploration

*To be defined.*

#### Research

*To be defined.*

##### Technology Tree

*To be defined.*

#### Combat (Unresolved)

*To be defined.*

##### Enemy Aliens (Unresolved)

*To be defined.*

##### Enemy Factions (Unresolved)

*To be defined.*

#### Fleet Management

*To be defined.*

## Physics

The game aims for a high level of physical realism, including support for
Lagrange points. The exact simulation model remains to be defined.

## Roadmap

*To be defined.*

## Known Questions

- What exists outside the Solar System?
- How is orbit stabilization handled?
