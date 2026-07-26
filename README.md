# 144mb

A collection of games that each fit on a single 1.44MB floppy disk.

Every directory here is a **complete, standalone program**. Nothing is shared
between games: no common library, no shared headers, no build framework. If two
games need the same code, it gets copied. That is deliberate — these are
size-constrained binaries, and a shared library drags in code that any given game
does not need. Each directory must build on its own, with nothing above it.

What *is* shared lives at the root: notes, conventions, and CI.

## The constraint

Written for a contest with one hard rule:

| | bytes |
|---|---|
| 3.5" HD floppy capacity | 1,474,560 |

Measured **after decompression**, so packers buy nothing. The runtime counts
toward the budget, which rules out engines — Unity, Godot, Electron and
PyInstaller all exceed the limit before a line of game code is written.

Entries must be standalone executables. Browser games are prohibited, which also
rules out anything needing DOSBox or an emulator.

Deadline: **4 September 2026**. Judged on completion, size compliance, and
entertainment value — in that order.

## Games

| game | status | size | % of disk |
|---|---|---|---|
| [convoy](convoy/) | v4 complete | 110,592 | 7.50% |
| [deepscan](deepscan/) | beta | 143,360 | 9.72% |
| [switchyard](switchyard/) | beta | 142,848 | 9.69% |
| [last-light](last-light/) | beta | 141,824 | 9.62% |
| [microcolony](microcolony/) | beta | 142,848 | 9.69% |
| [ten-paces](ten-paces/) | beta | 143,872 | 9.76% |

## Building

Each game has its own `build.sh` and needs only [Zig](https://ziglang.org) as a
C cross-compiler. For example:

```sh
cd convoy && ./build.sh
```

It produces a Windows `.exe` and fails the build if the binary exceeds the limit,
so going over is impossible to miss.

The five beta games also produce native headless executables. Running one plays
a deterministic automated session through the real game rules and reports the
terminal state:

```sh
cd deepscan
./build.sh
./build/deepscan_headless
```

Useful environment variables:

- `ZIG=/path/to/zig` — pick a specific compiler
- `ONLY_WIN=1` — skip the native development harness

## Adding a game

Create a directory with a `build.sh` that writes its executable into `build/`.
CI discovers games by looking for `build.sh`, so no workflow changes are needed.
Read [NOTES.md](NOTES.md) first — it records what has already been learned the
hard way.
