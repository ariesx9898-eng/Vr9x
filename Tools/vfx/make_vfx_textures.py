"""Procedural textures for the runtime spell effects (numpy + Pillow, deterministic).

Writes SourceArt/VFX/Textures/*.png:
  T_VFX_Noise        512^2 RGBA, tileable: R fBm, G Worley cells, B ridged fBm, A fine value noise
  T_VFX_Puff         256^2, soft cloud puff with a broken edge (alpha), for smoke / dust / steam sprites
  T_VFX_Dot          64^2, round glow dot (alpha), for sparks, embers, droplets, motes
  T_VFX_Streak       256x64, a tapered streak (alpha), for trails and stretched sparks
  T_VFX_MagicCircle  1024^2, rune circle (alpha), for Inferno's warning circle and mana effects
  T_VFX_Cracks       1024^2, radial ground cracks (alpha), for impacts and ground slams
  T_VFX_Scorch       1024^2, R = char mask, G = ember mask, for burn marks
  T_VFX_WaterN       512^2, tileable normal map (DirectX / Unreal convention) for water spells
  T_VFX_Flame        256x128, flame tongue along U (alpha, grey = heat) for stretched fire sprites
  T_VFX_FirePuff     256^2, billowing fire puff (alpha, grey = heat) for fire bodies
  T_VFX_Leaf         128^2, leaf with midrib and veins (alpha) for wind debris
  T_VFX_Mud          512^2 RGBA, tileable: R albedo variation, G bubble domes, B per-bubble phase, A boundary noise
  T_VFX_CrackNet     512^2, tileable crack network: R strength (1 main, ~0.6 branches, ~0.3 hairlines), G soft halo
  T_VFX_Crater       1024^2 RGBA: R albedo shade (dark bowl, fresh rim, thrown-dirt rays), G bowl mask, A opacity
  T_VFX_CraterN      1024^2, crater normal map (bowl, raised rim, ejecta clods; DirectX / Unreal convention)
  T_VFX_AimLine      1024x128, aim-line strip along U: R core line + spike markers, G chevrons (16 per tile, scrolled
                     by the material), B glow, A strip mask (the texture's newest file: build_and_setup.sh's marker)
Run: ~/.venvs/mushoku-bpy311/bin/python Tools/vfx/make_vfx_textures.py
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
OUT = os.path.join(ROOT, "SourceArt", "VFX", "Textures")
FONT = os.path.join(ROOT, "Content", "UI", "Fonts", "Cinzel.ttf")
rng = np.random.default_rng(1717)


def periodic_gradient_noise(size, period, seed):
    """Tileable 2D Perlin noise with `period` cells across `size` pixels."""
    r = np.random.default_rng(seed)
    angles = r.uniform(0, 2 * math.pi, (period, period))
    gx, gy = np.cos(angles), np.sin(angles)
    coords = np.arange(size) * period / size
    x = np.tile(coords, (size, 1))
    y = x.T
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0

    def dot(ix, iy, dx, dy):
        ix %= period
        iy %= period
        return gx[iy, ix] * dx + gy[iy, ix] * dy

    def fade(t):
        return t * t * t * (t * (t * 6 - 15) + 10)

    n00 = dot(x0, y0, fx, fy)
    n10 = dot(x0 + 1, y0, fx - 1, fy)
    n01 = dot(x0, y0 + 1, fx, fy - 1)
    n11 = dot(x0 + 1, y0 + 1, fx - 1, fy - 1)
    u, v = fade(fx), fade(fy)
    return (n00 * (1 - u) + n10 * u) * (1 - v) + (n01 * (1 - u) + n11 * u) * v


def fbm(size, base_period, octaves, seed, ridged=False):
    total = np.zeros((size, size))
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        n = periodic_gradient_noise(size, base_period * (2 ** o), seed + o * 101)
        if ridged:
            n = 1.0 - np.abs(n) * 1.6
        total += n * amp
        norm += amp
        amp *= 0.5
    total /= norm
    return (total - total.min()) / (total.max() - total.min())


def worley(size, cells, seed):
    r = np.random.default_rng(seed)
    pts = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells)), -1).reshape(-1, 2) + r.random((cells * cells, 2))) * (size / cells)
    yy, xx = np.mgrid[0:size, 0:size]
    best = np.full((size, size), 1e9)
    for ox in (-size, 0, size):
        for oy in (-size, 0, size):
            for px, py in pts:
                d = np.hypot(xx - (px + ox), yy - (py + oy))
                np.minimum(best, d, out=best)
    best /= best.max()
    return best


def to_img(a):
    return Image.fromarray(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8))


def save(name, img):
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name + ".png"))
    print(name, img.size, img.mode)


def noise_texture():
    s = 512
    r = fbm(s, 4, 5, 11)
    g = 1.0 - worley(s, 8, 12)
    b = fbm(s, 3, 5, 13, ridged=True)
    a = fbm(s, 16, 3, 14)
    save("T_VFX_Noise", Image.merge("RGBA", [to_img(c) for c in (r, g, b, a)]))


def radial(size):
    yy, xx = np.mgrid[0:size, 0:size]
    c = (size - 1) / 2
    return np.hypot(xx - c, yy - c) / c, np.arctan2(yy - c, xx - c)


def puff_texture():
    s = 256
    d, ang = radial(s)
    n = fbm(s, 4, 4, 21)
    edge = 0.72 + 0.28 * (n - 0.5) * 2
    alpha = np.clip(1.0 - d / edge, 0, 1) ** 1.6
    alpha *= 0.65 + 0.35 * fbm(s, 6, 4, 22)
    shade = 0.78 + 0.22 * fbm(s, 5, 3, 23)
    rgb = to_img(shade)
    save("T_VFX_Puff", Image.merge("RGBA", [rgb, rgb, rgb, to_img(alpha)]))


def dot_texture():
    s = 64
    d, _ = radial(s)
    core = np.exp(-(d / 0.18) ** 2)
    halo = np.clip(1 - d, 0, 1) ** 2.5
    a = np.clip(core + halo * 0.6, 0, 1)
    white = to_img(np.ones((s, s)))
    save("T_VFX_Dot", Image.merge("RGBA", [white, white, white, to_img(a)]))


def streak_texture():
    w, h = 256, 64
    x = np.linspace(0, 1, w)[None, :]
    y = np.linspace(-1, 1, h)[:, None]
    along = np.clip(x, 0, 1) ** 1.5 * np.clip((1 - x) * 8, 0, 1)
    across = np.exp(-(y / (0.25 + 0.35 * x)) ** 2)
    a = along * across
    white = to_img(np.ones((h, w)))
    save("T_VFX_Streak", Image.merge("RGBA", [white, white, white, to_img(a / a.max())]))


def magic_circle_texture():
    s = 1024
    img = Image.new("L", (s, s), 0)
    dr = ImageDraw.Draw(img)
    c = s / 2
    for r, w in ((500, 10), (470, 4), (360, 6), (340, 3), (180, 5), (160, 3), (60, 4)):
        dr.ellipse([c - r, c - r, c + r, c + r], outline=255, width=w)
    # Two interlaced triangles (hexagram) and an inner square.
    for rot in (0, math.pi):
        pts = [(c + 340 * math.cos(rot + math.pi / 2 + k * 2 * math.pi / 3), c + 340 * math.sin(rot + math.pi / 2 + k * 2 * math.pi / 3)) for k in range(3)]
        dr.polygon(pts, outline=255, width=5)
    sq = [(c + 175 * math.cos(math.pi / 4 + k * math.pi / 2), c + 175 * math.sin(math.pi / 4 + k * math.pi / 2)) for k in range(4)]
    dr.polygon(sq, outline=255, width=4)
    # Rune band: glyphs from Cinzel rotated around the circle between radii 360 and 470.
    try:
        font = ImageFont.truetype(FONT, 64)
    except OSError:
        font = ImageFont.load_default()
    glyphs = "ΑΒΓΔΘΛΞΠΣΦΨΩ" if False else "AEGHKMNRSTVXZ"
    count = 36
    for k in range(count):
        ch = glyphs[(k * 7) % len(glyphs)]
        tile = Image.new("L", (96, 96), 0)
        ImageDraw.Draw(tile).text((48, 48), ch, fill=255, font=font, anchor="mm")
        ang = k * 360 / count
        tile = tile.rotate(-ang - 90, resample=Image.BICUBIC)
        px = c + 415 * math.cos(math.radians(ang)) - 48
        py = c + 415 * math.sin(math.radians(ang)) - 48
        img.paste(255, (int(px), int(py)), tile)
    # Small orbiting circles at the hexagram points.
    for k in range(6):
        a = math.pi / 2 + k * math.pi / 3
        x, y = c + 340 * math.cos(a), c + 340 * math.sin(a)
        dr.ellipse([x - 28, y - 28, x + 28, y + 28], outline=255, width=4)
    img = img.filter(ImageFilter.GaussianBlur(1.2))
    glow = img.filter(ImageFilter.GaussianBlur(10))
    a = np.clip(np.asarray(img, np.float32) / 255 + np.asarray(glow, np.float32) / 255 * 0.6, 0, 1)
    white = to_img(np.ones((s, s)))
    save("T_VFX_MagicCircle", Image.merge("RGBA", [white, white, white, to_img(a)]))


def cracks_texture():
    s = 1024
    img = Image.new("L", (s, s), 0)
    dr = ImageDraw.Draw(img)
    c = s / 2
    r = np.random.default_rng(31)

    def branch(x, y, ang, length, width, depth):
        steps = int(length / 14)
        for _ in range(steps):
            ang += r.normal(0, 0.28)
            nx, ny = x + math.cos(ang) * 14, y + math.sin(ang) * 14
            dr.line([x, y, nx, ny], fill=255, width=max(1, int(width)))
            x, y = nx, ny
            width *= 0.965
            if depth < 3 and r.random() < 0.07:
                branch(x, y, ang + r.choice([-1, 1]) * r.uniform(0.4, 1.0), length * 0.45, width * 0.7, depth + 1)
            if math.hypot(x - c, y - c) > s * 0.48:
                break

    for k in range(11):
        a = k * 2 * math.pi / 11 + r.uniform(-0.2, 0.2)
        branch(c + math.cos(a) * 30, c + math.sin(a) * 30, a, r.uniform(280, 470), r.uniform(9, 14), 0)
    # Crushed centre.
    for _ in range(40):
        a, d = r.uniform(0, 2 * math.pi), r.uniform(0, 70)
        x, y = c + math.cos(a) * d, c + math.sin(a) * d
        dr.line([x, y, x + r.normal(0, 25), y + r.normal(0, 25)], fill=255, width=int(r.uniform(3, 8)))
    img = img.filter(ImageFilter.GaussianBlur(1.0))
    d, _ = radial(s)
    fade = np.clip((1.0 - d) / 0.25, 0, 1)
    a = np.asarray(img, np.float32) / 255 * fade
    white = to_img(np.ones((s, s)))
    save("T_VFX_Cracks", Image.merge("RGBA", [white, white, white, to_img(a)]))


def scorch_texture():
    s = 1024
    d, _ = radial(s)
    n = fbm(s, 5, 5, 41)
    char = np.clip((0.85 + 0.35 * (n - 0.5) * 2 - d) / 0.25, 0, 1) ** 1.3
    ember_noise = fbm(s, 24, 3, 42)
    ember = np.clip((ember_noise - 0.71) / 0.06, 0, 1) * np.clip((0.65 - d) / 0.3, 0, 1)
    zero = to_img(np.zeros((s, s)))
    save("T_VFX_Scorch", Image.merge("RGBA", [to_img(char), to_img(ember), zero, to_img(char)]))


def water_normal_texture():
    s = 512
    h = fbm(s, 4, 5, 51) * 0.6 + fbm(s, 9, 3, 52) * 0.4
    yy, xx = np.mgrid[0:s, 0:s] / s * 2 * math.pi
    h += 0.08 * np.sin(xx * 3 + yy * 2) + 0.05 * np.sin(xx * 5 - yy * 4)
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 6
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 6
    nx, ny, nz = -dx, -dy, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    rgb = [to_img(c / ln * 0.5 + 0.5) for c in (nx, ny, nz)]
    save("T_VFX_WaterN", Image.merge("RGB", rgb))


# ----------------------------------------------------------------------------------------- ability overhaul textures

def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def periodic_blur(a, sigma):
    """Gaussian blur that wraps around the edges (keeps a tileable texture tileable). Square arrays only."""
    f = np.fft.fftfreq(a.shape[0])
    g = np.exp(-2.0 * (math.pi * sigma) ** 2 * (f[:, None] ** 2 + f[None, :] ** 2))
    return np.real(np.fft.ifft2(np.fft.fft2(a) * g))


def worley_cells(size, cells, seed, warp=None):
    """Tileable Worley noise with cell ids: F1 and F2 in cell units and the index of the nearest feature point.
    warp: optional (dx, dy) pixel offsets (periodic arrays) that bend the cell borders."""
    r = np.random.default_rng(seed)
    pts = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells)), -1).reshape(-1, 2)
           + r.random((cells * cells, 2))) * (size / cells)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float64)
    reach = 2.5 * size / cells
    if warp is not None:
        xx = xx + warp[0]
        yy = yy + warp[1]
        reach += max(float(np.abs(warp[0]).max()), float(np.abs(warp[1]).max()))
    f1 = np.full((size, size), 1e9)
    f2 = np.full((size, size), 1e9)
    ids = np.zeros((size, size), np.int32)
    for idx, (px, py) in enumerate(pts):
        for ox in (-size, 0, size):
            for oy in (-size, 0, size):
                cx, cy = px + ox, py + oy
                x0, x1 = max(0, int(cx - reach)), min(size, int(cx + reach) + 1)
                y0, y1 = max(0, int(cy - reach)), min(size, int(cy + reach) + 1)
                if x0 >= x1 or y0 >= y1:
                    continue
                win = (slice(y0, y1), slice(x0, x1))
                d = np.hypot(xx[win] - cx, yy[win] - cy)
                a1 = f1[win]
                closer = d < a1
                f2[win] = np.where(closer, a1, np.minimum(f2[win], d))
                ids[win] = np.where(closer, idx, ids[win])
                f1[win] = np.where(closer, d, a1)
    cell = size / cells
    return f1 / cell, f2 / cell, ids


def height_to_normal(h, strength):
    """Normal map (DirectX / Unreal convention, like T_VFX_WaterN) from a height field in texture widths."""
    dx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * h.shape[1] * strength
    dy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * h.shape[0] * strength
    nx, ny, nz = -dx, -dy, np.ones_like(h)
    ln = np.sqrt(nx * nx + ny * ny + nz * nz)
    return [to_img(c / ln * 0.5 + 0.5) for c in (nx, ny, nz)]


def flame_texture():
    """Flame tongue for stretched fire sprites (U runs along the motion): a lens-shaped lick with a white-hot core,
    ragged licking edges and filaments along it. Nearly symmetric, so it reads right whichever way U runs."""
    w, h = 256, 128
    u = np.linspace(0.0, 1.0, w)[None, :]
    v = np.linspace(-1.0, 1.0, h)[:, None]

    def streaky(seed):
        # Square noise squeezed 8x across the flame: filaments and licks run along U.
        n = fbm(256, 6, 4, seed)[:, :64]
        return np.asarray(Image.fromarray((n * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC), np.float32) / 255

    fil = streaky(81)
    lick = streaky(82)
    prof = np.sin(math.pi * u) ** 0.75 * (1.0 - 0.18 * u)
    half = prof * (0.62 + 0.55 * (lick - 0.5))
    a = np.clip((half - np.abs(v)) / (0.22 * prof + 0.03), 0.0, 1.0) ** 1.3
    a *= 0.72 + 0.28 * fil
    core = np.exp(-(v / (0.3 * prof + 0.02)) ** 2) * np.sin(math.pi * u) ** 1.5
    heat = np.clip(0.5 + 0.3 * (fil - 0.5) * 2 + 0.5 * core, 0.0, 1.0)
    save("T_VFX_Flame", Image.merge("RGBA", [to_img(heat)] * 3 + [to_img(a / a.max())]))


def fire_puff_texture():
    """Billowing fire for fire bodies: a round puff made of bright rolling billows with darker creases between them,
    a hot centre and a ragged, licking rim."""
    s = 256
    d, _ = radial(s)
    cell = s / 5
    warp = ((fbm(s, 3, 4, 96) - 0.5) * 0.7 * cell, (fbm(s, 3, 4, 97) - 0.5) * 0.7 * cell)  # rolling, not cellular
    f1, _, _ = worley_cells(s, 5, 93, warp)
    billow = np.clip(1.0 - f1 * 1.15, 0.0, 1.0) ** 0.8
    turb = fbm(s, 4, 5, 94)
    edge = 0.74 + 0.3 * (fbm(s, 5, 4, 95) - 0.5) * 2
    a = np.clip((edge - d) / 0.28, 0.0, 1.0) ** 1.2 * (0.55 + 0.45 * billow)
    heat = np.clip(0.3 + 0.45 * billow + 0.25 * turb + 0.35 * np.exp(-(d / 0.38) ** 2), 0.0, 1.0)
    save("T_VFX_FirePuff", Image.merge("RGBA", [to_img(heat)] * 3 + [to_img(a / a.max())]))


def leaf_texture():
    """A small leaf for wind debris (lit sprites): a pointed oval along V with a midrib, side veins and a stem."""
    s = 128
    yy, xx = np.mgrid[0:s, 0:s] / (s - 1) * 2.0 - 1.0
    t = np.clip((yy + 0.82) / 1.7, 0.0, 1.0)
    width = 0.5 * np.sin(math.pi * t) ** 0.85 * (1.0 - 0.25 * t) * ((yy > -0.82) & (yy < 0.88))
    px = 2.0 / s
    blade = np.clip((width - np.abs(xx)) / px, 0.0, 1.0)
    stem = np.clip((0.022 - np.abs(xx + 0.04 * (yy + 0.82))) / px, 0.0, 1.0) * ((yy < -0.7) & (yy > -1.0))
    a = np.maximum(blade, stem)
    rib = np.clip(1.0 - np.abs(xx) / 0.025, 0.0, 1.0)
    veins = np.clip(1.0 - np.abs(np.sin((yy - np.abs(xx) * 0.9) * 17.0)) / 0.16, 0.0, 1.0) * (np.abs(xx) < width * 0.85)
    shade = 0.82 + 0.18 * fbm(s, 4, 3, 101) - 0.3 * rib - 0.16 * veins - 0.12 * np.clip(np.abs(xx) / (width + 1e-3), 0, 1)
    save("T_VFX_Leaf", Image.merge("RGBA", [to_img(np.clip(shade, 0.0, 1.0))] * 3 + [to_img(a)]))


def mud_texture():
    """Tileable liquefied mud for M_Decal_Mud: R albedo variation, G bubble domes (0 between bubbles), B each bubble
    cell's phase (0..1, constant over the cell: bubbles swell and pop out of step), A noise for the boundary."""
    s = 512
    albedo = fbm(s, 5, 5, 71) * 0.7 + fbm(s, 14, 3, 72) * 0.3
    cells = 12
    f1, _, ids = worley_cells(s, cells, 73)
    r = np.random.default_rng(74)
    radius = r.uniform(0.16, 0.36, cells * cells)
    present = (r.random(cells * cells) < 0.72).astype(np.float64)
    phase = r.random(cells * cells)
    dome = np.sqrt(np.clip(1.0 - (f1 / radius[ids]) ** 2, 0.0, 1.0)) * present[ids]
    edge = fbm(s, 6, 4, 75)
    save("T_VFX_Mud", Image.merge("RGBA", [to_img(albedo), to_img(dome), to_img(phase[ids]), to_img(edge)]))


def crack_net_texture():
    """Tileable crack network for conjured stone (M_VFX_Rock "Crack") and drying ground (M_Decal_Mud).
    R = crack strength: about 1 on the main fractures, 0.6 on the branches, 0.3 on the hairlines, so a material that
    thresholds it reveals more of the network as the threshold drops. G = a soft halo (glowing fractures)."""
    s = 512
    out = np.zeros((s, s))
    #        cells seed warp  half-width px  strength  keep (fraction of the borders drawn)
    layers = ((4, 61, 0.24, 2.6, 1.0, 1.0), (9, 62, 0.2, 1.5, 0.62, 0.6), (19, 63, 0.16, 0.9, 0.32, 0.5))
    for cells, seed, amp, half_px, strength, keep in layers:
        cell = s / cells
        wx = ((fbm(s, 4, 4, seed * 3) - 0.5) * 2 * amp + (fbm(s, 16, 2, seed * 3 + 1) - 0.5) * 0.35 * amp) * cell
        wy = ((fbm(s, 4, 4, seed * 3 + 2) - 0.5) * 2 * amp + (fbm(s, 16, 2, seed * 3 + 3) - 0.5) * 0.35 * amp) * cell
        f1, f2, _ = worley_cells(s, cells, seed, (wx, wy))
        width = 2.0 * half_px / cell * (0.55 + 0.9 * fbm(s, 8, 3, seed + 5))
        line = 1.0 - smoothstep(0.0, 1.0, (f2 - f1) / width)
        if keep < 1.0:
            line *= smoothstep(1.0 - keep - 0.06, 1.0 - keep + 0.06, fbm(s, 3, 3, seed + 7))
        out = np.maximum(out, line * strength)
    out = np.clip(periodic_blur(out, 0.5), 0.0, 1.0)
    halo = periodic_blur(out, 5.0)
    halo = np.clip(halo / halo.max() * 1.6, 0.0, 1.0)
    zero = to_img(np.zeros((s, s)))
    save("T_VFX_CrackNet", Image.merge("RGB", [to_img(out), to_img(halo), zero]))


def crater_textures():
    """A dug crater for M_Decal_Crater: a bowl (radius 0.5 of the decal), a raised rim of fresh soil and rays of thrown
    dirt with clods beyond it. T_VFX_Crater: R albedo shade, G bowl mask, A opacity. T_VFX_CraterN: its normals."""
    s = 1024
    d, ang = radial(s)
    rb = 0.5
    r = np.random.default_rng(111)
    # Rays of thrown dirt: a smooth periodic function of the angle, sharpened.
    ray = np.zeros_like(ang)
    for k in range(1, 30):
        ray += r.normal(0.0, 1.0 / k ** 0.7) * np.cos(k * ang + r.uniform(0.0, 2.0 * math.pi))
    ray = (ray - ray.min()) / (ray.max() - ray.min())
    ray = np.clip((ray - 0.35) / 0.5, 0.0, 1.0) ** 1.6
    n1 = fbm(s, 6, 5, 112)
    n2 = fbm(s, 24, 3, 113)
    rim_r = rb * (1.0 + 0.05 * (n1 - 0.5) * 2)
    bowl = -0.5 * np.clip(1.0 - (d / rim_r) ** 2, 0.0, 1.0)
    rim = 0.26 * np.exp(-((d - rim_r) / 0.07) ** 2)
    outside = np.clip((d - rim_r) / 0.05, 0.0, 1.0)
    ejecta = 0.1 * ray * np.exp(-np.clip(d - rim_r, 0.0, None) / 0.16) * outside
    clods = np.clip((n2 - 0.62) / 0.1, 0.0, 1.0) * 0.05 * (0.3 + ray) * outside * np.clip((0.92 - d) / 0.2, 0.0, 1.0)
    height = (bowl + rim + ejecta + clods + 0.025 * (n1 - 0.5)) * 0.06
    bowl_mask = np.clip((rim_r - d) / 0.08, 0.0, 1.0)
    albedo = 0.72 + 0.12 * (n1 - 0.5) * 2
    albedo = albedo * (1.0 - 0.45 * bowl_mask * np.clip(1.0 - d / rim_r, 0.0, 1.0) ** 0.6) + 0.28 * np.exp(-((d - rim_r) / 0.06) ** 2)
    albedo += 0.15 * clods / 0.05
    alpha_out = np.clip(ray * 1.3 * np.exp(-np.clip(d - rim_r, 0.0, None) / 0.14) + clods / 0.05 * 0.8, 0.0, 1.0)
    alpha = np.maximum(np.clip((rim_r + 0.07 + 0.03 * (n2 - 0.5) - d) / 0.04, 0.0, 1.0), alpha_out)
    alpha *= np.clip((0.97 - d) / 0.1, 0.0, 1.0)
    save("T_VFX_Crater", Image.merge("RGBA", [to_img(np.clip(albedo, 0.0, 1.0)), to_img(bowl_mask),
                                             to_img(np.zeros((s, s))), to_img(alpha)]))
    save("T_VFX_CraterN", Image.merge("RGB", height_to_normal(height, 1.0)))


def aim_line_texture():
    """Aim-line strip for M_Decal_AimLine, U along the line (0 at the caster, 1 at the far end), V across it.
    R: a crackling core line and diamond markers where Earth Spikes rise (320 / 640 / 960 / 1300 of 1300 cm);
    G: 16 chevrons pointing along +U, evenly spaced so the material can scroll them (tileable along U);
    B: a soft glow around the core; A: the strip mask (fades in at the caster, soft edges across)."""
    w, h = 1024, 128
    u = np.linspace(0.0, 1.0, w)[None, :]
    v = np.linspace(-1.0, 1.0, h)[:, None]
    crackle = np.asarray(Image.fromarray((fbm(256, 32, 3, 121)[:1, :] * 255).astype(np.uint8)).resize((w, 1), Image.BICUBIC),
                         np.float32) / 255
    core = np.exp(-(v / 0.07) ** 2) * (0.7 + 0.3 * crackle)
    marks = Image.new("L", (w, h), 0)
    dr = ImageDraw.Draw(marks)
    for k, frac in enumerate((320 / 1300, 640 / 1300, 960 / 1300, 0.975)):
        x = frac * w
        big = k == 3
        rx, ry = (22, 50) if big else (15, 36)
        dr.polygon([(x - rx, h / 2), (x, h / 2 - ry), (x + rx, h / 2), (x, h / 2 + ry)], outline=255, width=5 if big else 4)
        dr.polygon([(x - rx * 0.35, h / 2), (x, h / 2 - ry * 0.35), (x + rx * 0.35, h / 2), (x, h / 2 + ry * 0.35)], fill=255)
    marks = np.asarray(marks.filter(ImageFilter.GaussianBlur(1.0)), np.float32) / 255
    chev = Image.new("L", (w, h), 0)
    dc = ImageDraw.Draw(chev)
    period = w // 16
    for k in range(16):
        x0 = k * period + period * 0.3
        dc.line([(x0, h / 2 - 30), (x0 + period * 0.35, h / 2), (x0, h / 2 + 30)], fill=255, width=6, joint="curve")
    chev = np.asarray(chev.filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255
    glow = np.exp(-(v / 0.32) ** 2) * (0.85 + 0.15 * crackle)
    strip = smoothstep(0.0, 0.05, u) * (1.0 - smoothstep(0.75, 1.0, np.abs(v)))
    save("T_VFX_AimLine", Image.merge("RGBA", [to_img(np.clip(np.maximum(core, marks), 0.0, 1.0)), to_img(chev),
                                              to_img(np.clip(glow, 0.0, 1.0)), to_img(np.broadcast_to(strip, (h, w)))]))


if __name__ == "__main__":
    noise_texture()
    puff_texture()
    dot_texture()
    streak_texture()
    magic_circle_texture()
    cracks_texture()
    scorch_texture()
    water_normal_texture()
    flame_texture()
    fire_puff_texture()
    leaf_texture()
    mud_texture()
    crack_net_texture()
    crater_textures()
    aim_line_texture()  # keep last: T_VFX_AimLine.png is the generator's newest file (build_and_setup.sh marker)
