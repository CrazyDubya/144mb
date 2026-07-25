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
