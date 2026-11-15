#!/usr/bin/env python3
"""ObsSynthCT1 (Cross Table One) front panel mock-ups, two layouts to compare, in the panel language of ObsSynth4D
and ObsDrums (obssynth4d_mockup.py, obsdrums_mockup.py). Five operators, each STD (a classic shape) or WT (a
wavetable and its position), cross-modulating through a 5 x 5 FM matrix (each cell: how much the row modulates the
column; the diagonal: feedback; OUT: who sounds); two effects and the master.
  --layout grid  a 3 x 3 grid: OP1 OP2 OP3 / OP4 FM OP5 / FX1 FX2 MASTER
  --layout strip the five operators across the top, the FM matrix in the middle, the effects and master under it
Writes obssynthct1_<layout>[_carbonian].html (1600x900, inline SVG); render with
  chromium --headless --screenshot=out.png --window-size=1600,900 obssynthct1_grid.html"""
import math
import sys
from pathlib import Path

W, H = 1600, 900
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
LAYOUT = sys.argv[sys.argv.index("--layout") + 1] if "--layout" in sys.argv else "grid"
out = []


def add(s):
    out.append(s)


def panel(x, y, w, h, title=None, sub=""):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="url(#panel)" stroke="#0b0b0c" stroke-width="2"/>')
    add(f'<rect x="{x+1}" y="{y+1}" width="{w-2}" height="{h-2}" rx="5" fill="none" stroke="#ffffff" stroke-opacity=".06"/>')
    if title:
        add(f'<text x="{x+12}" y="{y+22}" class="title">{title}</text>')
        if sub:
            add(f'<text x="{x+w-12}" y="{y+22}" class="sub" text-anchor="end">{sub}</text>')
        add(f'<line x1="{x+10}" y1="{y+31}" x2="{x+w-10}" y2="{y+31}" stroke="#000" stroke-opacity=".55"/>')


def knob(cx, cy, r, v, label, value=None, halo=None):
    a0, a1 = -135, 135
    for i in range(11):
        a = math.radians(a0 + (a1 - a0) * i / 10 - 90)
        r1, r2 = r + 3, r + (7 if i in (0, 5, 10) else 5)
        add(f'<line x1="{cx+r1*math.cos(a):.1f}" y1="{cy+r1*math.sin(a):.1f}" x2="{cx+r2*math.cos(a):.1f}" '
            f'y2="{cy+r2*math.sin(a):.1f}" stroke="{DIM}" stroke-width="1"/>')
    if halo:
        a_from, a_to = a0 + (a1 - a0) * v, a0 + (a1 - a0) * min(1, max(0, v + halo))
        R = r + 10
        p0 = (cx + R * math.cos(math.radians(a_from - 90)), cy + R * math.sin(math.radians(a_from - 90)))
        p1 = (cx + R * math.cos(math.radians(a_to - 90)), cy + R * math.sin(math.radians(a_to - 90)))
        add(f'<path d="M{p0[0]:.1f},{p0[1]:.1f} A{R},{R} 0 0,{1 if a_to > a_from else 0} {p1[0]:.1f},{p1[1]:.1f}" fill="none" '
            f'stroke="{ACCENT}" stroke-opacity=".6" stroke-width="2.5" stroke-linecap="round"/>')
    add(f'<circle cx="{cx}" cy="{cy+2}" r="{r+1}" fill="#000" opacity=".5" filter="url(#blur)"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#knobbody)" stroke="#050505"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r*0.62:.1f}" fill="url(#cap)" stroke="#111"/>')
    a = math.radians(a0 + (a1 - a0) * v - 90)
    add(f'<line x1="{cx+r*0.15*math.cos(a):.1f}" y1="{cy+r*0.15*math.sin(a):.1f}" x2="{cx+r*0.9*math.cos(a):.1f}" '
        f'y2="{cy+r*0.9*math.sin(a):.1f}" stroke="#f4f2ec" stroke-width="{max(1.4, r/10):.1f}" stroke-linecap="round"/>')
    add(f'<text x="{cx}" y="{cy+r+16}" class="lbl" text-anchor="middle">{label}</text>')
    if value:
        add(f'<text x="{cx}" y="{cy+r+27}" class="val" text-anchor="middle">{value}</text>')


def fader(x, y, h, v, meter=None):
    add(f'<rect x="{x-3}" y="{y}" width="6" height="{h}" rx="3" fill="#0a0a0b" stroke="#000"/>')
    for i in range(9):
        yy = y + h * i / 8
        add(f'<line x1="{x-11}" y1="{yy:.1f}" x2="{x-7}" y2="{yy:.1f}" stroke="{DIM}" stroke-width=".8"/>')
    if meter is not None:
        add(f'<rect x="{x+9}" y="{y}" width="4" height="{h}" fill="{T["display"]}" stroke="#000" stroke-width=".5"/>')
        add(f'<rect x="{x+9}" y="{y+h*(1-meter):.1f}" width="4" height="{h*meter:.1f}" fill="{T["trace_hi"]}"/>')
    ky = y + h * (1 - v)
    add(f'<rect x="{x-10}" y="{ky-6:.1f}" width="20" height="12" rx="2" fill="url(#cap)" stroke="#111"/>')


def button(x, y, w, h, txt, on=False, cls="btn"):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{T["btn_on"] if on else T["btn_off"]}" '
        f'stroke="{ACCENT if on else "#000"}" stroke-opacity="{1 if on else .8}"/>')
    add(f'<text x="{x+w/2}" y="{y+h/2+3.5}" class="{cls}" text-anchor="middle" fill="{ACCENT if on else DIM}">{txt}</text>')


def wave(kind, ph):
    s = math.sin(ph)
    return {"sin": s, "saw": ((ph / math.pi) % 2) - 1, "sq": 1 if s >= 0 else -1, "tri": 2 * abs(((ph / math.pi) % 2) - 1) - 1}[kind]


def display_std(x, y, w, h, kind):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{T["display"]}" stroke="#000"/>')
    pts = [f"{x+6+t*(w-12)/119:.1f},{y+h/2 - 0.36*h*wave(kind, t/119*4*math.pi):.1f}" for t in range(120)]
    add(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{ACCENT}" stroke-width="1.6"/>')
    add(f'<text x="{x+8}" y="{y+13}" class="lcdsmall">{kind.upper()}</text>')


def display_wt(x, y, w, h, seed, pos):
    """a wavetable: its frames stacked in depth, the one at the position lit, the one playing (the matrix) brighter"""
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{T["display"]}" stroke="#000"/>')
    n, fw, amp = 10, w * 0.62, h * 0.16

    def frame(k):
        t = k / (n - 1)
        return [math.sin(i / 59 * 2 * math.pi) * (1 - t) + math.sin(i / 59 * 2 * math.pi * (2 + seed) + t * 3) * t * 0.7
                + 0.25 * math.sin(i / 59 * 2 * math.pi * (5 + seed)) * t * t for i in range(60)]

    for k in range(n - 1, -1, -1):
        bx, by = x + 8 + k * (w - fw - 16) / (n - 1), y + h - 12 - k * (h - 34) / (n - 1)
        f = frame(k)
        lit = k == round(pos * (n - 1))
        pts = " ".join(f"{bx+i*fw/59:.1f},{by-amp*v:.1f}" for i, v in enumerate(f))
        add(f'<polyline points="{pts}" fill="none" stroke="{ACCENT if lit else T["trace"]}" stroke-width="{2 if lit else 1}" '
            f'opacity="{1 if lit else .35 + .4*(1-k/n)}"/>')
    add(f'<text x="{x+8}" y="{y+13}" class="lcdsmall">◀ {["BASIC", "VOX", "GLASS", "ORGAN", "METAL"][seed]} ▶</text>')
    add(f'<text x="{x+w-8}" y="{y+13}" class="lcdsmall" text-anchor="end">{round(pos*31)+1:02d} / 32</text>')


OPS = [  # type, wave or table seed, position, ratio, level, out, colour of its row in the matrix
    ("WT", 1, 0.35, "1.00", 0.8, True),
    ("STD", "saw", 0, "2.00", 0.55, False),
    ("WT", 2, 0.7, "3.50", 0.4, False),
    ("STD", "sin", 0, "0.50", 0.65, True),
    ("WT", 4, 0.2, "7.13", 0.3, False),
]


def operator(i, x, y, w, h):
    typ, src, pos, ratio, level, outp = OPS[i]
    panel(x, y, w, h, f"OP {i+1}", "CARRIER" if outp else "MODULATOR")
    button(x + 12, y + 40, 46, 18, "STD", typ == "STD", "eng")
    button(x + 62, y + 40, 46, 18, "WT", typ == "WT", "eng")
    dh = min(96, h * 0.36)
    if typ == "WT":
        display_wt(x + 12, y + 64, w - 24, dh, src, pos)
    else:
        display_std(x + 12, y + 64, w - 24, dh, src)
    ky = y + 64 + dh + 34
    n = 5
    step = (w - 24) / n
    labels = [("RATIO", 0.45, ratio), ("FINE", 0.5, "+0"), ("POSITION" if typ == "WT" else "WAVE", pos if typ == "WT" else 0.2, None),
              ("LEVEL", level, None), ("PAN", 0.5, "C")]
    for k, (lab, v, val) in enumerate(labels):
        knob(x + 12 + step * (k + 0.5), ky, 14 if k else 17, v, lab, val, halo=0.2 if lab == "POSITION" else None)
    # the envelope: a little ADSR with its four faders' worth shown as a curve
    ey = ky + 50
    if y + h - ey > 40:
        eh = y + h - ey - 12
        add(f'<rect x="{x+12}" y="{ey}" width="{w-24}" height="{eh}" rx="3" fill="{T["display"]}" stroke="#000"/>')
        a, d, s, r = 0.08, 0.22, 0.55, 0.3
        X = lambda t: x + 18 + t * (w - 36)
        Y = lambda v: ey + eh - 6 - v * (eh - 14)
        add(f'<polyline points="{X(0)},{Y(0)} {X(a)},{Y(1)} {X(a+d)},{Y(s)} {X(1-r)},{Y(s)} {X(1)},{Y(0)}" fill="none" '
            f'stroke="{T["trace_hi"]}" stroke-width="1.5"/>')
        add(f'<text x="{x+18}" y="{ey+12}" class="lcdsmall">ENV · {"LEVEL" if outp else "INDEX"}</text>')


def matrix(x, y, w, h):
    panel(x, y, w, h, "FM", "ROW → COLUMN · DIAGONAL: FEEDBACK")
    # the algorithm, a start
    add(f'<rect x="{x+12}" y="{y+40}" width="{w-24}" height="26" rx="3" fill="{T["display"]}" stroke="#000"/>')
    add(f'<text x="{x+24}" y="{y+58}" class="lcd" style="font-size:12px">‹</text><text x="{x+w-24}" y="{y+58}" class="lcd" style="font-size:12px" text-anchor="end">›</text>')
    add(f'<text x="{x+w/2}" y="{y+58}" class="lcd" style="font-size:12px" text-anchor="middle">ALGO 07 · TWO PAIRS + ONE</text>')
    cells = {(1, 0): 0.6, (2, 3): 0.45, (4, 0): 0.25, (4, 3): 0.3, (1, 1): 0.15, (0, 2): 0.2}
    labels = ["1", "2", "3", "4", "5", "OUT"]
    gx, gy = x + 46, y + 92
    cs = min((w - 70) / 6, (h - 116) / 5.6)
    for c, lab in enumerate(labels):
        add(f'<text x="{gx+c*cs+cs/2}" y="{gy-6}" class="lbl" text-anchor="middle" fill="{ACCENT if lab == "OUT" else INK}">{lab}</text>')
    for r in range(5):
        add(f'<text x="{gx-14}" y="{gy+r*cs+cs/2+4}" class="lbl" text-anchor="middle">{r+1}</text>')
        for c in range(6):
            cx, cy = gx + c * cs, gy + r * cs
            diag = c == r
            add(f'<rect x="{cx+2}" y="{cy+2}" width="{cs-4}" height="{cs-4}" rx="3" fill="{T["row"] if diag else T["btn_off"]}" stroke="#000"/>')
            v = (OPS[r][4] if OPS[r][5] else 0) if c == 5 else cells.get((r, c), 0)
            if v:
                add(f'<circle cx="{cx+cs/2}" cy="{cy+cs/2}" r="{3+v*(cs/2-6):.1f}" fill="{ACCENT}" opacity="{0.35+0.6*v:.2f}"/>')
                add(f'<text x="{cx+cs-5}" y="{cy+cs-5}" class="val" text-anchor="end" style="font-size:7px">{int(v*100)}</text>')


def fx(x, y, w, h, title, knobs_):
    panel(x, y, w, h, title)
    n = len(knobs_)
    step = (w - 24) / n
    for k, (lab, v) in enumerate(knobs_):
        knob(x + 12 + step * (k + 0.5), y + h / 2 + 6, 17, v, lab)


def master(x, y, w, h):
    panel(x, y, w, h, "MASTER")
    knob(x + 50, y + h / 2 + 4, 20, 0.7, "VOLUME", "-3 dB")
    knob(x + 120, y + h / 2 + 4, 15, 0.3, "GLIDE")
    knob(x + 180, y + h / 2 + 4, 15, 0.5, "VOICES", "8")
    for side in range(2):
        mx = x + w - 60 + side * 24
        add(f'<rect x="{mx}" y="{y+42}" width="12" height="{h-60}" fill="{T["display"]}" stroke="#000"/>')
        add(f'<rect x="{mx}" y="{y+42+(h-60)*0.3:.1f}" width="12" height="{(h-60)*0.7:.1f}" fill="{ACCENT}" opacity=".85"/>')


# ── the top bar (the products' own: name and number, the display, SAVE, CC, SETUP) ──────────────
add(f'<rect width="{W}" height="{H}" fill="{T["bg"]}"/>')
add(f'<rect width="{W}" height="58" fill="url(#bar)"/><line x1="0" y1="58" x2="{W}" y2="58" stroke="#000"/>')
add(f'<text x="24" y="38" class="brand">OBSSYNTH</text><text x="183" y="38" class="brand4d">CT1</text>')
for i, (t, on) in enumerate([("MAIN", True), ("MOD MATRIX", False)]):
    add(f'<text x="{330+i*130}" y="36" class="tab" fill="{INK if on else DIM}" text-anchor="middle">{t}</text>')
add(f'<rect x="310" y="44" width="40" height="2" fill="{ACCENT}"/>')
add(f'<rect x="690" y="11" width="420" height="36" rx="4" fill="{T["display"]}" stroke="#000"/>')
add(f'<text x="900" y="24" class="lcdsmall" text-anchor="middle">FACTORY 03 / 24  ·  CC LAUNCHKEY 49  ·  CROSS</text>')
add(f'<text x="707" y="41" class="lcd">‹</text><text x="1093" y="41" class="lcd" text-anchor="end">›</text>')
add(f'<text x="900" y="42" class="lcd" text-anchor="middle">GLASS ORGAN</text>')
button(1124, 16, 64, 26, "SAVE")
button(1198, 16, 64, 26, "CC")
button(1272, 16, 74, 26, "SETUP")
add(f'<text x="1534" y="34" class="lbl" text-anchor="end" style="font-size:10px">MASTER</text>')
knob(1562, 29, 13, 0.7, "")

if LAYOUT == "grid":
    X0, Y0, G = 14, 70, 10
    cw, rh = (W - 2 * X0 - 2 * G) / 3, [300, 300, H - 70 - 600 - 2 * G - 12]
    ys = [Y0, Y0 + rh[0] + G, Y0 + rh[0] + rh[1] + 2 * G]
    cell = lambda c, r: (X0 + c * (cw + G), ys[r], cw, rh[r])
    operator(0, *cell(0, 0))
    operator(1, *cell(1, 0))
    operator(2, *cell(2, 0))
    operator(3, *cell(0, 1))
    matrix(*cell(1, 1))
    operator(4, *cell(2, 1))
    fx(*cell(0, 2), "FX 1 · FILTER · DRIVE", [("CUTOFF", 0.6), ("RESO", 0.3), ("DRIVE", 0.4), ("MIX", 0.7)])
    fx(*cell(1, 2), "FX 2 · DELAY · REVERB", [("TIME", 0.4), ("FDBK", 0.35), ("SIZE", 0.6), ("MIX", 0.3)])
    master(*cell(2, 2))
else:
    X0, Y0, G = 14, 70, 8
    ow = (W - 2 * X0 - 4 * G) / 5
    for i in range(5):
        operator(i, X0 + i * (ow + G), Y0, ow, 380)
    my = Y0 + 380 + G
    mh = 280
    matrix(X0, my, 560, mh + 132)
    fx(X0 + 568, my, 500, mh / 2 + 60, "FX 1 · FILTER · DRIVE", [("CUTOFF", 0.6), ("RESO", 0.3), ("DRIVE", 0.4), ("MIX", 0.7)])
    fx(X0 + 568, my + mh / 2 + 68, 500, mh / 2 + 64, "FX 2 · DELAY · REVERB", [("TIME", 0.4), ("FDBK", 0.35), ("SIZE", 0.6), ("MIX", 0.3)])
    master(X0 + 1076, my, W - X0 - 1076 - X0, mh + 132)

style = f"""
text{{font-family:'Inter','DejaVu Sans',sans-serif}}
.title{{font-size:12px;letter-spacing:2.4px;fill:{INK};font-weight:700}}
.sub{{font-size:9px;letter-spacing:1.4px;fill:{DIM}}}
.lbl{{font-size:9px;letter-spacing:1.3px;fill:{INK};font-weight:600}}
.val{{font-size:9px;fill:{ACCENT};font-family:'JetBrains Mono','DejaVu Sans Mono',monospace}}
.btn{{font-size:10px;letter-spacing:1px;font-weight:700}}
.eng{{font-size:8px;letter-spacing:1.6px;font-weight:700}}
.tab{{font-size:12px;letter-spacing:2.5px;font-weight:700}}
.brand{{font-size:22px;letter-spacing:5px;fill:{INK};font-weight:300}}
.brand4d{{font-size:22px;fill:{ACCENT};font-weight:700}}
.lcd{{font-family:'JetBrains Mono','DejaVu Sans Mono',monospace;font-size:15px;fill:{ACCENT}}}
.lcdsmall{{font-family:'JetBrains Mono','DejaVu Sans Mono',monospace;font-size:9px;fill:{ACCENT};opacity:.7;letter-spacing:1.5px}}
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
p = Path(__file__).with_name(f"obssynthct1_{LAYOUT}{SUFFIX}.html")
p.write_text(html)
print(p)
