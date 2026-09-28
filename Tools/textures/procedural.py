"""Procedural palette materials: Iron, Gold, Glass, Crystal (1 m tiles, 1024 px, seamless by construction).

Every pattern is built on the torus (periodic cellular noise, band-limited periodic fBm, scratches drawn with
wrap-around), so the textures tile exactly. Metals follow the usual metallic workflow: _D is the specular colour
(F0) in sRGB, metallic = 1 in the manifest.
"""
import numpy as np
from PIL import Image, ImageDraw

import texlib as T
import surface as SF
from common import rng_for, save_set

SIZE = 1024


def _scratches(rng, n, length_px, width=1, size=SIZE, angle=None):
    """Thin random scratches (0..1 mask) drawn on a torus."""
    img = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(img)
    for _ in range(n):
        x, y = rng.uniform(0, size, 2)
        a = rng.uniform(0, np.pi) if angle is None else angle + rng.normal(0, 0.25)
        ln = rng.uniform(0.3, 1.0) * length_px
        dx, dy = np.cos(a) * ln, np.sin(a) * ln
        v = int(rng.uniform(90, 255))
        for ox in (-size, 0, size):
            for oy in (-size, 0, size):
                d.line([(x + ox, y + oy), (x + dx + ox, y + dy + oy)], fill=v, width=width)
    return np.asarray(img).astype(np.float32) / 255.0


def _hammered(rng, cells, size=SIZE):
    """Overlapping shallow round dimples (hammer blows): high ridges between cells, bowls in the cells."""
    f1, f2, _ = T.worley_periodic(size, cells, rng, jitter=0.9)
    h = np.clip(f1, 0, 1.2) ** 2
    return h / max(float(h.max()), 1e-6)


def _metal(name, kind, rng, low_lin, high_lin, cells, rough, tilt, pits, streak):
    """Forged / beaten metal: shallow hammer dimples (mostly in the normal map), anisotropic brushed streaks,
    optional fine pitting, slightly darker dimple floors and brighter worn crests (a few % only)."""
    ham = _hammered(rng, cells)
    brush = T.fbm_periodic((SIZE, SIZE), rng, 10, 5, 0.6, aniso=(6.0, 0.6))
    grain = T.fbm_periodic((SIZE, SIZE), rng, 60, 3, 0.5)
    scr = T.blur_periodic(_scratches(rng, 50, 160, angle=0.0), 0.6)
    pit = np.zeros((SIZE, SIZE), np.float32)
    if pits:
        p1, _, pid = T.worley_periodic(SIZE, 110, rng, jitter=1.0)
        keep = (rng.random(110 * 110) < pits).astype(np.float32)
        pit = np.clip(1.0 - p1 / 0.18, 0.0, 1.0) * keep[pid]
    h = 0.8 * ham + 0.08 * brush + 0.04 * grain - 0.35 * pit - 0.15 * scr
    h01 = T.normalize01(h, 0.5, 99.5)
    t = np.clip(0.5 + 0.35 * (h01 - 0.5) + streak * brush + 0.02 * grain, 0.0, 1.0)
    lo, hi = np.asarray(low_lin, np.float32), np.asarray(high_lin, np.float32)
    lin = lo * (1 - t[..., None]) + hi * t[..., None]
    lin = lin * (1.0 - 0.45 * pit[..., None]) * (1.0 + 0.15 * scr[..., None])
    D = T.lin_to_srgb(np.clip(lin, 0, 1))
    r = dict(rough, seed=int(rng.integers(1 << 30)))
    N, M, info = SF.maps(D, h01, 1.0, tilt, [0.003, 0.012], 0.25, r)
    g = M[..., 1] + 0.25 * pit - 0.1 * scr + 0.05 * brush
    M[..., 1] = np.clip(g, rough["lo"], rough["hi"])
    info["roughness_range"] = [round(float(np.percentile(M[..., 1], 1)), 3),
                               round(float(np.percentile(M[..., 1], 99)), 3)]
    files = save_set("Palette", f"T_{name}", D, N, M)
    return dict(name=name, group="Palette", kind=kind, files=files, tile_m=1.0, size=SIZE, source="Procedural",
                metallic=1, roughness_range=info["roughness_range"], normal_strength=1.0,
                height_m=info["height_m"], mean_albedo=SF.mean_albedo_linear(D)), dict(D=D, N=N, M=M)


def iron(kind):
    """Blackened wrought iron for straps, hinges, grilles and lanterns."""
    rng = rng_for("Palette_Iron")
    return _metal("Iron", kind, rng, (0.12, 0.12, 0.125), (0.2, 0.2, 0.21), cells=22, tilt=9, pits=0.35,
                  streak=0.05, rough=dict(base=0.52, h=-0.25, lo=0.34, hi=0.78))


def gold(kind):
    """Polished, lightly beaten gold leaf / gilding."""
    rng = rng_for("Palette_Gold")
    return _metal("Gold", kind, rng, (0.86, 0.62, 0.24), (0.98, 0.76, 0.34), cells=12, tilt=5, pits=0.0,
                  streak=0.04, rough=dict(base=0.24, h=-0.15, lo=0.14, hi=0.42))


def glass(kind):
    """Old window glass: slightly wavy (cylinder glass), sparse seed bubbles, a faint film of grime."""
    rng = rng_for("Palette_Glass")
    wave = T.fbm_periodic((SIZE, SIZE), rng, 3, 3, 0.45, aniso=(1.0, 0.45))
    img = Image.new("L", (SIZE, SIZE), 0)
    d = ImageDraw.Draw(img)
    for _ in range(140):                                   # sparse seed bubbles, drawn on the torus
        x, y = rng.uniform(0, SIZE, 2)
        r = rng.uniform(0.8, 2.6)
        for ox in (-SIZE, 0, SIZE):
            for oy in (-SIZE, 0, SIZE):
                d.ellipse([x + ox - r, y + oy - r * 0.7, x + ox + r, y + oy + r * 0.7], fill=255)
    bubbles = T.blur_periodic(np.asarray(img).astype(np.float32) / 255.0, 0.7)
    grime = np.clip(T.fbm_periodic((SIZE, SIZE), rng, 6, 5, 0.6) * 0.5 + 0.1, 0.0, 1.0) ** 2
    h = wave + 0.15 * T.fbm_periodic((SIZE, SIZE), rng, 18, 3, 0.5) + 0.8 * bubbles
    h01 = T.normalize01(h, 0.5, 99.5)
    tint = T.srgb_to_lin(np.array([77, 102, 117], np.float32) / 255.0)
    lin = tint[None, None, :] * (1.0 + 0.06 * wave[..., None])
    lin = lin * (1 - 0.35 * grime[..., None]) + np.array([0.09, 0.09, 0.085], np.float32) * 0.35 * grime[..., None]
    D = T.lin_to_srgb(np.clip(lin, 0, 1))
    r = dict(base=0.06, lo=0.03, hi=0.35, seed=int(rng.integers(1 << 30)))
    N, M, info = SF.maps(D, h01, 1.0, 2.5, [0.02, 0.08], 0.15, r, normal_blur_px=2.0)
    M[..., 1] = np.clip(0.05 + 0.3 * grime, 0.03, 0.4)
    M[..., 0] = 1.0
    info["roughness_range"] = [round(float(np.percentile(M[..., 1], 1)), 3),
                               round(float(np.percentile(M[..., 1], 99)), 3)]
    files = save_set("Palette", "T_Glass", D, N, M)
    return dict(name="Glass", group="Palette", kind=kind, files=files, tile_m=1.0, size=SIZE, source="Procedural",
                metallic=0, translucent=True, opacity_hint=0.35, roughness_range=info["roughness_range"],
                normal_strength=1.0, height_m=info["height_m"], mean_albedo=SF.mean_albedo_linear(D)), \
        dict(D=D, N=N, M=M)


def crystal(kind):
    """Glowing blue magic crystal: flat facets (one tilted plane per cell), glowing internal fracture veins and
    brighter facet cores; T_Crystal_E.png is the emissive mask."""
    rng = rng_for("Palette_Crystal")
    cells = 7
    f1, f2, cid = T.worley_periodic(SIZE, cells, rng, jitter=0.95)
    k = cells * cells
    gx = rng.normal(0, 0.35, k).astype(np.float32)
    gy = rng.normal(0, 0.35, k).astype(np.float32)
    shade = rng.uniform(0.0, 1.0, k).astype(np.float32)
    # facet normals analytically (flat planes), plus a thin bevel along the facet edges
    edge = np.clip(1.0 - (f2 - f1) / 0.06, 0.0, 1.0)
    nx = -gx[cid] * (1 - edge)
    ny = -gy[cid] * (1 - edge)
    fine = T.fbm_periodic((SIZE, SIZE), rng, 30, 3, 0.5)
    fdx = (np.roll(fine, -1, 1) - np.roll(fine, 1, 1)) * 0.5
    fdy = (np.roll(fine, -1, 0) - np.roll(fine, 1, 0)) * 0.5
    nx = nx - 0.6 * fdx
    ny = ny - 0.6 * fdy
    nz = np.ones_like(nx)
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    N = np.stack([nx * inv, ny * inv, nz * inv], -1) * 0.5 + 0.5
    # veins: thinner cellular network at a finer scale + the facet boundaries
    v1, v2, _ = T.worley_periodic(SIZE, 17, rng, jitter=1.0)
    veins = np.clip(1.0 - (v2 - v1) / 0.05, 0.0, 1.0) ** 2
    veins *= np.clip(T.fbm_periodic((SIZE, SIZE), rng, 4, 3) * 0.6 + 0.5, 0.0, 1.0)
    core = np.clip(1.0 - f1 / 0.75, 0.0, 1.0)
    deep = T.srgb_to_lin(np.array([28, 70, 150], np.float32) / 255.0)
    light = T.srgb_to_lin(np.array([120, 200, 255], np.float32) / 255.0)
    t = np.clip(0.25 + 0.45 * shade[cid] + 0.3 * core + 0.1 * fine, 0.0, 1.0)
    lin = deep * (1 - t[..., None]) + light * t[..., None]
    lin = lin * (1 - 0.7 * veins[..., None]) + np.array([0.75, 0.93, 1.0], np.float32) * 0.7 * veins[..., None]
    lin = lin * (1 - 0.35 * edge[..., None]) + np.array([0.6, 0.85, 1.0], np.float32) * 0.35 * edge[..., None]
    D = T.lin_to_srgb(np.clip(lin, 0, 1))
    E = np.clip(0.18 + 0.35 * shade[cid] * core + 0.9 * veins + 0.25 * edge, 0.0, 1.0).astype(np.float32)
    h01 = T.normalize01(core * 0.6 + 0.4 * (1 - edge) + 0.05 * fine, 0.5, 99.5)
    ao = np.clip(1.0 - 0.25 * edge, 0, 1)
    rough = np.clip(0.1 + 0.12 * edge + 0.03 * fine, 0.04, 0.35)
    M = np.stack([ao, rough, h01], -1).astype(np.float32)
    files = save_set("Palette", "T_Crystal", D, N.astype(np.float32), M, E)
    emis = T.srgb_to_lin(np.array([0.40, 0.72, 1.00], np.float32))
    return dict(name="Crystal", group="Palette", kind=kind, files=files, tile_m=1.0, size=SIZE, source="Procedural",
                metallic=0, roughness_range=[round(float(np.percentile(rough, 1)), 3),
                                             round(float(np.percentile(rough, 99)), 3)],
                normal_strength=1.0, emissive_color=[round(float(c), 4) for c in emis], emissive_strength=2.0,
                emissive_note="emissive = emissive_color * emissive_strength * T_Crystal_E (linear mask)",
                mean_albedo=SF.mean_albedo_linear(D)), dict(D=D, N=N, M=M, E=E)


BUILDERS = {"Iron": iron, "Gold": gold, "Glass": glass, "Crystal": crystal}


def build(name, kind):
    return BUILDERS[name](kind)
