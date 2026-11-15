"""
Generate obssynth.ico with synthwave palette from the OBSSYNTH poster.
Palette: dark navy bg, teal sun halo, yellow/orange core, red road, purple mountains.
Sizes: 256, 128, 64, 48, 32, 16.
"""
from PIL import Image, ImageDraw, ImageFont
import math, os

SIZES = [256, 128, 64, 48, 32, 16]

# ── Synthwave palette ──────────────────────────────────────────────────────────
BG_TOP    = (14,  15,  48)   # dark navy top
BG_BOT    = (60,  20,  60)   # deep purple bottom
SUN_OUTER = (42, 195, 190)   # teal halo
SUN_MID   = (230, 130,  40)  # orange band
SUN_CORE  = (248, 232,  60)  # bright yellow core
ROAD_L    = (190,  50,  40)  # road red-orange
MTN       = ( 70,  30,  90)  # purple mountain
ACCENT    = (240, 100,  50)  # warm orange accent (text shadow / glow)
WHITE     = (240, 240, 255)


def lerp_color(c1, c2, t):
    return tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))


def draw_icon(size):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d   = ImageDraw.Draw(img)
    s   = size

    # ── Background gradient (top navy → bottom purple) ────────────────────────
    for y in range(s):
        t = y / s
        c = lerp_color(BG_TOP, BG_BOT, t)
        d.line([(0, y), (s, y)], fill=c + (255,))

    # ── Sun ───────────────────────────────────────────────────────────────────
    cx   = s // 2
    # Horizon sits at ~58% height
    hor  = int(s * 0.58)
    # Sun is clipped at horizon — draw only top semicircle
    r_out  = int(s * 0.38)
    r_mid  = int(s * 0.26)
    r_core = int(s * 0.14)
    sun_cy = hor  # centre on horizon line

    # Teal outer halo
    bbox_out = [cx - r_out, sun_cy - r_out, cx + r_out, sun_cy + r_out]
    d.ellipse(bbox_out, fill=SUN_OUTER + (255,))

    # Orange mid
    bbox_mid = [cx - r_mid, sun_cy - r_mid, cx + r_mid, sun_cy + r_mid]
    d.ellipse(bbox_mid, fill=SUN_MID + (255,))

    # Yellow core
    bbox_core = [cx - r_core, sun_cy - r_core, cx + r_core, sun_cy + r_core]
    d.ellipse(bbox_core, fill=SUN_CORE + (255,))

    # Horizontal scan lines across the sun (retro synthwave lines)
    if size >= 32:
        n_lines = max(4, r_out // 6)
        for li in range(n_lines):
            ly = sun_cy - r_out + (li * 2 * r_out // n_lines)
            if ly >= sun_cy:  # only above horizon
                break
            # clip to circle width at this y
            dy   = abs(ly - sun_cy)
            half = int(math.sqrt(max(0, r_out * r_out - dy * dy)))
            alpha = 160
            d.line([(cx - half, ly), (cx + half, ly)],
                   fill=BG_BOT + (alpha,), width=max(1, s // 80))

    # Clip sun below horizon (fill rect to erase lower half)
    d.rectangle([0, hor, s, s], fill=(0, 0, 0, 0))

    # ── Mountains ─────────────────────────────────────────────────────────────
    # Left mountain
    ml = [
        (0,       hor),
        (int(s * 0.05), int(s * 0.30)),
        (int(s * 0.32), hor),
    ]
    d.polygon(ml, fill=MTN + (255,))
    # Right mountain
    mr = [
        (s,       hor),
        (int(s * 0.95), int(s * 0.30)),
        (int(s * 0.68), hor),
    ]
    d.polygon(mr, fill=MTN + (255,))

    # ── Ground / road ─────────────────────────────────────────────────────────
    # Ground fill dark
    d.rectangle([0, hor, s, s], fill=(20, 8, 28, 255))

    # Road trapezoid (perspective)
    vp_x = cx  # vanishing point x
    vp_y = hor
    road_w_bot = int(s * 0.45)
    road_w_top = max(2, int(s * 0.03))
    road = [
        (vp_x - road_w_top, vp_y),
        (vp_x + road_w_top, vp_y),
        (vp_x + road_w_bot, s),
        (vp_x - road_w_bot, s),
    ]
    d.polygon(road, fill=ROAD_L + (200,))

    # Road centre dashes
    if size >= 32:
        for seg in range(4):
            t0 = 0.15 + seg * 0.22
            t1 = t0 + 0.10
            y0 = int(vp_y + (s - vp_y) * t0)
            y1 = int(vp_y + (s - vp_y) * t1)
            w  = max(1, int(road_w_top + (road_w_bot - road_w_top) * (t0 + t1) * 0.5) // 8)
            d.line([(cx, y0), (cx, y1)], fill=WHITE + (200,), width=w)

    # ── "OS" text centred (large sizes only) ──────────────────────────────────
    if size >= 64:
        # Draw a simple "OS" glyph using rectangles (no font dependency)
        ts  = max(8, size // 5)
        tx  = cx - ts
        ty  = int(s * 0.08)
        # "O" — ring: outer filled circle minus inner
        ro = ts // 2
        ri = max(2, ro - max(2, ts // 6))
        d.ellipse([tx - ro, ty, tx + ro, ty + 2 * ro], fill=WHITE + (255,))
        d.ellipse([tx - ri, ty + (ro - ri), tx + ri, ty + (ro - ri) + 2 * ri],
                  fill=BG_TOP + (255,))
        # "S" — simplified: two rectangles + connectors
        sx  = cx + ts // 4
        # top bar
        d.rectangle([sx, ty, sx + ts, ty + ts // 6], fill=ACCENT + (255,))
        # mid bar
        d.rectangle([sx, ty + ts * 2 // 5, sx + ts, ty + ts // 2 + ts // 6], fill=ACCENT + (255,))
        # bot bar
        d.rectangle([sx, ty + ts * 4 // 5, sx + ts, ty + ts], fill=ACCENT + (255,))
        # left top connector
        d.rectangle([sx, ty, sx + ts // 6, ty + ts // 2], fill=ACCENT + (255,))
        # right bot connector
        d.rectangle([sx + ts - ts // 6, ty + ts // 2, sx + ts, ty + ts], fill=ACCENT + (255,))

    # ── Rounded-square clip mask ───────────────────────────────────────────────
    mask = Image.new("L", (s, s), 0)
    md   = ImageDraw.Draw(mask)
    r_corner = max(2, s // 8)
    md.rounded_rectangle([0, 0, s - 1, s - 1], radius=r_corner, fill=255)
    img.putalpha(mask)

    return img


def main():
    out_path = os.path.join(os.path.dirname(__file__), "obssynth.ico")
    frames = []
    for sz in SIZES:
        frames.append(draw_icon(sz).convert("RGBA"))

    # Save multi-size ICO
    frames[0].save(
        out_path,
        format="ICO",
        sizes=[(f.width, f.height) for f in frames],
        append_images=frames[1:],
    )
    print(f"Icon written → {out_path}  ({len(frames)} sizes: {SIZES})")

    # Also save a 256 PNG preview
    png_path = os.path.join(os.path.dirname(__file__), "obssynth_preview.png")
    frames[0].save(png_path)
    print(f"Preview PNG  → {png_path}")


if __name__ == "__main__":
    main()
