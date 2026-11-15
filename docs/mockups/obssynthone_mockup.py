#!/usr/bin/env python3
"""ObsSynth One front panel mock-up: one oscillator with five engines (STD, WT, FM, GRAIN, SMP), one filter, two
envelopes, one LFO, a short effects row. The parameter names are the ones Strudel uses (cutoff, resonance, attack,
delay, room...), plus `pos` for the engine's position (the wavetable frame, the grain read point, the sample start,
the FM index), so the synth's panel and the language read the same.
Same look as the other panels: the helpers of obssynth4d_mockup.py (panel, knob, switch, led, fader, wavetable,
the FM algorithm drawings) are used as they are. Writes obssynthone_mockup.html (+ _carbonian) and the sheet of the
five engine displays, obssynthone_engines.html. Render with
  chromium --headless --screenshot=out.png --window-size=1280,780 obssynthone_mockup.html"""
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
_src = (HERE / "obssynth4d_mockup.py").read_text().split("# ── page")[0]
exec(compile(_src, str(HERE / "obssynth4d_mockup.py"), "exec"), globals())  # T, ACCENT, out, add, panel, knob...

W, H = 1280, 780
BANK = HERE.parents[1] / "pcm" / "bank_morph_32waves_1024samples_32float.wav"
ENGINES = ["STD", "WT", "FM", "GRAIN", "SMP"]
ENGINE = ENGINES.index(next((a[9:].upper() for a in sys.argv if a.startswith("--engine=")), "WT"))


def display(dx, dy, dw, dh):
    add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="4" fill="{T["display"]}" stroke="#000"/>')
    add(f'<rect x="{dx+1}" y="{dy+1}" width="{dw-2}" height="{dh-2}" rx="3" fill="none" stroke="{ACCENT}" stroke-opacity=".18"/>')


def lcd(dx, dy, dw, left, right=""):
    add(f'<text x="{dx+8}" y="{dy+14}" class="lcdsmall">{left}</text>')
    if right:
        add(f'<text x="{dx+dw-8}" y="{dy+14}" class="lcdsmall" text-anchor="end">{right}</text>')


def shape(dx, dy, dw, dh, kind):
    """STD: one cycle of the chosen shape, twice"""
    pts = []
    for i in range(0, 241):
        p = (i / 120) % 1
        v = {"sine": math.sin(2 * math.pi * p), "saw": 2 * p - 1, "square": 1 if p < .5 else -1,
             "tri": 1 - 4 * abs(p - .5)}[kind]
        pts.append(f"{dx+12+i*(dw-24)/240:.1f},{dy+dh/2+6-(dh/2-24)*v:.1f}")
    add(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{ACCENT}" stroke-width="1.8"/>')


def grains(dx, dy, dw, dh, pos):
    """GRAIN: the source wave faint, the read point, and the grains around it as small windows"""
    mid = dy + dh / 2 + 8
    pts = " ".join(f"{dx+10+i*(dw-20)/300:.1f},{mid-16*math.sin(i/9)*math.sin(i/70+.5):.1f}" for i in range(301))
    add(f'<polyline points="{pts}" fill="none" stroke="{T["trace"]}" stroke-width="1" opacity=".5"/>')
    cx = dx + 10 + pos * (dw - 20)
    seed = 7
    for k in range(26):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        off = ((seed % 1000) / 1000 - .5) * dw * .45
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        wd = 14 + (seed % 14)
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        hh = 8 + (seed % 26)
        x = cx + off
        add(f'<path d="M{x-wd/2:.1f},{mid+2} Q{x:.1f},{mid-hh:.1f} {x+wd/2:.1f},{mid+2}" fill="{ACCENT}" fill-opacity=".10" '
            f'stroke="{T["trace_hi"]}" stroke-width=".8" opacity="{.35 + .5*(1-abs(off)/(dw*.225)):.2f}"/>')
    add(f'<line x1="{cx:.1f}" y1="{dy+22}" x2="{cx:.1f}" y2="{dy+dh-8}" stroke="{ACCENT}" stroke-width="1.6"/>')


def sample(dx, dy, dw, dh, pos):
    """SMP: the recording, the start marker at the position, the loop as a band"""
    mid = dy + dh / 2 + 8
    n = 220
    for i in range(n):
        x = dx + 10 + i * (dw - 20) / n
        a = (0.2 + 0.8 * abs(math.sin(i / 11) * math.sin(i / 53 + .4))) * (1 - i / n * .55) * (dh / 2 - 22)
        add(f'<line x1="{x:.1f}" y1="{mid-a:.1f}" x2="{x:.1f}" y2="{mid+a:.1f}" stroke="{T["trace"]}" stroke-width="1.4" '
            f'opacity=".75"/>')
    sx = dx + 10 + pos * (dw - 20)
    add(f'<rect x="{sx:.1f}" y="{dy+20}" width="{(dw-20)*.35:.1f}" height="{dh-28}" fill="{ACCENT}" opacity=".10"/>')
    add(f'<line x1="{sx:.1f}" y1="{dy+20}" x2="{sx:.1f}" y2="{dy+dh-8}" stroke="{ACCENT}" stroke-width="1.6"/>')


def wt(dx, dy, dw, dh, pos):
    frames, total = wavetable(BANK)
    nf = len(frames)
    cur = round(pos * (nf - 1))
    fw, amp = dw * 0.56, 16
    for k in range(nf - 1, -1, -1):
        ox = dx + 12 + k * (dw - fw - 24) / (nf - 1)
        oy = dy + dh - 20 - k * (dh - 50) / (nf - 1)
        pts = " ".join(f"{ox + j * fw / (len(frames[k]) - 1):.1f},{oy - amp * frames[k][j]:.1f}" for j in range(len(frames[k])))
        on = k == cur
        add(f'<polyline points="{pts}" fill="none" stroke="{ACCENT if on else T["trace"]}" stroke-width="{1.8 if on else .8}" '
            f'opacity="{1 if on else .25 + .45 * (1 - k / nf)}"/>')
    return total


def fm(dx, dy, dw, dh):
    algo = ALGOS[4]
    rows = max(r for _, _, r in algo[1]) + 1
    step = min(18, (dh - 30) / max(rows, 1))
    algo_diagram(dx + dw * 0.5, dy + dh - 22, algo, box=min(14, step * .8), step=step)
    add(f'<text x="{dx+dw-8}" y="{dy+40}" class="lcdsmall" text-anchor="end">M ×1.00</text>')
    add(f'<text x="{dx+dw-8}" y="{dy+54}" class="lcdsmall" text-anchor="end">2 ×2.00  50%</text>')
    add(f'<text x="{dx+dw-8}" y="{dy+68}" class="lcdsmall" text-anchor="end">FB 0.00</text>')


def engine_display(eng, dx, dy, dw, dh, pos=0.38):
    display(dx, dy, dw, dh)
    if eng == "STD":
        lcd(dx, dy, dw, "◀ SAW ▶", "BLEP")
        shape(dx, dy, dw, dh, "saw")
    elif eng == "WT":
        total = wt(dx, dy, dw, dh, pos)
        lcd(dx, dy, dw, "◀ MORPH ▶", f"{round(pos*(total-1))+1:02d} / {total}")
    elif eng == "FM":
        lcd(dx, dy, dw, "◀ ALGO 5 ▶", "4OP MIX")
        fm(dx, dy, dw, dh)
    elif eng == "GRAIN":
        lcd(dx, dy, dw, "◀ CHOIR ▶", "26 GRAINS")
        grains(dx, dy, dw, dh, pos)
    else:
        lcd(dx, dy, dw, "◀ VIBES C4 ▶", "LOOP")
        sample(dx, dy, dw, dh, pos)


# what the big knob is called on each engine: it is `pos` for the language, the label says what it moves
POS_LABEL = {"STD": "PULSE W", "WT": "POSITION", "FM": "INDEX", "GRAIN": "READ", "SMP": "START"}

# ── the page ──────────────────────────────────────────────────────────────────────────────────────────
eng = ENGINES[ENGINE]
add(f'<rect width="{W}" height="{H}" fill="{T["bg"]}"/>')
# header: name, pages, the patch display, buttons, master
add(f'<rect x="0" y="0" width="{W}" height="58" fill="url(#bar)"/><line x1="0" y1="58" x2="{W}" y2="58" stroke="#000"/>')
add('<text x="24" y="37" class="brand">OBSSYNTH<tspan class="brand4d" dx="6">ONE</tspan></text>')
for i, (t, x) in enumerate(zip(["MAIN"], [300])):
    add(f'<text x="{x}" y="35" class="tab" fill="{INK if i == 0 else DIM}">{t}</text>')
    if i == 0:
        add(f'<rect x="{x}" y="44" width="40" height="2" fill="{ACCENT}"/>')
add(f'<rect x="480" y="11" width="330" height="36" rx="4" fill="{T["display"]}" stroke="#000"/>')
add(f'<rect x="481" y="12" width="328" height="34" rx="3" fill="none" stroke="{ACCENT}" stroke-opacity=".25"/>')
add(f'<text x="496" y="35" class="lcd">◀</text><text x="794" y="35" class="lcd" text-anchor="end">▶</text>')
add(f'<text x="645" y="28" class="lcdsmall" text-anchor="middle">FACTORY 01 / 32 · {eng}</text>')
add(f'<text x="645" y="42" class="lcd" text-anchor="middle">Morph Pad</text>')
for x, t in [(826, "SAVE"), (900, "CC"), (958, "SETUP")]:
    add(f'<rect x="{x}" y="15" width="{66 if t != "CC" else 50}" height="28" rx="4" fill="{T["btn_off"]}" stroke="#000"/>')
    add(f'<text x="{x + (33 if t != "CC" else 25)}" y="33" class="btn" text-anchor="middle" fill="{DIM}">{t}</text>')
add(f'<text x="1112" y="34" class="lbl" text-anchor="end">MASTER</text>')
knob(1146, 29, 15, 0.72, "", None)
add(f'<text x="1268" y="26" class="sub" text-anchor="end">VOICES 3/16</text><text x="1268" y="42" class="sub" text-anchor="end">CPU 2%</text>')

# row 1: the oscillator and the filter
ry = 72
panel(14, ry, 640, 268, "OSCILLATOR", eng)
led(14 + 640 - 70, ry + 17, True)
switch(30, ry + 52, ENGINES, ENGINE, "ENGINE", 52)
engine_display(eng, 28, ry + 80, 612, 92)
knob(78, ry + 206, 22, 0.38, POS_LABEL[eng], "0.38")
knob(162, ry + 212, 15, 0.5, "TUNE", "0")
knob(226, ry + 212, 15, 0.5, "FINE", "0.00")
knob(290, ry + 212, 15, 0.7, "GAIN", "0.70")
knob(354, ry + 212, 15, 0.5, "PAN", "C")
knob(432, ry + 212, 15, 0.2, "UNISON", "3")
knob(496, ry + 212, 15, 0.3, "DETUNE", "12 ct")
knob(560, ry + 212, 15, 0.0, "SUB", "—")
knob(616, ry + 212, 15, 0.0, "GLIDE", "—")

panel(664, ry, 602, 268, "FILTER", "LPF")
switch(680, ry + 52, ["LP", "HP", "BP", "NOTCH"], 0, "TYPE", 50)
knob(776, ry + 150, 44, 0.58, "CUTOFF", "2.4 kHz")
knob(910, ry + 140, 28, 0.35, "RESONANCE", "0.35")
knob(1010, ry + 108, 17, 0.1, "HPF", "40 Hz")
knob(1090, ry + 108, 17, 0.5, "KEYTRK", "50%")
knob(1170, ry + 108, 17, 0.3, "VEL", "30%")
knob(1010, ry + 200, 17, 0.65, "ENV AMT", "+65%")
knob(1090, ry + 200, 17, 0.4, "LFO AMT", "40%")
knob(1170, ry + 200, 17, 0.5, "POS AMT", "0%")

# row 2: envelopes and the lfo, three panels across the row
by = 350
pw = 410
panel(14, by, pw, 220, "AMP ENV", "ADSR")
for i, (l, v) in enumerate([("A", .18), ("D", .45), ("S", .7), ("R", .5)]):
    fader(14 + 70 + i * 90, by + 52, 116, v, l)
panel(14 + pw + 11, by, pw, 220, "FILTER ENV", "ADSR")
for i, (l, v) in enumerate([("A", .1), ("D", .55), ("S", .35), ("R", .45)]):
    fader(14 + pw + 11 + 70 + i * 90, by + 52, 116, v, l)
lx = 14 + 2 * (pw + 11)
panel(lx, by, 1266 - lx, 220, "LFO", "→ POS")
led(1266 - 70, by + 17, True)
switch(lx + 14, by + 52, ["SIN", "TRI", "SQ", "SAW", "RND"], 0, "SHAPE", 38)
knob(lx + 70, by + 150, 26, 0.32, "RATE", "0.5 Hz")
knob(lx + 170, by + 150, 26, 0.4, "DEPTH", "40%")
knob(lx + 260, by + 150, 16, 0.0, "SYNC", "free")

# row 3: effects
fy = 580
panel(14, fy, 300, 190, "DRIVE", "DISTORT · CRUSH")
knob(70, fy + 100, 24, 0.3, "DISTORT", "0.30")
knob(150, fy + 106, 18, 0.0, "CRUSH", "—")
knob(220, fy + 106, 18, 0.0, "COARSE", "—")
knob(280, fy + 106, 14, 0.4, "MIX", "")
panel(324, fy, 300, 190, "DELAY")
knob(380, fy + 100, 24, 0.35, "TIME", "3/16")
knob(460, fy + 106, 18, 0.4, "FEEDBACK", "0.40")
knob(540, fy + 106, 18, 0.25, "MIX", "0.25")
panel(634, fy, 300, 190, "REVERB", "ROOM")
knob(690, fy + 100, 24, 0.4, "ROOM", "0.40")
knob(770, fy + 106, 18, 0.5, "SIZE", "0.50")
knob(850, fy + 106, 18, 0.3, "MIX", "0.30")
panel(944, fy, 322, 190, "MASTER")
knob(1000, fy + 100, 24, 0.72, "VOLUME", "-3 dB")
knob(1070, fy + 106, 16, 0.5, "PAN", "C")
knob(1130, fy + 106, 16, 0.5, "BEND", "±2")
for i, v in enumerate([.62, .58]):
    add(f'<rect x="{1196+i*26}" y="{fy+46}" width="16" height="130" rx="2" fill="{T["display"]}" stroke="#000"/>')
    add(f'<rect x="{1196+i*26}" y="{fy+46+130*(1-v):.0f}" width="16" height="{130*v:.0f}" rx="2" fill="{ACCENT}" opacity=".85"/>')

style = f"""
svg{{display:block}}
text{{font-family:'Inter','DejaVu Sans',sans-serif}}
.title{{font-size:12px;letter-spacing:2.4px;fill:{INK};font-weight:700}}
.sub{{font-size:10px;letter-spacing:1.5px;fill:{DIM}}}
.lbl{{font-size:10px;letter-spacing:1.6px;fill:{INK};font-weight:600}}
.val{{font-size:10px;fill:{ACCENT};font-family:'JetBrains Mono','DejaVu Sans Mono',monospace}}
.btn{{font-size:9px;letter-spacing:1px;font-weight:700}}
.tab{{font-size:12px;letter-spacing:2.5px;font-weight:700}}
.brand{{font-size:22px;letter-spacing:5px;fill:{INK};font-weight:300}}
.brand4d{{font-size:22px;fill:{ACCENT};font-weight:700}}
.lcd{{font-family:'JetBrains Mono','DejaVu Sans Mono',monospace;font-size:15px;fill:{ACCENT}}}
.opn{{font-family:'JetBrains Mono','DejaVu Sans Mono',monospace;font-weight:700}}
.lcdsmall{{font-family:'JetBrains Mono','DejaVu Sans Mono',monospace;font-size:9px;fill:{ACCENT};opacity:.6;letter-spacing:2px}}
"""
defs = f"""
<defs>
 <linearGradient id="panel" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{T['panel_top']}"/><stop offset="1" stop-color="{T['panel_bottom']}"/></linearGradient>
 <linearGradient id="bar" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{T['bar_top']}"/><stop offset="1" stop-color="{T['bar_bottom']}"/></linearGradient>
 <radialGradient id="knobbody" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#3a3a3d"/><stop offset=".7" stop-color="#18181a"/><stop offset="1" stop-color="#0c0c0d"/></radialGradient>
 <radialGradient id="cap" cx=".35" cy=".3" r=".9"><stop offset="0" stop-color="#e9e9e6"/><stop offset=".45" stop-color="#9c9c99"/><stop offset="1" stop-color="#4b4b4a"/></radialGradient>
 <filter id="blur" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="3"/></filter>
</defs>"""


def page(w, h, body):
    return (f'<!DOCTYPE html><html><head><meta charset="utf-8"><style>html,body{{margin:0;background:{T["bg"]}}}{style}</style></head>'
            f'<body><svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">{defs}{body}</svg></body></html>')


name = f"obssynthone_mockup{SUFFIX}"
(HERE / f"{name}.html").write_text(page(W, H, "".join(out)))

# the sheet: the five displays side by side, the same size, the same position (0.38)
out.clear()
add(f'<rect width="1000" height="330" fill="{T["bg"]}"/>')
panel(10, 10, 980, 310, "ONE · ENGINES", "pos moves the lit part of each display")
for k, e in enumerate(ENGINES):
    x = 24 + k * 192
    engine_display(e, x, 56, 180, 200)
    add(f'<text x="{x+90}" y="282" class="lbl" text-anchor="middle" fill="{ACCENT}">{e}</text>')
    add(f'<text x="{x+90}" y="300" class="sub" text-anchor="middle">pos = {POS_LABEL[e].lower()}</text>')
(HERE / f"obssynthone_engines{SUFFIX}.html").write_text(page(1000, 330, "".join(out)))
print(HERE / f"{name}.html")
