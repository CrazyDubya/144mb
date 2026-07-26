# Five Full Games for a 1.44 MB Floppy

## The constraint

Despite the repository name, `144mb` is not a 144 MB game. It is a collection
of standalone games that must each fit on one 1.44 MB floppy: exactly
**1,474,560 bytes after decompression**.

The existing game, `convoy`, proves that the platform cost is small:

- Its complete Windows executable is roughly 117 KB.
- It supplies a 640×480 framebuffer, keyboard input, fixed-timestep simulation,
  synthesized stereo audio, procedural graphics, and a headless test harness.
- Runtime memory does not count against the disk limit, so simulations and
  working buffers can be large.
- Approximately 1.35 MB remains after a `convoy`-sized program. That capacity
  should improve what the player sees, hears, reads, and does.

Every new game must remain a complete standalone program. Useful techniques and
code can be copied from `convoy`, but the new games cannot rely on it or on a
shared engine.

These concepts target **1.18–1.40 MB**, or 80–95% of the disk. This is a
quality target rather than a padding target: every allocation must produce
clearer play, stronger atmosphere, more authored variety, or a more complete
campaign. Each build retains 5–15% headroom beneath the hard limit.

Every budget includes a separate **60,000-byte Win32 platform allowance** for
the window, framebuffer blit, keyboard input, fixed timestep, memory allocation,
and audio output. All listed game code and assets are embedded into the same
standalone Windows executable. Development-only headless harness and bot code do
not consume submission bytes.

All five games are written and presented straightforwardly in English. Icons
can improve scanning, but controls, objectives, costs, consequences, tutorials,
dialogue, and story are allowed to use as many clear English words as they need.

The production rule is hybrid:

- Generate systems that benefit from variation: layouts, weather, populations,
  schedules, acoustics, and daily challenges.
- Author what gives the game identity: key locations, characters, missions,
  tutorials, performances, music, dramatic scenes, and endings.
- Use the disk as a creative budget. A tiny executable is not a design goal.

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

### Authored content and presentation

The procedural trench provides replayable structure, but each depth band gets
authored visual and narrative material:

- Black or deep-blue background
- Sonar waves drawn as expanding circles
- Terrain revealed as fading, stippled contours
- Contacts leaving temporary echo silhouettes
- Illustrated instrument panels for each submarine class
- Particles showing current direction
- Hand-drawn wreck interiors and research installations
- Distinct multi-frame silhouettes for twelve major organisms
- Full-screen discovery plates for rare species and abyssal structures
- Captain portraits, contract briefings, specimen records, and debriefings
- Authored ascent sequences and six illustrated endings

Audio becomes part of the simulation:

- Propeller and hull noise
- Directional creature calls
- Sonar pings with distance-dependent echoes
- Pressure creaks
- Hydrophone static
- Recorded or carefully authored acoustic signatures for major creatures
- Sampled metal impacts, cable strain, water movement, and machinery
- A multi-part ambient score blended with procedural sonar and engine sound

Stereo positioning can assist navigation, but every essential cue needs a
visual equivalent for accessibility and machines without an audio device.

### 1.44 MB production budget

Seeded noise, spline segments, and cellular fields still generate the large
trench because variation is valuable there. Storage is concentrated on the
things procedural output cannot replace: recognizable creatures, readable
instruments, discoveries, story beats, and an authored acoustic identity.

| Component | Planned bytes |
|---|---:|
| Standalone Win32 platform, framebuffer, input, and audio output | 60,000 |
| Game simulation, acoustics, world generation, and rendering | 150,000 |
| Instrument panels, wrecks, organisms, and discovery art | 390,000 |
| Sampled ambience, creature signatures, effects, and score | 320,000 |
| Contracts, logs, encounters, specimen records, and endings | 260,000 |
| Fonts, interface graphics, palettes, and metadata | 80,000 |
| **Shipping target** | **1,260,000 (85.45%)** |
| **Remaining headroom** | **214,560 (14.55%)** |

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

### Authored content and presentation

The control diagram remains the functional center, but the campaign should not
look like one abstract panel repeated forever:

- Thick geometric track lines
- Bright block-occupancy colors
- Small numbered train markers
- Mechanical signal lamps
- Illustrated station backdrops and region-specific control desks
- A timetable strip showing upcoming pressure
- Eight hand-designed districts with multiple operating eras and conditions
- Distinct passenger, freight, maintenance, heritage, and emergency trains
- Dispatcher portraits, shift briefings, newspaper reports, and route maps
- Authored incidents such as a derby crowd, landslip, signal failure, stranded
  train, royal special, evacuation, and winter shutdown
- A complete graduated tutorial campaign before unrestricted timetables

Audio reinforces state:

- Relay clicks when routes lock
- Signal bells
- Distinct sampled horns and traction sounds for train classes
- Wheel rhythms based on train speed
- Alarm tones for unsafe or deadlocked states
- Station ambience, rain, wind, public announcements, and control-room sound
- Regional music cues and an adaptive score that intensifies with congestion

The network remains readable, but the allotted storage makes each district feel
like a place and each service like a physical train rather than an anonymous
number.

### 1.44 MB production budget

Track networks can be compact node-and-edge tables. Even a large authored
scenario occupies only a few kilobytes, allowing dozens of carefully designed
shifts. Procedural timetables extend those layouts after the campaign instead
of replacing authored progression.

| Component | Planned bytes |
|---|---:|
| Standalone Win32 platform, framebuffer, input, and audio output | 60,000 |
| Rail simulation, timetable system, renderer, and game logic | 160,000 |
| Eight districts, control desks, train art, maps, and weather | 340,000 |
| Train, station, control-room, incident, and music audio | 280,000 |
| Campaign shifts, tutorials, incidents, and daily variants | 270,000 |
| Fonts, briefings, reports, interface art, and metadata | 150,000 |
| **Shipping target** | **1,260,000 (85.45%)** |
| **Remaining headroom** | **214,560 (14.55%)** |

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

### Authored content and presentation

The art direction uses a controlled palette without using darkness as an excuse
to omit authored art:

- Near-black sea and sky
- Warm amber lighthouse interior
- Cold blue-green exterior light
- Distant colored navigation points
- Procedural rain, spray, and fog
- Detailed illustrated rooms that visibly deteriorate across seven nights
- Portraits and signal profiles for ships, crews, and recurring callers
- Hand-authored maritime charts, logbook pages, photographs, and evidence
- Full-screen contact sightings when the player obtains a clear view
- Bespoke wreck, rescue, revelation, and ending scenes

The lighthouse beam is a rotating, dithered cone whose visibility changes with
rain and fog density.

Audio carries much of the atmosphere:

- Procedural wind and rain
- Sampled foghorns with identifiable patterns
- Distinct English radio performances for major contacts
- Authored radio interference and signaling tones
- Generator rhythm
- Lens machinery
- Unexplained knocks and footsteps
- A scored opening, nightly transitions, escalating storm, and each ending

Every clue is also written in clear English in the radio transcript and
journal, so audio quality never becomes a deduction barrier.

### 1.44 MB production budget

Procedural weather and contact scheduling create variation. Storage goes to the
authored rooms, evidence, contact performances, mystery writing, and payoffs
that make a narrative game memorable.

| Component | Planned bytes |
|---|---:|
| Standalone Win32 platform, framebuffer, input, and audio output | 60,000 |
| Contact simulation, deduction logic, weather, and rendering | 140,000 |
| Rooms, contacts, evidence, sightings, rescues, and ending art | 390,000 |
| Radio performances, ambience, effects, and score | 430,000 |
| Seven-night script, mystery variants, logs, and endings | 270,000 |
| English fonts, journal, charts, interface art, and metadata | 100,000 |
| **Shipping target** | **1,390,000 (94.26%)** |
| **Remaining headroom** | **84,560 (5.74%)** |

The main risk is deduction fairness. Recorded performances and atmospheric
effects may enrich a clue, but the transcript, chart, and journal must preserve
all information needed to solve it.

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

### Authored content and presentation

The simulation generates behavior, while authored art makes its organisms
recognizable and its laboratory believable:

- Translucent organisms assembled from a large library of painted body parts
- Authored cilia, flagella, spores, feeding, infection, and division animation
- Colored chemical gradients
- Fluid currents represented by drifting particles
- Smooth zoom from the entire dish to individual organisms
- Evolutionary family trees and population graphs
- Six illustrated laboratory environments and story interludes
- A detailed field guide with English names, behavior, habitat, and discoveries
- Hand-designed tutorial samples that demonstrate one feedback loop at a time
- Major organisms and mission specimens with unique art rather than recombined
  generic parts

Audio combines microscope and laboratory ambience, tactile tool sounds,
organism-scale abstract effects, and musical layers driven by population
balance. Each laboratory and campaign chapter receives a distinct soundscape.

### 1.44 MB production budget

The simulation operates on compact arrays and grids. Thousands of agents consume
runtime memory but little executable storage. The disk is therefore available
for a broad component library, hero-species animation, laboratories, tutorials,
an encyclopedia, and richer audio instead of another layer of simulation for
its own sake.

| Component | Planned bytes |
|---|---:|
| Standalone Win32 platform, framebuffer, input, and audio output | 60,000 |
| Ecosystem simulation, chemistry, game logic, and renderer | 200,000 |
| Organism components, hero species, laboratories, and interludes | 440,000 |
| Laboratory ambience, tools, organism effects, and adaptive music | 260,000 |
| Missions, tutorials, sample definitions, and campaign events | 230,000 |
| Field guide, fonts, graphs, overlays, and interface art | 130,000 |
| **Shipping target** | **1,320,000 (89.52%)** |
| **Remaining headroom** | **154,560 (10.48%)** |

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

### Authored content and presentation

A stylized side-on or three-quarter view keeps combat readable while the disk
supports a substantially richer western:

- Layered character sprites with distinct faces, hats, coats, weapons, and poses
- High-contrast desert palette
- Twelve hand-authored, destructible tactical arenas
- Dust, smoke, muzzle flashes, and splinters as particles
- Brief cinematic camera movement during execution
- Timeline icons during planning
- Portraits and dialogue scenes for the recurring cast
- Authored execution, injury, surrender, interaction, and reaction animation
- Illustrated location introductions, travel transitions, and endings

Procedural skeletal animation handles interpolation and unusual action
combinations, but it draws from authored body parts and key poses. Major
characters and set pieces receive unique animation.

Audio includes distinct sampled weapons, ricochets by surface, footsteps,
destruction, crowd and animal ambience, short English character barks, and an
authored western score that rearranges during planning and execution.

### 1.44 MB production budget

The executable stores character and environment descriptions instead of
full-screen video or redundant frames. Arenas remain compact geometry, while
storage is spent on their visible materials, props, cast, animation, sound,
campaign scenes, and consequences.

| Component | Planned bytes |
|---|---:|
| Standalone Win32 platform, framebuffer, input, and audio output | 60,000 |
| Combat, timelines, AI, animation system, renderer, and game logic | 250,000 |
| Arenas, character sprites, portraits, props, scenes, and endings | 430,000 |
| Weapons, impacts, ambience, voices, effects, and score | 300,000 |
| Campaign encounters, dialogue, consequences, and tutorials | 260,000 |
| Fonts, timeline icons, interface art, and metadata | 100,000 |
| **Shipping target** | **1,400,000 (94.94%)** |
| **Remaining headroom** | **74,560 (5.06%)** |

The risk is development scope. Simultaneous plans create many interacting
combinations, requiring the strongest automated replay and scenario-testing
system of these concepts. Disk space remains ample; polish and test time are
the actual limitations.

---

## Direct comparison

| Game | Primary experience | Typical run | Shipping target | Technical risk | Deadline safety |
|---|---|---:|---:|---:|---:|
| `DEEPSCAN` | Suspense and exploration | 20–35 min | 1,260,000 bytes | Medium | High |
| `SWITCHYARD` | Real-time systems puzzle | 8–15 min shifts | 1,260,000 bytes | Low–medium | Very high |
| `LAST LIGHT` | Deduction and horror | 60–90 min campaign | 1,390,000 bytes | Medium | Medium |
| `MICROCOLONY` | Emergent ecosystem strategy | 15–30 min | 1,320,000 bytes | High | Medium |
| `TEN PACES` | Predictive tactical action | 30–50 min | 1,400,000 bytes | High | Low–medium |

## Recommendations

### Best artistic fit: DEEPSCAN

`DEEPSCAN` has the strongest identity. Its procedural trench supports
replayability, while authored wrecks, creatures, instruments, acoustic
signatures, discoveries, and endings give the expedition substance. It uses
darkness as a mechanic, not as a reason to avoid making art.

### Safest contest entry: SWITCHYARD

`SWITCHYARD` has the highest probability of becoming polished before the
deadline. Its deterministic rules allow generated timetables to be validated
automatically, and its compact scenario representation leaves ample room for
many authored districts, shifts, incidents, train identities, and soundscapes.

### Highest ceiling: TEN PACES

`TEN PACES` may offer the most immediately exciting result and makes excellent
use of a nearly full disk through arenas, animation, cast, audio, and campaign
consequences. It also has the largest implementation and testing surface.

### Most experimental: MICROCOLONY

`MICROCOLONY` is technically and visually original. Its large authored organism
library, laboratories, field guide, and graduated experiments are essential to
making the emergent simulation understandable. It also needs automated balance
sweeps.

### Strongest narrative atmosphere: LAST LIGHT

`LAST LIGHT` can create a memorable English-language narrative by spending
heavily on illustrated evidence, radio performances, environmental audio,
mystery variants, and endings. Its risk is ensuring that atmosphere never
obscures the facts required for fair deduction.

## Overall order

1. **DEEPSCAN** — best combination of identity, authored spectacle, and
   feasible systems.
2. **SWITCHYARD** — safest route to a content-rich, polished entry.
3. **TEN PACES** — highest action and presentation ceiling, with the most
   interaction risk.
4. **LAST LIGHT** — strongest authored narrative and audio opportunity.
5. **MICROCOLONY** — most original simulation, but hardest to teach and balance.
