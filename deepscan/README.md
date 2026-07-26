# DEEPSCAN — beta

Pilot a research submarine into a seeded trench. Complete one of three
contracts—survey, salvage, or abyss—by tagging four to six specimens, recovering
the expedition black box, and returning to the surface. Sonar reveals the world
but attracts predators; deep pressure, oxygen, battery, noise, and hull damage
all remain live during the expedition.

## Controls

- Arrow keys: thrust
- Z: active sonar
- X: toggle silent running
- H: hold for objective and controls; pauses play
- Enter: restart with a new seed
- Escape: quit

Build with `./build.sh`. The Windows game includes a start screen and reactive
sonar/engine synthesis. The deterministic harness accepts `-s SEED`, `-N RUNS`,
and `-t TICKS`; `./build/deepscan_headless -N 100` runs a completion sweep.
