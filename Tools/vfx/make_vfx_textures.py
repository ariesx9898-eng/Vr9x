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


if __name__ == "__main__":
    noise_texture()
    puff_texture()
    dot_texture()
    streak_texture()
    magic_circle_texture()
    cracks_texture()
    scorch_texture()
    water_normal_texture()
