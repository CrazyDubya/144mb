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

  > **Superseded.** This was tried and reversed. See *Do not mistake "no text"
  > for "universally readable"* below: a game with no words turned out to be a
  > guessing game, and the fix was a 32-glyph uppercase font costing about 300
  > bytes. Read that section before acting on this one.
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

## A market that moves on your trades needs a spread

convoy's markets remember: buying nudges a price up, selling knocks it down.
That mechanic shipped in v1 and was quietly broken the whole time. Buying bumps
the price about 10%, and you can then sell *into your own bump* at the raised
price. Buy at 20, the price becomes 22, sell for 22. Repeat forever.

Nobody noticed because no human would grind a stall a thousand times. The test
bot did it by accident the moment settlement archetypes made speciality goods
cheap enough for its buy rule to fire, and reported 4,141 credits at sector 0 on
day 1.

The fix is the one real markets use: a **bid-ask spread**. A stall pays less
than it charges — here 80% — so the round trip is always a loss and the
mechanic still works for its actual purpose, which is punishing you for dumping
forty units into one town.

**Any economy where the player's own trades move prices needs this check.** Ask
directly: can I buy and immediately resell at a profit? If yes, there is an
infinite money loop in the game whether or not anyone has found it.

## Overlapping buy and sell thresholds oscillate

A related trap, this one in the bot rather than the game. A rule that buys below
86% of average and a rule that sells above 74% of average both fire on the same
good at the same stall, so the agent buys and resells the same unit forever. The
thresholds look well separated until the spread and integer rounding are applied.

Tuning the numbers apart is a patch that will break again the next time either
rule moves. The structural fix is to make the states disjoint instead: record
what was bought at this stop and refuse to sell it here. Then no combination of
thresholds can oscillate.

Both stalls were caught only because the harness caps a run at 4,000 steps and
reports STALLED rather than hanging. Give any autonomous agent a step budget and
a distinct "made no progress" outcome -- an infinite loop that reports nothing
looks exactly like a slow test.

## Keep a sanitizer build one command away

A plain segfault tells you nothing: no file, no line, no reason. Phase 2 of
convoy crashed on every seed the moment upgrades and crew landed, and staring
at the diff produced three wrong theories in a row.

One run under AddressSanitizer and UBSan named it exactly:

    src/world.c:213: index 4 out of bounds for type 'int [4]'
    stack-buffer-overflow in roll_offers
    [32, 48) 'avail' <== Memory access at offset 48 overflows this variable

A scratch array declared `int avail[UPG_COUNT]` (4) was reused for the crew
list (5). One element past the end, straight into the stack.

Notes for doing this here:

- zig cc cannot link its own asan runtime for this target. The system gcc can,
  and the game core is portable C, so building the harness with plain gcc for
  diagnostics costs nothing.
- Pass `-fsanitize=address,undefined` together. UBSan caught the out-of-bounds
  index at the same time and printed it more legibly than ASan did.
- Set `ASAN_OPTIONS=detect_leaks=0`. The harness deliberately never frees its
  arena or framebuffer, and the leak report buries the real finding.

`tools/asan.sh` now does all of that in one command. Run it whenever anything
touches a fixed-size array or an index, not only when something has already
crashed -- a one-past-the-end write that happens to land on padding will not
crash at all, it will just quietly corrupt something later.

## Price every upgrade in the currency the player earns

Two of convoy's four upgrades were quietly worthless when first written.

Water tanks "halved the daily water burn, rounded up". With no crew aboard the
burn is 1, and (1+1)/2 is 1 -- no effect at all, in precisely the situation a
player is most likely to buy them. The scout was sold as "see two sectors
ahead" in a game that already draws the entire route on screen: a purchasable
nothing.

Neither was a crash, a stall, or a bad win rate. Every sweep was green.

Before shipping any upgrade, do the arithmetic in the player's currency:

- What does it cost?
- What does it save or earn, over the run that remains?
- Is that more than the cost?

convoy's tuned engine saves roughly 4 fuel over 13 hops, about 80 credits, and
was priced at 175. That is not a weak upgrade, it is a trap -- and the bot
proved it by buying them and *losing more often*: the win rate fell nine points
the moment kit became affordable enough to purchase.

A useful check: watch what the agent spends, not just whether it wins. An
option nobody takes and an option that loses money look identical in a win-rate
column.

## Verify the change is in the build before believing the number

Two consecutive convoy sweeps returned byte-identical results: 46 won, 104
died, 22 purchases, twice. Identical numbers from what should have been
different code is not a coincidence, it is a message.

`src/bot.c` did not contain the change. A scripted edit of the form

    s = open(path).read()
    s = s.replace(OLD, NEW)      # OLD did not match -- silently no-op
    open(path, 'w').write(s)

writes the file back unchanged when the pattern misses. Nothing fails, the
build succeeds, the sweep runs, and the number that comes out describes the
*old* code while appearing to describe the new.

Both sweeps had already been reasoned about and written up before the problem
surfaced. Those conclusions were worthless.

Guards, in order of preference:

1. **Use an editing tool that errors on a failed match.** `Edit` refuses when
   `old_string` is absent. A `.replace()` in a script does not.
2. **Grep for the new code before building**, not after reasoning about the
   result: `grep -c upgrade_payback src/bot.c`.
3. **Treat identical results across a change as a bug report.** Balance numbers
   are noisy; exact repetition means the input did not change.

The same applies to any generated artefact -- if a build, a config or a
migration "had no effect", check that it was actually applied before drawing
conclusions about what it means.

## Report what the agent did, not just whether it won

Four separate problems in convoy hid behind a healthy win rate:

- upgrades priced beyond anything a run ever banks, so never bought
- two upgrades whose effects were arithmetically zero
- crew whose keep exceeded anything they saved
- kit that was bought and *lost* runs

Every one of these produced clean sweeps. Zero crashes, zero stalls, win rate
in band. The game was fine; a third of its systems were furniture.

The cheapest fix is to make the harness print what the run ended up owning:

    BOT seed=16 WON sector=13 day=14 credits=18 cargo=6 upg=2 crew=1 steps=134

Two extra integers. With them, "nobody ever buys this" and "buying this loses"
are visible at a glance instead of requiring a bespoke investigation each time.

The general rule: **an option nobody takes and an option that loses are
indistinguishable in an outcome metric.** If a system can be engaged with,
instrument the engagement, not just the result.

## When two runs match exactly, suspect the harness before the feature

convoy's new mood system reported identical audio for a settlement and for the
open road. Identical numbers across a change had already been documented here
as a bug report rather than a coincidence, so the first suspicion was a silent
edit failure -- the previous cause.

It was not. The code was correct and the *test* was wrong: the scripts never
dismissed the three-panel opening cut scene, and while a cut scene owns the
screen `game_update` returns before it reaches the music. Both runs were still
playing the title theme, correctly.

What resolved it was a packed probe returning several pieces of state at once:

    return cut_running * 1000 + title * 100 + mood * 10 + mood_next;

`1000` said the cut scene was still up, which no amount of staring at the mood
code would have revealed. One integer, four facts, and the answer immediately.

Two things worth keeping:

- **A probe that returns one value answers one question.** When a guess has
  already been wrong twice, pack the whole state path into a single number and
  read it once rather than adding prints one at a time.
- **A test that drives a UI has to account for modal screens.** Idle ticks do
  not dismiss a dialog; only input does. Any scripted harness will eventually
  hit this, and it looks exactly like a broken feature.

---

## Content the optimal route is paid to skip

convoy's encounters were built as encounter *nodes* on a branching map. The
generator made 30% of nodes encounters, which should have produced about 3.6 of
them per fourteen-sector run. Instrumenting a 200-run sweep gave **1.76**, and
43 runs -- including 19 of the 84 that *won* -- met nobody at all.

Nothing was broken. Money is made in settlements, so the route that pays is the
route that skips the story. Every portrait, dialogue line and journal entry was
content a player was rewarded for avoiding.

The structural fix was to stop making the two compete: a settlement arrival now
also has a chance of carrying an encounter, returning to the market afterwards.
Runs meeting nobody fell from 43 to 7.

Two general lessons:

- **If content sits on one branch of a choice and reward sits on the other, the
  content does not exist for anyone playing well.** Frequency knobs will not fix
  this; the placement is the bug.
- **Count the content, not just the outcome.** A win-rate column cannot show
  that a fifth of winning runs never saw the cast. The counter that found it
  (`met=`) took five lines and should have been added the same day the feature
  was.

## Adding content changed the win rate, which was the real finding

Raising encounter frequency dropped the win rate from 42% to 30%. An
even-money encounter table would have added *variance* without moving the mean;
moving the mean twelve points proved the encounters were net-negative EV -- a
tax dressed up as a gamble. **If more of an optional system makes the game
harder, that system is not optional.**

## Distinctness has to be checked, not hoped for

Five characters' faces were generated from one seed each, with skin, clothing,
hat, beard and scar read from separate bit fields. Evenly spaced seeds gave
three near-identical faces; replacing them with a hash gave three different
near-identical faces. Both attempts assumed scattering the input scatters every
output field, which nothing guarantees.

Fixed by searching for five seeds that produce deliberately chosen combinations
and hard-coding them. **With a handful of something and disk space to spare,
pick them; do not roll for them.** Procedural generation earns its keep at
scale, not at five.

## A green result from a build that failed

A scripted `.replace()` silently missed its pattern (`int  world_event_char`
had two spaces, the pattern had one) so a header declaration never landed and
the build failed. The next command in the chain ran the sanitizer suite, which
reported **"sanitizers clean"** -- against the previous binary, still sitting on
disk. The same shape appeared again when a 200-seed sweep ran to completion
after a failed rebuild and produced a full, plausible, entirely stale table.

**A build failure must stop everything downstream of it.** Chain build and test
with `&&`, and treat any pass that arrives suspiciously fast, or any result
identical to the previous run, as a stale-binary report until proven otherwise.

## Content that cannot be reached by arithmetic, not by luck

convoy had an ending for arriving with none of the cargo the run was about. It
never once occurred -- not in 200 runs, not in 800, on any difficulty. The
instinct was to run more seeds.

The answer came from multiplying instead. Raiders demanded the cargo past a
certain point at a flat 45% for one or two crates; encounters average 2.75 per
run, about two fall past that point, and three of fourteen encounter kinds
search the hold. That is 2 x (3/14) x 0.45 = 0.19 demands per run at 1.5 crates
each -- **0.29 crates lost per run against six needed.** No sample size reaches
that. The ending was decoration and a thousand more runs would only have
confirmed it more slowly.

**Before sampling harder for a rare event, multiply out its rate.** If the
expected count per run is two orders of magnitude below what the event needs,
the sweep is not underpowered, the content is unreachable.

The fix was to make the threat escalate with progress rather than stay flat, so
late refusals cost several crates. Two things made it safe: it changed nothing
for players who pay (verified: win rates identical before and after), and it
was demonstrated with a dedicated probe rather than hoped for.

## A probe is not a strategy

Proving the ending reachable needed a player who refuses every offer. That is
not a good player -- it wins less often than the ordinary bot -- and it was
added as a separate switch rather than as bot behaviour.

Keeping them separate matters: the ordinary bot answers "is this balanced?" and
the probe answers "can this be reached at all?". Merging them would have made
the balance numbers meaningless and the reachability answer unconvincing.
**Test players should be as specialised as test cases.**

## A modal screen that eats input hides itself

convoy's opening cut scene ran before the title screen and again immediately
after, because the function that builds a world for the title to sit behind
also starts the opening. It shipped that way for three phases. Nobody noticed:
the automated player presses through everything, and no human had played it.

It surfaced only when a new title menu appeared not to respond. The cut scene
was swallowing the keys. **The symptom of a modal screen consuming input never
appears on the modal screen -- it appears on whatever was supposed to receive
that input, which looks broken instead.** When a newly added control does
nothing, suspect something upstream is still running.

## Difficulty should change what kills you

Three difficulty settings could have been one multiplier. Instead each one
leans on a different failure, and the claim was checked rather than asserted:
easy and normal are thirst-led (61% and 54% of deaths), hard inverts to
fuel-led (61%), because settlement density drops faster than the water margin
does.

Of the six numbers per difficulty, **resupply density moved the win rate more
than the other five combined**. Starting stock is what a player notices; how
often they can restock is what actually decides the run.

---

## Split your generator before you tune anything

convoy drew map layout, market offers, contracts, encounters and equipment
failure from a single RNG stream. Adding one die roll to an encounter table
therefore reshuffled every later offer and contract for that seed: the seed
stopped being the same run the moment any table was edited, so paired
before/after comparison was never valid. Worse, salvaged kit rolled for failure
only when it was fitted — so the convoy's own purchase decision moved the
stream and re-rolled its own encounters.

Split into three streams, an encounter-table edit now leaves every map hash
untouched across 100 seeds on each difficulty while the win rate moves. That
was verified by deliberately inserting an extra draw and checking.

**Do this in the first hour of any project with a seed and a tuning loop.** It
costs eight bytes of state and it is the difference between a comparison that
means something and one that does not.

## A price derived from a payback makes every test of it a tautology

convoy priced crew at 45% of what a crew member was calculated to return. The
bot's hiring rule was `payback > price`. That reduces to `p > 0.45p` — true for
every positive p, at any price, however wrong the payback figure happened to
be. It was wrong by a factor of ten.

The rule that fixes it: **an agent may take facts from the system it measures,
never valuations.** Prices, rates, capacities, what is reachable — all fine.
What something is *worth* has to be estimated independently, from observable
quantities, or the agent cannot disagree with the system and the disagreement
is the entire measurement.

## Freeze a reference agent before you improve the current one

When both the game and the agent that measures it are changing, a moved number
is unattributable. Copy the agent, freeze the copy, run both over the same
seeds every time. When the agent changes the frozen one holds the game fixed;
when the game changes they move together.

The frozen copy does not need to be good — convoy's plays badly and lies to
itself. It needs to be *constant*.

## A win-rate sweep cannot resolve a rare mechanic

An encounter kind that fires a quarter of a time per run contributes far less
than the standard error of any feasible sweep. Tuning one by watching the win
rate produces a confident wrong answer.

What worked was counting engagement directly: how often each kind came up, how
often it was taken, and — the column that mattered most — how often it was
refused because it *could not be paid* rather than because it was a bad deal.
Those two are the same keypress and mean opposite things. Seven of fourteen
kinds turned out to be non-decisions, and the largest single fix was in the
agent rather than the tables.

## Four tools reported success while doing nothing

In one release: a sync script that copied a tree to itself; the guard added to
fix it, defeated by piping its output so the shell saw `tail`'s exit status; a
harness flag that set a value on the world built for the title screen, which
the real run then rebuilt from scratch; and a probe script whose `sed` produced
a syntax error, built into `/dev/null`, and reported the same number for five
different configurations.

Every one produced a plausible result. Three were caught only because a number
that should have differed came back **exactly** identical.

- **Identical is a red flag, not a relief.** Two configurations agreeing to the
  unit is a defect report.
- **Never silence a build.** `>/dev/null` on a compile turns every later
  measurement into a report on the previous binary.
- **Never pipe a check whose exit status you depend on.**
- **When a guard has been defeated twice, move it.** The fix that finally held
  was a pre-commit hook comparing the shipped tree against the tested one —
  somewhere it cannot be piped, skipped, or run from the wrong directory.
