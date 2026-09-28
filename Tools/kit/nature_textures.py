"""Procedural PREVIEW textures for the nature kit's alpha-card materials (Pillow + numpy only).

These images are only used to render the contact sheets so that leaf / grass / flower cards read
like foliage instead of opaque quads. They are NOT exported with the GLBs and are not the game
textures (those live in SourceArt/Textures and are produced by the texture tools). Everything is
seeded, so the previews are deterministic.
"""
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

S = 512          # output size
SS = 2           # supersampling factor


def _canvas():
    return Image.new("RGBA", (S * SS, S * SS), (0, 0, 0, 0))


def _finish(img):
    img = img.resize((S, S), Image.LANCZOS)
    # un-premultiply-ish: bleed colour into transparent texels so mip/bilinear edges stay coloured
    a = np.asarray(img).astype(np.float32)
    rgb = a[..., :3]
    alpha = a[..., 3:4] / 255.0
    blur = np.asarray(Image.fromarray(a.astype(np.uint8)).filter(ImageFilter.GaussianBlur(6))).astype(np.float32)
    bal = blur[..., 3:4] / 255.0
    bleed = np.where(bal > 1e-3, blur[..., :3] / np.maximum(bal, 1e-3), rgb)
    rgb = np.where(alpha > 0.02, rgb / np.maximum(alpha, 1e-3) * alpha + rgb * 0, bleed)
    out = np.concatenate([np.clip(rgb, 0, 255), a[..., 3:4]], -1).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def _mix(c1, c2, t):
    return tuple(int(round(c1[i] + (c2[i] - c1[i]) * t)) for i in range(3))


def _jit(c, rng, amt=14):
    return tuple(max(0, min(255, int(x + rng.uniform(-amt, amt)))) for x in c)


def _leaf_poly(cx, cy, ang, length, width, lobes=0, tip=1.0, base_off=0.0):
    pts_l, pts_r = [], []
    n = 14
    ca, sa = math.cos(ang), math.sin(ang)
    for i in range(n + 1):
        t = i / n
        w = math.sin(math.pi * min(1.0, t * (0.98 + 0.02 * tip))) ** 0.75 * width * 0.5
        if lobes:
            w *= 0.78 + 0.22 * abs(math.sin(t * math.pi * lobes))
        x = (t - base_off) * length
        for sgn, arr in ((1, pts_l), (-1, pts_r)):
            px, py = x, sgn * w
            arr.append((cx + px * ca - py * sa, cy + px * sa + py * ca))
    return pts_l + pts_r[::-1]


def leaves(kind, seed):
    """Leaf-cluster card: a twig entering from the bottom centre with a clump of leaves."""
    rng = random.Random(seed)
    img = _canvas()
    d = ImageDraw.Draw(img)
    W = S * SS
    cfg = {
        "oak":   dict(n=150, L=(70, 110), w=(38, 58), lobes=3, dark=(34, 62, 22), light=(118, 158, 58), rad=0.40),
        "birch": dict(n=170, L=(46, 70), w=(30, 44), lobes=0, dark=(60, 96, 30), light=(170, 196, 84), rad=0.40),
        "giant": dict(n=110, L=(110, 160), w=(52, 78), lobes=0, dark=(22, 50, 22), light=(90, 140, 60), rad=0.42),
        "demon": dict(n=120, L=(80, 130), w=(22, 36), lobes=0, dark=(52, 18, 64), light=(176, 92, 196), rad=0.40),
        "jungle": dict(n=100, L=(120, 170), w=(56, 84), lobes=0, dark=(20, 58, 26), light=(96, 150, 64), rad=0.42),
    }[kind]
    cx, cy = W * 0.5, W * 0.50
    blobs = [(cx + rng.uniform(-0.18, 0.18) * W, cy + rng.uniform(-0.2, 0.16) * W, rng.uniform(0.18, 0.27) * W)
             for _ in range(7)]

    def inside(x, y):
        return any((x - bx) ** 2 + (y - by) ** 2 < br * br for bx, by, br in blobs)

    # twig
    twig = (78, 60, 44) if kind != "demon" else (40, 24, 36)
    d.line([(W * 0.5, W), (W * 0.5 + rng.uniform(-20, 20), W * 0.45)], fill=twig + (255,), width=10)
    items = []
    tries = 0
    while len(items) < cfg["n"] and tries < 20000:
        tries += 1
        x, y = rng.uniform(0.06, 0.94) * W, rng.uniform(0.05, 0.92) * W
        if not inside(x, y):
            continue
        # leaves radiate away from the twig line
        ang = math.atan2(y - W * 0.62, x - W * 0.5) + rng.uniform(-0.6, 0.6)
        L = rng.uniform(*cfg["L"]) * SS / 2 * 2
        w = rng.uniform(*cfg["w"]) * SS / 2 * 2
        depth = rng.random()
        items.append((depth, x, y, ang, L, w))
    items.sort()
    for depth, x, y, ang, L, w in items:
        up = 1.0 - y / W
        t = min(1.0, max(0.0, 0.25 + 0.55 * depth + 0.3 * (up - 0.5)))
        col = _jit(_mix(cfg["dark"], cfg["light"], t), rng, 10)
        if kind == "demon":
            poly = _leaf_poly(x, y, ang, L, w, 0, base_off=0.3)
            d.polygon(poly, fill=col + (255,))
            vein = _mix(col, (236, 120, 220), 0.6)
            d.line([(x - 0.3 * L * math.cos(ang), y - 0.3 * L * math.sin(ang)),
                    (x + 0.6 * L * math.cos(ang), y + 0.6 * L * math.sin(ang))], fill=vein + (255,), width=2 * SS)
        else:
            poly = _leaf_poly(x, y, ang, L, w, cfg["lobes"], base_off=0.25)
            d.polygon(poly, fill=col + (255,))
            vein = _mix(col, (20, 30, 10), 0.35)
            d.line([(x - 0.2 * L * math.cos(ang), y - 0.2 * L * math.sin(ang)),
                    (x + 0.65 * L * math.cos(ang), y + 0.65 * L * math.sin(ang))], fill=vein + (255,), width=SS)
    return _finish(img)


def needles(seed, snow=False):
    """Conifer bough spray: branch along V (bottom = trunk side), side twigs, needle strokes."""
    rng = random.Random(seed)
    img = _canvas()
    d = ImageDraw.Draw(img)
    W = S * SS
    dark, light = (22, 52, 34), (86, 132, 72)
    main = [(W * 0.5 + 8 * math.sin(t * 3.0) * SS, W * (1.0 - t)) for t in np.linspace(0.0, 0.97, 30)]
    d.line(main, fill=(70, 46, 30, 255), width=5 * SS)
    twigs = []
    for i in range(15):
        t = 0.05 + 0.9 * i / 15
        y = W * (1.0 - t)
        L = W * 0.50 * (1.0 - 0.7 * t) + 50
        for sgn in (-1, 1):
            ang = math.radians(56 + rng.uniform(-8, 8)) * sgn
            ex = W * 0.5 + math.sin(ang) * L
            ey = y - math.cos(abs(ang)) * L * 0.75
            twigs.append(((W * 0.5, y), (ex, ey)))
    for (sx, sy), (ex, ey) in twigs:
        d.line([(sx, sy), (ex, ey)], fill=(60, 40, 28, 255), width=2 * SS)
        n = 44
        for k in range(n):
            t = k / n
            px, py = sx + (ex - sx) * t, sy + (ey - sy) * t
            seg_ang = math.atan2(ey - sy, ex - sx)
            for sgn in (-1, 1):
                a = seg_ang + sgn * math.radians(rng.uniform(35, 65))
                ln = rng.uniform(26, 40) * SS * (1.0 - 0.35 * t)
                col = _jit(_mix(dark, light, rng.random() * 0.8 + 0.1 * (1 - t)), rng, 8)
                d.line([(px, py), (px + ln * math.cos(a), py + ln * math.sin(a))], fill=col + (255,), width=2 * SS)
    if snow:
        # lumpy snow lying along the twigs (chains of overlapping flattened blobs)
        for (sx, sy), (ex, ey) in twigs:
            if rng.random() < 0.4:
                continue
            k0 = rng.uniform(0.05, 0.3)
            k1 = rng.uniform(0.6, 0.95)
            for j in range(7):
                t = k0 + (k1 - k0) * j / 6
                px, py = sx + (ex - sx) * t, sy + (ey - sy) * t - 10 * SS
                r = rng.uniform(10, 19) * SS * (1.0 - 0.5 * abs(t - 0.5))
                c = _jit((238, 243, 250), rng, 5)
                d.ellipse([px - r, py - r * 0.6, px + r, py + r * 0.5], fill=_mix(c, (170, 188, 210), 0.45) + (255,))
                d.ellipse([px - r * 0.9, py - r * 0.75, px + r * 0.9, py + r * 0.2], fill=c + (255,))
    return _finish(img)


def blades(kind, seed):
    """Grass-type clump card: blades rooted along the bottom edge."""
    rng = random.Random(seed)
    img = _canvas()
    d = ImageDraw.Draw(img)
    W = S * SS
    cfg = {
        "grass": dict(n=70, dark=(30, 62, 20), light=(132, 178, 66), h=(0.55, 0.97), w=(10, 18)),
        "dry":   dict(n=60, dark=(120, 96, 54), light=(222, 200, 142), h=(0.5, 0.95), w=(8, 14)),
        "reeds": dict(n=46, dark=(58, 74, 34), light=(150, 164, 90), h=(0.7, 0.99), w=(10, 16)),
        "wheat": dict(n=46, dark=(150, 112, 46), light=(236, 200, 110), h=(0.62, 0.9), w=(5, 8)),
    }[kind]
    items = []
    for _ in range(cfg["n"]):
        x0 = W * (0.5 + rng.gauss(0, 0.2))
        h = rng.uniform(*cfg["h"]) * W
        lean = rng.uniform(-0.35, 0.35) * h + (x0 - W * 0.5) * 0.4
        items.append((rng.random(), x0, h, lean, rng.uniform(*cfg["w"]) * SS))
    items.sort()
    for depth, x0, h, lean, w in items:
        n = 12
        left, right = [], []
        for i in range(n + 1):
            t = i / n
            x = x0 + lean * t * t
            y = W - h * t
            ww = w * (1.0 - t) ** 0.8 + 0.5
            left.append((x - ww / 2, y))
            right.append((x + ww / 2, y))
        col_base = _mix(cfg["dark"], cfg["light"], 0.2 + 0.6 * depth)
        # gradient: draw in segments
        for i in range(n):
            t = (i + 0.5) / n
            col = _jit(_mix(cfg["dark"], col_base, min(1, t * 1.5)), rng, 5)
            d.polygon([left[i], right[i], right[i + 1], left[i + 1]], fill=col + (255,))
        if kind == "wheat":
            # ear: two rows of small grains along the top of the stalk, with awns
            tx, ty = (left[-1][0] + right[-1][0]) / 2, left[-1][1]
            bx, by = (left[-3][0] + right[-3][0]) / 2, left[-3][1]
            ang = math.atan2(ty - by, tx - bx)
            for g in range(12):
                s_ = g / 11
                gx = tx - math.cos(ang) * (s_ * 70 * SS)
                gy = ty - math.sin(ang) * (s_ * 70 * SS)
                col = _jit((222, 178, 86), rng, 12)
                for sgn in (-1, 1):
                    ox = sgn * 4 * SS
                    d.ellipse([gx + ox - 4 * SS, gy - 6 * SS, gx + ox + 4 * SS, gy + 6 * SS], fill=col + (255,))
                    d.line([(gx + ox, gy), (gx + ox + sgn * 10 * SS, gy - 26 * SS)], fill=(238, 214, 150, 255),
                           width=max(1, SS // 2))
        if kind == "reeds" and rng.random() < 0.18:
            tx, ty = left[-1][0], left[-1][1]
            d.rounded_rectangle([tx - 8 * SS, ty + 30 * SS, tx + 8 * SS, ty + 110 * SS], radius=8 * SS,
                                fill=_jit((96, 62, 36), rng, 8) + (255,))
    return _finish(img)


def flowers(seed):
    rng = random.Random(seed)
    img = _canvas()
    d = ImageDraw.Draw(img)
    W = S * SS
    # foliage at the bottom
    for _ in range(40):
        x0 = W * (0.5 + rng.gauss(0, 0.2))
        h = rng.uniform(0.25, 0.55) * W
        lean = rng.uniform(-0.3, 0.3) * h
        w = rng.uniform(10, 16) * SS
        col = _jit((60, 110, 40), rng, 16)
        d.polygon([(x0 - w / 2, W), (x0 + w / 2, W), (x0 + lean, W - h)], fill=col + (255,))
    palettes = [(248, 244, 236), (250, 214, 76), (226, 110, 150), (150, 110, 214), (110, 150, 236), (240, 128, 64)]
    for _ in range(26):
        x0 = W * (0.5 + rng.gauss(0, 0.22))
        h = rng.uniform(0.5, 0.92) * W
        tx = x0 + rng.uniform(-0.1, 0.1) * W
        ty = W - h
        d.line([(x0, W), (tx, ty)], fill=(62, 100, 40, 255), width=3 * SS)
        col = palettes[rng.randrange(len(palettes))]
        r = rng.uniform(16, 28) * SS
        for p in range(5):
            a = p / 5 * 2 * math.pi + rng.random()
            px, py = tx + math.cos(a) * r * 0.6, ty + math.sin(a) * r * 0.6
            d.ellipse([px - r * 0.5, py - r * 0.5, px + r * 0.5, py + r * 0.5], fill=_jit(col, rng, 10) + (255,))
        d.ellipse([tx - r * 0.3, ty - r * 0.3, tx + r * 0.3, ty + r * 0.3], fill=(240, 200, 60, 255))
    return _finish(img)


def frond(kind, seed):
    """Fern / palm frond card: rib along V from bottom (base) to top (tip), pinnae both sides."""
    rng = random.Random(seed)
    img = _canvas()
    d = ImageDraw.Draw(img)
    W = S * SS
    if kind == "fern":
        dark, light, n, maxlen, ang0, wid = (26, 64, 24), (104, 160, 62), 26, 0.46, 62, 18
    else:  # palm
        dark, light, n, maxlen, ang0, wid = (34, 70, 26), (130, 170, 70), 34, 0.48, 40, 14
    rib = [(W * 0.5, W * (1.0 - t)) for t in np.linspace(0, 0.98, 20)]
    for i in range(n):
        t = 0.04 + 0.94 * i / n
        y = W * (1.0 - t)
        env = math.sin(math.pi * min(1.0, 0.15 + t * 0.95)) if kind == "fern" else (1.0 - 0.7 * t)
        L = W * maxlen * env
        for sgn in (-1, 1):
            a = math.radians(ang0 + rng.uniform(-6, 6))
            ex = W * 0.5 + sgn * math.sin(a) * L
            ey = y - math.cos(a) * L * 0.9
            col = _jit(_mix(dark, light, 0.3 + 0.5 * rng.random()), rng, 8)
            # pinna as a tapered polygon
            k = 10
            left, right = [], []
            for j in range(k + 1):
                s = j / k
                px = W * 0.5 + (ex - W * 0.5) * s
                py = y + (ey - y) * s + (s * s * 30 * SS if kind == "palm" else 0)
                ww = wid * SS * math.sin(math.pi * min(1, s * 1.05 + 0.05)) ** 0.7
                left.append((px, py - ww / 2))
                right.append((px, py + ww / 2))
            d.polygon(left + right[::-1], fill=col + (255,))
            if kind == "fern":
                for j in range(1, k):
                    s = j / k
                    px, py = left[j]
                    d.ellipse([px - wid * 0.4 * SS, py - wid * 0.2 * SS, px + wid * 0.4 * SS, py + wid * 0.5 * SS],
                              fill=_jit(col, rng, 10) + (255,))
    d.line(rib, fill=(92, 110, 50, 255) if kind == "fern" else (150, 150, 90, 255), width=5 * SS)
    return _finish(img)


TEXTURE_BUILDERS = {
    "MT_LeavesOak": lambda: leaves("oak", 11),
    "MT_LeavesBirch": lambda: leaves("birch", 12),
    "MT_LeavesGiant": lambda: leaves("giant", 13),
    "MT_LeavesDemon": lambda: leaves("demon", 14),
    "MT_LeavesPalm": lambda: frond("palm", 15),
    "MT_NeedlesPine": lambda: needles(16, False),
    "MT_NeedlesSnow": lambda: needles(17, True),
    "MT_Grass": lambda: blades("grass", 18),
    "MT_GrassDry": lambda: blades("dry", 19),
    "MT_Reeds": lambda: blades("reeds", 20),
    "MT_Wheat": lambda: blades("wheat", 21),
    "MT_Flowers": lambda: flowers(22),
    "MT_Fern": lambda: frond("fern", 23),
}


def ensure_textures(cache_dir):
    os.makedirs(cache_dir, exist_ok=True)
    out = {}
    for name, fn in TEXTURE_BUILDERS.items():
        p = os.path.join(cache_dir, "PV_%s.png" % name[3:])
        if not os.path.exists(p):
            fn().save(p)
        out[name] = p
    return out


if __name__ == "__main__":
    import sys
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    paths = ensure_textures(d)
    ims = [Image.open(p) for p in paths.values()]
    sheet = Image.new("RGBA", (256 * len(ims), 256), (60, 70, 80, 255))
    for i, im in enumerate(ims):
        t = im.resize((256, 256))
        sheet.alpha_composite(t, (256 * i, 0))
    sheet.save(os.path.join(d, "_textures.png"))
    print("wrote", len(ims))
