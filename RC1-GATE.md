# Release candidate 1 gate

RC1 is a shippable build, not a renamed beta. The concept document remains the
design authority. This gate turns its promises into evidence that can be
checked before any game is called version 1.

## Every game

- Standalone Win32 executable, including its Win32 platform code, at or below
  1,474,560 bytes
- Meaningful use of the production budget, targeting the concept's 80–95%
  range without padding
- Fixed-timestep simulation and responsive keyboard input
- Plain-English objective, tutorial, controls, consequences, live status,
  victory, failure, and ending
- Authored campaign scenes embedded in the executable, not required as loose
  files
- Complete campaign with seeded variation and at least one optional or
  consequence-bearing decision
- Essential audio information duplicated visually
- Audio that is non-silent, clipped safely, and optional when no device exists
- Pause/help, restart, mute, and a deterministic daily/challenge seed
- Automated player that uses real rules, plus careless and boundary probes
- Deterministic replay check, sanitizer pass, warning-free build, and 100-seed
  completion/failure evidence
- Actual Windows launch, input, audio-initialization, and screenshot evidence
- Clean distributable containing the executable, README, controls, known
  limitations, and measured SHA-256

## DEEPSCAN

- Six-phase expedition: shelf, midnight zone, wreck field, vents, abyssal
  objective, ascent
- Contract choice and four starting submarine configurations
- Directional/graded sonar, lights, silent running, ballast, cargo, pressure,
  battery, oxygen, hull, noise, and repair tradeoffs
- Twelve recognizable contact types, optional discoveries, specimen records,
  debrief, and consequence-dependent endings

## SWITCHYARD

- Eight distinct districts and a graduated tutorial
- Blocks, braking distance, route locking, train length, compatible platforms,
  crew time, and passenger connections
- Passenger, freight, maintenance, heritage, emergency, and empty-stock service
  behavior
- Authored incidents, pause-and-plan option, grading, and date-seeded timetable

## LAST LIGHT

- Seven-night narrative with daylight repair/investigation decisions
- Lens, radio, chart, generator, journal, and exterior stations
- Multiple contacts, five evidence channels, persistent transcript, and a
  campaign-generated logically consistent hidden rule
- Written explanation of every loss and multiple consequence-dependent endings

## MICROCOLONY

- Six campaign samples, tutorial samples, and continuing date-seeded challenge
- Nutrient, light, acidity/temperature, species, contaminant, current, and
  quarantine interventions with side effects
- Recognizable producer, grazer, predator, decomposer, parasite, and spore
  behaviors
- Chemical overlays, population history, field guide, and reproducible
  ecosystem stability sweeps

## TEN PACES

- Eight campaign encounters lasting multiple planning rounds
- Move, aim, fire, interact, speak, wait, dive, melee, and reload actions
- Deterministic traveling bullets, cover, penetration, ricochet, doors,
  breakable lamps, smoke, injury, reactions, and friendly-fire rules
- Temperament-driven inferred intentions, surrender and nonlethal outcomes
- Persistent survivors, reputation, allies, consequences, and shaped finale

## Evidence policy

A source comment or version string is not evidence. A feature is complete only
when its player-visible path exists and an appropriate probe executes it. Cross
compilation proves the PE file exists; the Windows CI smoke test proves that it
launches. RC1 is declared only when both classes of evidence are current.
