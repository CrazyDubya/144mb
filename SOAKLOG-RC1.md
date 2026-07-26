# RC1 deterministic soak log

Date: 26 July 2026 UTC

These runs use the same native game core, automated player, rendering, state
hash, and synthesized-audio probe as `verify-rc1.sh`. They extend the mandatory
100-seed gate to look for rare campaign failures.

| Game | Start seed | Runs | Result |
|---|---:|---:|---:|
| DEEPSCAN | 1001 | 1,000 | 1,000/1,000 complete |
| SWITCHYARD | 1001 | 1,000 | 1,000/1,000 complete |
| LAST LIGHT | 1001 | 1,000 | 1,000/1,000 complete |
| MICROCOLONY | 1001 | 500 | 500/500 complete |
| TEN PACES | 1 | 10,000 | 10,000/10,000 complete |

The longer TEN PACES soak initially exposed rare morale failures after
ricochets and cover impacts. Speech can now counter intimidation, and aiming
steadies lost morale. The 10,000-seed run above was performed after those
changes.

Representative commands:

```sh
./deepscan/build/deepscan_headless -s 1001 -N 1000
./microcolony/build/microcolony_headless -s 1001 -N 500
./ten-paces/build/ten-paces_headless -s 1 -N 10000
```
