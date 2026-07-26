# DEEPSCAN — 1.0 release candidate 1

Choose one of three contracts and one of four submersibles, then descend through
the continental shelf, midnight zone, wreck field, thermal vents, and abyssal
plain. Sonar reveals specimens, salvage, and the abyssal structure—but also
wakes predators. Complete the selected objective and survive the ascent.

The survey, salvage, deep-diving, and experimental vessels trade hull, battery,
and cargo capacity. Battery, oxygen, pressure, hull, noise, lights, silent
running, repairs, cargo, twelve organism signatures, and optional discoveries
remain active throughout the expedition. The debrief names every organism added
to the expedition's specimen record.

## Controls

- Arrow keys: choose contract/vessel during briefing; thrust during expedition
- Z: emit sonar and cycle its strength
- X: toggle silent running
- C: toggle exterior lights
- V: repair the hull using battery power
- Enter: deploy; restart after a terminal state
- H: hold for objective and controls; pauses play
- M: toggle audio
- Escape: quit

`./build.sh` creates `build/deepscan.exe` and the deterministic
`build/deepscan_headless`. Harness options are `-s SEED`, `-N RUNS`,
`-t TICKS`, and `-L` for the careless loss probe.
