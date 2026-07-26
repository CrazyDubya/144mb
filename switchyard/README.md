# SWITCHYARD — 1.0 release candidate 1

Dispatch eight districts from the rural branch through the winter shutdown.
Seven service classes differ in speed, length, urgency, braking, connection
value, and platform needs. Route locking prevents unsafe changes, while weather
incidents, equipment failures, passenger connections, crew delay, and the wrong
platform can turn one hold into network-wide gridlock.

## Controls

- Z: change east–west/north–south priority when the route is unlocked
- X: hold or release all signals
- C: switch between passenger and freight platforms
- V: pause or resume for planning
- Enter: restart with a new timetable seed
- H: hold for objective and controls
- M: toggle audio
- Escape: quit

`./build.sh` creates `build/switchyard.exe` and
`build/switchyard_headless`. The harness accepts `-s`, `-N`, `-t`, and `-L`.
