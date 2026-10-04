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

As much as possible, for every feature in the game that requires a consumable
resource, that resource will be present in the game and accurate to its real
life counterpart. For example, if you want rocket fuel you better be prepared
to procure some methane or hydrogren and oxygen.

##### Extraction

Resources can be extracted from Natrual Bodies by Bases, and some Vehicles, by
having the requisite machine installed (and storage available).

##### Refinement

Likewise, resources can be refined by having the corresponding machine
installed and the proper materials present (and storage for the inputs and
outputs, of course).

##### Transportation

Resources can be transported along automated routes by Vehicles. Automated
Routes are calculated and simulated, but things like the final rendezvous and
docking are "hand-waved" so that the math does not need to be perfect for a
planned mission to go as planned. NOTE: This has to be carefully guranteed.

##### Consumption

Systems and Machines may consume Resources.

#### Orbital Decay

If something is put into orbit, it will have enough gravitational influence
affecting it to cause it to eventually have a deteriorating orbit, such that
orbital corrections would need to be performed by all Ships, Satellites, and
Stations. This can be mitigated somewhat by placing them in Lagrange points.

#### Ships

*To be defined.*

##### Creation

Outposts will be able to crate ships.

##### Customization

The core of each ship is its Chassis, which has Slots that allow you to install
systems and machines that determine the capabilites and other details of the
ship. Hopefully paint schemes can be a thing too.

#### Mechanical Systems

No system is perfect

##### Degradation

Machines will degrade over time and need repair. Your satellite will not last
forever.

##### Repair

Anything can be repaired. Unlike resources, this will not be as real-life
accurate; we'll do some major handwaving allowing things to be able to be fixed
with a limited list of "Parts" (advanced parts, electronic parts, chassis parts
grade 3, what have you) such that the ability to repair things is not the most
crunchy part of the game.

#### Life Support

Folks can die. Like the math and science in other parts of the game, this is
going to be accurate to the age and activity of the people in the game. Folks
will need to have the appropriate provided resources, with things like oxygen
consumption being directly calculated.

##### Food

Gotta feed them.

##### Water

*To be defined.*

##### Oxygen

*To be defined.*

##### Sleep

Not sure exactly how to tie this in, this may just fall under the heading of
comfot. If nothing else, it helps determine the available number of man-hours
you have per day. Some number of people have to be asleep at a certain time.

##### Comfort

I don't want to go tooooo crazy on this at the get go, but I do want it to be a
bad thing if you cut the corners on absolutely everything for everyone. For
example, maybe providing more food and water than they need and enabling them to
eat more than the bare minnimum provides a boost. Maybe having amenities raises
morale and gives people a temporary buff to their job performance.

#### Population

All the people not employed in the running of the space program. The goal of the
game is to eventually get as many as you can to a new home among the stars. At
every step of the journey though, not all of the population will be willing to
make the move, nor may there be transportation available for everyone. Tough
choices will need to be made, and some will be made as fait accompli. You can't
save everyone, but you must save humanity.

##### Management

*To be defined.*

##### Housing

They need to have a safe place to live until we can find all of humanity a new
home among the stars

##### Education

Some effor has to be put in by the player to guarantee that in further
generations, humanity is still able to train people to be able to perform the
tasks needed by the space program. In actuality, this may just be a way to
"promote" people from civilians into employees, but considering these pilots
can't live forever, there will need to be new humans taking over.

*To be defined.*

##### Life Support

Same as on a ship, the health of the population must be satisfied

*To be defined.*

##### Transportation

Transport ships must be built to move them from the dying earth to whever their new refuge will be

*To be defined.*

#### Terraforming

Earth is doomed (why is TBD), but other planets may have things performed to
affect their environment to ease the pressure on needing to find their new home
quite as urgently. NOTE: We likely need to ensure that earth can never be
restored to close to its previous status in order to ensure the teraforming of
next best options (Like Mars), and likewise no body can ever be terraformed to
the point that it is perfectly habitable, otherwise

*To be defined.*

#### Exploration

The player will need to explore other bodies and star systems to be able to find
a new home among the stars.

*To be defined.*

#### Research

Science points must be able to be gathered to allow a tech tree and tech
progression needed for there to be such a capability disparity as being
basically not far from where we are right now in real life to eventually
becoming a truly space faring civilzation.

*To be defined.*

##### Technology Tree

There will be one

*To be defined.*

#### Combat (Unresolved)

It would be so disappointing to have no combat in this game.

##### Enemy Factions (Unresolved)

In the solar system, there should be some sort of enemy faction to get the
player familiar with combat, or else it may be the only combat in the game and
would persist throughout. This has to be balanced against the fact that Earth
would have likely united a fair amount behind the shared goal of surviging
eradication.

*To be defined.*

##### Enemy Aliens (Unresolved)

In other star systems, the player may find hostile entities, which may have
been tutorialized by enemy factions in the solar system. This allows a robust
and varied combat experience moving forward.

#### Fleet Management

Vehicle and Outpost Management is the absolute core of the game and the vast
majority of the game loop.

*To be defined.*

## Physics

The game aims for a high level of physical realism, including support for
Lagrange points. The exact simulation model remains to be defined.

## Roadmap

*To be defined.*

## Known Questions

- What exists outside the Solar System?
- How is orbit stabilization handled?
