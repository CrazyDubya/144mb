# Notes

Cross-game findings. Read before starting a new entry; add to it when something
costs you an afternoon.

## The budget is not the hard part

1.44MB is enormous for code and tiny for assets. One uncompressed 320×200
256-colour image is 64,000 bytes — about 22 of them fills the disk. But a
complete Win32 platform layer with a framebuffer blit, keyboard input, frame
pacing and audio costs **under 60KB**.

The practical consequence: **generate content, do not author it.** Palettes from
formulas, worlds from a seed, music from a sequencer. Do that and the floppy
stops being a constraint at all — convoy is a finished-shape game at under 5% of
the disk.

The real constraint is the deadline, not the byte count.

## Toolchain

`zig cc` is the whole toolchain. It is a drop-in C cross-compiler that targets
`x86_64-windows-gnu` with the mingw-w64 headers and CRT bundled, needs no root,
and installs as a single tarball on any host architecture.

This matters because MinGW is **not packaged for aarch64** in Oracle Linux 9,
EPEL, or anywhere else we looked. Zig sidesteps the problem entirely and builds
the host-native development target from the same source tree.

Size flags that matter, measured on a Win32 window with a live framebuffer blit:

| flags | bytes |
|---|---|
| `-O2` | 189,440 |
| `-Os` | 57,856 |
| `-Os -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables` | 57,344 |

`-Os` is worth 3.3× on its own. Do not ship `-O2`.

## Architecture that pays off

Split the game core from the platform layer with a hard boundary: the core is
handed memory, an input struct, a pixel buffer and an audio buffer, and makes no
OS calls whatsoever.

This is not architectural purity — it buys three concrete things:

1. The same source builds a Windows exe and a native binary.
2. A **headless harness** can run the game with scripted input and dump frames as
   PNG, which is the only way to develop when the dev machine has no display and
   cannot execute the target binary.
3. Simulation state can be traced directly, so rules get verified by evidence
   rather than by squinting at screenshots.

Use a fixed timestep and integer maths throughout. Determinism means a seed
reproduces a run exactly, which makes bugs reportable and balance measurable.

## Judging is in Korean

Assume judges read Korean, are working through many entries, and will give any
one game a few minutes. Two consequences:

- **Make it language-free.** Icons, colours, signs and Arabic numerals only. The
  strongest version of this is to ship no alphabetic font at all, which makes
  accidental English impossible rather than merely unlikely. Keycaps (`Z`, `X`,
  `↵`) are fine — they label physical keys, not words.
- **Be legible in 30 seconds.** Anything that takes twenty minutes to become
  interesting will not be seen.

A trap worth naming: an icon with a quantity and no sign is ambiguous. `water ×2`
reads identically as a gain or a loss. Always draw an explicit `−` or `+`.

## Audio

Synthesise it. A drone, a square lead over a step sequencer, an LFSR noise layer
and a handful of one-shot effects cost about **2KB of code and zero bytes of
data**. Integer maths only — no libm, and bit-identical output everywhere.

Two defects that will not be obvious without measuring:

- Mixes come out far too quiet. Check peak against full scale; aim for roughly
  −3 dBFS with headroom for effects stacking on music.
- Envelope decay rates are unintuitive. A constant that sounds plausible can
  decay a note inside 50ms, which is a click rather than a note. Verify by
  measuring per-step RMS against the sequencer table.

**Always degrade gracefully when there is no audio device.** Check
`waveOutGetNumDevs()` and disable audio if the device fails to open. CI runners
have no audio hardware, and neither will some judges' machines. This is exactly
the failure that kills a submission on someone else's computer.

## Testing without the target platform

If you cannot run Windows locally, GitHub Actions `windows-latest` runners are
free and close the loop: build the exe, launch it, confirm it survives, and
screenshot the desktop so there is visual proof from the real platform.

Note that per-second billing does not exist for Windows VMs anywhere — AWS bills
Windows per hour and Azure per minute, because of the licence. The free runner is
the better answer regardless.

## Build a bot that plays, not a script that types

Fixed key sequences are cheap to write and they will lie to you. A script cannot
look at a price and decide, so it measures the floor of your difficulty curve and
nothing else. Ours reported a sensible-looking 27% win rate while the game was in
fact trivial.

Write a bot that reads the world state and drives the real UI — moving the same
cursor and pressing the same keys a player would. Compile it into the test
harness only, so it costs nothing in the shipped binary.

The first run of convoy's bot won **90%** of games. That single number was worth
more than every scripted test combined: it said the game had no difficulty curve
for anyone who understood it. Lengthening the route and thinning the settlements
brought competent play to 53% and careless play to 0%, which is the shape a
roguelike wants.

Balance against the bot, not against the script.

## Do not mistake "no text" for "universally readable"

convoy originally shipped with no alphabetic font at all, reasoning that icons,
colours and Arabic numerals read identically to a Korean judge and an English
one. That reasoning is sound and the conclusion was still wrong.

Read your own screenshots as a stranger would. Nothing said the droplet was
*water* rather than coolant, that the number beside it was a *price* rather than
a quantity, or that `−meds×1 / −water×2` was a choice between paying a cost and
taking a consequence. The result was not elegant minimalism — it was a puzzle
wrapped around the actual game, solvable only by dying repeatedly.

Judging weights entertainment value. A confused judge scores badly no matter how
principled the constraint was.

Keep the icons — they carry meaning at a glance, and they survive translation.
Add words so the glance is not a guess: name every good, label every column,
title every encounter, and ship a **how to play** screen. An uppercase-only 5x7
Latin font is 32 glyphs and about 300 bytes. Put every string in one header so a
translation is a data swap rather than a hunt through the drawing code.

## Provisioning depth is a hidden design constraint

Lengthening convoy's route from 9 hops to 13 broke it in a way no size or
balance number would have shown. The test bot stocked fuel and water for the
whole remaining journey — a sensible-looking rule — which at the start of a
13-hop route came to 29 units against a 30-slot hold. The hold filled with
consumables, leaving no room to trade and no way to earn, and the bot then
pressed BUY forever against a full hold.

Two lessons, one design and one mechanical:

- **The hold size and the route length are the same number.** If a player can
  carry the entire journey's consumables at once, resupply stops mattering and
  the economy is decoration. If they cannot carry any surplus, there is no
  trade. The interesting band is narrow and you have to check you are in it.
- **Any agent that walks a cursor to press a button must check the button will
  do something.** A no-op purchase looks identical to a pending one, and the
  loop never terminates. Guard on affordability *and* capacity before
  navigating, not after arriving.

The fix in both cases is to provision to the next few markets rather than to the
end of the road, which is also how the game reads better: you resupply as you
go.

## Never rebuild while a background sweep is running

Sweep loops re-invoke the binary per seed, so a rebuild halfway through swaps
the thing being measured. One run here reported 63% from a sample that was part
old-bot and part new-bot, which is worse than no number at all because it looks
like a result. Finish the sweep, then rebuild.
