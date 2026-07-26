# Five-game beta gate

> Historical milestone. All five games have passed this gate and are now
> governed by [RC1-GATE.md](RC1-GATE.md).

The five new games are considered beta when their complete game loops and
campaign structures are present, their presentation paths work on the target,
and automated evidence can reproduce completion. Beta does not mean final
balance, final asset volume, or release-candidate polish.

## Required in every beta

- A standalone Windows executable with no files required beside it
- A native headless build using the same simulation and rendering rules
- A start screen built from authored title art
- Plain-English objective, controls, live status, and hold-`H` help/pause
- Multiple stages, districts, nights, samples, contracts, or encounters
- Seeded variation and a deterministic automated player
- A terminal victory state, failure state, and in-game restart
- State-driven stereo synthesis that degrades gracefully without an audio device
- A hard build failure above 1,474,560 bytes
- Successful 100-seed completion sweep
- Clean AddressSanitizer and UndefinedBehaviorSanitizer run
- Windows CI launch and screenshot coverage

## Campaign delivered by each game

| Game | Beta campaign |
|---|---|
| DEEPSCAN | Three seeded contract classes, four to six specimens, black-box recovery, pressure/noise/resources, and ascent |
| SWITCHYARD | Rural, commuter, and freight districts with increasing service volume, speed, trackwork, route locking, delay, and collision failure |
| LAST LIGHT | Seven contacts with five evidence channels, limited generator power, saved-vessel count, and a three-error limit |
| MICROCOLONY | Pond balance, runoff recovery, and vent diversity samples with distinct population targets and limited interventions |
| TEN PACES | Four locations, visible enemy intentions, three-beat simultaneous plans, persistent campaign progress, and escalating opponents |

## Verification commands

```sh
for d in deepscan switchyard last-light microcolony ten-paces; do
  (cd "$d" && ./build.sh && ./build/*_headless -N 100)
done
```

The harness validates terminal completion and a non-silent audio buffer on every
seed. `-L` selects a deliberately careless test player and proves the loss path:

```sh
./build/deepscan_headless -L -N 20
```

CI runs both probes, separately compiles sanitized native binaries, and launches
every Windows executable for ten seconds before taking a screenshot.

## Beyond beta

The production budgets in `GAME-CONCEPTS.md` remain the direction for final
releases. Additional authored scenarios, animation, sampled sound, writing,
balance passes, and accessibility work should spend the remaining disk where
playtesting proves value. Unused bytes are not a virtue, but padding is not a
feature.
