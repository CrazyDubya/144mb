# Five-game 1.0 RC1 release notes

This candidate replaces the five small beta prototypes with production-sized,
standalone Windows games. Each executable embeds its own Win32 platform head,
English interface, fixed-timestep simulation, stereo synthesis, authored title,
six RGB565 campaign plates, and three RGB332 narrative plates.

## Candidate contents

| Game | RC1 campaign |
|---|---|
| DEEPSCAN | Three contracts, four submersibles, five descent bands, ascent, twelve contact signatures, pressure/noise/resources, salvage, specimens, abyssal scan, and several debrief outcomes |
| SWITCHYARD | Eight districts, seven service classes, braking and train length, route locking, compatible platforms, connections, incidents, delay, and network grading |
| LAST LIGHT | Seven nights, fourteen contacts, six stations, five generated imitation rules, written evidence, daylight choices, equipment pressure, and consequence endings |
| MICROCOLONY | Six samples, six organism roles, seven interventions, toxins, currents, quarantine, population history, field guide, and stability/extinction rules |
| TEN PACES | Eight encounters, nine planned actions, traveling bullets, ammunition, cover, smoke/light, morale, temperament, surrender, reputation, survivors, and shaped finale |

## Platform changes since beta

- Fixed 60 Hz simulation pacing
- Queued `WM_KEYDOWN` edges so brief presses are not dropped between ticks
- Automatic simulation pause when the game loses focus
- Hold-`H` help/pause and `M` audio mute
- Date-derived daily challenge seeds and persisted mute/completion settings
- Graceful no-audio operation and cleanup after partial WinMM failure
- 36,000-tick headless ceiling for longer campaign probes
- Deterministic malformed/edge-input fuzz probes under both sanitizers
- Release packaging with byte counts, headroom, and SHA-256 manifests

## Candidate limitations

- Actual Windows launch, title transition, keyboard input, continued rendering,
  and screenshot capture passed on Windows Server 2022 in GitHub Actions run
  30229548698 for commit `0a6dc8f`.
- Audio is synthesized rather than performed speech. Every clue and essential
  state is also written in English.
- RC1 intentionally retains roughly 12% disk headroom for defects found during
  target-platform and playtest review.
