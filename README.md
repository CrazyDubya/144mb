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
| [convoy](convoy/) | v3 complete | 108,544 | 7.36% |

## Building

Each game has its own `build.sh` and needs only [Zig](https://ziglang.org) as a
C cross-compiler:

```sh
cd convoy && ./build.sh
```

It produces a Windows `.exe` and fails the build if the binary exceeds the limit,
so going over is impossible to miss.

Useful environment variables:

- `ZIG=/path/to/zig` — pick a specific compiler
- `ONLY_WIN=1` — skip the native development harness

## Adding a game

Create a directory with a `build.sh` that writes its executable into `build/`.
CI discovers games by looking for `build.sh`, so no workflow changes are needed.
Read [NOTES.md](NOTES.md) first — it records what has already been learned the
hard way.
