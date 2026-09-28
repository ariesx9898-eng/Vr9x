#!/usr/bin/env python3
"""
generate_fittoa_terrain.py - procedural source terrain for the Fittoa / Buena Village map.

Outputs (all deterministic for a given --seed):
  SourceArt/Terrain/Fittoa/Fittoa_Height_2017.png      16-bit greyscale heightmap (UE Landscape import)
  SourceArt/Terrain/Fittoa/Fittoa_Height_2017.r16      same data, raw little-endian uint16
  SourceArt/Terrain/Fittoa/Fittoa_W_<layer>.png        8-bit weight maps (grass, farmland, forest_floor,
                                                       dirt_road, riverbed, rock) - they sum to 255 per pixel
  SourceArt/Terrain/Fittoa/Fittoa_Preview.png          colour preview (weights x hillshade)
  SourceArt/Terrain/Fittoa/fittoa_layout.json          landscape settings, key coordinates, roads, river (UE cm)
  Content/Data/Locations.json                          ARRAY of FMTLocationData rows (read by UMTDataRegistry)
  Content/Data/Fittoa_Roads.json                       road + river polylines for AMTVillageGenerator
  Docs/Images/Fittoa_Overview.png                      labelled overview map

Conventions (documented in Docs/World_Fittoa.md and SourceArt/Terrain/Fittoa/README.md):
  * 2017 x 2017 samples, 1 m per sample (Landscape scale X/Y = 100).
  * Image column -> UE +X (east), image row -> UE +Y. Image "up" (row 0) is NORTH, i.e. north = -Y.
  * Landscape scale Z = 100  ->  1 height unit = 100/128 cm = 0.78125 cm; value 32768 == Z 0 cm.
    Heights here are 0..~250 m, so values stay in 32768..64768 (no clipping).
  * New Landscape panel: Location (0,0,0), Rotation (0,0,0), Scale (100,100,100). UE centres the new
    landscape on that location, so the actor ends up at (-100800, -100800, 0) and
    WorldX = (col - 1008) * 100, WorldY = (row - 1008) * 100, WorldZ = height_cm.

Usage:  python3 Tools/generate_fittoa_terrain.py [--seed 1717] [--extra-roads roads.json] [--no-overview]
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "SourceArt", "Terrain", "Fittoa")
DATA_DIR = os.path.join(ROOT, "Content", "Data")
DOC_IMG_DIR = os.path.join(ROOT, "Docs", "Images")

N = 2017                      # samples per side (UE-friendly: 16x16 components of 2x2 sections of 63 quads)
M_PER_PX = 1.0                # metres per sample
XY_SCALE_CM = 100.0           # Landscape scale X/Y
Z_SCALE = 100.0               # Landscape scale Z
CM_PER_UNIT = Z_SCALE / 128.0 # 0.78125 cm per heightmap unit
HALF = (N - 1) / 2.0          # 1008

LAYERS = ["grass", "farmland", "forest_floor", "dirt_road", "riverbed", "rock"]

# ---------------------------------------------------------------------------------------------
# Layout (pixel coordinates, 1 px = 1 m; x = column = east, y = row = south). See Docs/World_Fittoa.md
# ---------------------------------------------------------------------------------------------
VILLAGE = (640.0, 1300.0)       # Buena square (south-west third of the tile)
VILLAGE_FLAT_R = 280.0          # fully flattened basin radius (m)
VILLAGE_FALLOFF_R = 450.0       # basin blends back into countryside by this radius
HILL_TREE = (455.0, 1105.0)     # lone big-tree hill (canon: hill where Rudeus practised magic)
GREYRAT_HOUSE = (560.0, 1215.0) # ASSUMPTION: on the village edge towards the hill
WATCHTOWER = (1500.0, 690.0)    # ASSUMPTION: invented ruin on a rise, north-east
ARENA = (1720.0, 1720.0)        # flat test pad far from the village (south-east)
DEEP_FOREST = (300.0, 560.0)
FORD = None                     # computed: where the Roa road crosses the stream

RIVER_CTRL = [(955, 0), (985, 180), (940, 380), (975, 560), (930, 760), (955, 960), (905, 1120),
              (915, 1300), (975, 1480), (950, 1680), (1010, 1860), (990, 2016)]

ROADS_CTRL = {
    # name: (half_width_m, control points)
    "Road_Roa": (3.0, [VILLAGE, (760, 1285), (850, 1245), (930, 1205), (1050, 1120), (1230, 985),
                        (1420, 850), (1610, 700), (1800, 600), (1930, 565), (2016, 560)]),
    "Road_West_Forest": (2.5, [VILLAGE, (540, 1330), (430, 1300), (300, 1260), (170, 1215), (40, 1180)]),
    "Road_Hill_Path": (1.8, [VILLAGE, (610, 1250), (565, 1222), (505, 1160), (468, 1118)]),
    "Road_South_Farms": (2.5, [VILLAGE, (655, 1400), (690, 1520), (740, 1640), (800, 1760)]),
    "Road_North_Trail": (2.0, [VILLAGE, (650, 1180), (700, 1030), (770, 880), (840, 720), (900, 560),
                                (1000, 400), (1060, 250)]),
    "Road_Watchtower": (1.8, [(1420, 850), (1455, 790), (1485, 730), (1500, 700)]),
    "Lane_Village_Loop": (2.0, [(760, 1285), (790, 1360), (760, 1440), (690, 1480), (600, 1470),
                                 (520, 1420), (505, 1330), (540, 1260), (610, 1250)]),
}


# ---------------------------------------------------------------------------------------------
# Noise (implemented here: gradient noise + fBm + ridged multifractal)
# ---------------------------------------------------------------------------------------------
def _fade(t):
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def gradient_noise(shape, period, rng):
    """Classic 2D Perlin gradient noise, roughly in [-0.7, 0.7]. period in pixels."""
    h, w = shape
    gw = int(math.ceil(w / period)) + 3
    gh = int(math.ceil(h / period)) + 3
    ang = rng.uniform(0.0, 2.0 * math.pi, (gh, gw)).astype(np.float32)
    gxg, gyg = np.cos(ang), np.sin(ang)
    ox, oy = rng.uniform(0.0, 1.0, 2)
    xs = (np.arange(w, dtype=np.float32) / period + ox).astype(np.float32)
    ys = (np.arange(h, dtype=np.float32) / period + oy).astype(np.float32)
    x0 = np.floor(xs).astype(np.int32)
    y0 = np.floor(ys).astype(np.int32)
    fx = (xs - x0)[None, :]
    fy = (ys - y0)[:, None]
    X0 = x0[None, :]
    Y0 = y0[:, None]

    def corner(ix, iy, dx, dy):
        return gxg[iy, ix] * dx + gyg[iy, ix] * dy

    n00 = corner(X0, Y0, fx, fy)
    n10 = corner(X0 + 1, Y0, fx - 1.0, fy)
    n01 = corner(X0, Y0 + 1, fx, fy - 1.0)
    n11 = corner(X0 + 1, Y0 + 1, fx - 1.0, fy - 1.0)
    u = _fade(fx)
    v = _fade(fy)
    nx0 = n00 + u * (n10 - n00)
    nx1 = n01 + u * (n11 - n01)
    return (nx0 + v * (nx1 - nx0)).astype(np.float32)


def fbm(shape, period, octaves, rng, persistence=0.5, lacunarity=2.0):
    total = np.zeros(shape, np.float32)
    amp, norm, p = 1.0, 0.0, float(period)
    for _ in range(octaves):
        total += amp * gradient_noise(shape, p, rng)
        norm += amp
        amp *= persistence
        p = max(2.0, p / lacunarity)
    return total / norm * 1.4  # ~[-1, 1]


def ridged(shape, period, octaves, rng, persistence=0.5, lacunarity=2.0):
    """Ridged multifractal in [0, 1]: sharp crests, used for the Red Wyrm foothills."""
    total = np.zeros(shape, np.float32)
    amp, norm, p = 1.0, 0.0, float(period)
    weight = np.ones(shape, np.float32)
    for _ in range(octaves):
        n = 1.0 - np.abs(gradient_noise(shape, p, rng) * 1.4)
        n = np.clip(n, 0.0, 1.0) ** 2
        n *= weight
        weight = np.clip(n * 1.5, 0.0, 1.0)
        total += amp * n
        norm += amp
        amp *= persistence
        p = max(2.0, p / lacunarity)
    return total / norm


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# ---------------------------------------------------------------------------------------------
# Polylines
# ---------------------------------------------------------------------------------------------
def catmull_rom(points, step=4.0):
    pts = [np.array(p, np.float64) for p in points]
    if len(pts) < 2:
        return np.array(pts)
    ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    out = []
    for i in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[i - 1], ext[i], ext[i + 1], ext[i + 2]
        seg_len = np.linalg.norm(p2 - p1)
        n = max(2, int(seg_len / step))
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
                              (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(pts[-1])
    return np.array(out)


def wobble(poly, rng, amp=3.0, wavelength=55.0):
    """Deterministic lateral meander so roads wind naturally; endpoints stay fixed."""
    if len(poly) < 3:
        return poly
    d = np.diff(poly, axis=0)
    seg = np.linalg.norm(d, axis=1)
    s = np.concatenate([[0.0], np.cumsum(seg)])
    total = s[-1]
    tang = np.gradient(poly, axis=0)
    tang /= np.maximum(np.linalg.norm(tang, axis=1, keepdims=True), 1e-6)
    normal = np.stack([-tang[:, 1], tang[:, 0]], axis=1)
    ph1, ph2 = rng.uniform(0, 2 * math.pi, 2)
    env = np.minimum(1.0, np.minimum(s / 30.0, (total - s) / 30.0))
    off = amp * env * (np.sin(s / wavelength * 2 * math.pi + ph1) * 0.7 +
                       np.sin(s / (wavelength * 0.43) * 2 * math.pi + ph2) * 0.3)
    return poly + normal * off[:, None]


def polyline_field(poly, radius, values=None):
    """Distance (m) from every pixel to the polyline (clipped at radius) and the along-line value at the
    nearest point (linear interpolation per segment). Windowed per segment for speed."""
    dist = np.full((N, N), np.float32(radius), np.float32)
    val = np.zeros((N, N), np.float32)
    if values is None:
        values = np.zeros(len(poly), np.float32)
    for i in range(len(poly) - 1):
        ax, ay = poly[i]
        bx, by = poly[i + 1]
        x0 = int(max(0, math.floor(min(ax, bx) - radius)))
        x1 = int(min(N, math.ceil(max(ax, bx) + radius) + 1))
        y0 = int(max(0, math.floor(min(ay, by) - radius)))
        y1 = int(min(N, math.ceil(max(ay, by) + radius) + 1))
        if x0 >= x1 or y0 >= y1:
            continue
        X = np.arange(x0, x1, dtype=np.float32)[None, :]
        Y = np.arange(y0, y1, dtype=np.float32)[:, None]
        abx, aby = bx - ax, by - ay
        l2 = abx * abx + aby * aby
        if l2 < 1e-9:
            t = np.zeros((y1 - y0, x1 - x0), np.float32)
        else:
            t = np.clip(((X - ax) * abx + (Y - ay) * aby) / l2, 0.0, 1.0)
        px = ax + t * abx
        py = ay + t * aby
        d = np.sqrt((X - px) ** 2 + (Y - py) ** 2)
        dsub = dist[y0:y1, x0:x1]
        vsub = val[y0:y1, x0:x1]
        m = d < dsub
        dsub[m] = d[m]
        vsub[m] = (values[i] + t * (values[i + 1] - values[i]))[m]
    return dist, val


def sample_bilinear(arr, x, y):
    x = min(max(x, 0.0), N - 1.001)
    y = min(max(y, 0.0), N - 1.001)
    ix, iy = int(x), int(y)
    fx, fy = x - ix, y - iy
    a = arr[iy, ix] * (1 - fx) + arr[iy, ix + 1] * fx
    b = arr[iy + 1, ix] * (1 - fx) + arr[iy + 1, ix + 1] * fx
    return float(a * (1 - fy) + b * fy)


def moving_average(v, win):
    if win <= 1 or len(v) < 3:
        return v.copy()
    k = np.ones(win) / win
    pad = win // 2
    vp = np.pad(v, (pad, pad), mode="edge")
    return np.convolve(vp, k, mode="valid")[: len(v)]


def dist_to(p, shape_like=None):
    X = np.arange(N, dtype=np.float32)[None, :]
    Y = np.arange(N, dtype=np.float32)[:, None]
    return np.sqrt((X - p[0]) ** 2 + (Y - p[1]) ** 2)


def px_to_world(x, y, z_m):
    return {"X": round((x - HALF) * XY_SCALE_CM, 1), "Y": round((y - HALF) * XY_SCALE_CM, 1),
            "Z": round(z_m * 100.0, 1)}


def world_to_px(wx, wy):
    return wx / XY_SCALE_CM + HALF, wy / XY_SCALE_CM + HALF


# ---------------------------------------------------------------------------------------------
def build(seed, extra_roads_path=None):
    t0 = time.time()
    rng = np.random.RandomState(seed)
    shape = (N, N)
    X = np.arange(N, dtype=np.float32)[None, :]
    Y = np.arange(N, dtype=np.float32)[:, None]

    # --- base rolling countryside (m) ---
    h = 30.0 + 7.0 * fbm(shape, 700, 4, rng) + 2.5 * fbm(shape, 160, 4, rng) + 0.4 * fbm(shape, 25, 3, rng)
    h = h.astype(np.float32)

    # --- Red Wyrm foothills along the northern border (ASSUMPTION: scale/shape) ---
    north = smoothstep(780.0, 60.0, Y) * np.ones((1, N), np.float32)
    rid = ridged(shape, 420, 6, rng)
    h += (north ** 1.6) * (35.0 + 175.0 * rid + 12.0 * fbm(shape, 90, 3, rng))

    # --- border rim: distant terrain / natural bounds (west, south, east except the Roa road exit) ---
    rim_noise = 0.6 + 0.4 * fbm(shape, 300, 3, rng)
    rim_w = smoothstep(190.0, 0.0, X) * np.ones((N, 1), np.float32)
    rim_s = smoothstep(190.0, 0.0, (N - 1) - Y) * np.ones((1, N), np.float32)
    road_gap = 1.0 - smoothstep(160.0, 60.0, np.abs(Y - 560.0))
    rim_e = smoothstep(170.0, 0.0, (N - 1) - X) * road_gap
    h += rim_noise * (24.0 * rim_w + 22.0 * rim_s + 14.0 * rim_e)

    # --- Buena basin: mostly flat, tiny micro relief ---
    dv = dist_to(VILLAGE)
    basin_target = float(np.mean(h[int(VILLAGE[1]) - 150:int(VILLAGE[1]) + 150,
                                   int(VILLAGE[0]) - 150:int(VILLAGE[0]) + 150]))
    w_basin = smoothstep(VILLAGE_FALLOFF_R, VILLAGE_FLAT_R, dv)
    micro = 0.35 * fbm(shape, 60, 2, rng)
    h = h * (1.0 - w_basin) + (basin_target + micro) * w_basin

    # --- lone big-tree hill (canon feature near the village) ---
    dt = dist_to(HILL_TREE)
    h += 15.0 * 0.5 * (1.0 + np.cos(np.pi * np.minimum(dt / 175.0, 1.0)))

    # --- watchtower rise (ASSUMPTION) ---
    dw = dist_to(WATCHTOWER)
    h += 22.0 * 0.5 * (1.0 + np.cos(np.pi * np.minimum(dw / 160.0, 1.0)))

    # --- arena pad: flat test area far from the village ---
    da = dist_to(ARENA)
    arena_target = float(np.mean(h[int(ARENA[1]) - 60:int(ARENA[1]) + 60, int(ARENA[0]) - 60:int(ARENA[0]) + 60]))
    w_ar = smoothstep(140.0, 75.0, da)
    h = h * (1.0 - w_ar) + arena_target * w_ar
    print(f"[terrain] base layers done ({time.time() - t0:.1f}s)")

    # --- roads: smooth + wobble, then gentle cut/fill toward a smoothed along-road profile ---
    roads = {}
    for name, (hw, ctrl) in ROADS_CTRL.items():
        poly = catmull_rom(ctrl, step=4.0)
        if not name.startswith("Road_Hill"):
            poly = wobble(poly, rng, amp=2.5 if name.startswith("Lane") else 3.5)
        roads[name] = {"half_width": hw, "poly": poly, "painted_only": False}
    if extra_roads_path:
        with open(extra_roads_path, "r", encoding="utf-8") as f:
            extra = json.load(f)
        for i, r in enumerate(extra.get("Roads", [])):
            pts = [world_to_px(p["X"], p["Y"]) for p in r.get("Points", [])]
            if len(pts) >= 2:
                roads[r.get("Name", f"Extra_{i}")] = {"half_width": r.get("HalfWidthCm", 250.0) / 100.0,
                                                      "poly": np.array(pts), "painted_only": False}
        print(f"[terrain] merged {len(extra.get('Roads', []))} extra roads from {extra_roads_path}")

    road_dist = np.full(shape, 1e6, np.float32)
    road_hw_at = np.zeros(shape, np.float32)
    for name, r in roads.items():
        poly = r["poly"]
        prof = np.array([sample_bilinear(h, p[0], p[1]) for p in poly], np.float32)
        prof = moving_average(prof, 15).astype(np.float32)
        infl = r["half_width"] + 12.0
        d, target = polyline_field(poly, infl, prof)
        w = smoothstep(r["half_width"] + 12.0, r["half_width"] + 1.0, d)
        h = h * (1.0 - w) + target * w
        closer = d < road_dist
        road_dist[closer] = d[closer]
        road_hw_at[closer] = r["half_width"]
        r["profile"] = prof
    print(f"[terrain] roads carved ({time.time() - t0:.1f}s)")

    # --- stream: monotonic bed profile (flows north -> south), smooth banks, shallow ford ---
    river = catmull_rom(RIVER_CTRL, step=4.0)
    river = wobble(river, rng, amp=6.0, wavelength=90.0)
    # ford = closest river sample to the Roa road
    roa = roads["Road_Roa"]["poly"]
    best = (1e9, 0, 0)
    for i in range(0, len(river)):
        dd = np.min(np.hypot(roa[:, 0] - river[i, 0], roa[:, 1] - river[i, 1]))
        if dd < best[0]:
            best = (dd, i, 0)
    ford_i = best[1]
    ford = (float(river[ford_i, 0]), float(river[ford_i, 1]))
    rs = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(river, axis=0), axis=1))])
    ground = np.array([sample_bilinear(h, p[0], p[1]) for p in river], np.float32)
    ground = moving_average(ground, 21)
    bed = np.minimum.accumulate(ground)            # never flows uphill
    ford_w = np.clip(1.0 - np.abs(rs - rs[ford_i]) / 45.0, 0.0, 1.0)
    # every other road/trail that crosses the stream also gets a shallow crossing (no 2.4 m trench in a trail)
    for rname, r in roads.items():
        rp = r["poly"]
        dmin = np.array([np.min(np.hypot(rp[:, 0] - q[0], rp[:, 1] - q[1])) for q in river[::2]])
        j = int(np.argmin(dmin)) * 2
        if dmin[j // 2] < 8.0 and abs(rs[j] - rs[ford_i]) > 60.0:
            ford_w = np.maximum(ford_w, np.clip(1.0 - np.abs(rs - rs[j]) / 30.0, 0.0, 1.0))
            print(f"[terrain] extra shallow crossing for {rname} at px ({river[j, 0]:.0f}, {river[j, 1]:.0f})")
    depth = 2.4 * (1.0 - ford_w) + 0.45 * ford_w   # ford: ~45 cm deep, walkable
    bed_profile = (bed - depth).astype(np.float32)
    half_w = (6.0 * (1.0 - ford_w) + 11.0 * ford_w).astype(np.float32)
    rd, rbed = polyline_field(river, 40.0, bed_profile)
    _, rhalf = polyline_field(river, 40.0, half_w)
    rhalf = np.where(rhalf > 0, rhalf, 6.0).astype(np.float32)
    w_r = smoothstep(rhalf + 16.0, rhalf, rd)
    carved = h * (1.0 - w_r) + rbed * w_r
    h = np.minimum(h, carved)                      # river only cuts, never builds levees
    print(f"[terrain] river carved, ford at px {ford} ({time.time() - t0:.1f}s)")

    # --- slope (degrees) ---
    gy, gx = np.gradient(h, M_PER_PX)
    slope = np.degrees(np.arctan(np.sqrt(gx * gx + gy * gy)))

    # --- weights (painter's order: road > riverbed > rock > forest > farmland > grass) ---
    w_road = smoothstep(road_hw_at + 1.6, road_hw_at - 0.4, road_dist)
    w_road = np.maximum(w_road, smoothstep(17.0, 13.0, dist_to(VILLAGE)))  # packed-dirt village square (14 m)
    w_river = smoothstep(rhalf + 5.0, rhalf + 1.0, rd)
    w_rock = np.clip(smoothstep(24.0, 36.0, slope) + smoothstep(120.0, 175.0, h) * (0.4 + 0.6 * rid), 0, 1)
    forest_noise = fbm(shape, 140, 4, rng)
    west = smoothstep(430.0, 250.0, X) * smoothstep(1950.0, 1700.0, Y)
    northband = smoothstep(960.0, 700.0, Y) * smoothstep(120.0, 300.0, Y) * smoothstep(1700.0, 1350.0, X)
    forest = np.clip(np.maximum(west, northband) + 0.45 * forest_noise, 0.0, 1.0)
    forest *= smoothstep(VILLAGE_FALLOFF_R - 60.0, VILLAGE_FALLOFF_R + 40.0, dv)  # keep the basin open
    forest *= smoothstep(120.0, 190.0, dt)                                        # hill stays bare (one big tree)
    forest *= smoothstep(150.0, 200.0, da)
    forest = smoothstep(0.35, 0.65, forest)
    farm_ring = smoothstep(70.0, 100.0, dv) * smoothstep(400.0, 340.0, dv)
    farm = farm_ring * smoothstep(-0.25, 0.1, fbm(shape, 90, 3, rng)) * smoothstep(150.0, 185.0, dt)
    farm *= smoothstep(14.0, 8.0, slope)

    remaining = np.ones(shape, np.float32)
    weights = {}
    for name, raw in (("dirt_road", w_road), ("riverbed", w_river), ("rock", w_rock),
                      ("forest_floor", forest), ("farmland", farm)):
        wv = np.clip(raw, 0.0, 1.0) * remaining
        weights[name] = wv
        remaining = remaining - wv
    weights["grass"] = np.clip(remaining, 0.0, 1.0)

    return {"h": h.astype(np.float32), "slope": slope, "weights": weights, "roads": roads, "river": river,
            "ford": ford, "river_half": half_w, "bed": bed_profile, "basin_z": basin_target,
            "arena_z": arena_target}


# ---------------------------------------------------------------------------------------------
def export(res, seed, overview=True):
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(DATA_DIR, exist_ok=True)
    os.makedirs(DOC_IMG_DIR, exist_ok=True)
    h = res["h"]
    hcm = h * 100.0
    units = np.clip(np.round(32768.0 + hcm / CM_PER_UNIT), 0, 65535).astype(np.uint16)
    clipped = int(np.sum((32768.0 + hcm / CM_PER_UNIT) > 65535) + np.sum((32768.0 + hcm / CM_PER_UNIT) < 0))
    Image.fromarray(units).save(os.path.join(OUT_DIR, "Fittoa_Height_2017.png"))
    units.astype("<u2").tofile(os.path.join(OUT_DIR, "Fittoa_Height_2017.r16"))

    # weights -> 8 bit, grass absorbs rounding so every pixel sums to exactly 255
    q = {}
    acc = np.zeros(h.shape, np.int32)
    for name in LAYERS[1:]:
        q[name] = np.clip(np.round(res["weights"][name] * 255.0), 0, 255).astype(np.int32)
        acc += q[name]
    over = np.maximum(acc - 255, 0)
    for name in ("farmland", "forest_floor", "rock", "riverbed", "dirt_road"):  # lowest priority first
        if not over.any():
            break
        take = np.minimum(over, q[name])
        q[name] -= take
        over -= take
    acc = sum(q[n] for n in LAYERS[1:])
    q["grass"] = np.clip(255 - acc, 0, 255)
    for name in LAYERS:
        Image.fromarray(q[name].astype(np.uint8)).save(os.path.join(OUT_DIR, f"Fittoa_W_{name}.png"))
    sums = sum(q[n] for n in LAYERS)

    # colour preview: weighted albedo x hillshade
    col = {"grass": (96, 140, 62), "farmland": (196, 170, 92), "forest_floor": (48, 84, 44),
           "dirt_road": (150, 118, 80), "riverbed": (70, 110, 140), "rock": (128, 124, 118)}
    rgb = np.zeros(h.shape + (3,), np.float32)
    for name in LAYERS:
        rgb += (q[name][..., None] / 255.0) * np.array(col[name], np.float32)[None, None, :]
    gy, gx = np.gradient(h)
    nx, ny, nz = -gx * 2.0, -gy * 2.0, np.ones_like(h)
    nl = np.sqrt(nx * nx + ny * ny + nz * nz)
    light = np.array([-0.55, -0.55, 0.63])
    light /= np.linalg.norm(light)
    shade = np.clip((nx * light[0] + ny * light[1] + nz * light[2]) / nl, 0.0, 1.0)
    rgb *= (0.45 + 0.75 * shade)[..., None]
    water = res["weights"]["riverbed"] > 0.6
    rgb[water] = rgb[water] * 0.4 + np.array([60, 110, 170], np.float32) * 0.6
    preview = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))
    preview.save(os.path.join(OUT_DIR, "Fittoa_Preview.png"))

    # ---- key locations (Z sampled from the final heightmap) ----
    ford = res["ford"]
    roa_poly = res["roads"]["Road_Roa"]["poly"]
    mid_road = roa_poly[int(len(roa_poly) * 0.62)]
    gate = roa_poly[-1] - np.array([45.0, 0.0])  # just inside the east edge where the Roa road exits
    locs = [
        # id, display, px, py, radius_cm, fast_travel, rank
        ("Buena_Village", "Buena Village", VILLAGE[0], VILLAGE[1] + 10, 30000, True, "F"),
        ("Buena_Square", "Buena Village Square", VILLAGE[0], VILLAGE[1], 2500, True, "F"),
        ("Buena_Greyrat_House", "Greyrat House", GREYRAT_HOUSE[0], GREYRAT_HOUSE[1], 1500, False, "F"),
        ("Buena_Farms", "Buena Wheat Fields", 760.0, 1470.0, 9000, False, "F"),
        ("Buena_Hill_Tree", "Hill of the Lone Tree", HILL_TREE[0], HILL_TREE[1], 3000, True, "F"),
        ("Buena_Forest_Edge", "Buena Forest Edge", 250.0, 1250.0, 6000, False, "F"),
        ("Fittoa_River_Ford", "Fittoa Stream Ford", ford[0], ford[1], 3000, True, "F"),
        ("Fittoa_Deep_Forest", "Deep Fittoa Forest", DEEP_FOREST[0], DEEP_FOREST[1], 12000, False, "E"),
        ("Fittoa_Watchtower_Ruins", "Old Watchtower Ruins", WATCHTOWER[0], WATCHTOWER[1], 4000, True, "D"),
        ("Fittoa_Road_To_Roa", "Road to Roa", float(mid_road[0]), float(mid_road[1]), 5000, False, "F"),
        ("Roa_Gate", "Roa Road Waystone (to the Citadel of Roa)", float(gate[0]), float(gate[1]), 3000, True, "F"),
        ("Fittoa_Wyrm_Foothills", "Red Wyrm Foothills", 1060.0, 250.0, 15000, False, "C"),
        ("Arena_Test", "Test Arena (dev)", ARENA[0], ARENA[1], 5000, False, "F"),
    ]
    loc_rows, loc_px = [], {}
    for lid, disp, x, y, rad, ft, rank in locs:
        z = sample_bilinear(h, x, y)
        loc_rows.append({"LocationID": lid, "DisplayName": disp, "Region": "Fittoa",
                         "WorldLocation": px_to_world(x, y, z), "DiscoveryRadius": float(rad),
                         "bFastTravel": ft, "RequiredRank": rank})
        loc_px[lid] = (x, y, z)
    with open(os.path.join(DATA_DIR, "Locations.json"), "w", encoding="utf-8") as f:
        json.dump(loc_rows, f, indent="\t")
        f.write("\n")

    def poly_world(poly, zs=None):
        out = []
        for i, p in enumerate(poly[::3] if len(poly) > 6 else poly):
            z = sample_bilinear(h, float(p[0]), float(p[1]))
            out.append(px_to_world(float(p[0]), float(p[1]), z))
        last = poly[-1]
        lw = px_to_world(float(last[0]), float(last[1]), sample_bilinear(h, float(last[0]), float(last[1])))
        if out[-1] != lw:
            out.append(lw)
        return out

    roads_json = {
        "Comment": "Generated by Tools/generate_fittoa_terrain.py. UE world space (cm). Roads are PAINTED into "
                   "the landscape dirt_road layer (no road meshes -> no z-fighting). AMTVillageGenerator reads "
                   "this file to align houses/fields and to build exclusion zones.",
        "Seed": seed,
        "Village": {"Center": px_to_world(VILLAGE[0], VILLAGE[1], loc_px["Buena_Square"][2]),
                    "FlatRadiusCm": VILLAGE_FLAT_R * 100.0},
        "Roads": [{"Name": n, "HalfWidthCm": r["half_width"] * 100.0, "Points": poly_world(r["poly"])}
                  for n, r in res["roads"].items()],
        "Rivers": [{"Name": "Fittoa_Stream", "HalfWidthCm": 600.0 + 1600.0,
                    "Comment": "HalfWidthCm includes the 16 m carved bank so nothing is placed on the banks",
                    "Points": poly_world(res["river"])}],
    }
    with open(os.path.join(DATA_DIR, "Fittoa_Roads.json"), "w", encoding="utf-8") as f:
        json.dump(roads_json, f, indent=1)

    layout = {
        "Generator": "Tools/generate_fittoa_terrain.py", "Seed": seed,
        "Heightmap": {"Resolution": [N, N], "File": "Fittoa_Height_2017.png", "Raw": "Fittoa_Height_2017.r16",
                      "MetresPerSample": M_PER_PX, "ZeroHeightValue": 32768, "CmPerUnit": CM_PER_UNIT,
                      "ClippedSamples": clipped,
                      "MinHeightCm": round(float(hcm.min()), 1), "MaxHeightCm": round(float(hcm.max()), 1)},
        "LandscapeImport": {"NewLandscapeLocation": [0, 0, 0], "Scale": [XY_SCALE_CM, XY_SCALE_CM, Z_SCALE],
                            "ResultingActorLocation": [-HALF * XY_SCALE_CM, -HALF * XY_SCALE_CM, 0],
                            "SectionSize": "63x63 quads", "SectionsPerComponent": "2x2",
                            "NumberOfComponents": "16x16", "WorldPartitionGridSize": 2,
                            "Layers": LAYERS},
        "Conventions": {"PlusX": "east", "MinusY": "north (image up)",
                        "WorldFromPixel": "X=(col-1008)*100, Y=(row-1008)*100, Z=height_cm"},
        "Locations": {k: px_to_world(v[0], v[1], v[2]) for k, v in loc_px.items()},
        "WeightSumCheck": {"min": int(sums.min()), "max": int(sums.max())},
        "BasinHeightCm": round(res["basin_z"] * 100.0, 1), "ArenaHeightCm": round(res["arena_z"] * 100.0, 1),
    }
    with open(os.path.join(OUT_DIR, "fittoa_layout.json"), "w", encoding="utf-8") as f:
        json.dump(layout, f, indent=1)

    if overview:
        render_overview(preview, res, loc_px)
    return layout


def render_overview(preview, res, loc_px):
    S = 1400
    img = preview.resize((S, S), Image.LANCZOS).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    k = S / (N - 1)
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 17)
        small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14)
        title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
    except OSError:
        font = small = title = ImageFont.load_default()
    # village radius rings
    vx, vy = VILLAGE[0] * k, VILLAGE[1] * k
    for r, c in ((VILLAGE_FLAT_R, (255, 255, 255, 110)), (400.0, (230, 200, 90, 90))):
        d.ellipse([vx - r * k, vy - r * k, vx + r * k, vy + r * k], outline=c, width=2)
    # roads + river
    for name, r in res["roads"].items():
        pts = [(float(p[0]) * k, float(p[1]) * k) for p in r["poly"]]
        d.line(pts, fill=(245, 225, 170, 235), width=max(2, int(r["half_width"] * 1.2)))
    rv = [(float(p[0]) * k, float(p[1]) * k) for p in res["river"]]
    d.line(rv, fill=(90, 170, 255, 255), width=4)
    # locations
    offsets = {"Buena_Village": (12, 18), "Buena_Square": (12, -8), "Buena_Greyrat_House": (-170, -26),
               "Buena_Farms": (12, 0), "Buena_Hill_Tree": (-150, -34), "Buena_Forest_Edge": (-60, 16),
               "Fittoa_River_Ford": (12, -24), "Roa_Gate": (-150, 14), "Fittoa_Road_To_Roa": (12, 4)}
    for lid, (x, y, z) in loc_px.items():
        px, py = x * k, y * k
        d.ellipse([px - 7, py - 7, px + 7, py + 7], fill=(220, 40, 40, 255), outline=(255, 255, 255, 255), width=2)
        ox, oy = offsets.get(lid, (12, -10))
        label = f"{lid}  ({z:.0f} m)"
        tb = d.textbbox((px + ox, py + oy), label, font=font)
        d.rectangle([tb[0] - 3, tb[1] - 2, tb[2] + 3, tb[3] + 2], fill=(0, 0, 0, 150))
        d.text((px + ox, py + oy), label, fill=(255, 255, 255, 255), font=font)
    # title, north arrow, scale bar, legend
    d.rectangle([0, 0, S, 44], fill=(0, 0, 0, 170))
    d.text((14, 8), "Fittoa Region - Buena Village map (2.0 x 2.0 km, 1 m/px)  |  generated, seed-deterministic",
           fill=(255, 255, 255, 255), font=title)
    ax, ay = S - 60, 110
    d.polygon([(ax, ay - 40), (ax - 16, ay + 6), (ax + 16, ay + 6)], fill=(255, 255, 255, 230))
    d.text((ax - 8, ay + 10), "N", fill=(255, 255, 255, 255), font=title)
    d.text((ax - 60, ay + 44), "(north = -Y)", fill=(255, 255, 255, 255), font=small)
    bx, by = 30, S - 40
    d.rectangle([bx - 8, by - 34, bx + 500 * k + 20, by + 14], fill=(0, 0, 0, 140))
    d.line([(bx, by), (bx + 500 * k, by)], fill=(255, 255, 255, 255), width=5)
    d.text((bx, by - 28), "500 m", fill=(255, 255, 255, 255), font=font)
    legend = [("dirt road (landscape paint layer)", (245, 225, 170)), ("stream (carved)", (90, 170, 255)),
              ("village flat basin (280 m)", (255, 255, 255)), ("farmland ring (~400 m)", (230, 200, 90))]
    lx, ly = S - 390, S - 130
    d.rectangle([lx - 10, ly - 10, S - 10, S - 10], fill=(0, 0, 0, 150))
    for i, (txt, c) in enumerate(legend):
        d.rectangle([lx, ly + i * 28, lx + 22, ly + i * 28 + 16], fill=c + (255,))
        d.text((lx + 32, ly + i * 28), txt, fill=(255, 255, 255, 255), font=small)
    img.save(os.path.join(DOC_IMG_DIR, "Fittoa_Overview.png"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seed", type=int, default=1717)
    ap.add_argument("--extra-roads", default=None,
                    help="JSON exported by AMTVillageGenerator::ExportRoadsJson (paints procedural lanes too)")
    ap.add_argument("--no-overview", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    res = build(args.seed, args.extra_roads)
    layout = export(res, args.seed, overview=not args.no_overview)
    hm = layout["Heightmap"]
    print(f"[terrain] heightmap {N}x{N}, height {hm['MinHeightCm'] / 100:.1f} m .. {hm['MaxHeightCm'] / 100:.1f} m, "
          f"clipped samples: {hm['ClippedSamples']}")
    print(f"[terrain] weight sum per pixel: {layout['WeightSumCheck']}")
    for k, v in layout["Locations"].items():
        print(f"  {k:26s} X={v['X']:10.1f} Y={v['Y']:10.1f} Z={v['Z']:9.1f}")
    print(f"[terrain] done in {time.time() - t0:.1f}s -> {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
