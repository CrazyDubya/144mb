#!/usr/bin/env python3
"""Builds the progress page, embedding all media as data URIs.

The Artifact CSP blocks every external host, so nothing may be referenced by
URL -- the GIF and stills are inlined base64.
"""
import base64
import os

MEDIA = "media"
OUT = "docs/progress.html"

BYTES_USED = 72192
LIMIT = 1474560


def uri(path, mime):
    with open(path, "rb") as f:
        return "data:%s;base64,%s" % (mime, base64.b64encode(f.read()).decode())


STILLS = [
    ("step_0000.png", "The market",
     "Five goods, each a price and a held quantity. The selected row shows its two "
     "actions inline &mdash; buy and sell &mdash; so nothing needs explaining. Water is "
     "cheap here at 7; further out it will not be."),
    ("step_0006.png", "The route",
     "Eight sectors, one way, no going back. Bone squares are markets, rust squares "
     "with a cross are encounters, the green square is the goal. The highlighted hop "
     "shows what it costs: one fuel."),
    ("step_0011.png", "An encounter",
     "Every encounter is one shape with different numbers in it. Pay the top price, "
     "or take the bottom consequence. The red bar means threat; opportunities are "
     "framed green. No sentence is involved."),
    ("step_0020.png", "The Green Zone",
     "Day eight. Arrived with two fuel, eight cargo, seventy-nine credits &mdash; and "
     "zero water. One more hop would have killed the crew."),
]

FACTS = [
    ("22", "colours in the entire game"),
    ("0", "audio samples &mdash; all synthesis"),
    ("0", "words of text on screen"),
    ("0", "crashes across 200 seeds"),
]


def main():
    os.makedirs("docs", exist_ok=True)
    gif = uri(os.path.join(MEDIA, "run.gif"), "image/gif")
    pct = BYTES_USED * 100.0 / LIMIT

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
    Twenty-one decisions, eight days, one seed. The convoy starts at a market,
    buys fuel it cannot reach the end without, and pushes east through raiders
    and breakdowns toward the Green Zone.
  </p>
  <div class="run">
    <img src="%(gif)s" alt="Animated playthrough of Convoy" />
    <p class="cap">
      Seed 16, played start to finish. The game is turn-based, so this is one
      frame per decision rather than a video &mdash; roughly what three to five
      minutes at the keyboard looks like.
    </p>
  </div>

  <h2>What a player sees</h2>
  <p>
    Nothing on screen is written in any language. Meaning is carried by icon,
    colour, sign and Arabic numerals, so the game reads identically to a judge in
    Seoul and one in London. There is no alphabetic font in the build at all,
    which makes accidental English impossible rather than merely unlikely.
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
    Every rule below was verified by tracing the simulation or by inspecting
    rendered frames &mdash; not by assuming. The last row is the one that matters:
    the submission binary has never executed on the platform it targets.
  </p>
  <div class="tbl">
    <table>
      <tr><th>Claim</th><th>How it was checked</th><th class="n">Result</th></tr>
      <tr><td>Trading moves prices permanently</td>
          <td>Dumped water into one market, traced every sale</td>
          <td class="n yes">15&rarr;6</td></tr>
      <tr><td>The economy is not optional</td>
          <td>Two bots, 60 seeds each, one buying fuel and one not</td>
          <td class="n yes">38%% vs 0%%</td></tr>
      <tr><td>No crashes</td>
          <td>200 seeds played to completion</td>
          <td class="n yes">0</td></tr>
      <tr><td>Music is audible and correct</td>
          <td>Per-step RMS against the sequencer table</td>
          <td class="n yes">2.2&times;</td></tr>
      <tr><td>Audio never clips</td>
          <td>Peak and DC measured over rendered output</td>
          <td class="n yes">&minus;2.1 dB</td></tr>
      <tr><td>Runs on Windows</td>
          <td>Not yet tested on any Windows machine</td>
          <td class="n no">unproven</td></tr>
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
