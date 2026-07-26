# Five Games for a 1.44 MB Floppy

## The constraint

Despite the repository name, `144mb` is not a 144 MB game. It is a collection
of standalone games that must each fit on one 1.44 MB floppy: exactly
**1,474,560 bytes after decompression**.

The existing game, `convoy`, proves that the limit is practical:

- Its complete Windows executable is roughly 117 KB.
- It supplies a 640×480 framebuffer, keyboard input, fixed-timestep simulation,
  synthesized stereo audio, procedural graphics, and a headless test harness.
- Runtime memory does not count against the disk limit. A game can allocate
  megabytes while running.
- Generated graphics, maps, animation, music, and sound effects cost very
  little executable space.
- Stored images, recorded audio, large fonts, and general-purpose engines are
  the expensive choices.

Every new game must remain a complete standalone program. Useful techniques and
code can be copied from `convoy`, but the new games cannot rely on it or on a
shared engine.

A realistic new entry should land between 100 and 350 KB, leaving most of the
floppy unused. The real constraint is development and balancing time before the
4 September 2026 deadline.

---

## 1. DEEPSCAN

### Core concept

`DEEPSCAN` is a submarine exploration and survival game played largely through
sound.

The player operates a small research submarine descending into a procedurally
generated trench. The ocean outside is almost completely black. Passive
instruments reveal noises, but the player must emit an active sonar pulse to
see the surrounding terrain clearly.

A sonar pulse provides information, but it also propagates through the world.
Anything capable of hearing it now knows approximately where the submarine is.

The central decision is:

> Is knowing what is nearby worth revealing where I am?

That produces tension without requiring scripted jump scares or large amounts
of authored content.

### Moment-to-moment play

The submarine has momentum rather than instant movement. The player controls:

- Thrust and direction
- Depth planes
- Active sonar strength and direction
- Exterior lights
- Silent running
- A small set of onboard systems

A typical sequence might be:

1. The player hears an unidentified scraping sound to the east.
2. Passive sonar shows its approximate direction but not its distance.
3. The player sends a weak directional ping.
4. The return briefly outlines a wreck, surrounding rock, and something moving
   behind it.
5. The player cuts the engine and drifts silently.
6. Battery power continues falling while pressure increases with depth.
7. The player decides whether the wreck is worth approaching.

Sonar contacts should be represented by shape, velocity, sound signature, and
echo pattern—not merely red enemy dots.

### Run structure

A run should last approximately 20–35 minutes and descend through several depth
bands:

1. **Continental shelf:** tutorial space, wreckage, and low pressure.
2. **Midnight zone:** limited visibility and territorial wildlife.
3. **Wreck field:** valuable salvage and dangerous navigation.
4. **Thermal vents:** sensor distortion, heat, and unusual organisms.
5. **Abyssal objective:** the run's main discovery.
6. **Ascent:** returning with specimens while damaged and pursued.

Before descending, the player chooses a contract such as photographing an
organism, recovering a black box, mapping a vent system, or finding a missing
vessel. Optional discoveries create risk and score, while reaching the primary
objective completes the expedition.

The ending depends on what the player retrieves and what they awaken.

### Systems and progression

Resources should remain few and interdependent:

- **Battery** powers propulsion, lights, sonar, and repairs.
- **Oxygen** imposes the overall run limit.
- **Hull integrity** determines safe depth.
- **Noise** determines detection risk.
- **Ballast** controls vertical motion and emergency ascent.
- **Cargo capacity** limits specimens and salvage.

Permanent progression should unlock starting configurations rather than raw
power:

- **Survey vessel:** better sonar and a fragile hull.
- **Salvage vessel:** manipulator arm and a larger hold.
- **Deep-diving vessel:** greater pressure tolerance but poor battery life.
- **Experimental vessel:** unusual instruments with unreliable readings.

This preserves uncertainty instead of allowing progression to eliminate it.

### Presentation

The visual identity can be strong despite requiring almost no stored artwork:

- Black or deep-blue background
- Sonar waves drawn as expanding circles
- Terrain revealed as fading, stippled contours
- Contacts leaving temporary echo silhouettes
- Instrument lights and compact oscilloscope displays
- Particles showing current direction

Audio becomes part of the simulation:

- Propeller and hull noise
- Directional creature calls
- Sonar pings with distance-dependent echoes
- Pressure creaks
- Hydrophone static
- Low synthesized musical layers merged with environmental sound

Stereo positioning can assist navigation, but every essential cue needs a
visual equivalent for accessibility and machines without an audio device.

### How it fits

`DEEPSCAN` is exceptionally well suited to the constraint. It needs no stored
backgrounds, creature recordings, or music tracks. Seeded noise, spline
segments, and cellular cave fields generate the trench. Creatures can be
parameterized articulated silhouettes. Sonar visuals consist of rasterized
geometry and fading history buffers.

| Component | Approximate size |
|---|---:|
| Win32 platform and framebuffer | 55–65 KB |
| Simulation, physics, and world generation | 25–45 KB |
| Rendering and interface | 25–40 KB |
| Synthesized audio and acoustics | 15–30 KB |
| Contracts, discoveries, and endings | 10–25 KB |
| **Expected total** | **130–205 KB** |

The primary risk is clarity: sound-based information must feel mysterious
without becoming frustrating or arbitrary.

---

## 2. SWITCHYARD

### Core concept

`SWITCHYARD` is a real-time railway dispatch game about controlling flow through
a constrained network.

The player does not drive trains. They operate signals, points, platforms,
sidings, and temporary speed restrictions. Every train has a destination and
timetable, but the network cannot accommodate everything simultaneously.

The central decision is:

> Which train should be delayed now to prevent the entire network from failing
> later?

It is a systems puzzle in motion. One apparently harmless decision can
propagate across the whole timetable.

### Moment-to-moment play

The game appears as one readable railway diagram. Trains move continuously
while the player:

- Toggles points
- Clears or holds signals
- Assigns platforms
- Authorizes reversing movements
- Creates temporary routes
- Responds to equipment failures

Early scenarios involve two or three trains. Later scenarios introduce:

- Express services that cannot be held for long
- Slow freight trains occupying several blocks
- Trains too long for particular platforms
- Single-track sections shared in both directions
- Passenger connections
- Emergency and medical services
- Track failures and bad weather
- Empty stock that must be repositioned

Collisions should be possible, but ordinary failure should more often result
from deadlock, gridlock, or accumulating delays. The game is about dispatching,
not twitch reactions.

### Scenario structure

A campaign can progress through several railway districts:

1. **Rural branch:** passing loops and basic signals.
2. **Commuter junction:** frequent trains and platform conflicts.
3. **Freight corridor:** long trains and siding management.
4. **Mountain district:** single track, tunnels, and weather.
5. **Central terminal:** dense arrivals, departures, and repositioning.
6. **Crisis shift:** failures combining everything learned.

Each shift lasts 8–15 minutes. Performance is graded on:

- Safety
- Punctuality
- Preserved passenger connections
- Freight priority
- Efficient platform use
- Unnecessary signal changes

A date-seeded daily timetable can give every player the same generated
challenge.

### Deeper mechanics

Block occupancy is the foundation: only one train can safely occupy a block,
and signals protect entry. More complexity emerges from a small set of
consistent rules:

- **Braking distance:** a signal cannot instantly stop a fast train.
- **Route locking:** points cannot change beneath or immediately ahead of a
  train.
- **Train length:** long freight trains can obstruct several switches.
- **Platform compatibility:** some services need sufficient length or
  electrification.
- **Crew time:** a badly delayed train may be unable to complete later service.
- **Passenger connections:** delaying one local train can save many passengers
  elsewhere.

Easier difficulties can provide pause-and-plan operation. Harder difficulties
keep the network running continuously.

### Presentation

The visual style should resemble a railway control panel:

- Thick geometric track lines
- Bright block-occupancy colors
- Small numbered train markers
- Mechanical signal lamps
- Minimal station architecture behind the diagram
- A timetable strip showing upcoming pressure

Audio reinforces state:

- Relay clicks when routes lock
- Signal bells
- Distant train horns
- Wheel rhythms based on train speed
- Alarm tones for unsafe or deadlocked states
- A restrained procedural pulse that intensifies with congestion

No realistic train artwork is required. The network itself is the game board.

### How it fits

Track networks can be compact node-and-edge tables. Even a large authored
scenario occupies only a few kilobytes. Timetables are similarly tiny, and
procedural variations can reuse the same layouts.

A train needs only an identifier, route, speed, length, priority, schedule, and
current block. Graphics consist almost entirely of lines, circles, symbols, and
text.

| Component | Approximate size |
|---|---:|
| Platform layer | 55–65 KB |
| Rail simulation and safety rules | 30–50 KB |
| Scenario and timetable generation | 20–35 KB |
| Diagram renderer and interface | 20–35 KB |
| Synthesized audio | 8–18 KB |
| Scenarios and tutorial text | 10–25 KB |
| **Expected total** | **145–225 KB** |

This is the safest of the five projects. Its principal risk is interface
clarity, not technology or content volume.

---

## 3. LAST LIGHT

### Core concept

`LAST LIGHT` is a supernatural deduction game about operating a lighthouse
during a week-long storm.

Ships appear beyond the harbor as incomplete combinations of navigation lights,
radio signals, silhouettes, and foghorns. The player must decide which channel
to illuminate and which vessels to trust.

Some contacts are genuine ships. Others imitate distress signals or display
almost-correct navigation lights.

The central decision is:

> Do I guide this contact into safety, or am I showing something the way
> ashore?

### Moment-to-moment play

The lighthouse has several stations:

- **Lens room:** rotate and focus the beam.
- **Radio room:** listen, tune frequencies, and respond.
- **Chart table:** compare bearings, tides, and known routes.
- **Generator room:** distribute power and repair machinery.
- **Keeper's quarters:** review notes and previous incidents.
- **Exterior gallery:** inspect nearby water at personal risk.

A contact is not solved by one clue. The player assembles evidence:

- A light arrangement suggests vessel size and direction.
- Radio procedure suggests origin and competence.
- Foghorn timing indicates distance.
- The tide makes some claimed positions impossible.
- A reported vessel name may appear in an old wreck log.
- Something may answer a message before it has physically arrived.

The player then illuminates a channel, transmits a warning, extinguishes the
light, or continues observing.

### Night structure

The campaign lasts seven nights:

1. Ordinary navigation teaches the basic rules.
2. Equipment failures divide the player's attention.
3. Radio and visual evidence begin to contradict one another.
4. Known supernatural behavior becomes identifiable.
5. Multiple genuine vessels compete for assistance.
6. Previous decisions return as consequences.
7. The final night reveals why the previous keeper vanished.

During daylight, the player repairs equipment and investigates the lighthouse.
These sections should be short and decision-focused rather than free-roaming.

Possible endings include saving the harbor, guiding the missing keeper home,
allowing the entity ashore, abandoning the lighthouse, becoming the next
signal, or discovering that the harbor itself is the trap.

### Deduction design

At the beginning of a campaign, the game generates a consistent hidden ruleset.
Possible rules include:

- The imitation cannot reproduce green light.
- It answers radio messages exactly nine seconds late.
- It cannot state the current tide correctly.
- It appears only on bearings containing a particular reef.
- It knows facts learned from previous victims.

The player gradually obtains evidence for these rules. Randomness changes which
rules are active, but every campaign must remain logically solvable.

Ambiguity may be atmospheric, but outcomes cannot be arbitrary. After losing,
the player should be able to identify the clue they misunderstood or ignored.

### Presentation

The art direction uses a tiny palette:

- Near-black sea and sky
- Warm amber lighthouse interior
- Cold blue-green exterior light
- Distant colored navigation points
- Procedural rain, spray, and fog
- Large shapes suggested rather than fully shown

The lighthouse beam is a rotating, dithered cone whose visibility changes with
rain and fog density.

Audio carries much of the atmosphere:

- Procedural wind and rain
- Foghorns with identifiable patterns
- Generated radio interference and signaling tones
- Generator rhythm
- Lens machinery
- Unexplained knocks and footsteps

Recorded dialogue should be avoided. Radio exchanges can use text accompanied
by synthesized interference.

### How it fits

The world consists mostly of darkness, particles, geometry, and text. A detailed
lighthouse interior can be constructed from rectangles, line art, and a few
tiny monochrome masks.

Several thousand words of writing consume only tens of kilobytes. A compact
Latin font costs hundreds of bytes, although Korean localization would require
a more selective glyph strategy.

| Component | Approximate size |
|---|---:|
| Platform layer | 55–65 KB |
| Contact simulation and deduction system | 20–35 KB |
| Lighthouse scenes and weather renderer | 30–50 KB |
| Synthesized audio | 15–25 KB |
| Story events, clues, and endings | 25–60 KB |
| Interface and journal | 15–25 KB |
| **Expected total** | **160–260 KB** |

The main risk is localization. The game depends on textual clues, while the
contest judges are Korean. It needs either a Korean translation with a compact
glyph set or an unusually strong symbolic clue language.

---

## 4. MICROCOLONY

### Core concept

`MICROCOLONY` is an ecosystem strategy game set inside a drop of water.

The player does not select organisms and issue orders. Instead, they influence
the environment:

- Place nutrients
- Adjust light
- Change acidity or temperature
- Introduce a species
- Remove contaminants
- Create currents
- Quarantine part of the dish

Organisms respond according to local rules. The player succeeds by understanding
and steering the ecosystem rather than directly controlling it.

The central decision is:

> How can I produce the desired outcome without destabilizing everything that
> supports it?

### Moment-to-moment play

The dish contains hundreds or thousands of small simulated organisms. Each has
simple behaviors:

- Seek nutrients
- Avoid toxins
- Follow light
- Consume smaller organisms
- Reproduce when energy is high
- Emit or consume chemicals
- Form colonies
- Infect hosts
- Enter dormant spores

The player observes population graphs, chemical overlays, and organism
behavior, then applies limited interventions.

A mission might ask the player to:

- Keep three species alive for ten minutes
- Produce a target quantity of a medicinal compound
- Eliminate a parasite without killing its host
- Restore an ecosystem damaged by industrial runoff
- Encourage two organisms to develop symbiosis
- Identify an unknown pathogen from its effects

### Emergent depth

Species are assembled from behavioral traits rather than individually scripted:

- Diet
- Movement pattern
- Reproduction method
- Environmental tolerances
- Waste product
- Defensive behavior
- Social or colony behavior
- Mutation rate

Interesting relationships can emerge from those traits:

1. A grazer controls algae but becomes prey for a predator.
2. The predator's waste fertilizes the algae.
3. Excess nutrients cause an algae bloom.
4. The bloom consumes oxygen.
5. Low oxygen kills the grazer first.
6. Without grazers, the bloom becomes irreversible.

The player reasons about feedback loops rather than individual units.

### Campaign and progression

The campaign presents increasingly unusual samples:

1. Pond water
2. Agricultural runoff
3. Hospital culture
4. Deep-sea vent sample
5. Extraterrestrial ice sample
6. Synthetic-organism containment failure

New laboratory tools expand how the player can influence a sample, but each
tool introduces side effects. Antibiotics, for example, can kill beneficial
bacteria and select for resistance.

Between missions, discoveries enter a compact organism catalog. Date-seeded
samples can provide continuing challenges after the campaign.

### Presentation

The game can generate its entire visual identity:

- Translucent organisms with procedural internal structures
- Cilia, flagella, spores, and cell division
- Colored chemical gradients
- Fluid currents represented by drifting particles
- Smooth zoom from the entire dish to individual organisms
- Evolutionary family trees and population graphs

An organism requires no bitmap. It can be assembled from:

- An ellipse or deforming polygon
- A generated membrane
- Several internal circles
- Parameterized appendages
- A palette selected from its traits

Audio can remain abstract: soft pulses, bubbling noise, and musical layers
driven by population balance.

### How it fits

The simulation operates on compact arrays and grids. Thousands of agents consume
runtime memory but almost no executable storage. Every visual is generated from
organism traits.

Even 100 defined species would not need 100 sets of artwork. They would need 100
small parameter records, potentially only 16–32 bytes each.

| Component | Approximate size |
|---|---:|
| Platform layer | 55–65 KB |
| Organism and chemical simulation | 40–70 KB |
| Procedural organism renderer | 30–55 KB |
| Laboratory interface and graphs | 20–35 KB |
| Missions and species definitions | 15–35 KB |
| Synthesized audio | 8–18 KB |
| **Expected total** | **170–275 KB** |

The risks are computational and educational. The ecosystem must be rich enough
to surprise players while remaining readable enough that failure does not feel
random. Automated simulation sweeps would be essential for finding population
collapse and unwinnable seeds.

---

## 5. TEN PACES

### Core concept

`TEN PACES` is a simultaneous-turn tactical western.

The player plans several seconds of action for every controlled character.
Enemies plan at the same time. Once committed, both plans execute together in a
short real-time sequence.

The player might order a character to:

1. Walk to a doorway.
2. Kick it open.
3. Aim toward a balcony.
4. Fire once.
5. Dive behind a table.

During execution, the enemy may leave the balcony early, shoot through the
door, surrender, or run across the planned line of fire.

The central decision is:

> What is everyone else likely to do during the next three seconds?

### Planning and execution

Each turn has two phases.

**Planning phase**

Time is frozen. The player assigns actions along a short timeline:

- Move
- Aim
- Fire
- Throw or interact
- Speak
- Wait
- Dive
- Melee
- Reload

The interface previews only the player's plan. Enemy intentions are inferred
from stance, gaze, cover, temperament, wounds, and previous behavior.

**Execution phase**

Every plan runs simultaneously for approximately three seconds. The player
cannot intervene. Bullets, doors, movement, reactions, and environmental
effects resolve deterministically. Time then freezes for the next planning
phase.

### Why it differs from ordinary tactics

The game is not about maximizing damage percentages. It is about creating and
interpreting intentions.

An enemy may:

- Shoot aggressively
- Hold fire unless approached
- Flee when isolated
- Protect another character
- Bluff during a standoff
- Surrender if disarmed
- Panic when a lamp breaks
- Fire toward an expected position

Speaking can itself be tactical. A threat might make a nervous outlaw surrender,
provoke an aggressive one into firing early, or distract someone long enough
for another character to move.

Nonlethal outcomes should matter. Killing everyone may be possible while
producing worse campaign consequences.

### Campaign structure

The campaign is a sequence of compact procedural or semi-authored scenes:

- Saloon standoff
- Train robbery
- Jailbreak
- Canyon ambush
- Hostage exchange
- Nighttime ranch defense
- Duel in the street
- Final confrontation shaped by earlier survivors

Characters remember what the player did. Sparing an outlaw may create a later
ally. Killing a deputy may turn a town hostile. Accidentally injuring a civilian
changes the player's reputation even if the mission succeeds.

A run should last 30–50 minutes, with individual encounters lasting 3–8 minutes.

### Simulation

The rules need to be deterministic and understandable:

- Bullets travel through space rather than using instant hit percentages.
- Accuracy depends on aiming time, motion, distance, light, and injury.
- Thin cover can be penetrated.
- Bullets ricochet from hard surfaces at shallow angles.
- Doors and furniture can move.
- Smoke and broken lamps modify visibility.
- Characters react to nearby shots and casualties.
- Friendly fire is always possible.

Planning displays predicted paths but cannot guarantee them because other
characters may alter the scene before an action executes.

### Presentation

A stylized side-on or three-quarter view minimizes art requirements:

- Flat silhouettes with generated hats, coats, and poses
- High-contrast desert palette
- Destructible line-and-rectangle environments
- Dust, smoke, muzzle flashes, and splinters as particles
- Brief cinematic camera movement during execution
- Timeline icons during planning

Procedural skeletal animation needs only a head, torso, upper and lower limbs,
weapon, and a handful of mathematically blended poses.

Audio includes synthesized gunshots with different envelopes, ricochets,
footsteps, environmental loops, and sparse procedural western music.

### How it fits

The executable stores character and environment descriptions instead of
animation frames. A saloon is a compact set of walls, doors, tables, lamps, and
cover polygons. Characters are assembled from shapes and palette choices.
Narrative encounters are small condition-and-consequence tables.

| Component | Approximate size |
|---|---:|
| Platform layer | 55–65 KB |
| Timeline and deterministic combat | 40–70 KB |
| Enemy planning and personality AI | 30–55 KB |
| Procedural animation and rendering | 35–60 KB |
| Campaign events and dialogue | 20–50 KB |
| Synthesized audio | 12–25 KB |
| **Expected total** | **195–325 KB** |

The risk is development scope. Simultaneous plans create many interacting
combinations, requiring the strongest automated replay and scenario-testing
system of these concepts. Disk space remains ample; polish and test time are
the actual limitations.

---

## Direct comparison

| Game | Primary experience | Typical run | Asset pressure | Technical risk | Deadline safety |
|---|---|---:|---:|---:|---:|
| `DEEPSCAN` | Suspense and exploration | 20–35 min | Very low | Medium | High |
| `SWITCHYARD` | Real-time systems puzzle | 8–15 min shifts | Extremely low | Low–medium | Very high |
| `LAST LIGHT` | Deduction and horror | 60–90 min campaign | Low | Medium | Medium |
| `MICROCOLONY` | Emergent ecosystem strategy | 15–30 min | Very low | High | Medium |
| `TEN PACES` | Predictive tactical action | 30–50 min | Medium | High | Low–medium |

## Recommendations

### Best artistic fit: DEEPSCAN

`DEEPSCAN` has the strongest identity and uses the storage constraint as an
artistic advantage. Darkness, sonar, procedural terrain, and synthesized audio
remove the need for expensive assets while reinforcing the game's central
mechanic. A tightly scoped version targeting a 25-minute run and an executable
around 180 KB is realistic.

### Safest contest entry: SWITCHYARD

`SWITCHYARD` has the highest probability of becoming polished before the
deadline. Its content is compact, its rules are deterministic, and generated
timetables can be validated automatically. It is also readable within seconds,
which matters when judges are evaluating many entries.

### Highest ceiling: TEN PACES

`TEN PACES` may offer the most immediately exciting result, but it has the
largest implementation and testing surface. It is a better choice only if the
scope is aggressively limited to a few reusable environments and a short
campaign.

### Most experimental: MICROCOLONY

`MICROCOLONY` is technically and visually original. Its challenge is making an
emergent simulation understandable. It needs excellent overlays, tutorials, and
automated balance sweeps.

### Strongest narrative atmosphere: LAST LIGHT

`LAST LIGHT` can create a memorable experience with minimal visual assets, but
its deduction and story depend on language. Localization is a significant
contest risk unless addressed from the beginning.

## Overall order

1. **DEEPSCAN** — best combination of identity, feasibility, and constraint fit.
2. **SWITCHYARD** — safest and fastest route to another polished entry.
3. **TEN PACES** — highest entertainment ceiling, with substantially more risk.
4. **LAST LIGHT** — memorable but localization-dependent.
5. **MICROCOLONY** — original but hardest to teach and balance.
