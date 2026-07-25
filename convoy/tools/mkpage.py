#!/usr/bin/env python3
"""Builds the progress page, embedding all media as data URIs.

The Artifact CSP blocks every external host, so nothing may be referenced by
URL -- the GIF and stills are inlined base64.
"""
import base64
import os

MEDIA = "media"
OUT = "docs/progress.html"

BYTES_USED = 104960
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
     "Enter starts a run, H opens the instructions. The convoy drives east toward the "
     "only green thing in the world."),
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
    ("ending.png", "Arrival",
     "Five endings, graded on what survived rather than on whether you arrived. "
     "Storms spoil seed and raiders take it, so reaching the Green Zone and "
     "succeeding are not the same thing."),
]

FACTS = [
    ("6", "crates you cannot sell"),
    ("14", "encounter kinds"),
    ("0", "bytes of stored art or audio"),
    ("7%", "of the floppy used"),
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
    border: 1px solid var(--rule); position: relative; overflow: hidden;
  }
  .bar span {
    position: absolute; inset: 0 auto 0 0; background: var(--accent);
    width: %(barpct)s%%;
  }
  .bar-foot { font-size: 12px; color: var(--muted); margin-top: 8px; }

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
      <div class="bar"><span></span></div>
      <div class="bar-foot">%(left)s bytes still free of %(limit)s</div>
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
          <td>A bot that ignores prices, 200 seeds</td>
          <td class="n yes">0%% win</td></tr>
      <tr><td>Skill is rewarded</td>
          <td>A price-aware bot playing the real UI, 150 seeds</td>
          <td class="n yes">39%% win</td></tr>
      <tr><td>Outfitting is a real choice</td>
          <td>Kit priced off remaining payback; parity measured at n=250</td>
          <td class="n yes">51 vs 49</td></tr>
      <tr><td>All five endings are reachable</td>
          <td>Outcome recorded across 200 bot runs</td>
          <td class="n no">4 of 5</td></tr>
      <tr><td>No crashes, deadlocks or memory errors</td>
          <td>200 seeds played out, plus AddressSanitizer and UBSan</td>
          <td class="n yes">0</td></tr>
      <tr><td>Audio never clips</td>
          <td>Peak, DC and per-row RMS measured against the pattern tables</td>
          <td class="n yes">&minus;3.1 dB</td></tr>
      <tr><td>Runs on Windows</td>
          <td>Launched and screenshotted on a windows-latest runner</td>
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
