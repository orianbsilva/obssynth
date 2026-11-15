#!/usr/bin/env python3
"""ObsDrums front panel mock-up: a column per instrument, as a rhythm machine lays them out, in the panel language of
ObsSynth4D (obssynth4d_mockup.py). Each column: the engine (CIRCUIT modelled circuits, FM, VA, SAMPLE, GRAIN), a
thumbnail of the sound's shape, TUNE and DECAY and the model's own two, FX send and pan, the level fader,
mute / solo and the pad. On the right the master; underneath, the chosen instrument's sixteen steps (gridseq's).
Writes obsdrums_mockup[_carbonian].html (1600x900, inline SVG); render with
  chromium --headless --screenshot=out.png --window-size=1600,900 obsdrums_mockup.html"""
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


def knob(cx, cy, r, v, label, value=None, bipolar=False, halo=None):
    a0, a1 = -135, 135
    for i in range(11):
        a = math.radians(a0 + (a1 - a0) * i / 10 - 90)
        r1, r2 = r + 3, r + (7 if i in (0, 5, 10) else 5)
        add(f'<line x1="{cx+r1*math.cos(a):.1f}" y1="{cy+r1*math.sin(a):.1f}" x2="{cx+r2*math.cos(a):.1f}" '
            f'y2="{cy+r2*math.sin(a):.1f}" stroke="{DIM}" stroke-width="1"/>')
    if halo:  # the matrix on it: a band from the knob's value
        a_from, a_to = a0 + (a1 - a0) * v, a0 + (a1 - a0) * min(1, max(0, v + halo))
        R = r + 10
        p0 = (cx + R * math.cos(math.radians(a_from - 90)), cy + R * math.sin(math.radians(a_from - 90)))
        p1 = (cx + R * math.cos(math.radians(a_to - 90)), cy + R * math.sin(math.radians(a_to - 90)))
        add(f'<path d="M{p0[0]:.1f},{p0[1]:.1f} A{R},{R} 0 0,{1 if a_to > a_from else 0} {p1[0]:.1f},{p1[1]:.1f}" fill="none" '
            f'stroke="{ACCENT}" stroke-opacity=".6" stroke-width="2.5" stroke-linecap="round"/>')
    add(f'<circle cx="{cx}" cy="{cy+2}" r="{r+1}" fill="#000" opacity=".5" filter="url(#blur)"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#knobbody)" stroke="#050505"/>')
    for i in range(24):
        a = math.radians(i * 15)
        add(f'<line x1="{cx+(r-1)*math.cos(a):.1f}" y1="{cy+(r-1)*math.sin(a):.1f}" x2="{cx+(r*0.8)*math.cos(a):.1f}" '
            f'y2="{cy+(r*0.8)*math.sin(a):.1f}" stroke="#2a2a2c" stroke-width=".7"/>')
    add(f'<circle cx="{cx}" cy="{cy}" r="{r*0.62:.1f}" fill="url(#cap)" stroke="#111"/>')
    a = math.radians(a0 + (a1 - a0) * v - 90)
    add(f'<line x1="{cx+r*0.15*math.cos(a):.1f}" y1="{cy+r*0.15*math.sin(a):.1f}" x2="{cx+r*0.9*math.cos(a):.1f}" '
        f'y2="{cy+r*0.9*math.sin(a):.1f}" stroke="#f4f2ec" stroke-width="{max(1.4, r/10):.1f}" stroke-linecap="round"/>')
    add(f'<text x="{cx}" y="{cy+r+17}" class="lbl" text-anchor="middle">{label}</text>')
    if value:
        add(f'<text x="{cx}" y="{cy+r+28}" class="val" text-anchor="middle">{value}</text>')


def fader(x, y, h, v, meter=None):
    add(f'<rect x="{x-3}" y="{y}" width="6" height="{h}" rx="3" fill="#0a0a0b" stroke="#000"/>')
    for i in range(9):
        yy = y + h * i / 8
        add(f'<line x1="{x-11}" y1="{yy:.1f}" x2="{x-7}" y2="{yy:.1f}" stroke="{DIM}" stroke-width=".8"/>')
    if meter is not None:  # the level now, beside the fader
        add(f'<rect x="{x+9}" y="{y}" width="4" height="{h}" fill="{T["display"]}" stroke="#000" stroke-width=".5"/>')
        add(f'<rect x="{x+9}" y="{y+h*(1-meter):.1f}" width="4" height="{h*meter:.1f}" fill="{T["trace_hi"]}"/>')
    ky = y + h * (1 - v)
    add(f'<rect x="{x-10}" y="{ky-6:.1f}" width="20" height="12" rx="2" fill="url(#cap)" stroke="#111"/>')
    add(f'<line x1="{x-8}" y1="{ky:.1f}" x2="{x+8}" y2="{ky:.1f}" stroke="#f4f2ec" stroke-width="1.4"/>')


def button(x, y, w, h, txt, on=False, cls="btn"):
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{T["btn_on"] if on else T["btn_off"]}" '
        f'stroke="{ACCENT if on else "#000"}" stroke-opacity="{1 if on else .8}"/>')
    add(f'<text x="{x+w/2}" y="{y+h/2+3.5}" class="{cls}" text-anchor="middle" fill="{ACCENT if on else DIM}">{txt}</text>')


def shape(x, y, w, h, kind, lit=False):
    """the sound's shape in a small display: the envelope over the waveform's character"""
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{T["display"]}" stroke="#000"/>')
    mid, n = y + h / 2, 90
    pts = []
    for i in range(n):
        t = i / (n - 1)
        if kind == "kick":
            env, f = math.exp(-t * 4.5), 18 * (1 - math.exp(-t * 9)) + 4 * t  # the pitch falling: the cycles widen
            s = math.sin(f * 1.7) * env
        elif kind == "snare":
            env = math.exp(-t * 7)
            s = (0.5 * math.sin(i * 0.9) * math.exp(-t * 11) + 0.6 * math.sin(i * i * 0.77) * env)
        elif kind == "clap":
            bursts = sum(math.exp(-(t - b) * 45) if t >= b else 0.0 for b in (0.02, 0.07, 0.12))
            env = min(1, bursts) * 0.9 + math.exp(-max(0, t - 0.14) * 6) * (t > 0.14) * 0.7
            s = math.sin(i * i * 1.31) * env
        elif kind == "rim":
            s = math.sin(i * 2.3) * math.exp(-t * 30)
        elif kind == "tom":
            env, f = math.exp(-t * 3.2), 9 * (1 - math.exp(-t * 5)) + 3 * t
            s = math.sin(f * 2.2) * env
        elif kind in ("chh", "ohh", "crash", "ride"):
            d = {"chh": 22, "ohh": 4.5, "crash": 2.4, "ride": 3.2}[kind]
            s = math.sin(i * i * 0.53 + i * 2.1) * math.exp(-t * d) * (0.85 if kind != "ride" else 0.6) \
                + (0.25 * math.sin(i * 1.9) * math.exp(-t * 2) if kind == "ride" else 0)
        else:  # fm perc
            s = math.sin(i * 0.8 + 3 * math.exp(-t * 8) * math.sin(i * 2.9)) * math.exp(-t * 5)
        pts.append(f"{x+3+t*(w-6):.1f},{mid - s*(h/2-4):.1f}")
    add(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{T["trace_hi"] if lit else T["trace"]}" stroke-width="1.1" '
        f'opacity="{1 if lit else .85}"/>')


# ── the top bar ─────────────────────────────────────────────────────────────────────────────────
add(f'<rect width="{W}" height="{H}" fill="{T["bg"]}"/>')
add(f'<rect width="{W}" height="58" fill="url(#bar)"/><line x1="0" y1="58" x2="{W}" y2="58" stroke="#000"/>')
add(f'<text x="24" y="38" class="brand">OBSDRUMS</text>')
add(f'<text x="210" y="38" class="brand4d">16</text>')
for i, (t, on) in enumerate([("KIT", True), ("MOD MATRIX", False), ("FX", False)]):
    add(f'<text x="{300+i*120}" y="36" class="tab" fill="{INK if on else DIM}" text-anchor="middle">{t}</text>')
add(f'<rect x="282" y="44" width="40" height="2" fill="{ACCENT}"/>')
add(f'<rect x="690" y="11" width="420" height="36" rx="4" fill="{T["display"]}" stroke="#000"/>')
add(f'<text x="900" y="24" class="lcdsmall" text-anchor="middle">KIT 07 / 32  ·  128 BPM  ·  GRIDSEQ</text>')
add(f'<text x="707" y="41" class="lcd">‹</text><text x="1093" y="41" class="lcd" text-anchor="end">›</text>')
add(f'<text x="900" y="42" class="lcd" text-anchor="middle">NIGHT SHIFT</text>')
button(1124, 16, 64, 26, "SAVE")
add(f'<text x="1300" y="34" class="lbl" text-anchor="end">SWING</text>')
knob(1330, 29, 14, 0.3, "")
add(f'<text x="1430" y="34" class="lbl" text-anchor="end">MASTER</text>')
knob(1462, 29, 15, 0.8, "")
add(f'<circle cx="1530" cy="29" r="9" fill="{ACCENT}" opacity=".9"/><text x="1546" y="34" class="sub">PLAY</text>')

# ── the instruments: a column each ──────────────────────────────────────────────────────────────
INSTR = [  # name, note, engine, shape, model knobs (label, value), level, lit (hit now), mute, solo, halo on tune
    ("KICK", 36, "CIRCUIT", "kick", [("ATTACK", .55), ("DRIVE", .4)], .82, True, False, False, .18),
    ("SNARE", 38, "CIRCUIT", "snare", [("TONE", .45), ("SNAPPY", .7)], .7, False, False, False, None),
    ("CLAP", 39, "CIRCUIT", "clap", [("SPREAD", .5), ("TONE", .6)], .6, False, False, False, None),
    ("RIM", 37, "CIRCUIT", "rim", [("TONE", .7), ("CLICK", .5)], .45, False, True, False, None),
    ("LOW TOM", 41, "CIRCUIT", "tom", [("SWEEP", .4), ("NOISE", .2)], .55, False, False, False, None),
    ("MID TOM", 45, "CIRCUIT", "tom", [("SWEEP", .4), ("NOISE", .2)], .55, False, False, False, None),
    ("HI TOM", 50, "SAMPLE", "tom", [("START", .1), ("FILTER", .8)], .5, False, False, False, None),
    ("CL HAT", 42, "CIRCUIT", "chh", [("TONE", .65), ("METAL", .5)], .62, True, False, False, None),
    ("OP HAT", 46, "CIRCUIT", "ohh", [("TONE", .65), ("METAL", .5)], .55, False, False, False, None),
    ("CRASH", 49, "SAMPLE", "crash", [("START", 0), ("FILTER", .9)], .4, False, False, False, None),
    ("RIDE", 51, "GRAIN", "ride", [("SIZE", .3), ("SPRAY", .25)], .45, False, False, False, None),
    ("FM PERC", 56, "FM", "fm", [("RATIO", .35), ("INDEX", .6)], .5, False, False, True, .1),
]
X0, CW, Y0, CH = 14, 116, 72, 700
for k, (name, note, engine, kind, model, level, lit, mute, solo, halo) in enumerate(INSTR):
    x = X0 + k * CW
    panel(x, Y0, CW - 6, CH)
    cx = x + (CW - 6) / 2
    add(f'<text x="{x+10}" y="{Y0+20}" class="title" style="font-size:11px;letter-spacing:1.6px">{name}</text>')
    add(f'<text x="{x+CW-16}" y="{Y0+20}" class="sub" text-anchor="end">{note}</text>')
    # the engine: a tag that cycles (CIRCUIT FM VA SAMPLE GRAIN)
    button(x + 8, Y0 + 30, CW - 22, 17, engine, True, "eng")
    shape(x + 8, Y0 + 54, CW - 22, 44, kind, lit)
    knob(cx, Y0 + 140, 21, [0.45, 0.5, 0.5, 0.6, 0.35, 0.45, 0.55, 0.6, 0.6, 0.5, 0.55, 0.4][k], "TUNE", halo=halo)
    knob(cx, Y0 + 222, 16, [0.6, 0.45, 0.5, 0.2, 0.55, 0.5, 0.45, 0.15, 0.6, 0.8, 0.7, 0.5][k], "DECAY")
    knob(x + 28, Y0 + 290, 12, model[0][1], model[0][0])
    knob(x + CW - 34, Y0 + 290, 12, model[1][1], model[1][0])
    knob(x + 28, Y0 + 352, 11, [0.1, 0.25, 0.3, 0, 0.15, 0.15, 0.15, 0.05, 0.2, 0.35, 0.3, 0.4][k], "FX")
    knob(x + CW - 34, Y0 + 352, 11, [0.5, 0.5, 0.55, 0.4, 0.3, 0.5, 0.7, 0.45, 0.55, 0.6, 0.65, 0.5][k], "PAN", bipolar=True)
    fader(cx - 4, Y0 + 396, 200, level, meter=(0.78 if lit else (0.05 if not mute else 0)) if k != 7 else 0.6)
    button(x + 8, Y0 + 616, (CW - 26) / 2, 18, "M", mute)
    button(x + 12 + (CW - 26) / 2, Y0 + 616, (CW - 26) / 2, 18, "S", solo)
    # the pad
    add(f'<rect x="{x+8}" y="{Y0+642}" width="{CW-22}" height="48" rx="5" fill="{ACCENT if lit else T["btn_off"]}" '
        f'fill-opacity="{0.85 if lit else 1}" stroke="#000"/>')
    if lit:
        add(f'<rect x="{x+4}" y="{Y0+638}" width="{CW-14}" height="56" rx="7" fill="{ACCENT}" opacity=".18" filter="url(#blur)"/>')
    add(f'<text x="{cx}" y="{Y0+671}" class="btn" text-anchor="middle" fill="{T["bg"] if lit else DIM}">{["Z","S","X","D","C","V","G","B","H","N","J","M"][k]}</text>')
# the hats' choke: a bracket over the two columns
hx = X0 + 7 * CW
add(f'<path d="M{hx+10},{Y0+CH+6} v6 h{2*CW-26} v-6" fill="none" stroke="{ACCENT}" stroke-opacity=".7"/>')
add(f'<text x="{hx+CW-3}" y="{Y0+CH+24}" class="sub" text-anchor="middle" fill="{ACCENT}">CHOKE</text>')

# ── the master ──────────────────────────────────────────────────────────────────────────────────
mx = X0 + 12 * CW
panel(mx, Y0, W - mx - 14, CH, "MASTER")
add(f'<text x="{mx+14}" y="{Y0+58}" class="sub">DELAY</text>')
knob(mx + 34, Y0 + 92, 17, 0.45, "TIME", "3/16")
knob(mx + 90, Y0 + 92, 17, 0.35, "FDBK")
knob(mx + 146, Y0 + 92, 14, 0.25, "MIX")
add(f'<text x="{mx+14}" y="{Y0+168}" class="sub">REVERB</text>')
knob(mx + 34, Y0 + 202, 17, 0.55, "SIZE")
knob(mx + 90, Y0 + 202, 17, 0.4, "DAMP")
knob(mx + 146, Y0 + 202, 14, 0.2, "MIX")
add(f'<text x="{mx+14}" y="{Y0+278}" class="sub">TUBE · GLUE</text>')
knob(mx + 34, Y0 + 312, 17, 0.35, "DRIVE")
knob(mx + 90, Y0 + 312, 17, 0.5, "GLUE")
knob(mx + 146, Y0 + 312, 14, 0.3, "MIX")
add(f'<text x="{mx+14}" y="{Y0+388}" class="sub">OUT</text>')
fader(mx + 60, Y0 + 400, 220, 0.8, meter=0.72)
fader(mx + 130, Y0 + 400, 220, 0.8, meter=0.69)
add(f'<text x="{mx+60}" y="{Y0+644}" class="lbl" text-anchor="middle">L</text><text x="{mx+130}" y="{Y0+644}" class="lbl" text-anchor="middle">R</text>')
add(f'<text x="{mx+95}" y="{Y0+676}" class="val" text-anchor="middle">-3.1 dB · LIM 0.0</text>')

# ── the steps: the chosen instrument's sixteen, as gridseq plays them ───────────────────────────
sy = Y0 + CH + 34
panel(X0, sy, W - 28, H - sy - 12)
add(f'<text x="{X0+14}" y="{sy+28}" class="title">KICK</text><text x="{X0+14}" y="{sy+44}" class="sub">STEPS · GRIDSEQ</text>')
steps = [1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0]
acc = [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0]
for i, on in enumerate(steps):
    bx = X0 + 140 + i * 88 + (i // 4) * 8
    playing = i == 4
    add(f'<rect x="{bx}" y="{sy+12}" width="78" height="36" rx="4" fill="{ACCENT if on else T["btn_off"]}" '
        f'fill-opacity="{0.9 if on else 1}" stroke="{T["trace_hi"] if playing else "#000"}" stroke-width="{2 if playing else 1}"/>')
    if acc[i]:
        add(f'<rect x="{bx+6}" y="{sy+16}" width="10" height="4" rx="1" fill="{T["bg"]}"/>')
    add(f'<text x="{bx+70}" y="{sy+43}" class="sub" text-anchor="end" fill="{T["bg"] if on else DIM}">{i+1}</text>')

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
p = Path(__file__).with_name(f"obsdrums_mockup{SUFFIX}.html")
p.write_text(html)
print(p)
