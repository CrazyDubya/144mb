# SWITCHYARD — beta

Dispatch a three-district campaign through the rural branch, commuter junction,
and freight corridor. Each district increases service volume and speed while
adding visible trackwork. Route locking prevents changing priority beneath a
committed train; collisions and accumulated delay still end the campaign.

- Z: change priority
- X: hold or release every signal
- H: hold for objective and controls; pauses play
- Enter: restart
- Escape: quit

Build with `./build.sh`. The Windows game includes relay/train synthesis and an
English start/help screen. Run `./build/switchyard_headless -N 100` for a seeded
completion sweep.
