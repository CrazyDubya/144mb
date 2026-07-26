# MICROCOLONY — 1.0 release candidate 1

Steer six ecosystems: pond water, agricultural runoff, hospital culture,
hydrothermal vents, alien ice, and synthetic containment. Producers, grazers,
predators, decomposers, parasites, and spores respond to local food webs,
toxins, light, acidity, currents, quarantine, and intervention side effects.

Each sample specifies tools that must be demonstrated and requires all six
populations to remain viable for 120 simulation steps. Population-history
traces and the illustrated field guide make feedback loops inspectable.

## Controls

- Arrow keys: move the pipette
- Z: add nutrients
- X: apply local antibiotic
- C: select light, acidity, introduce, clean, current, or quarantine
- V: apply the selected tool
- Enter: restart with a new sample seed
- H: hold for objective and controls; pauses play
- M: toggle audio
- Escape: quit

`./build.sh` creates `build/microcolony.exe` and
`build/microcolony_headless`. The harness accepts `-s`, `-N`, `-t`, and `-L`.
