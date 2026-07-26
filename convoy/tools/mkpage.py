#!/usr/bin/env python3
"""Builds the progress page, embedding all media as data URIs.

The Artifact CSP blocks every external host, so nothing may be referenced by
URL -- the GIF and stills are inlined base64.
"""
import base64
import os

MEDIA = "media"
OUT = "docs/progress.html"

# Read from the binary rather than typed in. It was hard-coded, so the
# published page silently misreported the size after every build that changed
# it -- which is every build worth publishing.
def _measure():
    import os
    for c in ("build/convoy.exe", "../build/convoy.exe",
              "/home/opc/convoy/build/convoy.exe"):
        if os.path.exists(c):
            return os.path.getsize(c)
    raise SystemExit("mkpage: cannot find convoy.exe to measure")

BYTES_USED = _measure()
LIMIT = 1474560


def uri(path, mime):
    with open(path, "rb") as f:
        return "data:%s;base64,%s" % (mime, base64.b64encode(f.read()).decode())


STILLS = [
    ("opening.png", "The reason",
     "The last greenhouse in the west burned in the spring. What survived fits in six "
     "crates, and the Green Zone has soil and water and nothing to plant. You cannot "
     "sell the seed &mdash; not for fuel, not for water, not to save your own life."),
    ("title.png", "The title",
     "Two choices before a run: how forgiving the road is, and whether to play a random "
     "map or today's. The daily map is the same for everyone, and the run ends with a "
     "score, so two people can argue about the same fourteen sectors. Difficulty does "
     "not scale one number &mdash; on the easy setting thirst is what kills you, on the "
     "hard one it is fuel."),
    ("dawn.png", "First light",
     "Every run is one day. You load the crates at dawn, and the sky is the clock: "
     "there is no timer on screen and none is needed."),
    ("midday.png", "The road",
     "Fourteen sectors, one way. Settlement badges name what each place trades, so a "
     "road can be read before fuel is spent reaching it."),
    ("night.png", "Late light",
     "The sky is the only clock, and by the far sectors it has gone. A dark town also "
     "has less on offer &mdash; the forecourt is shut more often and the job board is "
     "thinner &mdash; which bites hardest exactly where you can least afford it."),
    ("dialogue.png", "Somebody on the road",
     "Five people recur across a run, and they remember you. Pay what is asked and "
     "the next meeting is warmer and cheaper; wave them on and it is neither. The "
     "portraits are assembled from shapes at draw time &mdash; there is no image file "
     "anywhere in the binary."),
    ("journal.png", "Who you have met",
     "The PEOPLE tab is the run's memory: who turned up, how often, and where you "
     "stand with them. Encounters used to live only on encounter nodes, which meant "
     "the profitable route was the one that skipped the story &mdash; a fifth of "
     "winning runs met nobody at all. Now they find you in the market too."),
    ("end.png", "The reckoning",
     "Five endings, and which one you get depends on how much of the seed is still "
     "aboard. Arriving with none of it was, for three phases, an ending nobody could "
     "reach: the arithmetic came to 0.29 crates lost per run against six needed. "
     "Raiders now want more the deeper you are, and it costs nothing to a player who "
     "pays them."),
    ("ending.png", "Arrival",
     "Five endings, graded on what survived rather than on whether you arrived. "
     "Storms spoil seed and raiders take it, so reaching the Green Zone and "
     "succeeding are not the same thing."),
]

# Every released binary, so the bar can show growth rather than one number.
# Sizes taken from each release's own README at its tag.
VERSIONS = [
    ("v1", 82944,  "the shape of a game"),
    ("v2", 94208,  "an economy with reasons"),
    ("v3", 108544, "a payload, characters, difficulty"),
    ("v4", 110592, "numbers that can be trusted"),
]

FACTS = [
    ("14", "encounter kinds, all live choices"),
    ("6", "crates you cannot sell"),
    ("0", "bytes of stored art or audio"),
    ("7.5%", "of the floppy used"),
]


def main():
    os.makedirs("docs", exist_ok=True)
    gif = uri(os.path.join(MEDIA, "run.gif"), "image/gif")
    pct = BYTES_USED * 100.0 / LIMIT

    win = uri(os.path.join(MEDIA, "windows.png"), "image/png")

    stills_html = []
    for fn, title, caption in STILLS:
        src = uri(os.path.join(MEDIA, fn), "image/png")
        stills_html.append(
            '<figure class="shot">\n'
            '  <img src="%s" alt="%s" />\n'
            '  <figcaption><b>%s</b>%s</figcaption>\n'
            '</figure>' % (src, title, title, caption))

    # Two bars, deliberately.
    #
    # The top one is true to scale: every band is the bytes that release added,
    # as a fraction of the whole disk. At this size the later versions are
    # slivers -- which is the honest picture and the entire point of the
    # project, so it must not be exaggerated. An earlier version floored each
    # band to a visible minimum and the total then read 8.4% against an actual
    # 7.50%, overstating the one number the page exists to report.
    #
    # The second bar expands the used portion to full width so the growth
    # between releases is legible. Same data, stated scale, no distortion of
    # the first.
    COLOURS = ["#6b7f52", "#8a6a3e", "#a4553a", "#c07a3e"]
    bands, zoom, vkey, prev = [], [], [], 0
    for i, (name, size, blurb) in enumerate(VERSIONS):
        grew = size - prev
        bands.append('<i style="width:%.4f%%;background:%s" title="%s +%s bytes"></i>'
                     % (grew * 100.0 / LIMIT, COLOURS[i], name, "{:,}".format(grew)))
        zoom.append('<i style="width:%.3f%%;background:%s"></i>'
                    % (grew * 100.0 / BYTES_USED, COLOURS[i]))
        vkey.append(
            '<div><em style="background:%s"></em>'
            '<b>%s</b> %s<s> &middot; %s bytes%s</s></div>'
            % (COLOURS[i], name, blurb, "{:,}".format(size),
               "" if i == 0 else " &nbsp;+%s" % "{:,}".format(grew)))
        prev = size
    bands_html = "".join(bands)
    zoom_html  = "".join(zoom)
    vkey_html  = "\n".join(vkey)

    facts_html = "\n".join(
        '<div class="fact"><span class="fig">%s</span><span class="lbl">%s</span></div>'
        % (n, l) for n, l in FACTS)

    html = TEMPLATE % {
        "gif": gif,
        "used": "{:,}".format(BYTES_USED),
        "limit": "{:,}".format(LIMIT),
        "left": "{:,}".format(LIMIT - BYTES_USED),
        "pct": "%.2f" % pct,
        "barpct": "%.3f" % max(pct, 0.45),   # keep the sliver visible
        "bands": bands_html,
        "zoom": zoom_html,
        "vkey": vkey_html,
        "stills": "\n".join(stills_html),
        "facts": facts_html,
        "win": win,
    }
    with open(OUT, "w") as f:
        f.write(html)
    print("wrote %s (%.0f KB)" % (OUT, os.path.getsize(OUT) / 1024.0))


TEMPLATE = r"""<title>Convoy &mdash; a game that fits on a floppy disk</title>
<style>
  :root {
    --ink:    #1A1410;
    --panel:  #2B231A;
    --bone:   #E8DCC0;
    --rust:   #A84B20;
    --fuel:   #D9862B;
    --water:  #4FA8C9;
    --good:   #6FA84B;

    --ground: #F2E9D6;
    --surface:#E7DBC2;
    --text:   #241C14;
    --muted:  #6E5C42;
    --rule:   #CBBB9A;
    --accent: var(--rust);
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --ground: #14100C;
      --surface:#221B14;
      --text:   #E8DCC0;
      --muted:  #9A8A6E;
      --rule:   #3A2F22;
      --accent: #C9662F;
    }
  }
  :root[data-theme="dark"] {
    --ground: #14100C; --surface:#221B14; --text:#E8DCC0;
    --muted:#9A8A6E; --rule:#3A2F22; --accent:#C9662F;
  }
  :root[data-theme="light"] {
    --ground: #F2E9D6; --surface:#E7DBC2; --text:#241C14;
    --muted:#6E5C42; --rule:#CBBB9A; --accent:#A84B20;
  }

  * { box-sizing: border-box; }
  body {
    margin: 0;
    background: var(--ground);
    color: var(--text);
    font-family: Georgia, "Iowan Old Style", "Times New Roman", serif;
    font-size: 17px;
    line-height: 1.65;
    -webkit-font-smoothing: antialiased;
  }
  .wrap { max-width: 860px; margin: 0 auto; padding: 0 24px 96px; }

  .mono, .eyebrow, .fig, .lbl, .bar-nums, table {
    font-family: ui-monospace, "SF Mono", "Cascadia Mono", Menlo, Consolas, monospace;
    font-variant-numeric: tabular-nums;
  }
  .eyebrow {
    font-size: 11px; text-transform: uppercase; letter-spacing: .18em;
    color: var(--muted);
  }

  header { padding: 72px 0 40px; border-bottom: 1px solid var(--rule); }
  h1 {
    font-size: clamp(38px, 7vw, 64px); line-height: 1.02; margin: 14px 0 0;
    letter-spacing: -.02em; text-wrap: balance; font-weight: 400;
  }
  h1 em { font-style: normal; color: var(--accent); }
  .stand { max-width: 62ch; color: var(--muted); margin: 20px 0 0; font-size: 18px; }

  /* The byte budget is the thesis of the whole project, so it leads. */
  .budget { margin: 44px 0 0; }
  .bar-nums {
    display: flex; justify-content: space-between; align-items: baseline;
    font-size: 12px; color: var(--muted); margin-bottom: 10px;
  }
  .bar-nums b { color: var(--text); font-size: 22px; font-weight: 600; }
  .bar {
    height: 26px; background: var(--surface);
    border: 1px solid var(--rule); display: flex; overflow: hidden;
  }
  .bar i { display: block; height: 100%%; }
  .bar i.free { flex: 1 1 auto; background: transparent; }
  .bar-foot { font-size: 12px; color: var(--muted); margin-top: 8px; }

  /* Each release is a band, so the bar reads as growth rather than a total.
     At this scale every version is a sliver, which is the point -- but a
     sliver nobody can see is not evidence, so each has a minimum width. */
  .zoomlab {
    font-size: 11px; color: var(--muted); margin: 18px 0 6px;
    text-transform: uppercase; letter-spacing: .14em;
    font-family: ui-monospace, Menlo, Consolas, monospace;
  }
  .bar.zoom { height: 16px; }

  .vkey {
    display: flex; flex-wrap: wrap; gap: 6px 18px; margin-top: 12px;
    font-size: 12px; color: var(--muted);
    font-family: ui-monospace, Menlo, Consolas, monospace;
  }
  .vkey div { display: flex; align-items: center; gap: 7px; }
  .vkey em {
    width: 11px; height: 11px; border-radius: 2px; flex: none; font-style: normal;
  }
  .vkey b { color: var(--text); font-weight: 600; }
  .vkey s { color: var(--muted); text-decoration: none; opacity: .75; }

  h2 {
    font-size: 13px; text-transform: uppercase; letter-spacing: .18em;
    font-family: ui-monospace, Menlo, Consolas, monospace; font-weight: 600;
    color: var(--accent); margin: 72px 0 6px; padding-bottom: 10px;
    border-bottom: 1px solid var(--rule);
  }
  p { max-width: 65ch; }

  .run { margin: 26px 0 0; }
  .run img {
    display: block; width: 100%%; height: auto; image-rendering: pixelated;
    border: 1px solid var(--rule); background: #000;
  }
  .cap { font-size: 13px; color: var(--muted); margin-top: 10px; max-width: 65ch; }

  .shots { display: grid; gap: 40px; margin-top: 28px; }
  .shot { margin: 0; }
  .shot img {
    display: block; width: 100%%; height: auto; image-rendering: pixelated;
    border: 1px solid var(--rule); background: #000;
  }
  .shot figcaption {
    font-size: 14.5px; color: var(--muted); margin-top: 12px; max-width: 65ch;
  }
  .shot b {
    display: block; color: var(--text); font-weight: 600;
    font-family: ui-monospace, Menlo, Consolas, monospace;
    font-size: 12px; text-transform: uppercase; letter-spacing: .14em;
    margin-bottom: 5px;
  }

  .facts {
    display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 1px; background: var(--rule); border: 1px solid var(--rule);
    margin-top: 28px;
  }
  .fact { background: var(--ground); padding: 20px 18px; }
  .fig { display: block; font-size: 30px; color: var(--accent); line-height: 1; }
  .lbl {
    display: block; font-size: 11px; color: var(--muted); margin-top: 8px;
    text-transform: uppercase; letter-spacing: .1em; line-height: 1.5;
  }

  .tbl { overflow-x: auto; margin-top: 24px; }
  table { border-collapse: collapse; width: 100%%; font-size: 13.5px; }
  th, td { text-align: left; padding: 11px 14px; border-bottom: 1px solid var(--rule); }
  th { color: var(--muted); font-weight: 600; font-size: 11px;
       text-transform: uppercase; letter-spacing: .12em; }
  td.n { text-align: right; }
  .yes { color: var(--good); }
  .no  { color: var(--accent); }

  footer {
    margin-top: 80px; padding-top: 24px; border-top: 1px solid var(--rule);
    font-size: 13px; color: var(--muted);
  }
  @media (prefers-reduced-motion: reduce) { * { animation: none !important; } }
</style>

<div class="wrap">
  <header>
    <div class="eyebrow">Work in progress &middot; contest deadline 4 September 2026</div>
    <h1>A game that fits on a <em>floppy disk</em></h1>
    <p class="stand">
      Convoy is a post-apocalyptic trading roguelike built for a contest with one
      rule: the whole thing, decompressed, must fit in 1,474,560 bytes. It is
      written in C with no engine, no libraries and no runtime &mdash; the window,
      the renderer and the synthesiser are all part of the program.
    </p>

    <div class="budget">
      <div class="bar-nums">
        <span><b>%(used)s</b> bytes used</span>
        <span>%(pct)s%% of the disk</span>
      </div>
      <div class="bar">%(bands)s<i class="free"></i></div>
      <div class="bar-foot">%(left)s bytes still free of %(limit)s</div>

      <div class="zoomlab">the used sliver, expanded &mdash; growth by release</div>
      <div class="bar zoom">%(zoom)s</div>
      <div class="vkey">%(vkey)s</div>
    </div>
  </header>

  <h2>One complete run</h2>
  <p>
    One seed, played end to end by the test bot rather than a fixed script, so every
    trade is a response to an actual price. Watch the sky: the run begins at first
    light and finishes after dark, which is the only clock the game has.
  </p>
  <div class="run">
    <img src="%(gif)s" alt="Animated playthrough of Convoy" />
    <p class="cap">
      Seed 16, start to finish, every third decision. The game is turn-based, so
      this is one frame per move rather than a video &mdash; roughly what five
      minutes at the keyboard looks like.
    </p>
  </div>

  <h2>Proof it runs on Windows</h2>
  <p>
    Everything above was rendered by a Linux build on a machine with no display
    that cannot execute a Windows binary at all. So until this screenshot existed,
    the submission target had never run anywhere &mdash; the riskiest kind of
    untested code, since it is the only version that will ever be judged.
  </p>
  <div class="run">
    <img src="%(win)s" alt="Convoy running in a window on Windows Server 2025" />
    <p class="cap">
      Captured automatically on a GitHub Actions <span class="mono">windows-latest</span>
      runner on every push: the build launches, survives ten seconds of the real
      message loop, and screenshots itself. The pixels are identical to the Linux
      renders. The runner has no sound card, so the audio layer detected the
      missing device and disabled itself instead of failing to start &mdash; the
      exact failure that would otherwise surface first on a judge's machine.
    </p>
  </div>

  <h2>What a player sees</h2>
  <p>
    This shipped at first with no text whatsoever &mdash; icons, colours and
    numerals only, on the theory that pure iconography reads the same in every
    language. It did not survive being looked at by someone who had not written
    it. Nothing said the droplet was <em>water</em> rather than coolant, that the
    number beside it was a <em>price</em> rather than a count, or that
    &ldquo;&minus;meds&times;1 / &minus;water&times;2&rdquo; was a choice between
    paying a cost and taking a consequence. That is not minimalism; it is a puzzle
    wrapped around the game, solvable only by dying repeatedly.
  </p>
  <p>
    The icons stayed &mdash; they carry meaning at a glance and survive
    translation. Words were added so the glance is not a guess. The whole
    uppercase alphabet is 32 glyphs and about 300 bytes, or two hundredths of one
    percent of the disk.
  </p>
  <div class="shots">
%(stills)s
  </div>

  <h2>By the numbers</h2>
  <div class="facts">
%(facts)s
  </div>

  <h2>The release where the numbers stopped lying</h2>
  <p>
    Convoy shipped three versions steered by a win rate. Version four began by
    auditing that number and found it was measuring the wrong thing.
  </p>
  <p>
    The test bot &mdash; the only instrument the game had &mdash; sampled market
    prices <b>once per keypress</b> rather than once per market, so its running
    average was weighted by how long it happened to loiter in each shop. It sized
    its water reserve from a single day&rsquo;s ration, which alternates, so the
    reserve collapsed to two units whenever the parity fell wrong and the convoy
    thirsted to death carrying money. And its rule for buying crew was
    <code>payback &gt; price</code>, where the price is defined as 45%% of that
    same payback &mdash; <code>p &gt; 0.45p</code>, true for every positive value,
    at any price, however wrong the number behind it was.
  </p>
  <p>
    Fixing those three things made the bot play <b>twenty points better on a game
    that had not changed</b>. Three releases of recorded difficulty had been
    measuring the instrument&rsquo;s handicap. Everything downstream &mdash; the
    encounter tables, the crew economy, the difficulty curve &mdash; had been
    tuned against it.
  </p>

  <h2>What measurement found once it could be trusted</h2>
  <div class="tbl">
    <table>
      <tr><th>Looked like</th><th>Actually was</th></tr>
      <tr><td>Seven of fourteen encounters were dull</td>
          <td>The bot vetoed any fuel or water payment before pricing the deal.
              Removing the veto turned five dead encounters into real decisions
              with <b>no change to the game</b>.</td></tr>
      <tr><td>Plate armour was the priciest fitting</td>
          <td>It gave <b>zero</b> protection past sector four &mdash; the payload
              demand overwrote its effect three lines later &mdash; and measured
              as the least valuable of the four.</td></tr>
      <tr><td>The fuel economiser was a luxury</td>
          <td>Worth <b>+42 points</b>. The bot bought it a third of the time,
              blocked by a working-capital rule written when kit was overpriced.</td></tr>
      <tr><td>Breakdowns were refused as bad deals</td>
          <td>60%% of them <b>could not be paid</b>. Repairs cost scrap; scrap is
              the cheapest good, so it had all been sold. A refusal and an
              inability are the same keypress and mean opposite things.</td></tr>
      <tr><td>Nobody hired crew because they were expensive</td>
          <td>Every role is negative <b>even when granted free</b>. A specialist
              covers three of fourteen encounter kinds &mdash; it fires 0.8 times
              a run. That is a design problem, not a price.</td></tr>
      <tr><td>Difficulties failed in different ways</td>
          <td>They did, until the bot&rsquo;s water bug was fixed. The asymmetry
              was an artifact of the observer. Recorded rather than recreated.</td></tr>
    </table>
  </div>

  <h2>How it is measured</h2>
  <p>
    The game core makes no OS calls, so it runs headless at a fixed timestep with
    a bot pressing the same keys a player would. Version four rebuilt that rig
    into something that can be trusted:
  </p>
  <div class="tbl">
    <table>
      <tr><th>Instrument</th><th>What it answers</th></tr>
      <tr><td>Three separate RNG streams</td>
          <td>Everything drew from one, so editing an encounter table reshuffled
              every later market offer &mdash; a seed stopped being the same run
              the moment anything was tuned. Now a table edit leaves every map
              identical while the win rate moves.</td></tr>
      <tr><td>In-process sweeps</td>
          <td>Every sweep used to re-launch the binary per seed, which is how one
              straddles a rebuild and reports half of each. Now one process,
              byte-identical to the old way.</td></tr>
      <tr><td>A frozen reference agent</td>
          <td>When both the game and its observer change, a moved number is
              unattributable. The v4-entry bot is kept verbatim to hold one side
              still.</td></tr>
      <tr><td>Engagement counters</td>
          <td>Per encounter kind: taken, refused, and <i>could not afford</i>.
              An option nobody takes and an option nobody can take are identical
              in a win-rate column.</td></tr>
      <tr><td>Forced-policy A/Bs</td>
          <td>Grant a fitting free and measure. Settles &ldquo;is this worth
              buying&rdquo; without asking the bot&rsquo;s opinion of it.</td></tr>
      <tr><td>Determinism and exploit probes</td>
          <td>Each seed replayed and hashed step by step; every good swept at
              every price for a profitable round trip. Both run every phase.</td></tr>
      <tr><td>A 50&times; faster sweep</td>
          <td>A balance run never looks at a pixel, but the core drew a full
              frame twice a step. Shrinking the drawn area cut a phase gate from
              fifteen minutes to sixteen seconds &mdash; and the results are
              byte-identical, which proves rendering cannot influence a balance
              number.</td></tr>
    </table>
  </div>

  <h2>What is proven, and what is not</h2>
  <p>
    Every row below was verified by tracing the simulation, measuring rendered
    output, or running the binary &mdash; not by assuming. What remains is
    tuning, which needs a human playing rather than a bot.
  </p>
  <div class="tbl">
    <table>
      <tr><th>Claim</th><th>How it was checked</th><th class="n">Result</th></tr>
      <tr><td>The economy is not optional</td>
          <td>Fixed key sequences that cannot read a price, 60 seeds &times; 3</td>
          <td class="n yes">0%% win</td></tr>
      <tr><td>Skill is rewarded, and calibrated</td>
          <td>A price-aware bot playing the real UI, 1,000 seeds per difficulty</td>
          <td class="n yes">61 / 47 / 27</td></tr>
      <tr><td>Every encounter is a real decision</td>
          <td>Per-kind accept, refuse and <i>forced</i> rates, 400 seeds</td>
          <td class="n yes">14 of 14</td></tr>
      <tr><td>Every fitting is worth considering</td>
          <td>Take rate per fitting, plus forced-policy A/Bs granting each free</td>
          <td class="n yes">4 of 4</td></tr>
      <tr><td>Crew are worth hiring</td>
          <td>Same A/B: every role is negative even when granted free</td>
          <td class="n no">0 of 5</td></tr>
      <tr><td>All five endings are reachable</td>
          <td>Outcome recorded across 200 runs of a refuse-everything probe</td>
          <td class="n yes">5 of 5</td></tr>
      <tr><td>No profitable market round trip</td>
          <td>Every good at every price 1&ndash;200, with and without the trader</td>
          <td class="n yes">+0 best</td></tr>
      <tr><td>The same seed replays identically</td>
          <td>State hashed every step, each seed run twice, 150 &times; 3</td>
          <td class="n yes">identical</td></tr>
      <tr><td>No crashes, deadlocks or memory errors</td>
          <td>3,000 seeds played out, plus AddressSanitizer and UBSan on both agents</td>
          <td class="n yes">0</td></tr>
      <tr><td>Audio never clips</td>
          <td>Peak, DC and per-row RMS measured against the pattern tables</td>
          <td class="n yes">&minus;3.1 dB</td></tr>
      <tr><td>Runs on Windows</td>
          <td>Built, launched and screenshotted on a windows-latest runner</td>
          <td class="n yes">verified</td></tr>
      <tr><td>Played by a human</td>
          <td>Nobody has actually sat down with it yet</td>
          <td class="n no">not yet</td></tr>
    </table>
  </div>

  <footer>
    Built and rendered on a headless ARM64 Linux machine. Every image here was
    produced by the game itself writing pixels into a buffer, dumped straight to
    disk &mdash; the same code paths the Windows build uses to fill a window.
  </footer>
</div>
"""

if __name__ == "__main__":
    main()
