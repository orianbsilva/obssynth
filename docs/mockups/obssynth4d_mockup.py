#!/usr/bin/env python3
"""ObsSynth4D front panel mock-up: a hardware faceplate, modules on a grid,
knobs sized by importance, few words. Writes obssynth4d_mockup.html (1600x900, inline SVG); render with
  chromium --headless --screenshot=out.png --window-size=1600,900 obssynth4d_mockup.html
The modules and parameters are the engine's (src/app/synth_view/main_screen_imgui.cpp)."""
import math
from pathlib import Path

W, H = 1600, 900
# the colours: neutral greys and amber, or, on Carbonian (--carbonian, or the file exists and
# --neutral is not given), the system's palette (/var/lib/carbonian/palette/palette: key=#rrggbb lines),
# so the synth follows the desktop when the palette changes
import sys
T = {"bg": "#121213", "panel_top": "#34343a", "panel_bottom": "#26262a", "bar_top": "#2c2c30", "bar_bottom": "#1c1c1f",
     "display": "#0b0c0a", "trace": "#7fa36f", "trace_hi": "#9fd38a", "grid": "#1f2a1e", "btn_on": "#3a3833",
     "btn_off": "#1d1d1f", "row": "#2c2a26", "led_off": "#3a2c18", "accent": "#e8a33d", "ink": "#d9d6cf", "dim": "#8d8a84"}
PALETTE = Path("/var/lib/carbonian/palette/palette")
CARBONIAN = "--carbonian" in sys.argv or (PALETTE.exists() and "--neutral" not in sys.argv)
if CARBONIAN:
    pal = dict(l.split("=", 1) for l in PALETTE.read_text().splitlines() if "=" in l and not l.startswith("#"))
    g = pal.get
    T.update({"bg": g("background"), "panel_top": g("surface_2"), "panel_bottom": g("surface"), "bar_top": g("surface_2"),
              "bar_bottom": g("surface"), "display": g("idle_bg"), "trace": g("ramp_2"), "trace_hi": g("ramp_3"),
              "grid": g("line"), "btn_on": g("line"), "btn_off": g("surface"), "row": g("surface_2"), "led_off": g("muted"),
              "accent": g("accent"), "ink": g("foreground"), "dim": g("muted")})
ACCENT, INK, DIM = T["accent"], T["ink"], T["dim"]
SUFFIX = "_carbonian" if CARBONIAN else ""
out = []


def add(s):
    out.append(s)


def panel(x, y, w, h, title, sub=""):
    add(f'<g><rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="url(#panel)" stroke="#0b0b0c" stroke-width="2"/>')
    add(f'<rect x="{x+1}" y="{y+1}" width="{w-2}" height="{h-2}" rx="5" fill="none" stroke="#ffffff" stroke-opacity=".06"/>')
    add(f'<text x="{x+14}" y="{y+22}" class="title">{title}</text>')
    if sub:
        add(f'<text x="{x+w-14}" y="{y+22}" class="sub" text-anchor="end">{sub}</text>')
    add(f'<line x1="{x+12}" y1="{y+31}" x2="{x+w-12}" y2="{y+31}" stroke="#000" stroke-opacity=".55"/>')
    add(f'<line x1="{x+12}" y1="{y+32}" x2="{x+w-12}" y2="{y+32}" stroke="#fff" stroke-opacity=".07"/></g>')


def knob(cx, cy, r, v, label, value=None, bipolar=False):
    """v in 0..1; a scale of ticks from -135 to +135 degrees, a black body, a metal cap, a white line"""
    a0, a1 = -135, 135
    for i in range(11):
        a = math.radians(a0 + (a1 - a0) * i / 10 - 90)
        r1, r2 = r + 4, r + (8 if i in (0, 5, 10) else 6)
        add(f'<line x1="{cx+r1*math.cos(a):.1f}" y1="{cy+r1*math.sin(a):.1f}" x2="{cx+r2*math.cos(a):.1f}" '
            f'y2="{cy+r2*math.sin(a):.1f}" stroke="{DIM}" stroke-width="1.2"/>')
    add(f'<circle cx="{cx}" cy="{cy+3}" r="{r+1}" fill="#000" opacity=".55" filter="url(#blur)"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#knobbody)" stroke="#050505"/>')
    # grip: fine radial grooves on the skirt
    for i in range(36):
        a = math.radians(i * 10)
        add(f'<line x1="{cx+(r-1)*math.cos(a):.1f}" y1="{cy+(r-1)*math.sin(a):.1f}" x2="{cx+(r*0.8)*math.cos(a):.1f}" '
            f'y2="{cy+(r*0.8)*math.sin(a):.1f}" stroke="#2a2a2c" stroke-width=".8"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r*0.62:.1f}" fill="url(#cap)" stroke="#111"/>')
    a = math.radians(a0 + (a1 - a0) * v - 90)
    add(f'<line x1="{cx+r*0.15*math.cos(a):.1f}" y1="{cy+r*0.15*math.sin(a):.1f}" x2="{cx+r*0.9*math.cos(a):.1f}" '
        f'y2="{cy+r*0.9*math.sin(a):.1f}" stroke="#f4f2ec" stroke-width="{max(1.6, r/11):.1f}" stroke-linecap="round"/>')
    add(f'<text x="{cx}" y="{cy+r+22}" class="lbl" text-anchor="middle">{label}</text>')
    if value:
        add(f'<text x="{cx}" y="{cy+r+35}" class="val" text-anchor="middle">{value}</text>')


def switch(x, y, opts, sel, label=None, w=None):
    """a row of small push buttons, the chosen one lit"""
    w = w or 38
    if label:
        add(f'<text x="{x}" y="{y-6}" class="lbl">{label}</text>')
    for i, o in enumerate(opts):
        on = i == sel
        add(f'<rect x="{x+i*(w+3)}" y="{y}" width="{w}" height="18" rx="3" fill="{T["btn_on"] if on else T["btn_off"]}" '
            f'stroke="{ACCENT if on else "#000"}" stroke-opacity="{1 if on else .8}"/>')
        add(f'<text x="{x+i*(w+3)+w/2}" y="{y+13}" class="btn" text-anchor="middle" fill="{ACCENT if on else DIM}">{o}</text>')


def led(cx, cy, on=True):
    add(f'<circle cx="{cx}" cy="{cy}" r="4" fill="{ACCENT if on else T["led_off"]}" stroke="#000"/>')
    if on:
        add(f'<circle cx="{cx}" cy="{cy}" r="9" fill="{ACCENT}" opacity=".18" filter="url(#blur)"/>')


def fader(x, y, h, v, label):
    add(f'<rect x="{x-3}" y="{y}" width="6" height="{h}" rx="3" fill="#0a0a0b" stroke="#000"/>')
    for i in range(11):
        yy = y + h * i / 10
        add(f'<line x1="{x-12}" y1="{yy:.1f}" x2="{x-7}" y2="{yy:.1f}" stroke="{DIM}" stroke-width=".8"/>')
    ky = y + h * (1 - v)
    add(f'<rect x="{x-11}" y="{ky-7:.1f}" width="22" height="14" rx="2" fill="url(#cap)" stroke="#111"/>')
    add(f'<line x1="{x-9}" y1="{ky:.1f}" x2="{x+9}" y2="{ky:.1f}" stroke="#f4f2ec" stroke-width="1.5"/>')
    add(f'<text x="{x}" y="{y+h+20}" class="lbl" text-anchor="middle">{label}</text>')


def wave_icon(x, y, kind, on):
    c = ACCENT if on else DIM
    pts = {
        "sine": " ".join(f"{x+i:.1f},{y-6*math.sin(i/14*2*math.pi):.1f}" for i in range(15)),
        "saw": f"{x},{y+6} {x+14},{y-6} {x+14},{y+6}",
        "square": f"{x},{y+6} {x},{y-6} {x+7},{y-6} {x+7},{y+6} {x+14},{y+6} {x+14},{y-6}",
        "tri": f"{x},{y+6} {x+7},{y-6} {x+14},{y+6}",
        "pulse": f"{x},{y+6} {x},{y-6} {x+3},{y-6} {x+3},{y+6} {x+14},{y+6}",
        "noise": " ".join(f"{x+i},{y+(6 if (i*7)%5>2 else -5)*(1 if i%2 else .5):.1f}" for i in range(15)),
    }[kind]
    add(f'<polyline points="{pts}" fill="none" stroke="{c}" stroke-width="1.4"/>')


def wavetable(path, frames_wanted=16, points=64):
    """the frames of a bank (.wav: float32 or 16-bit, frames of 1024 samples one after another), a few, resampled"""
    import struct
    b = Path(path).read_bytes()
    fmt = struct.unpack_from("<H", b, 20)[0]
    bits = struct.unpack_from("<H", b, 34)[0]
    i = b.index(b"data")
    n = struct.unpack_from("<I", b, i + 4)[0]
    raw = b[i + 8:i + 8 + n]
    if fmt == 3 or bits == 32:
        smp = struct.unpack(f"<{len(raw)//4}f", raw)
    else:
        smp = [v / 32768 for v in struct.unpack(f"<{len(raw)//2}h", raw)]
    flen = 1024
    total = len(smp) // flen
    # frames of silence at the end (some banks are half empty): left out
    while total > 1 and max(abs(v) for v in smp[(total - 1) * flen:total * flen]) < 1e-6:
        total -= 1
    picks = [round(k * (total - 1) / (frames_wanted - 1)) for k in range(frames_wanted)]
    frames = [[smp[f * flen + int(j * flen / points)] for j in range(points)] for f in picks]
    return frames, total


def osc_wt(x, y, w, h, n, bank, label, pos):
    """an oscillator in wavetable mode: the table as a stack of its frames, the current one lit"""
    panel(x, y, w, h, f"OSC {n}", "WT")
    led(x + w - 70, y + 17, True)
    switch(x + 16, y + 52, ["STD", "WT", "FM"], 1, "ENGINE", 44)
    frames, total = wavetable(bank)
    # the display: frames from back (top right) to front (bottom left), the one at the position lit
    dx, dy, dw, dh = x + 14, y + 80, w - 28, 96
    add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="4" fill="{T['display']}" stroke="#000"/>')
    add(f'<rect x="{dx+1}" y="{dy+1}" width="{dw-2}" height="{dh-2}" rx="3" fill="none" stroke="{ACCENT}" stroke-opacity=".18"/>')
    nf = len(frames)
    cur = round(pos * (nf - 1))
    fw, amp = dw * 0.62, 15
    for k in range(nf - 1, -1, -1):
        ox = dx + 12 + k * (dw - fw - 24) / (nf - 1)
        oy = dy + dh - 24 - k * (dh - 52) / (nf - 1)
        pts = " ".join(f"{ox + j * fw / (len(frames[k]) - 1):.1f},{oy - amp * frames[k][j]:.1f}" for j in range(len(frames[k])))
        on = k == cur
        add(f'<polyline points="{pts}" fill="none" stroke="{ACCENT if on else T["trace"]}" '
            f'stroke-width="{1.8 if on else .8}" opacity="{1 if on else .25 + .45 * (1 - k / nf)}"/>')
    name = Path(bank).stem.replace("bank_", "").split("_")[0].upper()
    add(f'<text x="{dx+8}" y="{dy+14}" class="lcdsmall">◀ {name} ▶</text>')
    add(f'<text x="{dx+dw-8}" y="{dy+14}" class="lcdsmall" text-anchor="end">{round(pos*(total-1))+1:02d} / {total}</text>')
    # position large, the rest as on the other oscillators
    knob(x + 56, y + 210, 21, pos, "POSITION", f"{pos:.2f}")
    knob(x + 132, y + 212, 15, 0.5, "TUNE", "0")
    knob(x + 186, y + 212, 15, 0.5, "FINE")
    knob(x + 240, y + 212, 15, 0.62, "LEVEL")
    knob(x + 292, y + 212, 15, 0.5, "PAN")


# the FM algorithms as the engine computes them (oscillator.cpp): C the carrier, M the modulator, 2..4 the others.
# Each: operators (name, column, row; row 0 the bottom), links (from, to), feedback on (name or None)
ALGOS = [
    ("2OP", [("C", 0, 0), ("M", 0, 1)], [("M", "C")], None),
    ("2OP+FB", [("C", 0, 0), ("M", 0, 1)], [("M", "C")], "C"),
    ("4OP SER", [("C", 0, 0), ("M", 0, 1), ("2", 0, 2), ("3", 0, 3), ("4", 0, 4)], [("4", "3"), ("3", "2"), ("2", "M"), ("M", "C")], None),
    ("4OP PAR", [("C", 0, 0), ("M", -.6, 1), ("2", .6, 1), ("3", -.6, 2), ("4", .6, 2)], [("3", "M"), ("4", "2"), ("M", "C"), ("2", "C")], None),
    ("4OP MIX", [("C", 0, 0), ("M", -.6, 1), ("3", -.6, 2), ("4", -.6, 3), ("2", .6, 1)], [("4", "3"), ("3", "M"), ("M", "C"), ("2", "C")], None),
]


def algo_diagram(cx, bottom, algo, box=22, step=34, lit=True, levels=None):
    """the operators as boxes (the carrier outlined in the accent, an arrow out of it), the links as lines"""
    name, ops, links, fb = algo
    pos = {n: (cx + c * box * 1.9, bottom - r * step) for n, c, r in ops}
    col = ACCENT if lit else DIM
    for a, b in links:
        (x1, y1), (x2, y2) = pos[a], pos[b]
        add(f'<line x1="{x1}" y1="{y1+box/2}" x2="{x2}" y2="{y2-box/2}" stroke="{col}" stroke-width="1.3" opacity=".8"/>')
    if fb:
        x, y = pos[fb]
        h = box / 2
        add(f'<path d="M{x+h},{y} h{h*.8} v{-box} h{-h*1.8} v{h*.5}" fill="none" stroke="{col}" stroke-width="1.3" opacity=".8"/>')
    x, y = pos["C"]
    add(f'<line x1="{x}" y1="{y+box/2}" x2="{x}" y2="{y+box/2+10}" stroke="{col}" stroke-width="1.3"/>')
    add(f'<path d="M{x-4},{y+box/2+6} L{x},{y+box/2+11} L{x+4},{y+box/2+6}" fill="none" stroke="{col}" stroke-width="1.3"/>')
    for n, (x, y) in pos.items():
        carrier = n == "C"
        lv = (levels or {}).get(n, 1)
        add(f'<rect x="{x-box/2}" y="{y-box/2}" width="{box}" height="{box}" rx="3" fill="{T['display']}" '
            f'stroke="{col if carrier else T["trace"]}" stroke-width="{1.6 if carrier else 1}" opacity="{.45 + .55*lv:.2f}"/>')
        add(f'<text x="{x}" y="{y+box*0.18}" text-anchor="middle" class="opn" fill="{col if carrier else T["trace_hi"]}" '
            f'font-size="{box*0.5:.0f}">{n}</text>')


def osc_fm(x, y, w, h, n, algo_i, idx=0.45):
    """an oscillator in FM mode: the algorithm drawn on the display, then its knobs"""
    panel(x, y, w, h, f"OSC {n}", "FM")
    led(x + w - 70, y + 17, True)
    switch(x + 16, y + 52, ["STD", "WT", "FM"], 2, "ENGINE", 44)
    dx, dy, dw, dh = x + 14, y + 80, w - 28, 96
    add(f'<rect x="{dx}" y="{dy}" width="{dw}" height="{dh}" rx="4" fill="{T['display']}" stroke="#000"/>')
    add(f'<rect x="{dx+1}" y="{dy+1}" width="{dw-2}" height="{dh-2}" rx="3" fill="none" stroke="{ACCENT}" stroke-opacity=".18"/>')
    algo = ALGOS[algo_i]
    add(f'<text x="{dx+8}" y="{dy+14}" class="lcdsmall">◀ ALGO {algo_i+1} ▶</text>')
    add(f'<text x="{dx+8}" y="{dy+28}" class="lcdsmall" opacity=".9">{algo[0]}</text>')
    # the five, small, the chosen one lit: a glance at where it is
    for k in range(5):
        add(f'<rect x="{dx+10+k*13}" y="{dy+dh-16}" width="9" height="6" rx="1" fill="{ACCENT if k==algo_i else "#333"}"/>')
    rows = max(r for _, _, r in algo[1]) + 1
    step = min(16, (dh - 26) / max(rows, 1))
    algo_diagram(dx + dw * 0.52, dy + dh - 20, algo, box=min(13, step * .8), step=step)
    # ratios beside, as an operator panel prints them
    add(f'<text x="{dx+dw-8}" y="{dy+40}" class="lcdsmall" text-anchor="end">M ×1.00</text>')
    add(f'<text x="{dx+dw-8}" y="{dy+54}" class="lcdsmall" text-anchor="end">2 ×2.00  50%</text>')
    add(f'<text x="{dx+dw-8}" y="{dy+68}" class="lcdsmall" text-anchor="end">FB 0.00</text>')
    knob(x + 56, y + 210, 21, idx, "INDEX", f"{idx*20:.1f}")
    knob(x + 132, y + 212, 15, 1/16, "M ×", "1.00")
    knob(x + 186, y + 212, 15, 0.0, "FDBK", "0.00")
    knob(x + 240, y + 212, 15, 2/16, "2 ×", "2.00")
    knob(x + 292, y + 212, 15, 0.5, "2 LVL", "50%")


# ── page ──────────────────────────────────────────────────────────────────────────────────────────────
add(f'<rect width="{W}" height="{H}" fill="{T['bg']}"/>')

# top bar: name, pages, the patch display, master
add(f'<rect x="0" y="0" width="{W}" height="58" fill="url(#bar)"/>')
add(f'<line x1="0" y1="58" x2="{W}" y2="58" stroke="#000"/>')
add('<text x="24" y="37" class="brand">OBSSYNTH<tspan class="brand4d" dx="6">4D</tspan></text>')
for i, (t, x) in enumerate(zip(["MAIN", "MOD MATRIX", "SCOPE", "SETTINGS"], [250, 330, 476, 564])):
    on = i == 0
    add(f'<text x="{x}" y="35" class="tab" fill="{INK if on else DIM}">{t}</text>')
    if on:
        add(f'<rect x="{x}" y="44" width="40" height="2" fill="{ACCENT}"/>')
add(f'<rect x="690" y="11" width="420" height="36" rx="4" fill="{T['display']}" stroke="#000"/>')
add(f'<rect x="691" y="12" width="418" height="34" rx="3" fill="none" stroke="{ACCENT}" stroke-opacity=".25"/>')
add(f'<text x="706" y="35" class="lcd">◀</text><text x="1094" y="35" class="lcd" text-anchor="end">▶</text>')
add(f'<text x="900" y="28" class="lcdsmall" text-anchor="middle">BANK 1 · 07</text>')
add(f'<text x="900" y="42" class="lcd" text-anchor="middle">Warm Analog Pad</text>')
knob(1470, 29, 15, 0.72, "", None)
add(f'<text x="1430" y="34" class="lbl" text-anchor="end">MASTER</text>')
add(f'<text x="1580" y="26" class="sub" text-anchor="end">VOICES 3/32</text><text x="1580" y="42" class="sub" text-anchor="end">CPU 4%</text>')

# left: the patch browser
panel(14, 72, 230, 814, "PATCHES", "★")
cats = ["All", "Bass", "Keys", "Lead", "Pad", "FX", "User"]
for i, c in enumerate(cats):
    add(f'<text x="30" y="{122+i*24}" class="lbl" fill="{ACCENT if c=="Pad" else INK}">{c}</text>')
add('<line x1="26" y1="292" x2="232" y2="292" stroke="#000" stroke-opacity=".6"/>')
for i, p in enumerate(["Glass Choir", "Slow Bloom", "Warm Analog Pad", "Tape Strings", "Night Drive", "Polar Air",
                       "Velvet Sweep", "Dust Halo", "Orbit", "Low Cloud", "Brass Fog", "Hollow Bells"]):
    on = p == "Warm Analog Pad"
    if on:
        add(f'<rect x="22" y="{302+i*26}" width="214" height="22" rx="3" fill="{T['row']}"/>')
    add(f'<text x="32" y="{318+i*26}" class="item" fill="{ACCENT if on else INK}">{p}</text>')

# row 1: four oscillators
ox, oy, ow, oh = 256, 72, 325, 268
BANK = Path(__file__).resolve().parents[2] / "pcm" / "bank_morph_32waves_1024samples_32float.wav"
for o in range(4):
    x = ox + o * (ow + 10)
    if o == 1:
        osc_wt(x, oy, ow, oh, 2, BANK, "MORPH", 0.38)
        continue
    if o == 2:
        osc_fm(x, oy, ow, oh, 3, 4)
        continue
    panel(x, oy, ow, oh, f"OSC {o+1}", ["STD", "WT", "FM", "STD"][o])
    led(x + ow - 70, oy + 17, o != 3)
    switch(x + 16, oy + 52, ["STD", "WT", "FM"], [0, 1, 2, 0][o], "ENGINE", 44)
    # wave: a rotary switch with the shapes around it
    kinds = ["sine", "saw", "square", "tri", "pulse", "noise"]
    sel = [1, 0, 0, 2][o]
    cx, cy = x + 72, oy + 150
    for i, k in enumerate(kinds):
        a = math.radians(-115 + i * 46)
        wave_icon(cx + 46 * math.sin(a) - 7, cy - 4 - 46 * math.cos(a), k, i == sel)
    knob(cx, cy, 24, sel / 5, "WAVE")
    knob(x + 186, oy + 120, 30, [0.5, 0.5, 0.62, 0.38][o], "TUNE", ["0", "0", "+7", "-12"][o], True)
    knob(x + 266, oy + 112, 18, 0.5, "FINE", "0.00")
    knob(x + 166, oy + 212, 16, [0.8, 0.62, 0.45, 0.3][o], "LEVEL")
    knob(x + 228, oy + 212, 16, 0.5, "PAN")
    knob(x + 288, oy + 212, 16, [0.2, 0.55, 0.3, 0.1][o], "FX", ["CHORUS", "—", "DRIVE", "—"][o])

# row 2: the voice of OSC 1 (filter, its envelope, the amp envelope), the LFO, the XY pad
ry = 350
panel(256, ry, 470, 244, "FILTER", "OSC 1 ▾")
switch(272, ry + 52, ["LP", "HP", "BP", "NOTCH"], 0, "TYPE", 46)
knob(360, ry + 150, 44, 0.58, "CUTOFF", "2.4 kHz")
knob(486, ry + 140, 28, 0.35, "RESO", "0.35")
knob(590, ry + 96, 17, 0.1, "HPF", "40 Hz")
knob(668, ry + 96, 17, 0.5, "KEYTRK", "50%")
knob(590, ry + 180, 17, 0.65, "ENV AMT", "+65%")
knob(668, ry + 180, 17, 0.3, "VEL", "30%")

panel(736, ry, 300, 244, "FILTER ENV")
for i, (l, v) in enumerate([("A", .18), ("D", .45), ("S", .6), ("R", .5)]):
    fader(782 + i * 66, ry + 56, 136, v, l)
panel(1046, ry, 300, 244, "AMP ENV")
for i, (l, v) in enumerate([("A", .35), ("D", .3), ("S", .8), ("R", .62)]):
    fader(1092 + i * 66, ry + 56, 136, v, l)

panel(1356, ry, 230, 244, "LFO", "→ CUTOFF")
switch(1370, ry + 52, ["SIN", "TRI", "SQ", "SAW", "RND"], 1, "SHAPE", 38)
knob(1420, ry + 150, 26, 0.32, "RATE", "2.0 Hz")
knob(1520, ry + 150, 26, 0.4, "DEPTH", "40%")
led(1408, ry + 17, True)

# row 3: mixer, tube, delay, master filter, master fx, xy and scope
by = 606
panel(256, by, 330, 280, "MIXER")
for i in range(4):
    fader(296 + i * 64, by + 66, 150, [0.8, 0.62, 0.45, 0.3][i], f"{i+1}")
fader(552, by + 66, 150, 0.72, "MST")
add(f'<line x1="530" y1="{by+56}" x2="530" y2="{by+256}" stroke="#000" stroke-opacity=".5"/>')

panel(596, by, 210, 280, "TUBE", "12AX7")
switch(612, by + 52, ["FLAT", "AX7", "EL34"], 1, "MODEL", 54)
knob(660, by + 150, 26, 0.4, "DRIVE")
knob(752, by + 150, 20, 0.2, "ASYM")
knob(705, by + 232, 16, 0.5, "MIX")

panel(816, by, 290, 280, "DELAY")
knob(870, by + 98, 22, 0.35, "LEFT", "3/16")
knob(960, by + 98, 22, 0.5, "RIGHT", "1/4")
knob(1050, by + 98, 22, 0.4, "FDBK")
knob(870, by + 206, 18, 0.25, "MIX")
knob(960, by + 206, 18, 0.5, "DAMP")
add(f'<text x="1050" y="{by+200}" class="lbl" text-anchor="middle">PING-PONG</text>')
led(1050, by + 216, True)

panel(1116, by, 210, 280, "MASTER FILTER")
switch(1130, by + 52, ["LP", "HP", "BP", "BYP"], 0, None, 41)
knob(1180, by + 150, 30, 0.8, "CUTOFF", "12 kHz")
knob(1270, by + 150, 20, 0.15, "RESO")

panel(1336, by, 250, 280, "XY · SCOPE")
add(f'<rect x="1350" y="{by+42}" width="222" height="120" rx="4" fill="{T['display']}" stroke="#000"/>')
for i in range(1, 4):
    add(f'<line x1="{1350+i*55.5}" y1="{by+42}" x2="{1350+i*55.5}" y2="{by+162}" stroke="{T['grid']}"/>')
    add(f'<line x1="1350" y1="{by+42+i*30}" x2="1572" y2="{by+42+i*30}" stroke="{T['grid']}"/>')
add(f'<circle cx="1440" cy="{by+92}" r="7" fill="{ACCENT}" opacity=".9"/><circle cx="1440" cy="{by+92}" r="16" fill="{ACCENT}" opacity=".15"/>')
add(f'<text x="1356" y="{by+156}" class="sub">X CUTOFF</text><text x="1566" y="{by+56}" class="sub" text-anchor="end">Y RESO</text>')
add(f'<rect x="1350" y="{by+172}" width="222" height="92" rx="4" fill="{T['display']}" stroke="#000"/>')
pts = " ".join(f"{1352+i*2.2:.1f},{by+218-26*math.sin(i/16)*math.exp(-i/160)*(1+.3*math.sin(i/3)):.1f}" for i in range(100))
add(f'<polyline points="{pts}" fill="none" stroke="{T['trace_hi']}" stroke-width="1.4" opacity=".9"/>')

style = f"""
text{{font-family:'Inter','DejaVu Sans',sans-serif}}
.title{{font-size:12px;letter-spacing:2.4px;fill:{INK};font-weight:700}}
.sub{{font-size:10px;letter-spacing:1.5px;fill:{DIM}}}
.lbl{{font-size:10px;letter-spacing:1.6px;fill:{INK};font-weight:600}}
.val{{font-size:10px;fill:{ACCENT};font-family:'JetBrains Mono','DejaVu Sans Mono',monospace}}
.btn{{font-size:9px;letter-spacing:1px;font-weight:700}}
.tab{{font-size:12px;letter-spacing:2.5px;font-weight:700}}
.item{{font-size:12px}}
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
html = f"""<!DOCTYPE html><html><head><meta charset="utf-8"><style>html,body{{margin:0;background:{T["bg"]}}}{style}</style></head>
<body><svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{defs}{''.join(out)}</svg></body></html>"""
p = Path(__file__).with_name(f"obssynth4d_mockup{SUFFIX}.html")
p.write_text(html)
zoom = html.replace(f'width="{W}" height="{H}" viewBox="0 0 {W} {H}"', 'width="1005" height="828" viewBox="586 66 335 276"')
Path(__file__).with_name(f"obssynth4d_mockup_wt{SUFFIX}.html").write_text(zoom)
zoom = html.replace(f'width="{W}" height="{H}" viewBox="0 0 {W} {H}"', 'width="1005" height="828" viewBox="921 66 335 276"')
Path(__file__).with_name(f"obssynth4d_mockup_fm{SUFFIX}.html").write_text(zoom)
# the algorithm sheet, printed on the panel as the DX7 does
out.clear()
add(f'<rect width="1000" height="300" fill="{T['bg']}"/>')
panel(10, 10, 980, 280, "FM ALGORITHMS", "C carrier · M modulator · 2-4 operators")
for k, a in enumerate(ALGOS):
    cx = 110 + k * 195
    algo_diagram(cx, 240, a, box=26, step=36, lit=k == 4)
    add(f'<text x="{cx}" y="{278}" class="lbl" text-anchor="middle" fill="{ACCENT if k==4 else INK}">{k+1} · {a[0]}</text>')
sheet = f"""<!DOCTYPE html><html><head><meta charset="utf-8"><style>html,body{{margin:0;background:{T["bg"]}}}{style}</style></head>
<body><svg xmlns="http://www.w3.org/2000/svg" width="1000" height="300" viewBox="0 0 1000 300">{defs}{''.join(out)}</svg></body></html>"""
Path(__file__).with_name(f"fm_algorithms{SUFFIX}.html").write_text(sheet)
print(p)
