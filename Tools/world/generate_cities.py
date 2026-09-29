#!/usr/bin/env python3
"""City, town and village layouts for LA PLACE, built from the architecture kit.

Reads the frozen world (Content/Data/World.json, Content/Data/Locations.json, SourceArt/World/Height.r16) and the
architecture kit manifest (SourceArt/Kit/manifest_architecture.json), lays out walls, gates, streets, plazas,
buildings and props for every site, and writes:

  SourceArt/World/Scatter/Cities.json + Cities.bin   instance set (same format as Foliage.json / .bin): float32 LE
                                                      records (MeshIndex, X, Y, Z cm, Yaw, Pitch, Roll deg, Scale);
                                                      one group per site and class, City_<SiteId>_<Class>
  SourceArt/World/Cities/StreetCobble.png            6097 x 4573 8-bit masks on the landscape vertex grid
  SourceArt/World/Cities/StreetDirt.png              (255 = fully paved, ~1.2 vertex soft edge)
  SourceArt/World/Cities/_Plan_<SiteId>.png          top-down review plan per site
  Docs/Images/LaPlace_Cities.jpg                     contact sheet of the main sites
  Docs/LaPlace/Cities.md                             rules, per-site counts, rebuild command

Conventions (Spec section 2, Kit.md "Architecture kit"): world X east, Y south, Z up, centimetres. Unreal yaw 0 = +X,
90 = +Y. A kit point (bx, by, bz) m in Blender lands at local (100 bx, -100 by, 100 bz) cm, so building fronts face
local +Y and a front facing world direction (dx, dy) needs yaw = atan2(dy, dx) - 90. Layout maths runs in a local
frame per site: metres, origin at the site centre, x east, y south.

Pipeline per site: water / road rasters -> landmarks and special buildings (slid to flat ground where needed) -> wall
line by polar dynamic programming, exact 12 m segment chain with towers and gates at road crossings -> squares, main
streets (the World.json roads), avenues -> tensor-field streamline streets -> frontage placement at full density ->
thinning to the budget along a radial density profile -> props -> exact validation. Bridges come from Bridges[].

Deterministic (fixed seed), numpy + Pillow only, ~35 s:

    ~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_cities.py              # everything
    ~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_cities.py --no-plans   # skip review images
    ~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_cities.py --sites Ars  # preview plans only
"""
import argparse
import json
import math
import os
import sys
import time
import zlib

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WORLD_JSON = os.path.join(ROOT, "Content", "Data", "World.json")
LOCATIONS_JSON = os.path.join(ROOT, "Content", "Data", "Locations.json")
KIT_MANIFEST = os.path.join(ROOT, "SourceArt", "Kit", "manifest_architecture.json")
WORLD_DIR = os.path.join(ROOT, "SourceArt", "World")
OUT_SCATTER = os.path.join(WORLD_DIR, "Scatter")
OUT_CITIES = os.path.join(WORLD_DIR, "Cities")
DOC_IMAGE = os.path.join(ROOT, "Docs", "Images", "LaPlace_Cities.jpg")
DOC_MD = os.path.join(ROOT, "Docs", "LaPlace", "Cities.md")
GENERATOR = "Tools/world/generate_cities.py"
SEED = 20260928
HLOD_CITY = "/Game/LaPlace/World/HLOD/HLOD_City.HLOD_City"
CLASSES = ("Buildings", "Walls", "Props", "Landmark")

# Per-site caps for landmark-like kit buildings placed by the frontage pass (towers stay rare accents)
DEFAULT_CAPS = {"SM_Asura_Tower_A": 6, "SM_Noble_Tower_A": 3, "SM_North_Tower_A": 5, "SM_Millis_Tower_A": 6,
                "SM_Demon_Tower_A": 5, "SM_Desert_Tower_A": 5, "SM_Asura_Inn_A": 10, "SM_Desert_Inn_A": 8,
                "SM_Millis_Chapel_A": 4, "SM_Asura_Manor_A": 3, "SM_North_Workshop_A": 18}

# Raster flags (per-site 0.5 m grid)
F_STREET, F_PLAZA, F_WATER, F_WALL, F_BUILD, F_LANDMARK, F_RESERVED, F_OUTSIDE, F_PROP, F_SHORE, F_KEEPIN, F_ROAD = \
    (1 << i for i in range(12))
F_BLOCK = F_STREET | F_PLAZA | F_WATER | F_SHORE | F_WALL | F_LANDMARK | F_RESERVED | F_OUTSIDE | F_KEEPIN
CELL = 0.5            # site raster cell (m)
MASK_EDGE = 3.6       # street mask soft edge width (m), centred on the paved edge (~1.2 landscape vertices)
MIN_GAP = 1.0         # minimum clear gap between buildings (m)
STREET_CLEAR = 0.6    # raster clearance between a building and a street / plaza cell (m)
STREET_PAD = 0.35     # streets are rasterised this much wider, so the exact clearance is >= ~0.6 m


def log(msg):
    print(msg, flush=True)


# ============================================================================================ data


class Terrain:
    """Full-resolution landscape heights (cm) with bilinear sampling in world cm."""

    def __init__(self, world):
        ls = world["Landscape"]
        self.nx, self.ny = ls["VerticesX"], ls["VerticesY"]
        self.q = float(ls["QuadSizeCm"])
        self.x0, self.y0 = ls["LocationCm"]["X"], ls["LocationCm"]["Y"]
        z0, zs = ls["LocationCm"]["Z"], ls["ScaleCm"]["Z"]
        raw = np.fromfile(os.path.join(ROOT, ls["Heightmap"]), dtype="<u2").reshape(self.ny, self.nx)
        self.h = (z0 + (raw.astype(np.float32) - 32768.0) * np.float32(zs / 128.0)).astype(np.float32)

    def z(self, x, y):
        x = np.asarray(x, np.float64)
        y = np.asarray(y, np.float64)
        u = np.clip((x - self.x0) / self.q, 0, self.nx - 1.001)
        v = np.clip((y - self.y0) / self.q, 0, self.ny - 1.001)
        i = np.floor(u).astype(np.int64)
        j = np.floor(v).astype(np.int64)
        fu = u - i
        fv = v - j
        h = self.h
        top = h[j, i] * (1 - fu) + h[j, i + 1] * fu
        bot = h[j + 1, i] * (1 - fu) + h[j + 1, i + 1] * fu
        return top * (1 - fv) + bot * fv


class Asset:
    """Kit asset with its footprint in Unreal local metres (x = Blender x, y = -Blender y; fronts face +y)."""

    def __init__(self, a):
        self.name = a["name"]
        self.category = a["category"]
        self.kind = a.get("kind", "")
        self.path = "/Game/LaPlace/Kit/%s/%s" % (a["category"], a["name"])
        (bx0, by0, _bz0), (bx1, by1, _bz1) = a["bounds"]
        self.x0, self.x1 = float(bx0), float(bx1)
        self.y0, self.y1 = float(-by1), float(-by0)
        self.ocx = 0.5 * (self.x0 + self.x1)
        self.ocy = 0.5 * (self.y0 + self.y1)
        self.hx = 0.5 * (self.x1 - self.x0)
        self.hy = 0.5 * (self.y1 - self.y0)
        self.w = self.x1 - self.x0
        self.d = self.y1 - self.y0
        self.h = float(a["height"])
        self.min_z = float(a.get("min_z", 0.0))
        self.attach = a.get("attach", {}) or {}
        self.big = self.w * self.d > 230.0 or self.h > 20.0

    def __repr__(self):
        return self.name


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


# ============================================================================================ geometry


def unit(v):
    v = np.asarray(v, np.float64)
    n = float(np.hypot(v[0], v[1]))
    return v / n if n > 1e-12 else np.array([1.0, 0.0])


def ang_of(v):
    return math.degrees(math.atan2(v[1], v[0]))


def yaw_facing(dx, dy):
    """Actor yaw that makes a kit front (local +Y) face world direction (dx, dy)."""
    return math.degrees(math.atan2(dy, dx)) - 90.0


def wrap_deg(a):
    return (a + 180.0) % 360.0 - 180.0


def obb_of(asset, x, y, yaw, scale=1.0, pad=0.0):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    ox, oy = asset.ocx * scale, asset.ocy * scale
    return (x + ox * c - oy * s, y + ox * s + oy * c, asset.hx * scale + pad, asset.hy * scale + pad, c, s)


def obb_corners(o):
    cx, cy, hx, hy, c, s = o
    return [(cx + ex * hx * c - ey * hy * s, cy + ex * hx * s + ey * hy * c) for ex, ey in ((-1, -1), (1, -1), (1, 1), (-1, 1))]


def obb_overlap(a, b, gap=0.0):
    """Separating-axis test; True when the boxes are closer than `gap` along every candidate axis."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    for ux, uy in ((a[4], a[5]), (-a[5], a[4]), (b[4], b[5]), (-b[5], b[4])):
        ra = a[2] * abs(ux * a[4] + uy * a[5]) + a[3] * abs(-ux * a[5] + uy * a[4])
        rb = b[2] * abs(ux * b[4] + uy * b[5]) + b[3] * abs(-ux * b[5] + uy * b[4])
        if abs(dx * ux + dy * uy) >= ra + rb + gap:
            return False
    return True


def points_in_poly(px, py, poly):
    inside = np.zeros(np.shape(px), bool)
    xj, yj = poly[-1]
    for xi, yi in poly:
        cond = ((yi > py) != (yj > py)) & (px < (xj - xi) * (py - yi) / ((yj - yi) if abs(yj - yi) > 1e-12 else 1e-12) + xi)
        inside ^= cond
        xj, yj = xi, yi
    return inside


def seg_dist(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    l2 = vx * vx + vy * vy
    t = np.clip(((px - ax) * vx + (py - ay) * vy) / np.maximum(l2, 1e-12), 0.0, 1.0)
    return np.hypot(px - (ax + t * vx), py - (ay + t * vy))


def seg_intersect(p1, p2, q1, q2):
    """Intersection parameters (t on p, u on q) of two segments, or None."""
    rx, ry = p2[0] - p1[0], p2[1] - p1[1]
    sx, sy = q2[0] - q1[0], q2[1] - q1[1]
    den = rx * sy - ry * sx
    if abs(den) < 1e-12:
        return None
    qx, qy = q1[0] - p1[0], q1[1] - p1[1]
    t = (qx * sy - qy * sx) / den
    u = (qx * ry - qy * rx) / den
    if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
        return t, u
    return None


class Line:
    """Polyline with arc-length parametrisation (local metres)."""

    def __init__(self, pts, closed=False):
        p = np.asarray(pts, np.float64).reshape(-1, 2)
        if closed and len(p) > 2 and np.hypot(*(p[0] - p[-1])) > 1e-6:
            p = np.vstack([p, p[:1]])
        self.p = p
        seg = np.hypot(*np.diff(p, axis=0).T) if len(p) > 1 else np.zeros(0)
        self.cum = np.r_[0.0, np.cumsum(seg)]
        self.L = float(self.cum[-1])
        self.closed = closed

    def at(self, s):
        s = np.asarray(s, np.float64)
        if self.closed and self.L > 0:
            s = np.mod(s, self.L)
        else:
            s = np.clip(s, 0.0, self.L)
        k = np.clip(np.searchsorted(self.cum, s, side="right") - 1, 0, len(self.p) - 2)
        d = self.cum[k + 1] - self.cum[k]
        t = (s - self.cum[k]) / np.where(d > 1e-12, d, 1e-12)
        return self.p[k] + (self.p[k + 1] - self.p[k]) * np.asarray(t)[..., None]

    def tangent(self, s, h=2.0):
        return unit(self.at(s + h) - self.at(s - h))

    def resample(self, step):
        n = max(2, int(math.ceil(self.L / step)) + 1)
        ss = np.linspace(0.0, self.L, n)
        if self.closed:
            ss = ss[:-1]
        return self.at(ss)

    def project(self, q):
        """Arc parameter of the point on the line nearest to q."""
        a = self.p[:-1]
        b = self.p[1:]
        v = b - a
        l2 = np.maximum((v * v).sum(1), 1e-12)
        t = np.clip(((q[0] - a[:, 0]) * v[:, 0] + (q[1] - a[:, 1]) * v[:, 1]) / l2, 0, 1)
        px = a[:, 0] + t * v[:, 0]
        py = a[:, 1] + t * v[:, 1]
        d = np.hypot(px - q[0], py - q[1])
        k = int(np.argmin(d))
        return float(self.cum[k] + t[k] * math.sqrt(l2[k])), float(d[k])


def smooth_noise(rng, terms=3, kmin=2, kmax=7):
    ks = rng.integers(kmin, kmax + 1, terms).astype(float)
    ph = rng.uniform(0, 2 * math.pi, terms)
    am = rng.uniform(0.5, 1.0, terms) / np.sqrt(ks)
    am /= am.sum()

    def f(th):
        th = np.asarray(th, np.float64)
        return sum(a * np.sin(k * th + p) for a, k, p in zip(am, ks, ph))
    return f


def weighted_order(rng, items, weights, k):
    """Up to k distinct items drawn by weight (Efraimidis-Spirakis)."""
    w = np.asarray(weights, np.float64)
    ok = w > 0
    if not ok.any():
        return []
    idx = np.flatnonzero(ok)
    keys = rng.random(len(idx)) ** (1.0 / w[idx])
    order = idx[np.argsort(-keys)][:k]
    return [items[i] for i in order]


# ============================================================================================ rasters


class Grid:
    """Square raster of uint16 flags over a site (local metres, cell centres at -half + (i + .5) * cell)."""

    def __init__(self, half, cell):
        self.half = float(half)
        self.cell = float(cell)
        self.n = int(math.ceil(2 * half / cell))
        self.flags = np.zeros((self.n, self.n), np.uint16)

    def centres(self, i0, i1):
        return -self.half + (np.arange(i0, i1) + 0.5) * self.cell

    def get(self, x, y):
        x = np.asarray(x, np.float64)
        y = np.asarray(y, np.float64)
        i = np.floor((x + self.half) / self.cell).astype(np.int64)
        j = np.floor((y + self.half) / self.cell).astype(np.int64)
        ok = (i >= 0) & (i < self.n) & (j >= 0) & (j < self.n)
        out = np.full(np.shape(i), F_OUTSIDE, np.uint16)
        out[ok] = self.flags[j[ok], i[ok]]
        return out

    def flag_at(self, x, y):
        i = int(math.floor((x + self.half) / self.cell))
        j = int(math.floor((y + self.half) / self.cell))
        if 0 <= i < self.n and 0 <= j < self.n:
            return int(self.flags[j, i])
        return F_OUTSIDE

    def _win(self, xmin, xmax, ymin, ymax):
        c, h, n = self.cell, self.half, self.n
        i0 = max(0, int(math.floor((xmin + h) / c)))
        i1 = min(n, int(math.floor((xmax + h) / c)) + 1)
        j0 = max(0, int(math.floor((ymin + h) / c)))
        j1 = min(n, int(math.floor((ymax + h) / c)) + 1)
        if i0 >= i1 or j0 >= j1:
            return None
        return i0, i1, j0, j1

    def obb_mask(self, o, margin=0.0):
        cx, cy, hx, hy, c, s = o
        hx += margin
        hy += margin
        ex = abs(c) * hx + abs(s) * hy
        ey = abs(s) * hx + abs(c) * hy
        w = self._win(cx - ex, cx + ex, cy - ey, cy + ey)
        if w is None:
            return None
        i0, i1, j0, j1 = w
        dx = self.centres(i0, i1)[None, :] - cx
        dy = self.centres(j0, j1)[:, None] - cy
        m = (np.abs(dx * c + dy * s) <= hx) & (np.abs(-dx * s + dy * c) <= hy)
        return (j0, j1, i0, i1), m

    def hit_obb(self, o, margin, mask):
        cx, cy, hx, hy, c, s = o
        ex = abs(c) * (hx + margin) + abs(s) * (hy + margin)
        ey = abs(s) * (hx + margin) + abs(c) * (hy + margin)
        if cx - ex < -self.half or cx + ex > self.half or cy - ey < -self.half or cy + ey > self.half:
            return True
        r = self.obb_mask(o, margin)
        if r is None:
            return True
        (j0, j1, i0, i1), m = r
        return bool(np.any(self.flags[j0:j1, i0:i1][m] & mask))

    def paint_obb(self, o, margin, flag):
        r = self.obb_mask(o, margin)
        if r is None:
            return
        (j0, j1, i0, i1), m = r
        sub = self.flags[j0:j1, i0:i1]
        sub[m] |= np.uint16(flag)

    def paint_polyline(self, pts, hw, flag, closed=False):
        p = np.asarray(pts, np.float64)
        if closed:
            p = np.vstack([p, p[:1]])
        # merge into chunks of ~12 m to keep the number of numpy calls small
        for k in range(0, len(p) - 1, 6):
            q = p[k:k + 7]
            if len(q) < 2:
                continue
            w = self._win(q[:, 0].min() - hw, q[:, 0].max() + hw, q[:, 1].min() - hw, q[:, 1].max() + hw)
            if w is None:
                continue
            i0, i1, j0, j1 = w
            xs = self.centres(i0, i1)[None, :]
            ys = self.centres(j0, j1)[:, None]
            d = np.full((j1 - j0, i1 - i0), 1e9)
            for a, b in zip(q[:-1], q[1:]):
                d = np.minimum(d, seg_dist(xs, ys, a[0], a[1], b[0], b[1]))
            sub = self.flags[j0:j1, i0:i1]
            sub[d <= hw] |= np.uint16(flag)

    def paint_circle(self, x, y, r, flag):
        w = self._win(x - r, x + r, y - r, y + r)
        if w is None:
            return
        i0, i1, j0, j1 = w
        d = np.hypot(self.centres(i0, i1)[None, :] - x, self.centres(j0, j1)[:, None] - y)
        sub = self.flags[j0:j1, i0:i1]
        sub[d <= r] |= np.uint16(flag)

    def paint_poly(self, poly, flag):
        poly = np.asarray(poly, np.float64)
        w = self._win(poly[:, 0].min(), poly[:, 0].max(), poly[:, 1].min(), poly[:, 1].max())
        if w is None:
            return
        i0, i1, j0, j1 = w
        xs, ys = np.meshgrid(self.centres(i0, i1), self.centres(j0, j1))
        m = points_in_poly(xs, ys, [tuple(v) for v in poly])
        sub = self.flags[j0:j1, i0:i1]
        sub[m] |= np.uint16(flag)

    def any_in_circle(self, x, y, r, mask):
        w = self._win(x - r, x + r, y - r, y + r)
        if w is None:
            return True
        i0, i1, j0, j1 = w
        d = np.hypot(self.centres(i0, i1)[None, :] - x, self.centres(j0, j1)[:, None] - y)
        return bool(np.any(self.flags[j0:j1, i0:i1][d <= r] & mask))


class TensorField:
    """2D street-direction field (Chen et al. 2008 style) on a coarse grid: weighted sum of radial, grid and
    polyline-aligned basis tensors stored as (cos 2phi, sin 2phi); family 0 follows phi, family 1 is perpendicular."""

    def __init__(self, half, cell=4.0):
        self.half = float(half)
        self.cell = float(cell)
        self.n = int(math.ceil(2 * half / cell)) + 1
        xs = -self.half + np.arange(self.n) * self.cell
        self.X, self.Y = np.meshgrid(xs, xs)
        self.c2 = np.zeros_like(self.X)
        self.s2 = np.zeros_like(self.X)

    def _acc(self, phi, w):
        self.c2 += w * np.cos(2.0 * phi)
        self.s2 += w * np.sin(2.0 * phi)

    def radial(self, cx, cy, wfn):
        dx, dy = self.X - cx, self.Y - cy
        self._acc(np.arctan2(dy, dx), wfn(np.hypot(dx, dy)))

    def grid(self, angle, w):
        self._acc(np.full(self.X.shape, angle), w if np.ndim(w) else np.full(self.X.shape, float(w)))

    def polyline(self, pts, sigma, weight):
        p = np.asarray(pts, np.float64)
        dmin = np.full(self.X.shape, 1e9)
        phi = np.zeros(self.X.shape)
        for a, b in zip(p[:-1], p[1:]):
            d = seg_dist(self.X, self.Y, a[0], a[1], b[0], b[1])
            m = d < dmin
            dmin[m] = d[m]
            phi[m] = math.atan2(b[1] - a[1], b[0] - a[0])
        self._acc(phi, weight * np.exp(-(dmin / sigma) ** 2))

    def finish(self, noise=None):
        phi = 0.5 * np.arctan2(self.s2, self.c2)
        if noise is not None:
            phi = phi + noise(self.X, self.Y)
        self.cc = np.cos(2.0 * phi)
        self.ss = np.sin(2.0 * phi)

    def direction(self, x, y, fam):
        u = (x + self.half) / self.cell
        v = (y + self.half) / self.cell
        i = min(max(int(u), 0), self.n - 2)
        j = min(max(int(v), 0), self.n - 2)
        fu = min(max(u - i, 0.0), 1.0)
        fv = min(max(v - j, 0.0), 1.0)
        cc, ss = self.cc, self.ss
        c = (cc[j, i] * (1 - fu) + cc[j, i + 1] * fu) * (1 - fv) + (cc[j + 1, i] * (1 - fu) + cc[j + 1, i + 1] * fu) * fv
        s = (ss[j, i] * (1 - fu) + ss[j, i + 1] * fu) * (1 - fv) + (ss[j + 1, i] * (1 - fu) + ss[j + 1, i + 1] * fu) * fv
        phi = 0.5 * math.atan2(s, c)
        if fam:
            phi += 0.5 * math.pi
        return math.cos(phi), math.sin(phi)


def field_noise(rng, amp_deg, wavelength):
    """Smooth rotation field (radians) for organic street curvature."""
    terms = []
    for _ in range(4):
        a = rng.uniform(0, math.pi)
        k = 2 * math.pi / (wavelength * rng.uniform(0.7, 1.4))
        terms.append((math.cos(a) * k, math.sin(a) * k, rng.uniform(0, 2 * math.pi)))
    amp = math.radians(amp_deg) / 2.0

    def f(X, Y):
        out = np.zeros_like(X)
        for kx, ky, ph in terms:
            out += np.sin(kx * X + ky * Y + ph)
        return amp * out
    return f


class ObbIndex:
    """Spatial hash of oriented boxes for exact separating-axis tests."""

    def __init__(self, cell=24.0):
        self.cell = cell
        self.buckets = {}

    def _keys(self, o, pad):
        cx, cy, hx, hy, c, s = o
        ex = abs(c) * hx + abs(s) * hy + pad
        ey = abs(s) * hx + abs(c) * hy + pad
        c0 = int(math.floor((cx - ex) / self.cell))
        c1 = int(math.floor((cx + ex) / self.cell))
        r0 = int(math.floor((cy - ey) / self.cell))
        r1 = int(math.floor((cy + ey) / self.cell))
        return [(i, j) for i in range(c0, c1 + 1) for j in range(r0, r1 + 1)]

    def add(self, o, item):
        for k in self._keys(o, 0.0):
            self.buckets.setdefault(k, []).append((o, item))

    def hits(self, o, gap=0.0, skip=None):
        seen = set()
        out = []
        for k in self._keys(o, gap):
            for ob, item in self.buckets.get(k, ()):
                if id(item) in seen or item is skip:
                    continue
                seen.add(id(item))
                if obb_overlap(o, ob, gap):
                    out.append(item)
        return out

    def hit(self, o, gap=0.0):
        seen = set()
        for k in self._keys(o, gap):
            for ob, item in self.buckets.get(k, ()):
                if id(item) in seen:
                    continue
                seen.add(id(item))
                if obb_overlap(o, ob, gap):
                    return True
        return False


# ============================================================================================ layout records


class Placement:
    __slots__ = ("asset", "x", "y", "z", "yaw", "pitch", "roll", "scale", "cls", "obb", "tag")

    def __init__(self, asset, x, y, z, yaw, pitch, roll, scale, cls, obb, tag=""):
        self.asset = asset
        self.x, self.y, self.z = x, y, z
        self.yaw, self.pitch, self.roll, self.scale = yaw, pitch, roll, scale
        self.cls = cls
        self.obb = obb
        self.tag = tag


class Street:
    __slots__ = ("line", "hw", "kind", "mat", "prio", "sides", "zone")

    def __init__(self, line, hw, kind, mat, prio, sides=(True, True)):
        self.line = line
        self.hw = hw
        self.kind = kind
        self.mat = mat
        self.prio = prio
        self.sides = sides
        self.zone = None


class Plaza:
    __slots__ = ("x", "y", "r", "poly", "kind", "mat", "frontage")

    def __init__(self, x, y, r, kind, mat, frontage=True, poly=None):
        self.x, self.y, self.r = x, y, r
        self.poly = poly
        self.kind = kind
        self.mat = mat
        self.frontage = frontage


# ============================================================================================ site builder


class SiteBuilder:
    """Local-frame layout of one site: rasters, walls, streets, placements."""

    def __init__(self, G, site):
        self.G = G
        self.site = site
        self.id = site["Id"]
        self.style = site["Style"]
        self.cx = float(site["Center"]["X"])
        self.cy = float(site["Center"]["Y"])
        self.R = site["RadiusCm"] / 100.0
        self.ground = site["GroundZ"]
        self.crc = zlib.crc32(self.id.encode("utf-8"))
        self.rng = np.random.default_rng([SEED, self.crc])
        self.landmarks = []
        for lm in site.get("Landmarks", []):
            self.landmarks.append(dict(name=lm["Name"], x=(lm["Location"]["X"] - self.cx) / 100.0,
                                       y=(lm["Location"]["Y"] - self.cy) / 100.0, z=lm["Location"]["Z"],
                                       yaw=float(lm["Yaw"]), r=lm["RadiusCm"] / 100.0, rise=lm.get("RiseCm", 0) / 100.0))
        ext = 1.25 * self.R + 45.0
        for lm in self.landmarks:
            ext = max(ext, math.hypot(lm["x"], lm["y"]) + lm["r"] + 70.0)
        self.half = ext
        self.grid = Grid(ext, CELL)
        self.dgrid = Grid(ext, 2.0)          # street direction raster: 0 = none, 1 + angle (0..179 deg)
        self.items = []
        self.index = ObbIndex(24.0)          # buildings, walls, landmarks
        self.pindex = ObbIndex(8.0)          # props
        self.streets = []
        self.plazas = []
        self.gates = []                      # dict(x, y, ey, road)
        self.wall_r = None                   # callable theta -> wall centreline radius (m), or None
        self.wall_ranges = []                # list of (theta_a, theta_b) covered by the wall (radians, a < b)
        self.wall_curves = []
        self.spawns = []
        self.notes = []
        self.frontage_jobs = []
        self.fill_density = None
        self.palette = None
        self.target = 0
        self.water = None
        self.stats = {}
        self.caps = dict(DEFAULT_CAPS)
        self.used = {}
        self.road_lines = []

    # ------------------------------------------------------------------ coordinates
    def tz(self, x, y):
        """Terrain height (cm) at local metres."""
        return self.G.terrain.z(self.cx + 100.0 * np.asarray(x, np.float64), self.cy + 100.0 * np.asarray(y, np.float64))

    def asset(self, name):
        return self.G.kit[name]

    def lm(self, name):
        for m in self.landmarks:
            if m["name"] == name:
                return m
        return None

    # ------------------------------------------------------------------ setup
    def paint_water(self):
        """Sea (Z < 0.4 m), lakes and rivers; F_SHORE is water grown by 4 m (no buildings)."""
        cell = 1.0
        n = int(math.ceil(2 * self.half / cell))
        xs = -self.half + (np.arange(n) + 0.5) * cell
        X, Y = np.meshgrid(xs, xs)
        Z = self.tz(X, Y)
        water = Z < 40.0
        for lake in self.G.world.get("Lakes", []):
            poly = [((p["X"] - self.cx) / 100.0, (p["Y"] - self.cy) / 100.0) for p in lake["Polygon"]]
            pa = np.array(poly)
            if pa[:, 0].max() < -self.half or pa[:, 0].min() > self.half or pa[:, 1].max() < -self.half or pa[:, 1].min() > self.half:
                continue
            inside = points_in_poly(X, Y, poly)
            water |= inside & (Z < lake["WaterZ"] + 60.0)
        for riv in self.G.world.get("Rivers", []):
            pts = np.array([((p["X"] - self.cx) / 100.0, (p["Y"] - self.cy) / 100.0, p.get("WidthCm", 600) / 100.0) for p in riv["Points"]])
            near = (np.abs(pts[:, 0]) < self.half + 30) & (np.abs(pts[:, 1]) < self.half + 30)
            if not near.any():
                continue
            k = np.flatnonzero(near)
            k0, k1 = max(0, k.min() - 1), min(len(pts) - 1, k.max() + 1)
            for a, b in zip(pts[k0:k1], pts[k0 + 1:k1 + 1]):
                hw = 0.5 * max(a[2], b[2]) + 1.0
                water |= seg_dist(X, Y, a[0], a[1], b[0], b[1]) <= hw
        self.water1m = water
        shore = water.copy()
        for _ in range(4):
            s2 = shore.copy()
            s2[1:, :] |= shore[:-1, :]
            s2[:-1, :] |= shore[1:, :]
            s2[:, 1:] |= shore[:, :-1]
            s2[:, :-1] |= shore[:, 1:]
            shore = s2
        # upsample 1 m -> grid cells
        g = self.grid
        ii = np.clip(((g.centres(0, g.n) + self.half) / cell).astype(np.int64), 0, n - 1)
        w_up = water[np.ix_(ii, ii)]
        s_up = shore[np.ix_(ii, ii)]
        g.flags[w_up] |= np.uint16(F_WATER)
        g.flags[s_up & ~w_up] |= np.uint16(F_SHORE)

    def paint_roads(self):
        """Corridors of the World.json roads (half width + 3 m) so landmarks and special buildings keep off them."""
        for road, p in self._roads_local():
            d = np.hypot(p[:, 0], p[:, 1])
            k = np.flatnonzero(d < self.half + 20)
            if len(k) < 2:
                continue
            q = p[max(0, k.min() - 1):min(len(p), k.max() + 2)]
            self.grid.paint_polyline(q, 0.5 * road["WidthCm"] / 100.0 + 3.0, F_ROAD)

    def fit_spot(self, name, x, y, facing, max_shift=20.0, step=4.0, yaw_range=15.0, yaw_step=5.0,
                 avoid=None, margin=2.0, shift_cost=0.05, yaw_cost=0.03):
        """Best nearby spot (and a small turn) for a big building: least ground relief under the footprint, then
        closest to the requested spot. Returns (x, y, facing, relief_m)."""
        a = self.asset(name)
        avoid = avoid if avoid is not None else (F_WATER | F_SHORE | F_LANDMARK | F_BUILD | F_ROAD | F_KEEPIN)
        best = None
        rng_ = np.arange(-max_shift, max_shift + 1e-6, step)
        for dy in rng_:
            for dx in rng_:
                if dx * dx + dy * dy > max_shift * max_shift + 1e-6:
                    continue
                for dyaw in np.arange(-yaw_range, yaw_range + 1e-6, yaw_step):
                    o = obb_of(a, x + dx, y + dy, facing + dyaw - 90.0)
                    if self.grid.hit_obb(o, margin, avoid):
                        continue
                    zmin, zmax = self.footprint_z(o, 5.0)
                    relief = (zmax - zmin) / 100.0
                    score = relief + shift_cost * math.hypot(dx, dy) + yaw_cost * abs(dyaw)
                    if best is None or score < best[0]:
                        best = (score, x + dx, y + dy, facing + dyaw, relief)
        if best is None:
            o = obb_of(a, x, y, facing - 90.0)
            zmin, zmax = self.footprint_z(o, 5.0)
            return x, y, facing, (zmax - zmin) / 100.0
        return best[1], best[2], best[3], best[4]

    def collect_spawns(self):
        for L in self.G.locations:
            x = (L["WorldLocation"]["X"] - self.cx) / 100.0
            y = (L["WorldLocation"]["Y"] - self.cy) / 100.0
            if math.hypot(x, y) > 1.15 * self.R:
                continue
            if L.get("bSpawnPoint") or L.get("bFastTravel"):
                self.spawns.append(dict(id=L["LocationID"], x=x, y=y, spawn=bool(L.get("bSpawnPoint")),
                                        cam=L.get("PreviewCamera"), yaw=L.get("SpawnYaw", 0.0)))

    def clear_spawns(self, mat):
        """Paved clearing (r 10 m) around spawn / fast-travel points; nothing may stand within 8.5 m."""
        for sp in self.spawns:
            self.add_plaza(sp["x"], sp["y"], 10.0, "spawn", mat, frontage=False)
            self.grid.paint_circle(sp["x"], sp["y"], 9.0, F_RESERVED)

    # ------------------------------------------------------------------ placement
    def footprint_z(self, o, step=3.5):
        cx, cy, hx, hy, c, s = o
        nx = max(3, int(math.ceil(2 * hx / step)) + 1)
        ny = max(3, int(math.ceil(2 * hy / step)) + 1)
        lx, ly = np.meshgrid(np.linspace(-hx + 0.15, hx - 0.15, nx), np.linspace(-hy + 0.15, hy - 0.15, ny))
        x = cx + lx * c - ly * s
        y = cy + lx * s + ly * c
        z = self.tz(x, y)
        return float(z.min()), float(z.max())

    def commit(self, a, x, y, z, yaw, cls, pitch=0.0, roll=0.0, scale=1.0, obb=None, tag="", solid=True, paint=None):
        o = obb if obb is not None else obb_of(a, x, y, yaw, scale)
        p = Placement(a, x, y, z, yaw, pitch, roll, scale, cls, o, tag)
        self.items.append(p)
        if solid:
            self.index.add(o, p)
        else:
            self.pindex.add(o, p)
        if paint:
            self.grid.paint_obb(o, 0.0, paint)
        return p

    def try_building(self, a, x, y, yaw, cls="Buildings", clear=STREET_CLEAR, gap=MIN_GAP, tol=None, block=F_BLOCK,
                     sink=None, tag="", scale=1.0):
        o = obb_of(a, x, y, yaw, scale)
        if self.grid.hit_obb(o, clear, block):
            return None
        if self.index.hit(o, gap):
            return None
        zmin, zmax = self.footprint_z(o)
        if tol is None:
            tol = 2.5 if a.big else 1.2
        if zmax - zmin > tol * 100.0:
            return None
        if sink is None:
            sink = float(self.frng.uniform(15.0, 40.0)) if hasattr(self, "frng") else 25.0
        return self.commit(a, x, y, zmin - sink, yaw, cls, obb=o, tag=tag, scale=scale, paint=F_BUILD)

    def place_fixed(self, name, x, y, facing_deg, cls, margin=6.0, flag=F_LANDMARK, sink=30.0, tag="", pad_index=True,
                    fit=None, quiet=False):
        """Landmarks / special buildings at a given spot (fit=(max_shift, yaw_range) lets it slide to flatter ground
        off the road corridors); logged if the ground stays rough."""
        a = self.asset(name)
        if fit is not None:
            x0, y0 = x, y
            x, y, facing_deg, relief = self.fit_spot(name, x, y, facing_deg, max_shift=fit[0], yaw_range=fit[1],
                                                     step=max(2.0, fit[0] / 8.0),
                                                     shift_cost=fit[2] if len(fit) > 2 else 0.05)
            moved = math.hypot(x - x0, y - y0)
            if moved > 0.5:
                self.notes.append("%s: %s moved %.0f m / turned to flatter ground (relief %.1f m)" % (self.id, name, moved, relief))
        yaw = facing_deg - 90.0
        o = obb_of(a, x, y, yaw)
        zmin, zmax = self.footprint_z(o, step=4.0)
        if zmax - zmin > 250.0 and not quiet:
            self.notes.append("%s: ground under %s varies %.1f m" % (self.id, name, (zmax - zmin) / 100.0))
        p = self.commit(a, x, y, zmin - sink, yaw, cls, obb=o, tag=tag)
        self.grid.paint_obb(o, margin, flag)
        self.grid.paint_obb(o, 0.0, F_BUILD)
        return p

    def try_prop(self, name, x, y, yaw, clear=0.25, gap=0.3, block=None, sink=None, allow_plaza=True, tag="",
                 scale=1.0, check_index=True):
        a = self.asset(name)
        o = obb_of(a, x, y, yaw, scale)
        if block is None:
            block = F_STREET | F_WATER | F_WALL | F_LANDMARK | F_BUILD | F_RESERVED | F_OUTSIDE | F_KEEPIN
            if not allow_plaza:
                block |= F_PLAZA
        if self.grid.hit_obb(o, clear, block):
            return None
        if check_index and (self.pindex.hit(o, gap) or self.index.hit(o, 0.3)):
            return None
        zmin, zmax = self.footprint_z(o, step=1.5)
        if zmax - zmin > 80.0:
            return None
        if sink is None:
            sink = 20.0 if a.min_z < -0.1 else 3.0
        return self.commit(a, x, y, zmin - sink, yaw, "Props", obb=o, tag=tag, scale=scale, solid=False, paint=F_PROP)

    # ------------------------------------------------------------------ streets
    def add_plaza(self, x, y, r, kind, mat, frontage=True, poly=None, paint=True):
        pl = Plaza(x, y, r, kind, mat, frontage, poly)
        self.plazas.append(pl)
        if paint:
            if poly is not None:
                self.grid.paint_poly(poly, F_PLAZA)
            else:
                self.grid.paint_circle(x, y, r, F_PLAZA)
        return pl

    def _paint_dir(self, pts, hw, reach=11.0):
        p = np.asarray(pts)
        g = self.dgrid
        rr = hw + reach
        for a, b in zip(p[:-1], p[1:]):
            ang = int(round(math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])))) % 180
            w = g._win(min(a[0], b[0]) - rr, max(a[0], b[0]) + rr, min(a[1], b[1]) - rr, max(a[1], b[1]) + rr)
            if w is None:
                continue
            i0, i1, j0, j1 = w
            d = seg_dist(g.centres(i0, i1)[None, :], g.centres(j0, j1)[:, None], a[0], a[1], b[0], b[1])
            sub = g.flags[j0:j1, i0:i1]
            sub[d <= rr] = np.uint16(1 + ang)

    def add_street(self, pts, hw, kind, mat, prio, sides=(True, True), closed=False, clip=True, min_len=14.0,
                   par_check=True, block=None, need_connect=False, reach=11.0):
        """Clip a street against water / landmarks / outside / parallel neighbours, paint it and register the pieces."""
        line = Line(pts, closed)
        if line.L < 2.0:
            return []
        P = line.resample(2.0)
        if closed:
            P = np.vstack([P, P[:1]])
        if not clip:
            runs = [P]
        else:
            if block is None:
                block = F_WATER | F_LANDMARK | F_OUTSIDE | F_WALL | F_RESERVED | F_KEEPIN
            fl = self.grid.get(P[:, 0], P[:, 1])
            ok = (fl & block) == 0
            # the street body must not cut into landmarks / water either
            t = np.gradient(P, axis=0)
            tn = np.hypot(t[:, 0], t[:, 1]) + 1e-9
            nx, ny = -t[:, 1] / tn, t[:, 0] / tn
            for sgn in (-1.0, 1.0):
                f2 = self.grid.get(P[:, 0] + sgn * nx * hw, P[:, 1] + sgn * ny * hw)
                ok &= (f2 & (F_WATER | F_LANDMARK | F_KEEPIN)) == 0
            if par_check:
                d = self.dgrid.get(P[:, 0], P[:, 1]).astype(np.int64)
                ang = (np.degrees(np.arctan2(t[:, 1], t[:, 0])).round().astype(np.int64)) % 180
                has = (d > 0) & (d != F_OUTSIDE)
                diff = np.abs(((d - 1) - ang + 90) % 180 - 90)
                ok &= ~(has & (diff < 28))
            runs = []
            cur = []
            for k in range(len(P)):
                if ok[k]:
                    cur.append(P[k])
                else:
                    if len(cur) > 1:
                        runs.append(np.array(cur))
                    cur = []
            if len(cur) > 1:
                runs.append(np.array(cur))
            if closed and len(runs) > 1 and ok[0] and ok[-1]:
                runs[0] = np.vstack([runs[-1], runs[0][1:]])
                runs.pop()
        out = []
        for r in runs:
            ln = Line(r, closed and len(runs) == 1 and bool(ok.all()) if clip else closed)
            if ln.L < min_len:
                continue
            if need_connect:
                fa = self.grid.any_in_circle(r[0][0], r[0][1], hw + 2.5, F_STREET | F_PLAZA)
                fb = self.grid.any_in_circle(r[-1][0], r[-1][1], hw + 2.5, F_STREET | F_PLAZA)
                if not (fa or fb):
                    continue
            st = Street(ln, hw, kind, mat, prio, sides)
            self.streets.append(st)
            self.grid.paint_polyline(ln.p, hw + STREET_PAD, F_STREET)
            self._paint_dir(ln.p, hw, reach)
            out.append(st)
        return out

    # ------------------------------------------------------------------ walls
    def wall_dp(self, r0, th_range=None, keep_margin=16.0):
        """Wall centreline radius r(theta): a polar dynamic programme near r0 that avoids water and landmarks,
        keeps every F_KEEPIN / F_LANDMARK area inside and prefers gentle ground. th_range=(a, b) gives an open arc."""
        step = 3.0
        closed = th_range is None
        if closed:
            n = max(96, int(round(2 * math.pi * r0 / step)))
            th = np.arange(n) * (2 * math.pi / n)
            dth = 2 * math.pi / n
        else:
            a, b = th_range
            n = max(8, int(round((b - a) * r0 / step)) + 1)
            th = np.linspace(a, b, n)
            dth = (b - a) / (n - 1)
        # keep-in extent along each ray
        rr = np.arange(2.0, 1.6 * r0, 1.5)
        cx = np.cos(th)[:, None] * rr[None, :]
        cy = np.sin(th)[:, None] * rr[None, :]
        inside_flags = self.grid.get(cx, cy) & np.uint16(F_KEEPIN | F_LANDMARK)
        r_far = np.where(inside_flags.any(1), rr[np.argmax(np.where(inside_flags > 0, rr[None, :], -1), axis=1)], 0.0)
        r_far = np.where(inside_flags.any(1), r_far, 0.0)
        rs = np.arange(0.84 * r0, max(1.22 * r0, float(r_far.max()) + keep_margin + 12.0) + 1e-6, 1.5)
        X = np.cos(th)[:, None] * rs[None, :]
        Y = np.sin(th)[:, None] * rs[None, :]
        Z = self.tz(X, Y) / 100.0
        gr = np.gradient(Z, 1.5, axis=1)
        if closed:
            gt = (np.roll(Z, -1, 0) - np.roll(Z, 1, 0)) / (2.0 * rs[None, :] * dth)
        else:
            gt = np.gradient(Z, axis=0) / (rs[None, :] * dth)
        slope = np.hypot(gr, gt)
        fl = self.grid.get(X, Y)
        bad = (fl & np.uint16(F_WATER | F_SHORE | F_LANDMARK | F_KEEPIN)) != 0
        for sh in (1, 2, 3, 4):
            bad[:, sh:] |= bad[:, :-sh]
            bad[:, :-sh] |= bad[:, sh:]
        b2 = bad.copy()
        for sh in (1, 2):
            if closed:
                b2 |= np.roll(bad, sh, 0) | np.roll(bad, -sh, 0)
            else:
                b2[sh:] |= bad[:-sh]
                b2[:-sh] |= bad[sh:]
        bad = b2 | (rs[None, :] < r_far[:, None] + keep_margin)
        cost = 3.0 * ((rs[None, :] - r0) / (0.1 * r0)) ** 2 + 300.0 * np.maximum(slope - 0.03, 0.0) ** 2 + 1e7 * bad
        M = len(rs)
        shifts = np.array([-2, -1, 0, 1, 2])
        pen = 0.8 * shifts.astype(float) ** 2
        INF = 1e18
        m0 = int(np.argmin(np.abs(rs - r0)))
        if closed:
            k0 = int(np.argmin(cost[:, m0]))
            order = (np.arange(n) + k0) % n
        else:
            order = np.arange(n)
        C = cost[order]
        D = np.full(M, INF)
        if closed:
            D[m0] = C[0, m0]
        else:
            D[:] = C[0]
        back = np.zeros((n, M), np.int8)
        for k in range(1, n):
            best = np.full(M, INF)
            arg = np.zeros(M, np.int8)
            for sh, p in zip(shifts, pen):
                prev = np.full(M, INF)
                if sh >= 0:
                    prev[:M - sh] = D[sh:]
                else:
                    prev[-sh:] = D[:M + sh]
                cand = prev + p
                bt = cand < best
                best[bt] = cand[bt]
                arg[bt] = sh
            D = best + C[k]
            back[k] = arg
        if closed:
            fin = [(D[m0 + sh] + p if 0 <= m0 + sh < M else INF, m0 + sh) for sh, p in zip(shifts, pen)]
            m = min(fin)[1]
        else:
            m = int(np.argmin(D))
        path = np.zeros(n, np.int64)
        for k in range(n - 1, -1, -1):
            path[k] = m
            if k > 0:
                m = m + int(back[k][m])
        r = np.empty(n)
        r[order] = rs[path]
        # smooth (moving average over ~15 m)
        ker = np.ones(5) / 5.0
        if closed:
            r = np.convolve(np.r_[r[-2:], r, r[:2]], ker, mode="valid")
        else:
            r = np.convolve(np.r_[[r[0]] * 2, r, [r[-1]] * 2], ker, mode="valid")
        bad_path = bad[order, path].any() if closed else bad[np.arange(n), path].any()
        if bad_path:
            self.notes.append("%s: wall path crosses a keep-out (water / landmark) somewhere" % self.id)
        return th, r

    def land_arcs(self, r0):
        """Angular intervals (radians) where a wall ring of radius ~r0 stays on dry land (ports)."""
        th = np.radians(np.arange(0, 360, 1.0))
        land = np.ones(len(th), bool)
        for f in (0.92, 1.0, 1.06):
            fl = self.grid.get(np.cos(th) * r0 * f, np.sin(th) * r0 * f)
            land &= (fl & np.uint16(F_WATER | F_SHORE)) == 0
        if land.all():
            return None
        arcs = []
        n = len(th)
        start = None
        k0 = int(np.argmin(land))  # a water index, so every land run is contiguous from here
        for k in range(1, n + 1):
            i = (k0 + k) % n
            if land[i] and start is None:
                start = k
            if (not land[i] or k == n) and start is not None:
                a = th[(k0 + start) % n]
                length = (k - start) * (2 * math.pi / n)
                arcs.append((a, a + length - 2 * math.pi / n))
                start = None
        out = []
        for a, b in arcs:
            if b - a > math.radians(55):
                out.append((a + math.radians(3.0), b - math.radians(3.0)))
        return out

    def build_walls(self, prefix, r0, lane_off=10.0, arcs="auto"):
        """City wall from kit pieces: straight runs of exactly-12 m segments between towers, gates where roads cross."""
        seg = self.asset("SM_%s_Wall_12m" % prefix)
        tower = self.asset("SM_%s_WallTower" % prefix)
        gate = self.asset("SM_%s_Gate" % prefix)
        if arcs == "auto":
            arcs = self.land_arcs(r0) if self.style in ("DemonPort", "MillisPort") or self.id == "EastPort" else None
        ranges = [None] if arcs is None else arcs
        curves = []
        rfun_th, rfun_r = [], []
        for rng_ in ranges:
            th, r = self.wall_dp(r0, rng_)
            pts = np.stack([np.cos(th) * r, np.sin(th) * r], 1)
            curves.append((Line(pts, closed=rng_ is None), rng_))
            rfun_th.append(th)
            rfun_r.append(r)
        self.wall_curves = curves
        if arcs is None:
            th0, r0s = rfun_th[0], rfun_r[0]

            def rw(t, th0=th0, r0s=r0s):
                return np.interp(np.mod(t, 2 * math.pi), np.r_[th0, 2 * math.pi], np.r_[r0s, r0s[0]])
            self.wall_r = rw
            self.wall_ranges = [(0.0, 2 * math.pi)]
        else:
            def rw(t, arcs_=list(zip(rfun_th, rfun_r))):
                t = np.asarray(t, np.float64)
                out = np.full(t.shape, np.nan)
                for thv, rv in arcs_:
                    tt = np.mod(t - thv[0], 2 * math.pi) + thv[0]
                    m = (tt >= thv[0]) & (tt <= thv[-1])
                    out[m] = np.interp(tt[m], thv, rv)
                return out
            self.wall_r = rw
            self.wall_ranges = [(float(t[0]), float(t[-1])) for t in rfun_th]
        # gates where roads cross the wall curves
        for curve, rng_ in curves:
            gates = self._gates_on(curve)
            self._chain(curve, gates, seg, tower, gate, prefix)
        self._paint_outside(r_inset=3.0)
        self.lane_off = lane_off

    def _roads_local(self):
        out = []
        for road in self.G.world["Roads"]:
            p = np.array([((q["X"] - self.cx) / 100.0, (q["Y"] - self.cy) / 100.0) for q in road["Points"]])
            d = np.hypot(p[:, 0], p[:, 1])
            if d.min() > 1.3 * self.R + 60:
                continue
            out.append((road, p))
        return out

    def _gates_on(self, curve):
        """Road crossings on a wall curve -> list of gate dicts sorted by arc position."""
        crossings = []
        wp = curve.p
        rwp = np.hypot(wp[:, 0], wp[:, 1])
        for road, p in self._roads_local():
            d = np.hypot(p[:, 0], p[:, 1])
            idx = np.flatnonzero(d < 1.6 * self.R + 80)
            if len(idx) < 2:
                continue
            for k in range(max(0, idx.min() - 1), min(len(p) - 1, idx.max() + 1)):
                a, b = p[k], p[k + 1]
                # quick reject by radius
                ra, rb = math.hypot(*a), math.hypot(*b)
                lo, hi = min(ra, rb) - 5, max(ra, rb) + 5
                cand = np.flatnonzero((np.maximum(rwp[:-1], rwp[1:]) >= lo) & (np.minimum(rwp[:-1], rwp[1:]) <= hi))
                for j in cand:
                    hit = seg_intersect(a, b, wp[j], wp[j + 1])
                    if hit is None:
                        continue
                    t, u = hit
                    x = a + (b - a) * t
                    dirv = unit(b - a)
                    # outward: away from the centre
                    if dirv[0] * x[0] + dirv[1] * x[1] < 0:
                        dirv = -dirv
                    s = curve.cum[j] + u * (curve.cum[j + 1] - curve.cum[j])
                    crossings.append(dict(s=float(s), x=float(x[0]), y=float(x[1]), d=dirv, road=road["Id"],
                                          w=road["WidthCm"] / 100.0, cls=road["Class"]))
        crossings.sort(key=lambda c: c["s"])
        # merge crossings of overlapping roads (< 19 m apart)
        groups = []
        for c in crossings:
            if groups and abs(c["s"] - groups[-1][-1]["s"]) < 19.0:
                groups[-1].append(c)
            else:
                groups.append([c])
        if curve.closed and len(groups) > 1 and (groups[0][0]["s"] + curve.L - groups[-1][-1]["s"]) < 19.0:
            groups[0] = groups.pop() + groups[0]
        gates = []
        for g in groups:
            best = max(g, key=lambda c: (c["w"], c["cls"] == "Highway"))
            s = float(np.mean([c["s"] for c in g])) if max(c["s"] for c in g) - min(c["s"] for c in g) < 30 else best["s"]
            P = curve.at(s)
            tang = curve.tangent(s, 3.0)
            nrm = np.array([-tang[1], tang[0]])
            if nrm[0] * P[0] + nrm[1] * P[1] < 0:
                nrm = -nrm
            dv = unit(np.mean([c["d"] for c in g], axis=0))
            dev = wrap_deg(ang_of(dv) - ang_of(nrm))
            dev = max(-30.0, min(30.0, dev))
            ey = np.array([math.cos(math.radians(ang_of(nrm) + dev)), math.sin(math.radians(ang_of(nrm) + dev))])
            gates.append(dict(s=s, x=float(P[0]), y=float(P[1]), ey=ey, road=best["road"], w=best["w"],
                              roads=sorted(set(c["road"] for c in g))))
        # keep gates at least 24 m apart (two gate meshes then share one 12 m wall segment between them)
        k = 1
        while k < len(gates):
            if gates[k]["s"] - gates[k - 1]["s"] < 23.5:
                if gates[k]["s"] - gates[k - 1]["s"] >= 19.0:
                    mid = 0.5 * (gates[k]["s"] + gates[k - 1]["s"])
                    for gg, off in ((gates[k - 1], -12.0), (gates[k], 12.0)):
                        gg["s"] = mid + off
                        P = curve.at(gg["s"])
                        gg["x"], gg["y"] = float(P[0]), float(P[1])
                    k += 1
                else:
                    gates.pop(k)
            else:
                k += 1
        return gates

    def _chain(self, curve, gates, seg, tower, gate_a, prefix):
        """Towers + straight runs of 12 m segments between fixed points (gate joints / arc ends)."""
        fixed = []   # list of (s, point, kind) in arc order: kind 'j' gate joint or 'e' arc end
        for g in gates:
            ey = g["ey"]
            ex = np.array([ey[1], -ey[0]])          # local +X for yaw = facing(ey) - 90
            c = np.array([g["x"], g["y"]])
            j1, j2 = c - 6.0 * ex, c + 6.0 * ex
            s1, _ = curve.project(j1)
            s2, _ = curve.project(j2)
            if curve.closed:
                # order along the traversal around the gate position
                d1 = (s1 - g["s"] + curve.L / 2) % curve.L - curve.L / 2
                d2 = (s2 - g["s"] + curve.L / 2) % curve.L - curve.L / 2
                s1, s2 = g["s"] + d1, g["s"] + d2
            if s1 > s2:
                j1, j2, s1, s2 = j2, j1, s2, s1
            g["j"] = (j1, j2)
            g["sj"] = (s1, s2)
            yaw = yaw_facing(ey[0], ey[1])
            o = obb_of(gate_a, c[0], c[1], yaw)
            zmin, zmax = self.footprint_z(o, step=3.0)
            self.commit(gate_a, c[0], c[1], zmin - 20.0, yaw, "Walls", obb=o, tag="gate")
            self.grid.paint_obb(o, 2.0, F_WALL)
            if not g.get("inner"):
                self.gates.append(dict(x=c[0], y=c[1], ey=ey, road=g["road"], roads=g["roads"], w=g["w"]))
            else:
                self.inner_gates = getattr(self, "inner_gates", []) + [dict(x=c[0], y=c[1], ey=ey, roads=g["roads"])]
        pieces = []   # (A, sA, B, sB)
        if curve.closed:
            if not gates:
                s0 = 0.0
                A = curve.at(s0)
                pieces.append((A, s0, A, s0 + curve.L, True, True))
            else:
                for k, g in enumerate(gates):
                    nxt = gates[(k + 1) % len(gates)]
                    sA = g["sj"][1]
                    sB = nxt["sj"][0] + (curve.L if k == len(gates) - 1 else 0.0)
                    pieces.append((g["j"][1], sA, nxt["j"][0], sB, False, False))
        else:
            pts = [(0.0, curve.at(0.0), "e")]
            for g in gates:
                pts.append((g["sj"][0], g["j"][0], "j"))
                pts.append((g["sj"][1], g["j"][1], "j"))
            pts.append((curve.L, curve.at(curve.L), "e"))
            for k in range(0, len(pts), 2):
                (sA, A, kA), (sB, B, kB) = pts[k], pts[k + 1]
                pieces.append((A, sA, B, sB, kA == "e", kB == "e"))
        for A, sA, B, sB, tA, tB in pieces:
            self._run_chain(curve, A, sA, B, sB, tA, tB, seg, tower)

    def _run_chain(self, curve, A, sA, B, sB, tower_a, tower_b, seg, tower):
        S = sB - sA
        if S < 4.0:
            return
        N = max(1, int(round(S / 12.0)))
        # run lengths (segments between towers) follow curvature: 2 on tight bends, 3-4 on gentle ones
        runs = []
        acc = 0
        while acc < N:
            s_here = sA + 12.0 * (acc + 1.5)
            t1 = curve.tangent(s_here - 12.0, 3.0)
            t2 = curve.tangent(s_here + 12.0, 3.0)
            bend = abs(wrap_deg(ang_of(t2) - ang_of(t1))) / 24.0     # deg per metre
            pa, pb = curve.at(s_here - 12.0), curve.at(s_here + 24.0)
            grade = abs(float(self.tz(pb[0], pb[1]) - self.tz(pa[0], pa[1]))) / 100.0 / 36.0
            if bend > 0.16 or grade > 0.08:
                n = 2
            elif bend > 0.07:
                n = 3 if self.rng.random() < 0.7 else 2
            else:
                n = int(self.rng.choice([3, 3, 4, 4, 2]))
            n = min(n, N - acc)
            runs.append(n)
            acc += n
        if len(runs) > 1 and runs[-1] == 1:
            runs[-2] += 1
            runs.pop()

        def walk(scale):
            p = np.asarray(A, float)
            s = sA
            verts = [p]
            for n in runs[:-1]:
                L = 12.0 * scale * n
                s_hi = s + L * 1.6 + 2.0
                ss = np.arange(s, s_hi, 0.5)
                q = curve.at(ss)
                d = np.hypot(q[:, 0] - p[0], q[:, 1] - p[1])
                kk = np.flatnonzero(d >= L)
                if len(kk) == 0:
                    s = s_hi
                else:
                    k = int(kk[0])
                    if k == 0:
                        s = ss[0]
                    else:
                        # refine between ss[k-1] and ss[k]
                        lo, hi = ss[k - 1], ss[k]
                        for _ in range(20):
                            mid = 0.5 * (lo + hi)
                            if np.hypot(*(curve.at(mid) - p)) >= L:
                                hi = mid
                            else:
                                lo = mid
                        s = hi
                p = curve.at(s)
                verts.append(p)
            return verts, float(np.hypot(*(np.asarray(B) - p))) - 12.0 * scale * runs[-1]

        scale0 = S / (12.0 * N)
        lo, hi = scale0 - 0.015, scale0 + 0.015
        flo = walk(lo)[1]
        fhi = walk(hi)[1]
        grow = 0
        while (flo < 0 or fhi > 0) and grow < 8:
            grow += 1
            if flo < 0:
                lo -= 0.015 * grow
                flo = walk(lo)[1]
            if fhi > 0:
                hi += 0.015 * grow
                fhi = walk(hi)[1]
        if flo < 0 or fhi > 0:
            # the fixed run pattern cannot close; fall back to a single scaled run pattern over N
            scale = S / (12.0 * N)
            verts, _ = walk(scale)
            self.notes.append("%s: wall arc of %.0f m closed with scale %.3f" % (self.id, S, scale))
        else:
            for _ in range(40):
                mid = 0.5 * (lo + hi)
                f = walk(mid)[1]
                if f > 0:
                    lo = mid
                else:
                    hi = mid
            scale = 0.5 * (lo + hi)
            verts, _ = walk(scale)
        verts = verts + [np.asarray(B, float)]
        self.wall_scale_log = getattr(self, "wall_scale_log", []) + [scale]
        # runs
        for k, n in enumerate(runs):
            P, Q = np.asarray(verts[k]), np.asarray(verts[k + 1])
            self._run_segments(P, Q, n, seg)
        # towers on interior vertices, plus arc ends
        tv = list(range(1, len(verts) - 1))
        if tower_a:
            tv = [0] + tv
        if tower_b:
            tv = tv + [len(verts) - 1]
        for k in tv:
            V = np.asarray(verts[k])
            dirs = []
            if k > 0:
                dirs.append(unit(V - np.asarray(verts[k - 1])))
            if k < len(verts) - 1:
                dirs.append(unit(np.asarray(verts[k + 1]) - V))
            dv = unit(np.sum(dirs, axis=0))
            yaw = ang_of(dv)
            o = obb_of(tower, V[0], V[1], yaw)
            zmin, _ = self.footprint_z(o, step=2.5)
            self.commit(tower, float(V[0]), float(V[1]), zmin - 20.0, yaw, "Walls", obb=o, tag="tower")
            self.grid.paint_obb(o, 2.0, F_WALL)

    def _run_segments(self, P, Q, n, seg):
        d = Q - P
        L = float(np.hypot(*d))
        if L < 1.0:
            return
        u = d / L
        sc = L / (12.0 * n)
        # outward normal must be local +Y: (-u_y, u_x) for yaw = atan2(u)
        mid = 0.5 * (P + Q) - np.asarray(getattr(self, "wall_center", (0.0, 0.0)))
        nrm = np.array([-u[1], u[0]])
        flip = nrm[0] * mid[0] + nrm[1] * mid[1] < 0
        # terrain profile along the run (centreline and both faces) -> base line z = a + b t
        ts = np.linspace(0.0, L, max(4, int(L / 2.0) + 1))
        zs = []
        for off in (-2.0 * sc, 0.0, 2.6 * sc):
            o_n = nrm * (off if not flip else -off)
            zz = self.tz(P[0] + u[0] * ts + o_n[0], P[1] + u[1] * ts + o_n[1])
            zs.append(zz)
        zs = np.array(zs)
        zc = zs[1]
        bfit = float(np.polyfit(ts, zc, 1)[0]) if len(ts) > 2 else 0.0
        for lim in (3.0, 4.5, 6.0):          # a few degrees; steeper only where the run would be buried > 2.5 m
            bmax = math.tan(math.radians(lim)) * 100.0
            b = max(-bmax, min(bmax, bfit))
            a = float(np.min(zs - b * ts[None, :])) - 20.0
            burial = float(np.max(zs - (a + b * ts[None, :])))
            if burial <= 250.0:
                break
        if burial > 260.0:
            self.notes.append("%s: wall run of %.0f m buried up to %.1f m on one side" % (self.id, L, burial / 100.0))
        yaw = ang_of(u) + (180.0 if flip else 0.0)
        pitch = math.degrees(math.atan(b / 100.0)) * (-1.0 if flip else 1.0)
        for k in range(n):
            t = (k + 0.5) * L / n
            C = P + u * t
            o = obb_of(seg, C[0], C[1], yaw, sc)
            self.commit(seg, float(C[0]), float(C[1]), a + b * t, yaw, "Walls", pitch=pitch, scale=sc, obb=o, tag="wall")
            self.grid.paint_obb(o, 2.0, F_WALL)

    def _paint_outside(self, r_inset=3.0):
        if self.wall_r is None:
            return
        g = self.grid
        xs = g.centres(0, g.n)
        # rows in chunks to limit memory
        for j0 in range(0, g.n, 256):
            j1 = min(g.n, j0 + 256)
            X, Y = np.meshgrid(xs, xs[j0:j1])
            th = np.arctan2(Y, X)
            r = np.hypot(X, Y)
            rw = self.wall_r(th)
            out = np.isfinite(rw) & (r > rw - r_inset)
            if len(self.wall_ranges) == 1 and self.wall_ranges[0] == (0.0, 2 * math.pi):
                pass
            sub = g.flags[j0:j1]
            sub[out] |= np.uint16(F_OUTSIDE)

    def r_bound(self, th, default=None):
        """Settlement boundary radius along theta: the wall lane where there is a wall, else `default`."""
        th = np.asarray(th, np.float64)
        d = self.R * 0.95 if default is None else default
        if self.wall_r is None:
            return np.full(th.shape, d)
        rw = self.wall_r(th)
        return np.where(np.isfinite(rw), rw - getattr(self, "lane_off", 10.0), d)

    def wall_lane(self, hw=2.5, mat="dirt"):
        if self.wall_r is None:
            return
        for (a, b) in self.wall_ranges:
            if b - a >= 2 * math.pi - 1e-6:
                th = np.linspace(0, 2 * math.pi, 721)[:-1]
                closed = True
            else:
                th = np.linspace(a, b, max(8, int((b - a) * 180)))
                closed = False
            r = self.wall_r(th)
            r = np.where(np.isfinite(r), r, np.nanmax(r))
            r = r - self.lane_off
            pts = np.stack([np.cos(th) * r, np.sin(th) * r], 1)
            self.add_street(pts, hw, "wall", mat, 9.0, sides=(True, True), closed=closed, par_check=False,
                            block=F_WATER | F_LANDMARK | F_RESERVED | F_KEEPIN, min_len=20.0)

    # ------------------------------------------------------------------ roads -> main streets
    def main_streets(self, hw_min, mat, extra_out=12.0, prio=1.0):
        """Road polylines inside the settlement become main streets (not clipped: they run through the gates)."""
        out = []
        for road, p in self._roads_local():
            d = np.hypot(p[:, 0], p[:, 1])
            th = np.arctan2(p[:, 1], p[:, 0])
            if self.wall_r is not None:
                rw = self.wall_r(th)
                lim = np.where(np.isfinite(rw), rw + extra_out, self.R * 1.05)
            else:
                lim = np.full(len(p), self.R + extra_out)
            inside = d <= lim
            if not inside.any():
                continue
            # contiguous runs of inside points
            k = np.flatnonzero(inside)
            runs = np.split(k, np.flatnonzero(np.diff(k) > 1) + 1)
            for rk in runs:
                if len(rk) < 2:
                    continue
                a = max(0, rk[0] - 1)
                b = min(len(p) - 1, rk[-1] + 1)
                seg = p[a:b + 1].copy()
                # trim the outer ends exactly at the limit
                hw = max(hw_min, 0.5 * road["WidthCm"] / 100.0)
                self.road_lines.append((seg.copy(), hw))
                # where the road ends in the central / market square, the square itself is the paving
                keep = np.ones(len(seg), bool)
                for pl in self.plazas:
                    if pl.kind not in ("central", "market", "square"):
                        continue
                    if pl.poly is not None:
                        inside = points_in_poly(seg[:, 0], seg[:, 1], [tuple(v) for v in pl.poly])
                    elif pl.r > 0:
                        inside = np.hypot(seg[:, 0] - pl.x, seg[:, 1] - pl.y) < pl.r - 3.0
                    else:
                        continue
                    if inside[0] or inside[-1]:
                        keep &= ~inside
                if not keep.all():
                    k2 = np.flatnonzero(keep)
                    if len(k2) < 2:
                        continue
                    pieces = np.split(k2, np.flatnonzero(np.diff(k2) > 1) + 1)
                    segs = [seg[pc] for pc in pieces if len(pc) > 1]
                else:
                    segs = [seg]
                sts = []
                for sg in segs:
                    sts += self.add_street(sg, hw, "main", mat, prio, clip=False, par_check=False, min_len=4.0)
                for st in sts:
                    st.zone = road["Id"]
                out += sts
        return out

    def boulevard_between_mains(self, mains, max_sep=34.0, mat="cobble"):
        """Pave the strip between two nearly parallel main streets (e.g. Rapan's twin caravan roads)."""
        for i in range(len(mains)):
            for j in range(i + 1, len(mains)):
                A = mains[i].line.resample(3.0)
                Bl = mains[j].line
                quads = []
                prev = None
                for p in A:
                    s, dist = Bl.project(p)
                    if 3.0 < dist < max_sep and math.hypot(*p) > 20:
                        q = Bl.at(s)
                        if prev is not None:
                            quads.append([prev[0], p, q, prev[1]])
                        prev = (p, q)
                    else:
                        prev = None
                for qd in quads:
                    poly = np.array(qd)
                    self.grid.paint_poly(poly, F_PLAZA)
                    self.plazas.append(Plaza(float(poly[:, 0].mean()), float(poly[:, 1].mean()), 0.0, "boulevard", mat, False, poly))

    # ------------------------------------------------------------------ streamline network
    def streamline_network(self, field, dsep, kinds, max_lines=700, step=2.0, t_stop=None, min_len=22.0,
                           struct_kinds=("main", "avenue", "ring", "wall", "shoprow", "grid_main"), max_turn=32.0,
                           dtest=0.5, plaza_seeds=True):
        """Evenly spaced streamlines of both tensor-field families become streets, seeded outward from the structural
        streets (Jobard-Lefer): crossing seeds along every accepted line, parallel seeds dsep to either side. Lines stop
        at water, landmarks, plazas, the wall, near a same-family line (dtest x dsep) or, with probability t_stop, where
        they cross another street (T-junctions). dsep(x, y, fam) -> m; kinds(line, fam, length) -> (kind, hw, mat, prio)."""
        import collections
        rng = self.rng
        cell = 8.0
        H = ({}, {})
        t_stop = t_stop or (lambda x, y: 0.12)
        block = F_WATER | F_LANDMARK | F_OUTSIDE | F_WALL | F_RESERVED | F_KEEPIN | F_PLAZA
        cos_turn = math.cos(math.radians(max_turn))

        def key(x, y):
            return int(math.floor(x / cell)), int(math.floor(y / cell))

        def hadd(fam, pts):
            for x, y in pts:
                H[fam].setdefault(key(x, y), []).append((x, y))

        def close(fam, x, y, dist):
            ci, cj = key(x, y)
            k = int(math.ceil(dist / cell))
            d2 = dist * dist
            for i in range(ci - k, ci + k + 1):
                for j in range(cj - k, cj + k + 1):
                    for qx, qy in H[fam].get((i, j), ()):
                        if (qx - x) ** 2 + (qy - y) ** 2 < d2:
                            return True
            return False

        def trace(x0, y0, fam):
            halves = []
            own = {}                         # the line's own points (both halves), to stop spirals / self-overlap
            cur = [0, 0]                     # current half, index within it

            def own_close(x, y, dist):
                ci, cj = key(x, y)
                k = int(math.ceil(dist / cell))
                d2 = dist * dist
                h, n_now = cur
                for i in range(ci - k, ci + k + 1):
                    for j in range(cj - k, cj + k + 1):
                        for qx, qy, qh, qn in own.get((i, j), ()):
                            if qh == h:
                                older = n_now - qn > 25
                            else:
                                older = qn > 25 or n_now > 25     # both halves leave the seed side by side
                            if older and (qx - x) ** 2 + (qy - y) ** 2 < d2:
                                return True
                return False

            def own_add(x, y):
                own.setdefault(key(x, y), []).append((x, y, cur[0], cur[1]))
                cur[1] += 1
            for half, sgn in enumerate((1.0, -1.0)):
                pts = []
                x, y = x0, y0
                px = py = None
                was_out = 0.0
                cur[0], cur[1] = half, 0
                for _ in range(1500):
                    dx, dy = field.direction(x, y, fam)
                    if px is None:
                        dx, dy = dx * sgn, dy * sgn
                    elif dx * px + dy * py < 0:
                        dx, dy = -dx, -dy
                    ex, ey = field.direction(x + dx * step * 0.5, y + dy * step * 0.5, fam)
                    if ex * dx + ey * dy < 0:
                        ex, ey = -ex, -ey
                    if px is not None and ex * px + ey * py < cos_turn:
                        break
                    nx_, ny_ = x + ex * step, y + ey * step
                    fl = self.grid.flag_at(nx_, ny_)
                    if fl & block:
                        if (fl & F_PLAZA) and not (fl & (F_LANDMARK | F_KEEPIN | F_WATER | F_OUTSIDE | F_WALL)):
                            pts.append((nx_, ny_))
                        break
                    if close(fam, nx_, ny_, dtest * dsep(nx_, ny_, fam)):
                        break
                    if own_close(nx_, ny_, max(4.0, 0.5 * dtest * dsep(nx_, ny_, fam))):
                        pts.append((nx_, ny_))
                        break
                    pts.append((nx_, ny_))
                    own_add(nx_, ny_)
                    if fl & F_STREET:
                        if was_out > 8.0 and rng.random() < t_stop(nx_, ny_):
                            break
                        was_out = 0.0
                    else:
                        was_out += step
                    x, y, px, py = nx_, ny_, ex, ey
                halves.append(pts)
            return list(reversed(halves[1])) + [(x0, y0)] + halves[0]

        queue = collections.deque()
        for st in list(self.streets):
            if st.kind not in struct_kinds:
                continue
            P = st.line.resample(2.0)
            T = np.gradient(P, axis=0)
            T = T / (np.hypot(T[:, 0], T[:, 1])[:, None] + 1e-9)
            for (x, y), t in zip(P, T):
                for fam in (0, 1):
                    dx, dy = field.direction(x, y, fam)
                    if abs(dx * t[0] + dy * t[1]) > 0.8:
                        H[fam].setdefault(key(x, y), []).append((x, y))
            s = float(rng.uniform(0.0, 10.0))
            while s < st.line.L:
                x, y = st.line.at(s)
                t = st.line.tangent(s, 3.0)
                for fam in (0, 1):
                    dx, dy = field.direction(x, y, fam)
                    if abs(dx * t[0] + dy * t[1]) < 0.55:
                        nx_, ny_ = -t[1], t[0]
                        for sgn in (1.0, -1.0):
                            queue.append((x + nx_ * sgn * (st.hw + 1.5), y + ny_ * sgn * (st.hw + 1.5), fam))
                s += 0.5 * min(dsep(x, y, 0), dsep(x, y, 1))
        if plaza_seeds:
            for pl in self.plazas:
                if pl.kind not in ("central", "market", "gate", "forecourt", "square", "promenade") or pl.poly is not None:
                    continue
                m = max(6, int(2 * math.pi * pl.r / 12.0))
                for k in range(m):
                    a = 2 * math.pi * k / m
                    x, y = pl.x + math.cos(a) * (pl.r + 2.0), pl.y + math.sin(a) * (pl.r + 2.0)
                    best = max((0, 1), key=lambda f: abs(field.direction(x, y, f)[0] * math.cos(a) + field.direction(x, y, f)[1] * math.sin(a)))
                    queue.append((x, y, best))
        lines = 0
        why = {"blocked": 0, "close": 0, "short": 0, "clipped": 0}
        while queue and lines < max_lines:
            x, y, fam = queue.popleft()
            if self.grid.flag_at(x, y) & block:
                why["blocked"] += 1
                continue
            if close(fam, x, y, 0.8 * dsep(x, y, fam)):
                why["close"] += 1
                continue
            line = trace(x, y, fam)
            if len(line) < 3:
                why["short"] += 1
                continue
            arr = np.array(line)
            L = float(np.sum(np.hypot(*np.diff(arr, axis=0).T)))
            if L < min_len:
                why["short"] += 1
                continue
            kind, hw, mat, prio = kinds(arr, fam, L)
            got = self.add_street(arr, hw, kind, mat, prio, clip=True, par_check=False, min_len=min_len * 0.6,
                                  block=F_WATER | F_LANDMARK | F_OUTSIDE | F_WALL | F_RESERVED | F_KEEPIN | F_PLAZA)
            if not got:
                why["clipped"] += 1
                continue
            for st in got:
                st.zone = "fam%d" % fam
            hadd(fam, line)
            lines += 1
            ln = Line(arr)
            s = 0.5 * dsep(arr[0][0], arr[0][1], 1 - fam)
            while s < ln.L:
                q = ln.at(s)
                t = ln.tangent(s, 3.0)
                nv = np.array([-t[1], t[0]])
                queue.append((float(q[0]), float(q[1]), 1 - fam))
                d = dsep(q[0], q[1], fam)
                for sgn in (1.0, -1.0):
                    # step out by the larger of the local and the target separation (it grows outward)
                    q2 = q + nv * sgn * d
                    d2 = max(d, dsep(q2[0], q2[1], fam))
                    queue.append((float(q[0] + nv[0] * sgn * d2), float(q[1] + nv[1] * sgn * d2), fam))
                s += 0.5 * min(dsep(q[0], q[1], 1 - fam), d)
        self.stats["streamlines"] = lines
        self.stats["streamline_rejects"] = why
        return lines

    def split_streets(self, min_piece=5.0):
        """Split every street into edges at crossings and T-junctions, so each block edge can be thinned on its own."""
        S = self.streets
        coarse = [st.line.resample(5.0) if st.line.L > 10 else st.line.p for st in S]
        boxes = [(c[:, 0].min() - st.hw - 3, c[:, 0].max() + st.hw + 3, c[:, 1].min() - st.hw - 3,
                  c[:, 1].max() + st.hw + 3) for c, st in zip(coarse, S)]
        cuts = [[] for _ in S]
        for i in range(len(S)):
            A = coarse[i]
            for j in range(i + 1, len(S)):
                bi, bj = boxes[i], boxes[j]
                if bi[1] < bj[0] or bj[1] < bi[0] or bi[3] < bj[2] or bj[3] < bi[2]:
                    continue
                B = coarse[j]
                if len(A) > 1 and len(B) > 1:
                    a0, a1 = A[:-1][:, None, :], A[1:][:, None, :]
                    b0, b1 = B[:-1][None, :, :], B[1:][None, :, :]
                    r = a1 - a0
                    s = b1 - b0
                    den = r[..., 0] * s[..., 1] - r[..., 1] * s[..., 0]
                    q = b0 - a0
                    with np.errstate(divide="ignore", invalid="ignore"):
                        t = (q[..., 0] * s[..., 1] - q[..., 1] * s[..., 0]) / den
                        u = (q[..., 0] * r[..., 1] - q[..., 1] * r[..., 0]) / den
                    hit = (np.abs(den) > 1e-9) & (t >= 0) & (t <= 1) & (u >= 0) & (u <= 1)
                    for ki, kj in zip(*np.nonzero(hit)):
                        P = A[ki] + (A[ki + 1] - A[ki]) * t[ki, kj]
                        cuts[i].append(S[i].line.project(P)[0])
                        cuts[j].append(S[j].line.project(P)[0])
                # T-junctions: an end of one street lying on the other
                for (me, other, oi) in ((i, j, S[j]), (j, i, S[i])):
                    L0 = S[me].line
                    for P in (L0.p[0], L0.p[-1]):
                        s_o, d_o = oi.line.project(P)
                        if d_o < oi.hw + 2.0 and 3.0 < s_o < oi.line.L - 3.0:
                            cuts[other].append(s_o)
        out = []
        for st, cs in zip(S, cuts):
            L = st.line.L
            cs = sorted(c for c in cs if 2.0 < c < L - 2.0)
            merged = []
            for c in cs:
                if not merged or c - merged[-1] > 6.0:
                    merged.append(c)
            if not merged or st.kind in ("wall",):
                out.append(st)
                continue
            edges = [0.0] + merged + [L]
            for a, b_ in zip(edges[:-1], edges[1:]):
                if b_ - a < min_piece:
                    continue
                ss = np.linspace(a, b_, max(2, int((b_ - a) / 2.0) + 1))
                ln = Line(st.line.at(ss))
                ne = Street(ln, st.hw, st.kind, st.mat, st.prio, st.sides)
                ne.zone = st.zone
                out.append(ne)
        self.streets = out
        return len(out)

    def street_axis_near(self, x, y, kinds=("main", "avenue")):
        """Tangent of the nearest street of the given kinds at the point nearest to (x, y)."""
        best = None
        for st in self.streets:
            if st.kind not in kinds:
                continue
            s, d = st.line.project(np.array([x, y]))
            if best is None or d < best[0]:
                best = (d, st.line.tangent(s, 4.0))
        return None if best is None else best[1]

    def connector(self, x0, y0, x1, y1, hw, kind, mat, prio=2.0):
        """Straight street from (x0, y0) toward (x1, y1) that stops where it meets the existing network."""
        L = math.hypot(x1 - x0, y1 - y0)
        n = max(2, int(L / 1.0))
        ts = np.linspace(0, 1, n)
        P = np.stack([x0 + (x1 - x0) * ts, y0 + (y1 - y0) * ts], 1)
        fl = self.grid.get(P[:, 0], P[:, 1])
        hit = np.flatnonzero(((fl & (F_STREET | F_PLAZA)) != 0) & (ts * L > 4.0))
        end = int(hit[0]) + 3 if len(hit) else n
        pts = P[:min(end, n)]
        if len(pts) < 2:
            return []
        return self.add_street(pts, hw, kind, mat, prio, clip=False, par_check=False, min_len=3.0)

    def prune_disconnected(self, seeds):
        """Drop streets that are not connected to the seed plazas / main streets (2 m flood fill)."""
        self.prune_seeds = list(seeds)
        g = Grid(self.half, 2.0)
        for st in self.streets:
            g.paint_polyline(st.line.p, max(st.hw, 1.2), 1)
        for pl in self.plazas:
            if pl.kind in ("garden",):
                continue
            if pl.poly is not None:
                g.paint_poly(pl.poly, 1)
            else:
                g.paint_circle(pl.x, pl.y, max(pl.r, 1.5), 1)
        m = g.flags > 0
        seen = np.zeros_like(m)
        stack = []
        for x, y in seeds:
            i = int((x + self.half) / 2.0)
            j = int((y + self.half) / 2.0)
            if 0 <= i < g.n and 0 <= j < g.n and m[j, i]:
                stack.append((j, i))
                seen[j, i] = True
        while stack:
            j, i = stack.pop()
            for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                jj, ii = j + dj, i + di
                if 0 <= jj < g.n and 0 <= ii < g.n and m[jj, ii] and not seen[jj, ii]:
                    seen[jj, ii] = True
                    stack.append((jj, ii))
        keep = []
        dropped = 0
        for st in self.streets:
            p = st.line.resample(4.0)
            ii = np.clip(((p[:, 0] + self.half) / 2.0).astype(int), 0, g.n - 1)
            jj = np.clip(((p[:, 1] + self.half) / 2.0).astype(int), 0, g.n - 1)
            if seen[jj, ii].any():
                keep.append(st)
            else:
                dropped += 1
        if dropped:
            self.streets = keep
            # repaint the street raster
            self.grid.flags &= np.uint16(0xFFFF ^ F_STREET)
            for st in self.streets:
                self.grid.paint_polyline(st.line.p, st.hw + STREET_PAD, F_STREET)
        return dropped

    # ------------------------------------------------------------------ frontage
    def frontage_line(self, line, hw, side, kind, rules, jitter):
        """Walk one side of a street / plaza edge placing buildings front-to-street. rules(p, kind) gives
        (q_open, open_len, gap, setback, run): once a run of at least `run` buildings is built, a garden gap of
        open_len opens with probability q_open, so houses cluster in rows instead of scattering."""
        rng = self.frng
        L = line.L
        s = float(rng.uniform(0.0, 2.5))
        last = []
        placed = 0
        run_left = 0
        while s < L - 2.0:
            p = line.at(s)
            q_open, open_len, gap, setback, run = rules(p, kind)
            pal = self.palette(p, kind)
            names = [a for a, w in pal]
            wts = [0.0 if self.cap_full(a) else w * (0.1 if a.name in last[-2:] else 1.0) for a, w in pal]
            picks = weighted_order(rng, names, wts, 3)
            # fall back to the shallowest and the narrowest house of the palette (fits behind deep neighbours)
            for extra in (min(names, key=lambda a: a.d), min(names, key=lambda a: a.w)):
                if extra not in picks and not self.cap_full(extra):
                    picks.append(extra)
            ok = False
            for a in picks:
                w = a.w
                s1 = s + w
                if s1 > L and not line.closed:
                    continue
                P0 = line.at(s)
                P1 = line.at(s1)
                t = P1 - P0
                tl = float(np.hypot(*t))
                if tl < 0.75 * w:
                    continue
                t = t / tl
                Pm = line.at(s + 0.5 * w)
                nrm = np.array([-t[1], t[0]]) * side
                yaw = yaw_facing(-nrm[0], -nrm[1]) + float(rng.normal(0.0, jitter))
                sb = float(rng.uniform(*setback))
                base = Pm + nrm * (hw + sb + a.y1)
                if abs(a.ocx) > 1e-3:
                    ex = np.array([math.cos(math.radians(yaw)), math.sin(math.radians(yaw))])
                    base = base - ex * a.ocx
                pl = self.try_building(a, float(base[0]), float(base[1]), yaw, tag=kind)
                if pl is not None:
                    ok = True
                    placed += 1
                    last.append(a.name)
                    self.used[a.name] = self.used.get(a.name, 0) + 1
                    s = s1 + float(rng.uniform(*gap))
                    run_left -= 1
                    if run_left <= 0 and q_open > 0 and rng.random() < q_open:
                        s += float(rng.uniform(*open_len))
                        run_left = int(rng.integers(run[0], run[1] + 1))
                    break
            if not ok:
                s += 2.0
        return placed

    def cap_full(self, a):
        cap = self.caps.get(a.name)
        return cap is not None and self.used.get(a.name, 0) >= cap

    def plaza_line(self, pl, n=None):
        if pl.poly is not None:
            return Line(pl.poly, closed=True)
        n = n or max(16, int(2 * math.pi * pl.r / 2.0))
        th = np.linspace(0, 2 * math.pi, n, endpoint=False)
        return Line(np.stack([pl.x + np.cos(th) * pl.r, pl.y + np.sin(th) * pl.r], 1), closed=True)

    def run_frontage(self, rules, jitter):
        """Line every plaza and street in priority order (plazas, main streets, rings, then the rest, inner first)."""
        self.frng = np.random.default_rng([SEED, self.crc, 17])
        self.used = {}
        for p in self.items:
            if p.cls == "Buildings":
                self.used[p.asset.name] = self.used.get(p.asset.name, 0) + 1
        jobs = []
        for pl in self.plazas:
            if not pl.frontage or (pl.r <= 0 and pl.poly is None):
                continue
            ln = self.plaza_line(pl)
            area = 0.5 * np.sum(ln.p[:-1, 0] * ln.p[1:, 1] - ln.p[1:, 0] * ln.p[:-1, 1])
            side = -1.0 if area > 0 else 1.0
            kind = "garden" if pl.kind == "garden" else "plaza"
            jobs.append((0.5 if kind == "plaza" else 2.6, math.hypot(pl.x, pl.y), ln, 0.0, side, kind, None))
        for st in self.streets:
            mid = st.line.at(st.line.L / 2)
            for k, side in enumerate((1.0, -1.0)):
                if not st.sides[k]:
                    continue
                jobs.append((st.prio, math.hypot(*mid), st.line, st.hw, side, st.kind, st))
        jobs.sort(key=lambda j: (j[0], j[1]))
        n = 0
        self.front_runs = []
        for prio, dist, ln, hw, side, kind, st in jobs:
            n0 = len(self.items)
            n += self.frontage_line(ln, hw, side, kind, rules, jitter)
            seq = [p for p in self.items[n0:] if p.cls == "Buildings"]
            self.front_runs.append((kind, seq, st))
        return n

    def calibrate(self, target, lo_budget, hi_budget, make_fns, keep_fn=None, iters=26, drop_kinds=()):
        """Place the frontage at full density once, then thin toward the density profile keep_fn(p, kind, alpha):
        minor streets (drop_kinds) are kept or dropped whole with probability keep (dropped ones become gardens and
        fields and leave the network); other frontage loses whole runs of houses. alpha is bisected to the target."""
        self.split_streets()
        rules, jit = make_fns(0.0)
        self.run_frontage(rules, jit)
        in_runs = set(id(p) for _, seq, _ in self.front_runs for p in seq)
        n_fixed = sum(1 for p in self.items if p.cls == "Buildings" and id(p) not in in_runs)
        n_full = n_fixed + len(in_runs)
        self.stats["capacity"] = n_full
        if keep_fn is None or n_full <= target:
            self.stats["density_alpha"] = 0.0
            return n_full
        streets = []
        seen = set()
        for kind, seq, st in self.front_runs:
            if st is not None and st.kind in drop_kinds and id(st) not in seen:
                seen.add(id(st))
                streets.append(st)
        ru = np.random.default_rng([SEED, self.crc, 41]).random(len(streets))
        s_mid = [tuple(st.line.at(st.line.L / 2)) for st in streets]

        def thin(alpha):
            rng = np.random.default_rng([SEED, self.crc, 23])
            gone = set()
            for st, u, pm in zip(streets, ru, s_mid):
                if u > keep_fn(pm, st.kind, alpha):
                    gone.add(id(st))
            drop = set()
            for kind, seq, st in self.front_runs:
                if not seq:
                    continue
                if st is not None and id(st) in gone:
                    drop.update(id(p) for p in seq)
                    continue
                mild = st is not None and st.kind in drop_kinds
                i = 0
                kp0 = max(0.02, min(1.0, keep_fn((seq[0].x, seq[0].y), kind, alpha)))
                if mild:
                    kp0 = min(1.0, 0.55 + 0.45 * kp0)
                keeping = rng.random() < kp0 ** 0.5
                while i < len(seq):
                    p = seq[i]
                    kp = max(0.02, min(1.0, keep_fn((p.x, p.y), kind, alpha)))
                    if mild:
                        kp = min(1.0, 0.55 + 0.45 * kp)
                    if kp >= 0.999:
                        i += 1
                        keeping = True
                        continue
                    if keeping:
                        i += int(rng.integers(2, 6)) if kp > 0.5 else int(rng.integers(1, 4))
                        keeping = False
                    else:
                        m = int(round(3.0 * (1.0 - kp) / kp * float(rng.uniform(0.6, 1.4))))
                        for q in seq[i:i + m]:
                            drop.add(id(q))
                        i += max(m, 0)
                        keeping = True
            return drop, gone
        lo, hi = 0.0, 8.0
        best = None
        for _ in range(iters):
            mid = 0.5 * (lo + hi)
            n = n_full - len(thin(mid)[0])
            key = (0 if lo_budget <= n <= hi_budget else 1, abs(n - target))
            if best is None or key < best[2]:
                best = (mid, n, key)
            if n > target:
                lo = mid
            else:
                hi = mid
            if abs(n - target) <= 2:
                break
        alpha = best[0]
        drop, gone = thin(alpha)
        if gone:
            self.streets = [st for st in self.streets if id(st) not in gone]
            self.grid.flags &= np.uint16(0xFFFF ^ F_STREET)
            for st in self.streets:
                self.grid.paint_polyline(st.line.p, st.hw + STREET_PAD, F_STREET)
            before = set(id(st) for st in self.streets)
            self.prune_disconnected(self.prune_seeds)
            lost = before - set(id(st) for st in self.streets)
            for kind, seq, st in self.front_runs:
                if st is not None and id(st) in lost:
                    drop.update(id(p) for p in seq)
        self.items = [p for p in self.items if id(p) not in drop]
        self.front_runs = [(k, [p for p in seq if id(p) not in drop], st) for k, seq, st in self.front_runs]
        self.index = ObbIndex(24.0)
        for p in self.items:
            if p.cls != "Props":
                self.index.add(p.obb, p)
        self.grid.flags &= np.uint16(0xFFFF ^ F_BUILD)
        for p in self.items:
            if p.cls in ("Buildings", "Landmark"):
                self.grid.paint_obb(p.obb, 0.0, F_BUILD)
        n = sum(1 for p in self.items if p.cls == "Buildings")
        self.stats["density_alpha"] = round(alpha, 3)
        self.stats["streets_dropped"] = len(gone)
        if n > hi_budget:
            self.notes.append("%s: %d buildings above the budget %d" % (self.id, n, hi_budget))
        return n

    # ------------------------------------------------------------------ props
    def near_road(self, x, y, pad=2.5):
        """True when (x, y) lies on a road's through-corridor (the original road line, also across squares)."""
        for q, hw in self.road_lines:
            if np.min(np.hypot(q[:, 0] - x, q[:, 1] - y)) > hw + pad + 12.0:
                continue
            d = min(float(seg_dist(x, y, a[0], a[1], c[0], c[1])) for a, c in zip(q[:-1], q[1:]))
            if d < hw + pad:
                return True
        return False

    def free_spot(self, x, y, radius, rmax, avoid=F_STREET | F_WATER | F_BUILD | F_WALL | F_LANDMARK | F_PROP):
        """(x, y) or the nearest spot within rmax whose disc of `radius` is off streets, road corridors and solids."""
        for rr in np.arange(0.0, rmax + 1e-6, 2.0):
            m = 1 if rr == 0 else max(8, int(2 * math.pi * rr / 3.0))
            for k in range(m):
                a = 2 * math.pi * k / m
                px, py = x + math.cos(a) * rr, y + math.sin(a) * rr
                if not self.grid.any_in_circle(px, py, radius, avoid) and not self.near_road(px, py, radius + 0.5):
                    return px, py
        return None

    def clear_of_streets(self, x, y, r):
        """Exact test: the disc (x, y, r) stays outside every paved street band."""
        for st in self.streets:
            p = st.line.p
            if x < p[:, 0].min() - st.hw - r - 1 or x > p[:, 0].max() + st.hw + r + 1 or \
                    y < p[:, 1].min() - st.hw - r - 1 or y > p[:, 1].max() + st.hw + r + 1:
                continue
            d = seg_dist(x, y, p[:-1, 0], p[:-1, 1], p[1:, 0], p[1:, 1])
            if float(np.min(d)) < st.hw + r:
                return False
        return True

    def lamps_along(self, kinds, spacing=26.0, name="SM_Prop_LampPost_A"):
        """Lamp posts just outside the paved edge, alternating sides."""
        n = 0
        for st in self.streets:
            if st.kind not in kinds:
                continue
            L = st.line.L
            for k, side in enumerate((1.0, -1.0)):
                s = float(self.rng.uniform(4, spacing / 2)) + (spacing / 2 if side < 0 else 0.0)
                while s < L - 3:
                    p = st.line.at(s)
                    t = st.line.tangent(s, 2.0)
                    nrm = np.array([-t[1], t[0]]) * side
                    q = p + nrm * (st.hw + 0.5)
                    ok = (self.grid.flag_at(q[0], q[1]) & (F_PLAZA | F_WATER)) == 0 and self.clear_of_streets(q[0], q[1], 0.3)
                    if ok and self.try_prop(name, float(q[0]), float(q[1]), ang_of(t), clear=0.15, gap=2.0,
                                            block=F_WATER | F_WALL | F_LANDMARK | F_BUILD | F_RESERVED | F_OUTSIDE | F_KEEPIN):
                        n += 1
                        s += spacing
                    else:
                        s += 3.0
        return n

    def plaza_centrepiece(self, pl, kind="fountain", benches=6, statues=0, lamps=True):
        n = 0
        cx, cy = pl.x, pl.y
        if kind in ("fountain", "statue"):
            spot = self.free_spot(pl.x, pl.y, 3.6 if kind == "fountain" else 2.2, max(0.0, min(pl.r - 4.0, 12.0)))
            if spot is None:
                kind = "none"
            else:
                cx, cy = spot
        if kind == "fountain":
            if self.try_prop("SM_Prop_Fountain_A", cx, cy, float(self.rng.uniform(0, 45)),
                             block=F_WATER | F_BUILD | F_WALL | F_LANDMARK | F_STREET, check_index=True):
                n += 1
            rb = 6.3
        elif kind == "statue":
            if self.try_prop("SM_Prop_Statue_A", cx, cy, float(self.rng.uniform(0, 360)),
                             block=F_WATER | F_BUILD | F_WALL | F_LANDMARK | F_STREET):
                n += 1
            rb = 4.0
        else:
            rb = 3.0
        pl = Plaza(cx, cy, pl.r, pl.kind, pl.mat, pl.frontage, pl.poly) if (cx, cy) != (pl.x, pl.y) else pl
        a0 = float(self.rng.uniform(0, 2 * math.pi))
        for k in range(benches):
            a = a0 + 2 * math.pi * k / benches
            bx, by = pl.x + math.cos(a) * rb, pl.y + math.sin(a) * rb
            if self.near_road(bx, by, 1.5):
                continue
            if self.try_prop("SM_Prop_Bench_A", bx, by, yaw_facing(pl.x - bx, pl.y - by), clear=0.1, gap=0.4,
                             block=F_WATER | F_BUILD | F_WALL | F_LANDMARK | F_STREET):
                n += 1
        for k in range(statues):
            a = a0 + math.pi / 4 + 2 * math.pi * k / max(1, statues)
            rr = min(pl.r - 3.0, max(rb + 5.0, 0.62 * pl.r))
            sx, sy = pl.x + math.cos(a) * rr, pl.y + math.sin(a) * rr
            if self.near_road(sx, sy, 2.0):
                continue
            if self.try_prop("SM_Prop_Statue_A", sx, sy, yaw_facing(pl.x - sx, pl.y - sy) + 180.0, clear=0.3, gap=1.0,
                             block=F_WATER | F_BUILD | F_WALL | F_LANDMARK | F_STREET):
                n += 1
        if lamps and pl.r > 8:
            m = max(4, int(2 * math.pi * pl.r / 16.0))
            for k in range(m):
                a = a0 + 2 * math.pi * (k + 0.5) / m
                lx, ly = pl.x + math.cos(a) * (pl.r - 1.2), pl.y + math.sin(a) * (pl.r - 1.2)
                if self.near_road(lx, ly, 1.0):
                    continue
                if self.try_prop("SM_Prop_LampPost_A", lx, ly, 0.0, clear=0.4, gap=1.5,
                                 block=F_WATER | F_BUILD | F_WALL | F_LANDMARK | F_STREET):
                    n += 1
        return n

    def market_stalls(self, pl, names, rows_axis=None, spacing=4.2, aisle=7.0, extras=("SM_Prop_Crates_A", "SM_Prop_Barrels_A"),
                      fill=0.85, avoid_center=0.0):
        """Rows of stalls facing aisles inside a plaza (stalls avoid street bands running through it)."""
        rng = self.rng
        ax = unit(rows_axis) if rows_axis is not None else np.array([1.0, 0.0])
        ay = np.array([-ax[1], ax[0]])
        n = 0
        R = pl.r
        v = -R
        row = 0
        while v <= R:
            face = 1.0 if row % 2 == 0 else -1.0
            u = -R
            while u <= R:
                p = np.array([pl.x, pl.y]) + ax * u + ay * v
                if math.hypot(p[0] - pl.x, p[1] - pl.y) < R - 3.0 and math.hypot(p[0] - pl.x, p[1] - pl.y) >= avoid_center \
                        and rng.random() < fill and not self.near_road(float(p[0]), float(p[1]), 3.0):
                    nm = names[int(rng.integers(len(names)))]
                    yaw = yaw_facing(*(ay * face))
                    if self.try_prop(nm, float(p[0]), float(p[1]), yaw + float(rng.normal(0, 2.0)), clear=0.6, gap=0.8,
                                     block=F_STREET | F_WATER | F_WALL | F_LANDMARK | F_BUILD | F_RESERVED | F_OUTSIDE):
                        n += 1
                        if extras and rng.random() < 0.35:
                            e = extras[int(rng.integers(len(extras)))]
                            q = p + ax * (2.6 if rng.random() < 0.5 else -2.6)
                            if self.try_prop(e, float(q[0]), float(q[1]), float(rng.uniform(0, 360)), clear=0.4, gap=0.4,
                                             block=F_STREET | F_WATER | F_WALL | F_LANDMARK | F_BUILD | F_RESERVED | F_OUTSIDE):
                                n += 1
                u += spacing
            v += aisle if row % 2 == 0 else 3.4
            row += 1
        return n

    def shopfront_goods(self, kinds_prefix=("Shop", "Inn", "Workshop"), prob=0.55):
        n = 0
        rng = self.rng
        for p in list(self.items):
            if p.cls != "Buildings" or not any(k in p.asset.name for k in kinds_prefix):
                continue
            if rng.random() > prob:
                continue
            a = p.asset
            c, s = math.cos(math.radians(p.yaw)), math.sin(math.radians(p.yaw))
            for lx in ((a.x1 - 1.0), (a.x0 + 1.0)):
                ly = a.y1 + 1.1
                wx = p.x + lx * c - ly * s
                wy = p.y + lx * s + ly * c
                nm = "SM_Prop_Crates_A" if rng.random() < 0.5 else "SM_Prop_Barrels_A"
                if self.try_prop(nm, wx, wy, p.yaw + float(rng.uniform(-20, 20)), clear=0.2, gap=0.3,
                                 block=F_STREET | F_PLAZA | F_WATER | F_WALL | F_LANDMARK | F_BUILD | F_RESERVED | F_OUTSIDE):
                    n += 1
                    break
        return n

    def gate_props(self):
        n = 0
        for g in self.gates:
            ey = g["ey"]
            ex = np.array([ey[1], -ey[0]])
            for side in (1.0, -1.0):
                q = np.array([g["x"], g["y"]]) - ey * 20.0 + ex * side * (0.5 * max(g["w"], 6.0) + 2.0)
                if self.try_prop("SM_Prop_Signpost_A", float(q[0]), float(q[1]), yaw_facing(-ey[0], -ey[1]) + 180.0, clear=0.2):
                    n += 1
                    break
            for k in range(3):
                q = np.array([g["x"], g["y"]]) - ey * (16.0 + 3.0 * k) - ex * (0.5 * max(g["w"], 6.0) + 2.2 + self.rng.uniform(0, 1.5))
                nm = "SM_Prop_Crates_A" if k % 2 == 0 else "SM_Prop_Barrels_A"
                if self.try_prop(nm, float(q[0]), float(q[1]), float(self.rng.uniform(0, 360)), clear=0.2):
                    n += 1
        return n

    # ------------------------------------------------------------------ ports
    def harbour(self, lm_name="Harbour Docks", piers=5, boats=6):
        lm = self.lm(lm_name)
        if lm is None:
            return 0
        rng = self.rng
        # candidate shore points around the docks landmark: land >= 0.8 m with sea >= 1.2 m deep 12 m seaward
        cands = []
        for rad in np.arange(0.0, 70.0, 4.0):
            for a in np.linspace(0, 2 * math.pi, max(8, int(2 * math.pi * rad / 6.0)), endpoint=False):
                x = lm["x"] + math.cos(a) * rad
                y = lm["y"] + math.sin(a) * rad
                z = float(self.tz(x, y))
                if not (60.0 < z < 320.0):
                    continue
                # seaward direction = downhill
                gx = float(self.tz(x + 2.0, y) - self.tz(x - 2.0, y))
                gy = float(self.tz(x, y + 2.0) - self.tz(x, y - 2.0))
                dvec = unit([-gx, -gy])
                zw = self.tz(x + dvec[0] * np.array([6.0, 10.0, 16.0]), y + dvec[1] * np.array([6.0, 10.0, 16.0]))
                if zw[1] < -80.0 and zw[2] < -100.0:
                    cands.append((rad, x, y, dvec))
        pier = self.asset("SM_Prop_Pier_10m")
        boat = self.asset("SM_Prop_Boat_A")
        placed = []
        n = 0
        for rad, x, y, dvec in sorted(cands, key=lambda c: c[0]):
            if len(placed) >= piers:
                break
            if any(math.hypot(x - px, y - py) < 16.0 for px, py in placed):
                continue
            # pier pivot 5 m seaward of the shore end; deck at bank level
            zs = float(self.tz(x, y))
            yaw = yaw_facing(dvec[0], dvec[1])
            length = 2 if rng.random() < 0.5 else 1
            ok = True
            recs = []
            for k in range(length):
                c = np.array([x, y]) + dvec * (5.0 + 10.0 * k)
                o = obb_of(pier, c[0], c[1], yaw)
                if self.pindex.hit(o, 1.0) or self.index.hit(o, 1.0) or self.grid.hit_obb(o, 0.0, F_BUILD | F_WALL | F_LANDMARK):
                    ok = False
                    break
                recs.append((c, o))
            if not ok:
                continue
            for c, o in recs:
                self.commit(pier, float(c[0]), float(c[1]), zs - 5.0, yaw, "Props", obb=o, solid=False, tag="pier")
                n += 1
            placed.append((x, y))
            # boats moored beside the pier
            for side in (1.0, -1.0):
                if rng.random() < 0.55:
                    nrm = np.array([-dvec[1], dvec[0]]) * side
                    c = np.array([x, y]) + dvec * (6.0 + 3.0 * rng.random() + 10.0 * (length - 1)) + nrm * 3.2
                    zw = self.tz(c[0], c[1])
                    if float(zw) > -70.0:
                        continue
                    byaw = yaw_facing(dvec[0], dvec[1]) + (180.0 if rng.random() < 0.5 else 0.0) + float(rng.normal(0, 4))
                    o = obb_of(boat, c[0], c[1], byaw)
                    if self.pindex.hit(o, 0.5) or self.index.hit(o, 0.5):
                        continue
                    self.commit(boat, float(c[0]), float(c[1]), 0.0, byaw, "Props", obb=o, solid=False, tag="boat")
                    n += 1
        self.stats["piers"] = len(placed)
        return n


# ============================================================================================ palettes


def pal(G, *pairs):
    return [(G.kit[n], w) for n, w in pairs]


def asura_palettes(G):
    return dict(
        prime=pal(G, ("SM_Asura_Shop_A", 3.0), ("SM_Asura_House_L_A", 2.2), ("SM_Asura_House_L_B", 1.6),
                  ("SM_Asura_House_M_B", 2.0), ("SM_Asura_House_M_A", 1.6), ("SM_Asura_Inn_A", 1.0),
                  ("SM_Asura_House_S_B", 0.6), ("SM_Asura_Tower_A", 0.12)),
        street=pal(G, ("SM_Asura_House_M_A", 2.4), ("SM_Asura_House_M_B", 2.4), ("SM_Asura_House_S_A", 2.0),
                   ("SM_Asura_House_S_B", 2.0), ("SM_Asura_House_L_A", 1.3), ("SM_Asura_House_L_B", 0.9),
                   ("SM_Asura_Shop_A", 1.1), ("SM_Asura_Inn_A", 0.25), ("SM_Asura_Tower_A", 0.1)),
        lane=pal(G, ("SM_Asura_House_S_A", 3.0), ("SM_Asura_House_S_B", 3.0), ("SM_Asura_House_M_A", 2.0),
                 ("SM_Asura_House_M_B", 1.8), ("SM_Asura_House_L_A", 0.4), ("SM_Asura_Shop_A", 0.3)),
        noble=pal(G, ("SM_Noble_House_A", 3.0), ("SM_Noble_House_B", 3.0), ("SM_Noble_House_C", 2.6),
                  ("SM_Noble_Tower_A", 0.22), ("SM_Asura_Manor_A", 0.35), ("SM_Asura_House_L_B", 0.35)),
        market=pal(G, ("SM_Asura_Shop_A", 3.2), ("SM_Asura_Inn_A", 1.2), ("SM_Asura_House_L_A", 2.0),
                   ("SM_Asura_House_L_B", 1.4), ("SM_Asura_House_M_B", 1.8), ("SM_Asura_House_M_A", 1.2),
                   ("SM_Asura_Tower_A", 0.15)),
        core=pal(G, ("SM_Asura_House_M_A", 2.5), ("SM_Asura_House_M_B", 2.5), ("SM_Asura_House_L_A", 2.0),
                 ("SM_Asura_House_L_B", 1.3), ("SM_Asura_Shop_A", 1.6), ("SM_Asura_House_S_B", 0.7),
                 ("SM_Asura_House_S_A", 0.5), ("SM_Asura_Inn_A", 0.3), ("SM_Asura_Tower_A", 0.08)),
    )


def north_palettes(G):
    return dict(
        prime=pal(G, ("SM_North_Shop_A", 3.0), ("SM_North_House_B", 2.0), ("SM_North_House_A", 2.0),
                  ("SM_North_House_C", 1.0), ("SM_North_Tower_A", 0.15), ("SM_North_Workshop_A", 0.3)),
        street=pal(G, ("SM_North_House_A", 2.5), ("SM_North_House_B", 2.5), ("SM_North_House_C", 2.2),
                   ("SM_North_Shop_A", 1.0), ("SM_North_Workshop_A", 0.5), ("SM_North_Tower_A", 0.1)),
        lane=pal(G, ("SM_North_House_A", 2.5), ("SM_North_House_C", 2.5), ("SM_North_House_B", 1.5),
                 ("SM_North_Workshop_A", 0.7)),
        works=pal(G, ("SM_North_Workshop_A", 2.2), ("SM_North_House_C", 2.0), ("SM_North_House_A", 1.8),
                  ("SM_North_House_B", 1.4), ("SM_North_Tower_A", 0.1)),
    )


def millis_palettes(G):
    return dict(
        prime=pal(G, ("SM_Millis_Shop_A", 3.0), ("SM_Millis_House_B", 2.0), ("SM_Millis_House_A", 2.0),
                  ("SM_Millis_House_C", 1.5), ("SM_Millis_Tower_A", 0.14), ("SM_Millis_Chapel_A", 0.1)),
        street=pal(G, ("SM_Millis_House_A", 3.0), ("SM_Millis_House_C", 3.0), ("SM_Millis_House_B", 2.0),
                   ("SM_Millis_Shop_A", 1.0), ("SM_Millis_Tower_A", 0.07), ("SM_Millis_Chapel_A", 0.05)),
        lane=pal(G, ("SM_Millis_House_C", 3.0), ("SM_Millis_House_A", 2.4), ("SM_Millis_House_B", 1.2),
                 ("SM_Millis_Shop_A", 0.3)),
    )


def demon_palettes(G):
    return dict(
        prime=pal(G, ("SM_Demon_House_A", 3.0), ("SM_Demon_House_B", 2.4), ("SM_Demon_Hut_B", 1.0),
                  ("SM_Demon_Hut_C", 0.6), ("SM_Demon_Tower_A", 0.15)),
        street=pal(G, ("SM_Demon_House_A", 2.2), ("SM_Demon_House_B", 1.6), ("SM_Demon_Hut_A", 2.0),
                   ("SM_Demon_Hut_B", 2.0), ("SM_Demon_Hut_C", 1.6), ("SM_Demon_Tower_A", 0.1)),
        lane=pal(G, ("SM_Demon_Hut_A", 3.0), ("SM_Demon_Hut_B", 2.6), ("SM_Demon_Hut_C", 2.6),
                 ("SM_Demon_House_A", 0.8), ("SM_Demon_House_B", 0.6)),
    )


def desert_palettes(G):
    return dict(
        prime=pal(G, ("SM_Desert_Shop_A", 3.0), ("SM_Desert_House_A", 2.0), ("SM_Desert_House_C", 2.0),
                  ("SM_Desert_House_B", 1.4), ("SM_Desert_Inn_A", 0.5), ("SM_Desert_Tower_A", 0.14)),
        street=pal(G, ("SM_Desert_House_A", 3.0), ("SM_Desert_House_B", 3.0), ("SM_Desert_House_C", 2.5),
                   ("SM_Desert_Shop_A", 1.0), ("SM_Desert_Inn_A", 0.25), ("SM_Desert_Tower_A", 0.08)),
        lane=pal(G, ("SM_Desert_House_B", 3.0), ("SM_Desert_House_A", 2.6), ("SM_Desert_House_C", 2.0),
                 ("SM_Desert_Shop_A", 0.3)),
        inns=pal(G, ("SM_Desert_Inn_A", 1.6), ("SM_Desert_House_B", 2.4), ("SM_Desert_House_A", 2.0),
                 ("SM_Desert_House_C", 1.6), ("SM_Desert_Shop_A", 1.0), ("SM_Desert_Tower_A", 0.1)),
    )


def kind_class(kind):
    if kind in ("plaza", "main", "avenue"):
        return "prime"
    if kind in ("ring", "radial", "grid_main"):
        return "street"
    return "lane"


def standard_rules(b, q_edge=0.55, open_edge=(14.0, 34.0), gap_core=(0.9, 1.6), gap_edge=(2.0, 4.0), sb_core=(1.0, 1.8),
                   sb_edge=(1.4, 3.0), power=1.5, prime_open=0.25, run_core=(6, 12), run_edge=(2, 5)):
    """Frontage rules factory for the calibrated pass: continuous rows near the centre, rows broken by gardens (and
    wider gaps / setbacks) toward the walls; k scales the garden gaps."""
    def make(k):
        def rules(p, kind):
            th = math.atan2(p[1], p[0])
            rb = float(b.r_bound(np.array([th]))[0])
            sh = min(1.0, (math.hypot(p[0], p[1]) / max(rb, 1.0)) ** power)
            q = k * (0.02 + q_edge * sh)
            if kind in ("plaza", "main", "avenue", "shoprow"):
                q *= prime_open
            q = min(0.95, q)
            grow = (0.5 + sh) * (1.0 + 0.6 * k * sh)
            ol = (open_edge[0] * grow, open_edge[1] * grow)
            gap = (gap_core[0] + (gap_edge[0] - gap_core[0]) * sh, gap_core[1] + (gap_edge[1] - gap_core[1]) * sh)
            sb = (sb_core[0] + (sb_edge[0] - sb_core[0]) * sh, sb_core[1] + (sb_edge[1] - sb_core[1]) * sh)
            run = (int(round(run_core[0] + (run_edge[0] - run_core[0]) * sh)),
                   int(round(run_core[1] + (run_edge[1] - run_core[1]) * sh)))
            return q, ol, gap, sb, run
        return rules, b.jitter
    return make


def standard_keep(b, power=1.5, floor=0.22, prime_floor=0.85, zone_keep=None):
    """Density profile for thinning: keep 1 - alpha * t^power of the rows (t = radius / boundary), never below
    `floor` (no empty rings near the walls); prime frontage (plazas, main streets) keeps at least prime_floor."""
    def keep(p, kind, alpha):
        if zone_keep is not None:
            z = zone_keep(p, kind)
            if z is not None:
                return z
        th = math.atan2(p[1], p[0])
        rb = float(b.r_bound(np.array([th]))[0])
        t = min(1.0, math.hypot(p[0], p[1]) / max(rb, 1.0))
        k = max(floor, 1.0 - alpha * t ** power)
        if kind in ("plaza", "main", "avenue", "shoprow"):
            k = max(k, prime_floor)
        return k
    return keep


# ============================================================================================ style builders


def target_for(b):
    # Nanite + instancing + HLODs make dense cities cheap: capitals feel like capitals.
    tbl = dict(Ars=1400, Sharia=620, Millishion=900, Rapan=600, Rikarisu=260, Roa=240, KingDragon=220, EastPort=150,
               Wenport=170, ZantPort=160, WestPort=150, Buena=42, SwordSanctuary=26)
    return tbl.get(b.id, 100)


def budget_range(b):
    if b.id == "Ars":
        return 900, 1500
    if b.style == "MillisCapital":
        return 550, 950
    if b.style in ("NorthernCity", "DesertCity"):
        return 380, 650
    if b.style in ("RuralVillage", "NorthernVillage"):
        return 15, 50
    if b.style == "LabyrinthGate":
        return 0, 0
    return 90, 270


def build_asura_capital(b):
    """Ars: Asura walls and gates, the Silver Palace on its rise with the noble quarter around it and along the
    processional avenue, the Royal Market and the Hall of Government in the market ring, dense wards outside."""
    G = b.G
    P = asura_palettes(G)
    b.jitter = 1.2
    pal_lm = b.lm("Silver Palace")
    rm = b.lm("Royal Market")
    hall = b.lm("Hall of Government")
    face = np.array([math.cos(math.radians(pal_lm["yaw"])), math.sin(math.radians(pal_lm["yaw"]))])
    b.place_fixed("SM_Landmark_SilverPalace", pal_lm["x"], pal_lm["y"], pal_lm["yaw"], "Landmark", margin=4.0, sink=30.0)
    # the generator's mound: flat top of r, smootherstep ramp of 1.6 x 46 m; < 0.6 m of it is left at ~r + 60 m
    mound_r = pal_lm["r"] + 57.0
    ward_r = mound_r + 5.0                  # white Noble wall round the foot of the mound
    b.grid.paint_circle(pal_lm["x"], pal_lm["y"], ward_r + 8.0, F_KEEPIN)
    ph = b.place_fixed("SM_Noble_Hall_A", hall["x"], hall["y"], hall["yaw"], "Buildings", margin=5.0, sink=30.0,
                       fit=(12.0, 10.0))
    hall = dict(hall, x=ph.x, y=ph.y, yaw=ph.yaw + 90.0)
    b.collect_spawns()
    b.build_walls("Asura", 432.0)
    palace_ward_wall(b, pal_lm["x"], pal_lm["y"], ward_r, face)
    central = b.add_plaza(0.0, 0.0, 40.0, "central", "cobble")
    market = b.add_plaza(rm["x"], rm["y"], rm["r"], "market", "cobble")
    hf = np.array([math.cos(math.radians(hall["yaw"])), math.sin(math.radians(hall["yaw"]))])
    hall_a = G.kit["SM_Noble_Hall_A"]
    fc = np.array([hall["x"], hall["y"]]) + hf * (hall_a.hy + 13.0)
    b.add_plaza(float(fc[0]), float(fc[1]), 15.0, "forecourt", "cobble")
    stair = np.array([pal_lm["x"], pal_lm["y"]]) + face * 47.0
    fore = stair + face * 16.0
    b.add_plaza(float(fore[0]), float(fore[1]), 20.0, "forecourt", "cobble", frontage=False)
    for g in b.gates:
        c = np.array([g["x"], g["y"]]) - g["ey"] * 22.0
        b.add_plaza(float(c[0]), float(c[1]), 13.0, "gate", "cobble")
    b.clear_spawns("cobble")
    b.main_streets(4.6, "cobble")
    av0 = unit(stair) * 38.0
    b.add_street(np.array([av0, stair + face * 4.0]), 7.0, "avenue", "cobble", 1.0, clip=False, par_check=False)
    b.connector(float(fc[0]), float(fc[1]), 0.0, 0.0, 4.0, "avenue", "cobble")
    axis = unit(stair)
    side = np.array([-axis[1], axis[0]])
    # noble gardens flanking the avenue and around the foot of the palace mound
    b.gardens = []
    spots = [axis * t + side * sgn * 36.0 for t in (100.0, 175.0, 250.0) for sgn in (1.0, -1.0)]
    for a in np.radians(np.arange(0, 360, 36)):
        spots.append(np.array([pal_lm["x"], pal_lm["y"]]) + np.array([math.cos(a), math.sin(a)]) * (ward_r + 36.0))
    for c in spots:
        if not b.grid.any_in_circle(c[0], c[1], 15.0, F_WATER | F_LANDMARK | F_KEEPIN | F_OUTSIDE | F_PLAZA | F_WALL | F_RESERVED):
            b.gardens.append(b.add_plaza(float(c[0]), float(c[1]), 12.0, "garden", None))
            b.grid.paint_circle(c[0], c[1], 12.0, F_RESERVED)
    # ring road around the palace mound (noble mansions face it)
    th = np.linspace(0, 2 * math.pi, 181)[:-1]
    rr = ward_r + 13.0
    ring_pts = np.stack([pal_lm["x"] + np.cos(th) * rr, pal_lm["y"] + np.sin(th) * rr], 1)
    b.add_street(ring_pts, 3.5, "ring", "cobble", 2.5, closed=True, par_check=False,
                 block=F_WATER | F_LANDMARK | F_OUTSIDE | F_WALL | F_RESERVED)
    b.wall_lane(2.5, "dirt")
    d_pal = lambda p: math.hypot(p[0] - pal_lm["x"], p[1] - pal_lm["y"])

    def is_noble(p):
        d_av = float(seg_dist(p[0], p[1], 0.0, 0.0, stair[0], stair[1]))
        return d_pal(p) < mound_r + 95.0 or (d_av < 80.0 and math.hypot(p[0], p[1]) > 42.0)

    # street-direction field: radial around the centre, aligned with the roads and the avenue, rings around the
    # palace mound and the Royal Market
    F = TensorField(b.half, 4.0)
    F.radial(0.0, 0.0, lambda d: np.ones_like(d))
    for st in b.streets:
        if st.kind in ("main", "avenue"):
            F.polyline(st.line.p, 45.0, 2.5)
    F.radial(pal_lm["x"], pal_lm["y"], lambda d: 3.0 * np.exp(-((d - mound_r) / 80.0) ** 2))
    F.radial(rm["x"], rm["y"], lambda d: 1.2 * np.exp(-((d - rm["r"]) / 30.0) ** 2))
    F.finish(field_noise(b.rng, 12.0, 280.0))

    def tfrac(x, y):
        rb = float(b.r_bound(np.array([math.atan2(y, x)]))[0])
        return min(1.0, math.hypot(x, y) / max(rb, 1.0))

    def dsep(x, y, fam):
        t = tfrac(x, y)
        d = (40.0 + 90.0 * t ** 1.3) if fam == 0 else (34.0 + 95.0 * t ** 1.6)
        if is_noble((x, y)):
            d = max(d * 1.4, 60.0)
        return d

    def kinds(line, fam, L):
        rm_ = float(np.mean(np.hypot(line[:, 0], line[:, 1])))
        tf = tfrac(float(np.mean(line[:, 0])), float(np.mean(line[:, 1])))
        if fam == 1 and L > 160 and tf < 0.75:
            return "ring", 3.2, "cobble", 3.0 + rm_ / 1000.0
        if tf < 0.58:
            return "street", 2.8, "cobble", 5.0 + rm_ / 1000.0
        return "lane", 2.3, "dirt", 6.0 + rm_ / 1000.0
    b.streamline_network(F, dsep, kinds, t_stop=lambda x, y: 0.06 + 0.45 * tfrac(x, y) ** 2)
    b.prune_disconnected([(0.0, 0.0)])

    def zone(p):
        if is_noble(p):
            return "noble"
        r = math.hypot(p[0], p[1])
        if math.hypot(p[0] - rm["x"], p[1] - rm["y"]) < 85 or math.hypot(p[0] - hall["x"], p[1] - hall["y"]) < 70:
            return "market"
        if r < 153:
            return "core"
        if r < 288:
            return "ring"
        return "ward"

    def palette(p, kind):
        z = zone(p)
        if z == "noble":
            return P["noble"]
        if z == "market" and kind in ("plaza", "main", "ring", "street"):
            return P["market"]
        if kind in ("plaza", "main", "avenue"):
            return P["prime"]
        if z == "core":
            return P["core"]
        if z == "ring":
            return P["street"]
        return P["lane"]
    b.palette = palette
    base = standard_rules(b, q_edge=0.75, open_edge=(14.0, 34.0), power=2.0)

    def make(k):
        rules, jit = base(k)

        def rules2(p, kind):
            if zone(p) == "noble":
                return min(0.8, 0.3 + 0.25 * k), (12.0, 26.0), (3.0, 6.0), (2.0, 4.0), (1, 3)
            return rules(p, kind)
        return rules2, jit
    lo, hi = budget_range(b)
    keep = standard_keep(b, power=1.6, floor=0.24,
                         zone_keep=lambda p, kind: (0.55 if kind in ("garden", "avenue", "ring") else 0.4) if zone(p) == "noble" else None)
    b.calibrate(target_for(b), lo, hi, make, keep_fn=keep)
    # props
    b.plaza_centrepiece(central, "fountain", benches=8, statues=2)
    for gd in b.gardens:
        b.plaza_centrepiece(gd, "fountain" if b.rng.random() < 0.5 else "statue", benches=4, statues=0, lamps=True)
    b.market_stalls(market, ["SM_Prop_MarketStall_A", "SM_Prop_MarketStall_B", "SM_Prop_MarketStall_C"],
                    rows_axis=b.street_axis_near(rm["x"], rm["y"]))
    for pl in b.plazas:
        if pl.kind == "forecourt":
            b.plaza_centrepiece(pl, "statue", benches=2, statues=0, lamps=True)
        if pl.kind == "gate":
            b.plaza_centrepiece(pl, "none", benches=0, statues=0, lamps=True)
    b.lamps_along(("main", "avenue", "ring"), spacing=24.0)
    for t in np.arange(70.0, float(np.hypot(*stair)) - 80.0, 45.0):
        for sgn in (1.0, -1.0):
            q = axis * t + side * sgn * 9.0
            b.try_prop("SM_Prop_Statue_A", float(q[0]), float(q[1]), yaw_facing(-side[0] * sgn, -side[1] * sgn), clear=0.3)
    b.gate_props()
    b.shopfront_goods()


def organic_streets(b, dsep0, dsep1, noise_deg=12.0, wavelength=260.0, extra_fields=None, t_stop=(0.06, 0.45),
                    hw=(3.2, 2.8, 2.3), mats=("cobble", "cobble", "dirt"), lane_from=0.58, ring_len=160.0,
                    dsep_mod=None, center=(0.0, 0.0), road_sigma=45.0):
    """Street network from a tensor field: radial around `center` (ring + radial families), aligned with the main
    streets and avenues, radial around squares; evenly spaced streamlines with spacing dsep0(t) / dsep1(t) for the
    radial / ring family (t = radius / boundary radius). Returns tfrac(x, y)."""
    F = TensorField(b.half, 4.0)
    F.radial(center[0], center[1], lambda d: np.ones_like(d))
    for st in b.streets:
        if st.kind in ("main", "avenue"):
            F.polyline(st.line.p, road_sigma, 2.5)
    for pl in b.plazas:
        if pl.kind in ("central", "market", "forecourt", "square") and pl.poly is None and pl.r >= 12:
            F.radial(pl.x, pl.y, lambda d, r=pl.r: 1.2 * np.exp(-((d - r) / 30.0) ** 2))
    if extra_fields:
        extra_fields(F)
    F.finish(field_noise(b.rng, noise_deg, wavelength) if noise_deg > 0 else None)

    def tfrac(x, y):
        rb = float(b.r_bound(np.array([math.atan2(y, x)]))[0])
        return min(1.0, math.hypot(x, y) / max(rb, 1.0))

    def dsep(x, y, fam):
        t = tfrac(x, y)
        d = dsep0(t) if fam == 0 else dsep1(t)
        return dsep_mod(x, y, fam, d) if dsep_mod else d

    def kinds(line, fam, L):
        rm_ = float(np.mean(np.hypot(line[:, 0], line[:, 1])))
        tf = tfrac(float(np.mean(line[:, 0])), float(np.mean(line[:, 1])))
        if fam == 1 and L > ring_len and tf < 0.75:
            return "ring", hw[0], mats[0], 3.0 + rm_ / 1000.0
        if tf < lane_from:
            return "street", hw[1], mats[1], 5.0 + rm_ / 1000.0
        return "lane", hw[2], mats[2], 6.0 + rm_ / 1000.0
    b.streamline_network(F, dsep, kinds, t_stop=lambda x, y: t_stop[0] + t_stop[1] * tfrac(x, y) ** 2)
    return tfrac


def wall_radius(b, default_frac):
    for d in b.site["Districts"]:
        if "Wall" in d["Name"]:
            return 0.5 * (d["InnerRadiusCm"] + d["OuterRadiusCm"]) / 100.0
    return default_frac * b.R


def gate_squares(b, r=12.0, back=21.0, mat="cobble"):
    for g in b.gates:
        c = np.array([g["x"], g["y"]]) - g["ey"] * back
        b.add_plaza(float(c[0]), float(c[1]), r, "gate", mat)


def door_link(b, p, to, hw=3.2, kind="radial", mat="cobble", fore=8.0, plaza=True):
    """Forecourt in front of a building's door and a street from it to the network."""
    f = np.array([-math.sin(math.radians(p.yaw)), math.cos(math.radians(p.yaw))])
    door = np.array([p.x, p.y]) + f * (p.asset.y1 + 2.5)
    if plaza:
        c = door + f * (fore - 2.0)
        b.add_plaza(float(c[0]), float(c[1]), fore, "forecourt", mat, frontage=False)
    b.connector(float(door[0] + f[0] * fore), float(door[1] + f[1] * fore), to[0], to[1], hw, kind, mat)
    return door


def harbour_square(b, mat="cobble"):
    d = b.lm("Harbour Docks")
    if d is None:
        return None
    dp = np.array([d["x"], d["y"]])
    # the square sits on dry land between the town centre and the docks
    best = None
    for f in np.linspace(0.2, 0.8, 13):
        c = dp * f
        if not b.grid.any_in_circle(c[0], c[1], 16.0, F_WATER | F_SHORE | F_LANDMARK | F_WALL):
            best = c
    if best is None:
        return None
    return b.add_plaza(float(best[0]), float(best[1]), 14.0, "market", mat)


def palace_ward_wall(b, cx, cy, r, face, prefix="Noble"):
    """Inner wall ring (exactly-12 m segments between towers) around a palace precinct, gate toward `face`."""
    seg = b.asset("SM_%s_Wall_12m" % prefix)
    tower = b.asset("SM_%s_WallTower" % prefix)
    gate = b.asset("SM_%s_Gate" % prefix)
    th = np.linspace(0.0, 2 * math.pi, 721)[:-1]
    curve = Line(np.stack([cx + np.cos(th) * r, cy + np.sin(th) * r], 1), closed=True)
    P = np.array([cx, cy]) + face * r
    s, _ = curve.project(P)
    b.wall_center = (cx, cy)
    b._chain(curve, [dict(s=s, x=float(P[0]), y=float(P[1]), ey=face, road="Avenue", roads=["Avenue"], w=14.0,
                          inner=True)], seg, tower, gate, prefix)
    b.wall_center = (0.0, 0.0)


def build_asura_town(b):
    """Roa / King Dragon / East Port: Asura walls and gates, a keep by the market square, the Boreas manor (Roa) or
    the Dragon King castle ward (King Dragon), harbour docks (East Port), Asura houses and shops."""
    G = b.G
    P = asura_palettes(G)
    b.jitter = 1.5
    b.collect_spawns()
    mk = b.lm("Roa Market")
    market_c = (mk["x"], mk["y"]) if mk else (0.0, 0.0)
    market_r = max(16.0, mk["r"]) if mk else 16.0
    doors = []
    if b.id == "Roa":
        man = b.lm("Boreas Manor")
        doors.append(b.place_fixed("SM_Asura_Manor_A", man["x"], man["y"], man["yaw"], "Buildings", margin=5.0, fit=(15.0, 15.0)))
        doors.append(place_keep_by_market(b, "SM_Landmark_RoaKeep", "Landmark", market_c, market_r))
    elif b.id == "KingDragon":
        castle_compound(b, b.lm("Dragon King Castle"), "Asura", keep="SM_Asura_Keep_A",
                        inner=("SM_Asura_Tower_A", "SM_Asura_Tower_A", "SM_Asura_House_L_B"))
    else:
        doors.append(place_keep_by_market(b, "SM_Asura_Keep_A", "Buildings", market_c, market_r))
    b.build_walls("Asura", wall_radius(b, 0.94))
    central = b.add_plaza(market_c[0], market_c[1], market_r, "market", "cobble")
    gate_squares(b, 11.0, 20.0)
    hs = harbour_square(b)
    b.clear_spawns("cobble")
    mains = b.main_streets(4.0, "cobble")
    for p in doors:
        if p is not None:
            door_link(b, p, market_c)
    if b.id == "KingDragon":
        cg = b.compound_gate
        b.connector(cg[0], cg[1], market_c[0], market_c[1], 3.4, "radial", "cobble")
    if hs is not None:
        b.connector(hs.x, hs.y, market_c[0], market_c[1], 3.2, "radial", "cobble")
    b.wall_lane(2.3, "dirt")
    organic_streets(b, lambda t: 36.0 + 62.0 * t ** 1.3, lambda t: 32.0 + 70.0 * t ** 1.5, noise_deg=14.0,
                    hw=(3.0, 2.6, 2.2), mats=("cobble", "dirt", "dirt"), center=market_c, ring_len=120.0)
    b.prune_disconnected([market_c])
    b.palette = lambda p, kind: P["market"] if math.hypot(p[0] - market_c[0], p[1] - market_c[1]) < 45 and kind in (
        "plaza", "main", "ring", "street") else P[kind_class(kind)]
    make = standard_rules(b, power=1.6)
    keep = standard_keep(b, power=1.5, floor=0.3)
    lo, hi = budget_range(b)
    b.calibrate(target_for(b), lo, hi, make, keep_fn=keep)
    b.market_stalls(central, ["SM_Prop_MarketStall_A", "SM_Prop_MarketStall_B", "SM_Prop_MarketStall_C"],
                    rows_axis=b.street_axis_near(market_c[0], market_c[1]), avoid_center=4.0)
    spot = b.free_spot(market_c[0], market_c[1], 1.8, 8.0)
    if spot is not None:
        b.try_prop("SM_Rural_Well", spot[0], spot[1], 0.0, block=F_BUILD | F_WALL | F_LANDMARK | F_WATER | F_STREET)
    for pl in b.plazas:
        if pl.kind in ("gate", "forecourt"):
            b.plaza_centrepiece(pl, "none", benches=0, statues=0, lamps=True)
    b.lamps_along(("main", "ring"), spacing=26.0)
    b.gate_props()
    b.shopfront_goods()
    if b.lm("Harbour Docks"):
        if hs is not None:
            b.market_stalls(hs, ["SM_Prop_MarketStall_A", "SM_Prop_MarketStall_C"], fill=0.6, avoid_center=3.0)
        b.harbour()


def place_keep_by_market(b, name, cls, market_c, market_r):
    """Keep beside the market square, facing it, on the side away from the roads."""
    a = b.asset(name)
    roads = b._roads_local()
    best = None
    for ang in np.arange(0, 360, 7.5):
        u = np.array([math.cos(math.radians(ang)), math.sin(math.radians(ang))])
        dist = market_r + 10.0 + a.hy
        c = np.array(market_c) + u * dist
        yaw = yaw_facing(-u[0], -u[1])
        o = obb_of(a, c[0], c[1], yaw, 1.0, 6.0)
        if b.grid.hit_obb(o, 0.0, F_WATER | F_SHORE | F_LANDMARK | F_BUILD | F_ROAD):
            continue
        zmin, zmax = b.footprint_z(o, 4.0)
        dmin = 1e9
        for road, p in roads:
            for q0, q1 in zip(p[:-1], p[1:]):
                if min(math.hypot(*q0), math.hypot(*q1)) > b.R * 1.2:
                    continue
                dmin = min(dmin, float(seg_dist(c[0], c[1], q0[0], q0[1], q1[0], q1[1])))
        clear = dmin - (a.hx + a.hy) * 0.75
        score = min(clear, 30.0) - 3.0 * (zmax - zmin) / 100.0
        if best is None or score > best[0]:
            best = (score, c, u)
    if best is None:
        return None
    _, c, u = best
    return b.place_fixed(name, float(c[0]), float(c[1]), ang_of(-u), cls, margin=5.0)


def castle_compound(b, lm, prefix, keep, inner=()):
    """A square ward of city-wall pieces (4 segments a side) with corner towers, a gate facing the town centre, a keep
    in the middle and a few buildings fitted inside."""
    seg = b.asset("SM_%s_Wall_12m" % prefix)
    tower = b.asset("SM_%s_WallTower" % prefix)
    gate = b.asset("SM_%s_Gate" % prefix)
    c = np.array([lm["x"], lm["y"]])
    u = unit(-c) if np.hypot(*c) > 1 else np.array([0.0, 1.0])       # toward the centre = front
    v = np.array([u[1], -u[0]])
    half = 24.0
    corners = [c + (su * u + sv * v) * half for su, sv in ((1, 1), (1, -1), (-1, -1), (-1, 1))]
    for k in range(4):
        P, Q = corners[k], corners[(k + 1) % 4]
        d = unit(Q - P)
        for i in range(4):
            if k == 0 and i == 1:
                continue                     # the gate replaces this segment
            C = P + d * (6.0 + 12.0 * i)
            nrm = np.array([-d[1], d[0]])
            yaw = ang_of(d) + (0.0 if np.dot(nrm, C - c) > 0 else 180.0)
            o = obb_of(seg, C[0], C[1], yaw)
            zmin, _ = b.footprint_z(o, 3.0)
            b.commit(seg, float(C[0]), float(C[1]), zmin - 20.0, yaw, "Walls", obb=o, tag="compound")
            b.grid.paint_obb(o, 1.5, F_LANDMARK)
    P, Q = corners[0], corners[1]
    G_ = P + unit(Q - P) * 18.0
    yaw = yaw_facing(u[0], u[1])
    o = obb_of(gate, G_[0], G_[1], yaw)
    zmin, _ = b.footprint_z(o, 3.0)
    b.commit(gate, float(G_[0]), float(G_[1]), zmin - 20.0, yaw, "Walls", obb=o, tag="compound")
    b.grid.paint_obb(o, 1.5, F_LANDMARK)
    for k in range(4):
        V = corners[k]
        ty = ang_of(u) + 45.0
        o = obb_of(tower, V[0], V[1], ty)
        zmin, _ = b.footprint_z(o, 2.5)
        b.commit(tower, float(V[0]), float(V[1]), zmin - 20.0, ty, "Walls", obb=o, tag="compound")
        b.grid.paint_obb(o, 1.5, F_LANDMARK)
    b.grid.paint_poly(np.array(corners), F_LANDMARK)
    placed = []
    ka = b.asset(keep)
    kc = c - u * 3.0
    o = obb_of(ka, kc[0], kc[1], yaw)
    zmin, _ = b.footprint_z(o, 3.0)
    placed.append(b.commit(ka, float(kc[0]), float(kc[1]), zmin - 30.0, yaw, "Buildings", obb=o, tag="compound"))
    inner_half = half - 2.9 - 1.2           # inner wall face + clearance
    for nm in inner:
        a = b.asset(nm)
        done = False
        for fy in (-1.0, 1.0):
            for fx in (1.0, -1.0):
                for rot in (0.0, 90.0):
                    ww, dd = (a.hx, a.hy) if rot == 0.0 else (a.hy, a.hx)
                    p = c + v * fx * (inner_half - ww) + u * fy * (inner_half - dd)
                    if fy > 0 and abs(np.dot(p - G_, v)) < 12.0:
                        continue                 # keep the gate passage clear
                    yy = yaw_facing(-v[0] * fx, -v[1] * fx) if rot == 90.0 else yaw
                    o = obb_of(a, p[0], p[1], yy)
                    if any(obb_overlap(o, q.obb, 1.0) for q in placed):
                        continue
                    cs = obb_corners(o)
                    if max(abs(np.dot(np.array(q) - c, u)) for q in cs) > inner_half + 0.05 or \
                            max(abs(np.dot(np.array(q) - c, v)) for q in cs) > inner_half + 0.05:
                        continue
                    zmin, _ = b.footprint_z(o, 3.0)
                    placed.append(b.commit(a, float(p[0]), float(p[1]), zmin - 30.0, yy, "Buildings", obb=o, tag="compound"))
                    done = True
                    break
                if done:
                    break
            if done:
                break
    gp = G_ + u * 10.0
    b.compound_gate = (float(gp[0]), float(gp[1]))
    b.add_plaza(float(gp[0] + u[0] * 4), float(gp[1] + u[1] * 4), 9.0, "forecourt", "cobble", frontage=False)


def build_north_city(b):
    """Sharia: North walls, Ranoa University dominating the north, the adventurer guild, a row of magic shops,
    workshops in the outer district, towers."""
    G = b.G
    P = north_palettes(G)
    b.jitter = 1.8
    uni = b.lm("Ranoa University of Magic")
    guild = b.lm("Adventurer Guild")
    shops = b.lm("Magic Shops Row")
    b.collect_spawns()
    pu = b.place_fixed("SM_Landmark_RanoaUniversity", uni["x"], uni["y"], uni["yaw"], "Landmark", margin=5.0,
                       fit=(64.0, 12.0, 0.025))
    pg = b.place_fixed("SM_North_Guild_A", guild["x"], guild["y"], guild["yaw"], "Buildings", margin=4.0, fit=(12.0, 15.0))
    b.build_walls("North", wall_radius(b, 0.97))
    central = b.add_plaza(0.0, 0.0, 30.0, "central", "cobble")
    gate_squares(b, 12.0, 22.0)
    b.clear_spawns("cobble")
    mains = b.main_streets(4.0, "cobble")
    b.boulevard_between_mains(mains, 26.0)
    uf = np.array([-math.sin(math.radians(pu.yaw)), math.cos(math.radians(pu.yaw))])
    ufc = np.array([pu.x, pu.y]) + uf * (G.kit["SM_Landmark_RanoaUniversity"].y1 + 17.0)
    b.add_plaza(float(ufc[0]), float(ufc[1]), 16.0, "forecourt", "cobble")
    b.add_street(np.array([unit(ufc) * 29.0, ufc]), 5.0, "avenue", "cobble", 1.0, clip=False, par_check=False)
    door_link(b, pg, (0.0, 0.0), hw=3.2, kind="radial", fore=8.0)
    sc = np.array([shops["x"], shops["y"]])
    tdir = np.array([-sc[1], sc[0]]) / max(1.0, np.hypot(*sc))
    b.add_street(np.array([sc - tdir * 34.0, sc + tdir * 34.0]), 3.4, "shoprow", "cobble", 0.8, par_check=False,
                 clip=True)
    b.connector(float(sc[0]), float(sc[1]), 0.0, 0.0, 3.2, "radial", "cobble")
    b.wall_lane(2.4, "dirt")
    organic_streets(b, lambda t: 38.0 + 70.0 * t ** 1.3, lambda t: 33.0 + 85.0 * t ** 1.6, noise_deg=12.0,
                    hw=(3.2, 2.8, 2.3), mats=("cobble", "cobble", "dirt"), ring_len=150.0)
    b.prune_disconnected([(0.0, 0.0)])
    shop_a = G.kit["SM_North_Shop_A"]

    def palette(p, kind):
        if kind == "shoprow":
            return [(shop_a, 1.0)]
        r = math.hypot(*p)
        if r > 150 and kind not in ("plaza", "main", "avenue"):
            return P["works"]
        return P[kind_class(kind)]
    b.palette = palette
    make = standard_rules(b, power=1.6)
    keep = standard_keep(b, power=1.5, floor=0.26)
    lo, hi = budget_range(b)
    b.calibrate(target_for(b), lo, hi, make, keep_fn=keep)
    b.plaza_centrepiece(central, "fountain", benches=6, statues=1)
    for pl in b.plazas:
        if pl.kind == "forecourt":
            b.plaza_centrepiece(pl, "statue" if pl.r > 12 else "none", benches=2 if pl.r > 12 else 0, statues=0, lamps=True)
        if pl.kind == "gate":
            b.plaza_centrepiece(pl, "none", benches=0, statues=0, lamps=True)
    b.lamps_along(("main", "avenue", "ring", "shoprow"), spacing=24.0)
    b.gate_props()
    b.shopfront_goods()


def build_demon(b):
    """Rikarisu (crater town) / Wenport (port): Demon walls, the great hall, dense irregular streets of huts, carved
    houses and tower-houses, demon stalls and totems."""
    G = b.G
    P = demon_palettes(G)
    b.jitter = 7.0
    b.collect_spawns()
    hall = b.lm("Adventurer Hall")
    ph = None
    if b.id == "Rikarisu" and hall:
        ph = b.place_fixed("SM_Landmark_RikarisuHall", hall["x"], hall["y"], hall["yaw"], "Landmark", margin=4.0,
                           fit=(18.0, 15.0))
    # Rikarisu: the wall rings the "Dense Streets" district; the "Rock Dwellings" stay outside it in the crater
    b.build_walls("Demon", 0.80 * b.R if b.style == "DemonTown" else 0.93 * b.R)
    mk = b.lm("Crater Market")
    mc = (mk["x"], mk["y"]) if mk else (0.0, 0.0)
    central = b.add_plaza(mc[0], mc[1], (mk["r"] if mk else 18.0), "market", "cobble")
    gate_squares(b, 11.0, 20.0)
    hs = harbour_square(b)
    b.clear_spawns("cobble")
    mains = b.main_streets(3.6, "cobble")
    if ph is not None:
        door_link(b, ph, mc, hw=3.6, kind="avenue", fore=11.0)
    if hs is not None:
        b.connector(hs.x, hs.y, mc[0], mc[1], 3.2, "radial", "cobble")
    b.wall_lane(2.2, "dirt")
    organic_streets(b, lambda t: 24.0 + 70.0 * t ** 1.5, lambda t: 21.0 + 70.0 * t ** 1.6, noise_deg=30.0,
                    wavelength=150.0, t_stop=(0.2, 0.35), hw=(2.5, 2.2, 1.9), mats=("dirt", "dirt", "dirt"), center=mc,
                    ring_len=90.0, lane_from=0.45)
    b.prune_disconnected([mc])

    def palette(p, kind):
        r = math.hypot(p[0] - mc[0], p[1] - mc[1]) / max(1.0, b.R)
        if kind in ("plaza", "main", "avenue") and r < 0.6:
            return P["prime"]
        if r > 0.75:
            return P["lane"]
        return P[kind_class(kind)]
    b.palette = palette
    make = standard_rules(b, power=1.4, gap_core=(1.0, 2.0), gap_edge=(1.8, 3.6))
    keep = standard_keep(b, power=2.2, floor=0.15)
    lo, hi = budget_range(b)
    outside = 26 if b.style == "DemonTown" else 0
    b.calibrate(target_for(b) - outside, lo, hi, make, keep_fn=keep)
    if outside:
        rock_dwellings(b, P, outside)
    b.market_stalls(central, ["SM_Demon_Stall_A"], spacing=5.2, aisle=6.2, fill=1.0,
                    extras=("SM_Prop_Crates_A", "SM_Prop_Barrels_A"), avoid_center=6.0)
    spot = b.free_spot(mc[0], mc[1], 1.6, 8.0)
    if spot is not None:
        b.try_prop("SM_Demon_Totem_A", spot[0], spot[1], float(b.rng.uniform(0, 360)),
                   block=F_BUILD | F_WALL | F_LANDMARK | F_WATER | F_STREET)
    for pl in b.plazas:
        if pl.kind in ("gate", "forecourt", "market"):
            for k in range(3):
                a = b.rng.uniform(0, 2 * math.pi)
                x, y = pl.x + math.cos(a) * (pl.r - 1.5), pl.y + math.sin(a) * (pl.r - 1.5)
                b.try_prop("SM_Demon_Totem_A", x, y, float(b.rng.uniform(0, 360)), clear=0.3,
                           block=F_BUILD | F_WALL | F_LANDMARK | F_WATER | F_STREET)
    for st in b.streets:
        if st.kind in ("main", "ring", "street") and b.rng.random() < 0.5:
            s = float(b.rng.uniform(0.2, 0.8)) * st.line.L
            p = st.line.at(s)
            t = st.line.tangent(s)
            q = p + np.array([-t[1], t[0]]) * (st.hw + 1.2)
            b.try_prop("SM_Demon_Totem_A", float(q[0]), float(q[1]), float(b.rng.uniform(0, 360)), clear=0.2)
    b.gate_props()
    b.shopfront_goods(("House_A", "House_B", "Hut_A", "Hut_B", "Hut_C"), 0.3)
    if hs is not None:
        b.market_stalls(hs, ["SM_Demon_Stall_A"], spacing=5.4, aisle=6.5, fill=0.6, avoid_center=3.0)
    if b.lm("Harbour Docks"):
        b.harbour()


def rock_dwellings(b, P, n_max):
    """Huts in clusters along a dirt path ringing the town outside its wall (Rikarisu's rock dwellings)."""
    if b.wall_r is None:
        return 0
    th = np.linspace(-math.pi, math.pi, 541)[:-1]
    rw = b.wall_r(th)
    r = rw + 24.0
    pts = np.stack([np.cos(th) * r, np.sin(th) * r], 1)
    # open the crater floor beyond the wall band for building
    g = b.grid
    xs = g.centres(0, g.n)
    for j0 in range(0, g.n, 256):
        j1 = min(g.n, j0 + 256)
        X, Y = np.meshgrid(xs, xs[j0:j1])
        far = np.hypot(X, Y) > b.wall_r(np.arctan2(Y, X)) + 8.0
        sub = g.flags[j0:j1]
        sub[far] &= np.uint16(0xFFFF ^ F_OUTSIDE)
    sts = b.add_street(pts, 1.9, "path", "dirt", 8.0, closed=True, par_check=False, min_len=30.0,
                       block=F_WATER | F_LANDMARK | F_WALL | F_KEEPIN)
    huts = pal(b.G, ("SM_Demon_Hut_A", 3.0), ("SM_Demon_Hut_B", 3.0), ("SM_Demon_Hut_C", 3.0), ("SM_Demon_House_B", 0.4))
    old_pal = b.palette
    b.palette = lambda p, kind: huts
    b.frng = np.random.default_rng([SEED, b.crc, 53])
    n0 = len(b.items)
    for st in sts:
        for side in (1.0, -1.0):
            b.frontage_line(st.line, st.hw, side, "path",
                            lambda p, kind: (0.55, (18.0, 46.0), (1.2, 3.0), (1.5, 3.5), (2, 4)), 9.0)
    new = [p for p in b.items[n0:] if p.cls == "Buildings"]
    if len(new) > n_max:
        order = b.frng.permutation(len(new))
        extra = set(id(new[k]) for k in order[n_max:])
        b.items = [p for p in b.items if id(p) not in extra]
        b.index = ObbIndex(24.0)
        for p in b.items:
            if p.cls != "Props":
                b.index.add(p.obb, p)
        b.grid.flags &= np.uint16(0xFFFF ^ F_BUILD)
        for p in b.items:
            if p.cls in ("Buildings", "Landmark"):
                b.grid.paint_obb(p.obb, 0.0, F_BUILD)
    b.palette = old_pal
    return min(len(new), n_max)


def build_millis(b):
    """Millishion / Zant Port / West Port: white Millis walls, an orderly grid of streets and square plazas, the
    cathedral on its square, the Holy Knights ward, chapels and towers; piers at the ports."""
    G = b.G
    P = millis_palettes(G)
    b.jitter = 0.4
    b.collect_spawns()
    cat = b.lm("Holy Cathedral")
    keep = b.lm("Holy Knights Keep")
    prom = b.lm("Lakeside Promenade")
    if cat:
        b.place_fixed("SM_Landmark_MillisCathedral", cat["x"], cat["y"], cat["yaw"], "Landmark", margin=5.0)
    if keep:
        castle_compound(b, keep, "Millis", keep="SM_Millis_Tower_A", inner=("SM_Millis_Chapel_A", "SM_Millis_House_B"))
    if prom:
        b.grid.paint_circle(prom["x"], prom["y"], prom["r"] + 2.0, F_KEEPIN)
    b.build_walls("Millis", wall_radius(b, 0.95))
    if prom:
        b.grid.flags &= np.uint16(0xFFFF ^ F_KEEPIN)
        if not b.grid.any_in_circle(prom["x"], prom["y"], prom["r"] * 0.6, F_OUTSIDE | F_WATER):
            b.add_plaza(prom["x"], prom["y"], prom["r"], "promenade", "cobble")
    if cat:
        phi = math.radians(cat["yaw"])
    else:
        phi = 0.0
        roads = b._roads_local()
        if roads:
            p = roads[0][1]
            d = np.hypot(p[:, 0], p[:, 1])
            k = int(np.argmin(d))
            k2 = min(len(p) - 1, k + 6)
            phi = math.atan2(p[k2, 1] - p[k, 1], p[k2, 0] - p[k, 0])
    ax = np.array([math.cos(phi), math.sin(phi)])
    ay = np.array([-ax[1], ax[0]])
    if cat:
        ca = G.kit["SM_Landmark_MillisCathedral"]
        hwx, hwy = ca.hy + 36.0, ca.hx + 24.0
        cc = np.array([cat["x"], cat["y"]]) + ax * 10.0
        poly = np.array([cc + ax * sx * hwx + ay * sy * hwy for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
        central = b.add_plaza(float(cc[0]), float(cc[1]), 0.0, "central", "cobble", poly=poly)
    else:
        central = harbour_square(b) or b.add_plaza(0.0, 0.0, 16.0, "market", "cobble")
    gate_squares(b, 12.0, 22.0)
    b.clear_spawns("cobble")
    mains = b.main_streets(4.2, "cobble")
    if b.id == "Millishion":
        for k in range(6):
            a = phi + math.pi / 6 + k * math.pi / 3
            c = np.array([math.cos(a), math.sin(a)]) * 0.55 * b.R
            poly = np.array([c + ax * sx * 17.0 + ay * sy * 17.0 for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))])
            if not b.grid.any_in_circle(c[0], c[1], 24.0, F_WATER | F_LANDMARK | F_OUTSIDE | F_PLAZA | F_WALL):
                b.add_plaza(float(c[0]), float(c[1]), 0.0, "square", "cobble", poly=poly)
    if keep:
        cg = b.compound_gate
        b.connector(cg[0], cg[1], central.x, central.y, 3.4, "avenue", "cobble")
    b.wall_lane(2.6, "cobble")
    big = b.id == "Millishion"

    def fields(F):
        F.grid(phi, 1.2)
    F = TensorField(b.half, 4.0)
    fields(F)
    for st in b.streets:
        if st.kind in ("main", "avenue"):
            F.polyline(st.line.p, 32.0, 2.0)
    F.finish(None)

    def tfrac(x, y):
        rb = float(b.r_bound(np.array([math.atan2(y, x)]))[0])
        return min(1.0, math.hypot(x, y) / max(rb, 1.0))

    def dsep(x, y, fam):
        t = tfrac(x, y)
        if big:
            return (60.0 + 34.0 * t) if fam == 0 else (52.0 + 30.0 * t)
        return (44.0 + 14.0 * t) if fam == 0 else (38.0 + 12.0 * t)

    def kinds(line, fam, L):
        rm_ = float(np.mean(np.hypot(line[:, 0], line[:, 1])))
        if L > (220.0 if big else 120.0):
            return "grid_main", 3.4, "cobble", 3.0 + rm_ / 1000.0
        return "grid", 2.8, "cobble", 5.0 + rm_ / 1000.0
    b.streamline_network(F, dsep, kinds, t_stop=lambda x, y: 0.05, max_turn=25.0)
    b.prune_disconnected([(central.x, central.y)] + [(g["x"], g["y"]) for g in b.gates])

    def palette(p, kind):
        if kind in ("plaza", "main", "avenue", "grid_main"):
            return P["prime"]
        if kind == "grid":
            return P["street"]
        return P["lane"]
    b.palette = palette
    make = standard_rules(b, power=1.5, gap_core=(1.0, 1.6), gap_edge=(1.6, 3.2), sb_core=(1.0, 1.6),
                          sb_edge=(1.2, 2.4))
    keep = standard_keep(b, power=1.8, floor=0.14, prime_floor=0.8)
    lo, hi = budget_range(b)
    b.calibrate(target_for(b), lo, hi, make, keep_fn=keep, drop_kinds=("grid",))
    if cat:
        cf = np.array([cat["x"], cat["y"]]) + ax * (G.kit["SM_Landmark_MillisCathedral"].hy + 20.0)
        b.plaza_centrepiece(Plaza(float(cf[0]), float(cf[1]), 14.0, "tmp", None), "fountain", benches=6, statues=2, lamps=False)
    else:
        b.market_stalls(central, ["SM_Prop_MarketStall_A", "SM_Prop_MarketStall_B", "SM_Prop_MarketStall_C"], avoid_center=4.0)
    for pl in b.plazas:
        if pl.kind == "square":
            b.plaza_centrepiece(Plaza(pl.x, pl.y, 15.0, "tmp", None), "fountain" if b.rng.random() < 0.6 else "statue",
                                benches=4, statues=0, lamps=True)
        if pl.kind in ("promenade", "gate"):
            b.plaza_centrepiece(pl, "statue" if pl.kind == "promenade" else "none",
                                benches=4 if pl.kind == "promenade" else 0, lamps=True)
    b.lamps_along(("main", "grid_main", "avenue"), spacing=22.0)
    b.gate_props()
    b.shopfront_goods(("Shop",), 0.4)
    if b.lm("Harbour Docks"):
        b.harbour()


def build_desert(b):
    """Rapan: Desert walls, the adventurer guild, the grand bazaar, caravan yards by the harbour-road gate, inns and
    dense sandstone houses in irregular streets."""
    G = b.G
    P = desert_palettes(G)
    b.jitter = 4.0
    b.collect_spawns()
    cy_lm = b.lm("Caravan Yards")
    bz = b.lm("Grand Bazaar")
    guild_a = G.kit["SM_Landmark_RapanGuild"]
    roads = b._roads_local()
    best = None
    for ang in np.arange(0, 360, 5.0):
        u = np.array([math.cos(math.radians(ang)), math.sin(math.radians(ang))])
        c = u * 118.0
        dmin = 1e9
        for road, p in roads:
            for q0, q1 in zip(p[:-1], p[1:]):
                if min(math.hypot(*q0), math.hypot(*q1)) > b.R * 1.1:
                    continue
                dmin = min(dmin, float(seg_dist(c[0], c[1], q0[0], q0[1], q1[0], q1[1])))
        if cy_lm:
            dmin = min(dmin, math.hypot(c[0] - cy_lm["x"], c[1] - cy_lm["y"]) - 40.0)
        o = obb_of(guild_a, c[0], c[1], yaw_facing(-u[0], -u[1]))
        zmin, zmax = b.footprint_z(o, 4.0)
        score = min(dmin, 90.0) - 5.0 * (zmax - zmin) / 100.0
        if best is None or score > best[0]:
            best = (score, c, u)
    _, gc, gu = best
    pgd = b.place_fixed("SM_Landmark_RapanGuild", float(gc[0]), float(gc[1]), ang_of(-gu), "Landmark", margin=5.0)
    b.build_walls("Desert", wall_radius(b, 0.96))
    central = b.add_plaza(bz["x"], bz["y"], bz["r"], "market", "cobble")
    gate_squares(b, 12.0, 22.0)
    b.clear_spawns("cobble")
    mains = b.main_streets(3.8, "cobble")
    b.boulevard_between_mains(mains, 30.0)
    door_link(b, pgd, (0.0, 0.0), hw=4.0, kind="avenue", fore=13.0)
    b.frng = np.random.default_rng([SEED, b.crc, 3])
    if cy_lm:
        yard = G.kit["SM_Desert_CaravanYard_A"]
        cc = np.array([cy_lm["x"], cy_lm["y"]])
        b.add_plaza(float(cc[0]), float(cc[1]), 14.0, "yard", "dirt", frontage=False)
        u = unit(cc)
        v = np.array([-u[1], u[0]])
        for off in (v * 34.0, -v * 34.0, -u * 32.0, u * 30.0):
            p = cc + off
            f = unit(-off)
            got = b.try_building(yard, float(p[0]), float(p[1]), yaw_facing(f[0], f[1]), tol=2.6,
                                 block=F_WATER | F_SHORE | F_WALL | F_LANDMARK | F_OUTSIDE | F_PLAZA | F_STREET | F_ROAD,
                                 sink=30.0, clear=1.0)
            if got is not None:
                b.grid.paint_obb(got.obb, 2.5, F_LANDMARK)
    b.wall_lane(2.4, "dirt")
    organic_streets(b, lambda t: 33.0 + 52.0 * t ** 1.2, lambda t: 29.0 + 64.0 * t ** 1.4, noise_deg=20.0,
                    wavelength=200.0, t_stop=(0.12, 0.4), hw=(3.0, 2.6, 2.1), mats=("cobble", "dirt", "dirt"),
                    ring_len=130.0, lane_from=0.55)
    b.prune_disconnected([(0.0, 0.0)])

    def palette(p, kind):
        r = math.hypot(*p)
        if r > 0.55 * b.R and kind in ("main", "ring", "plaza"):
            return P["inns"]
        return P[kind_class(kind)]
    b.palette = palette
    make = standard_rules(b, power=1.4, gap_core=(1.0, 1.8), gap_edge=(1.6, 3.4))
    keep = standard_keep(b, power=1.4, floor=0.3)
    lo, hi = budget_range(b)
    b.calibrate(target_for(b), lo, hi, make, keep_fn=keep)
    b.market_stalls(central, ["SM_Prop_MarketStall_A", "SM_Prop_MarketStall_B", "SM_Prop_MarketStall_C"],
                    avoid_center=5.0)
    spot = b.free_spot(bz["x"], bz["y"], 3.6, 10.0)
    if spot is not None:
        b.try_prop("SM_Prop_Fountain_A", spot[0], spot[1], 0.0, block=F_BUILD | F_WALL | F_LANDMARK | F_WATER | F_STREET)
    for pl in b.plazas:
        if pl.kind == "boulevard" and b.rng.random() < 0.5:
            b.try_prop("SM_Prop_MarketStall_%s" % "ABC"[int(b.rng.integers(3))], pl.x, pl.y, float(b.rng.uniform(0, 360)),
                       clear=0.8, block=F_STREET | F_WATER | F_WALL | F_LANDMARK | F_BUILD | F_RESERVED)
        if pl.kind in ("forecourt", "gate"):
            b.plaza_centrepiece(pl, "none", benches=0, statues=0, lamps=True)
        if pl.kind == "yard":
            for k in range(9):
                a = b.rng.uniform(0, 2 * math.pi)
                rr = b.rng.uniform(3, pl.r - 2)
                x, y = pl.x + math.cos(a) * rr, pl.y + math.sin(a) * rr
                nm = ("SM_Prop_Crates_A", "SM_Prop_Barrels_A", "SM_Rural_Cart")[k % 3]
                b.try_prop(nm, x, y, float(b.rng.uniform(0, 360)), clear=0.4)
    b.lamps_along(("main", "avenue", "ring"), spacing=26.0)
    b.gate_props()
    b.shopfront_goods(("Shop", "Inn"), 0.6)


def build_village(b):
    """Buena: dirt paths from the square, 25-45 rural houses, barns, sheds and wells, the windmill (body + blades) and
    the Greyrat house on their rises; fences on the field edges, haystacks and carts on the farms. No walls."""
    G = b.G
    rng = b.rng
    b.jitter = 4.0
    b.collect_spawns()
    grey = b.lm("Greyrat House")
    mill = b.lm("Windmill")
    sq = b.lm("Village Square")
    sqc = (sq["x"], sq["y"]) if sq else (0.0, 0.0)
    sq_r = min(sq["r"], 17.0) if sq else 16.0
    targets = []
    if grey:
        pg = b.place_fixed("SM_Rural_GreyratHouse", grey["x"], grey["y"], grey["yaw"], "Buildings", margin=3.0, sink=25.0,
                           fit=(14.0, 25.0))
        gf = np.array([-math.sin(math.radians(pg.yaw)), math.cos(math.radians(pg.yaw))])
        targets.append(np.array([pg.x, pg.y]) + gf * (pg.asset.y1 + 7.0))
    else:
        # a quiet spot near the edge, away from the road
        grey_a = G.kit["SM_Rural_GreyratHouse"]
        for a in np.linspace(0, 2 * math.pi, 24, endpoint=False):
            q = np.array([math.cos(a), math.sin(a)]) * 0.95 * b.R
            if not b.grid.any_in_circle(q[0], q[1], 14.0, F_WATER | F_SHORE):
                pg = b.place_fixed("SM_Rural_GreyratHouse", float(q[0]), float(q[1]), ang_of(-q), "Buildings", margin=3.0)
                targets.append(q * 0.9)
                break
    if mill:
        body = G.kit["SM_Rural_Windmill_Body"]
        blades = G.kit["SM_Rural_Windmill_Blades"]
        p = b.place_fixed("SM_Rural_Windmill_Body", mill["x"], mill["y"], mill["yaw"], "Buildings", margin=3.0, sink=20.0)
        hub = body.attach.get("hub", [0.0, -3.15, 11.25])
        c, s = math.cos(math.radians(p.yaw)), math.sin(math.radians(p.yaw))
        lx, ly = hub[0], -hub[1]                       # Blender (x, y) -> Unreal local (x, -y)
        bx, by = p.x + lx * c - ly * s, p.y + lx * s + ly * c
        o = obb_of(blades, bx, by, p.yaw)
        b.commit(blades, bx, by, p.z + hub[2] * 100.0, p.yaw, "Props", pitch=17.0, obb=o, solid=False, tag="blades")
        mf = np.array([-math.sin(math.radians(p.yaw)), math.cos(math.radians(p.yaw))])
        targets.append(np.array([p.x, p.y]) + mf * 8.0)
    b.add_plaza(sqc[0], sqc[1], sq_r, "square", "dirt")
    b.clear_spawns("dirt")
    mains = b.main_streets(2.8, "dirt")
    used = [math.atan2(t[1] - sqc[1], t[0] - sqc[0]) for t in targets]
    for st in mains:
        q = st.line.p[np.argmax(np.hypot(st.line.p[:, 0], st.line.p[:, 1]))]
        used.append(math.atan2(q[1], q[0]))
    for k in range(4):
        best = None
        for a in np.linspace(-math.pi, math.pi, 72, endpoint=False):
            dmin = min([abs(wrap_deg(math.degrees(a - u))) for u in used] + [360])
            if best is None or dmin > best[0]:
                best = (dmin, a)
        a = best[1] + rng.normal(0, 0.12)
        used.append(a)
        targets.append(np.array(sqc) + np.array([math.cos(a), math.sin(a)]) * b.R * rng.uniform(0.95, 1.1))
    paths = []
    for t in targets:
        d = t - np.array(sqc)
        L = float(np.hypot(*d))
        ts = np.linspace(0, 1, max(8, int(L / 4)))
        nrm = np.array([-d[1], d[0]]) / max(L, 1.0)
        bend = rng.normal(0, 9.0)
        pts = np.array(sqc) + np.outer(ts, d) + np.outer(np.sin(math.pi * ts) * bend, nrm)
        paths += b.add_street(pts, 1.9, "path", "dirt", 3.0, clip=True, par_check=True, min_len=10.0,
                              block=F_WATER | F_LANDMARK | F_RESERVED)
    # a lane looping round the village core links the paths
    th = np.linspace(0, 2 * math.pi, 120, endpoint=False)
    nz = smooth_noise(rng, 3, 2, 5)
    rr = 0.42 * b.R * (1.0 + 0.12 * nz(th))
    b.add_street(np.stack([sqc[0] + np.cos(th) * rr, sqc[1] + np.sin(th) * rr], 1), 1.7, "path", "dirt", 4.0, closed=True,
                 par_check=True, min_len=18.0, block=F_WATER | F_LANDMARK | F_RESERVED)
    b.prune_disconnected([sqc])
    Rp = pal(G, ("SM_Rural_House_A", 3.0), ("SM_Rural_House_B", 3.0), ("SM_Rural_House_C", 2.0))
    b.palette = lambda p, kind: Rp

    def make(k):
        def rules(p, kind):
            return 0.3, (8.0, 22.0), (4.0, 10.0), (2.5, 5.0), (1, 3)
        return rules, 4.0

    def keep(p, kind, alpha):
        t = math.hypot(p[0] - sqc[0], p[1] - sqc[1]) / b.R
        return max(0.15, 1.0 - alpha * t)
    lo, hi = budget_range(b)
    b.calibrate(24, lo, hi, make, keep_fn=keep)
    b.frng = np.random.default_rng([SEED, b.crc, 29])
    farmsteads(b, paths, sqc)
    fences_on_fields(b)
    wells_and_carts(b, sqc)


def farmsteads(b, paths, sqc, max_farms=5):
    """House + barn + shed + haystacks around a yard at the outer end of paths."""
    G = b.G
    rng = b.frng
    barnA, barnB = G.kit["SM_Rural_Barn_A"], G.kit["SM_Rural_Barn_B"]
    shed = G.kit["SM_Rural_Shed_A"]
    houses = [G.kit[n] for n in ("SM_Rural_House_A", "SM_Rural_House_B", "SM_Rural_House_C")]
    n_farms = 0
    for st in b.streets:
        if st.kind not in ("path", "main") or n_farms >= max_farms:
            continue
        L = st.line.L
        for frac in (0.62, 0.86):
            if n_farms >= max_farms:
                break
            s = frac * L
            p = st.line.at(s)
            r = math.hypot(p[0] - sqc[0], p[1] - sqc[1])
            if r < 0.55 * b.R:
                continue
            t = st.line.tangent(s)
            for side in ((1.0, -1.0) if rng.random() < 0.5 else (-1.0, 1.0)):
                nrm = np.array([-t[1], t[0]]) * side
                yard_c = p + nrm * (st.hw + 16.0)
                if b.grid.any_in_circle(yard_c[0], yard_c[1], 14.0, F_WATER | F_LANDMARK | F_BUILD | F_STREET | F_RESERVED):
                    continue
                barn = barnA if n_farms % 2 == 0 else barnB
                hs = houses[int(rng.integers(3))]
                ok = 0
                # farmhouse facing the path
                hp = p + t * (-9.0) + nrm * (st.hw + 3.0 + hs.y1)
                if b.try_building(hs, float(hp[0]), float(hp[1]), yaw_facing(-nrm[0], -nrm[1]) + rng.normal(0, 3), tol=1.4):
                    ok += 1
                bp = p + t * 11.0 + nrm * (st.hw + 6.0 + barn.hx + 4.0)
                if b.try_building(barn, float(bp[0]), float(bp[1]), yaw_facing(-t[0], -t[1]) + rng.normal(0, 3), tol=1.6):
                    ok += 1
                sp = yard_c + nrm * 12.0 + t * rng.uniform(-6, 6)
                if b.try_building(shed, float(sp[0]), float(sp[1]), yaw_facing(-nrm[0], -nrm[1]) + rng.normal(0, 6), tol=1.0, gap=0.8):
                    ok += 1
                for k in range(int(rng.integers(1, 4))):
                    q = yard_c + nrm * rng.uniform(4, 18) + t * rng.uniform(-14, 14)
                    b.try_prop("SM_Rural_Haystack", float(q[0]), float(q[1]), float(rng.uniform(0, 360)), clear=0.5, gap=1.0)
                q = bp - t * (barn.hx + 3.0)
                b.try_prop("SM_Rural_Cart", float(q[0]), float(q[1]), float(rng.uniform(0, 360)), clear=0.4)
                if ok:
                    # a fenced paddock behind the yard (three sides, the open side faces the farm)
                    corner = yard_c + nrm * 22.0 + t * float(rng.uniform(-6, 6))
                    fence_polyline(b, [corner - t * 9.0, corner - t * 9.0 + nrm * 14.0, corner + t * 9.0 + nrm * 14.0,
                                       corner + t * 9.0])
                    n_farms += 1
                    break
    b.stats["farmsteads"] = n_farms


def fence_polyline(b, pts):
    """Chain post-and-rail fence pieces along a polyline: 4 m pieces, a 2 m piece for the remainder."""
    f4, f2 = b.asset("SM_Rural_Fence_4m"), b.asset("SM_Rural_Fence_2m")
    n = 0
    for P, Q in zip(pts[:-1], pts[1:]):
        P, Q = np.asarray(P, float), np.asarray(Q, float)
        L = float(np.hypot(*(Q - P)))
        u = (Q - P) / max(L, 1e-6)
        s = 0.0
        while s < L - 1.0:
            piece, ln = (f4, 4.0) if L - s >= 3.5 else (f2, 2.0)
            a0 = P + u * s
            a1 = P + u * (s + ln)
            c = 0.5 * (a0 + a1)
            z0, z1 = float(b.tz(a0[0], a0[1])), float(b.tz(a1[0], a1[1]))
            pitch = max(-12.0, min(12.0, math.degrees(math.atan((z1 - z0) / (ln * 100.0)))))
            o = obb_of(piece, float(c[0]), float(c[1]), ang_of(u))
            if not b.grid.hit_obb(o, 0.3, F_STREET | F_PLAZA | F_WATER | F_BUILD | F_LANDMARK | F_RESERVED | F_WALL):
                b.commit(piece, float(c[0]), float(c[1]), 0.5 * (z0 + z1) - 6.0, ang_of(u), "Props", pitch=pitch, obb=o,
                         solid=False, tag="fence", paint=F_PROP)
                n += 1
            s += ln
    return n


def mask_contours(mask):
    """Marching-squares contours of a boolean raster (0.5 level) as polylines in (col, row) coordinates."""
    m = mask.astype(np.uint8)
    h, w = m.shape
    idx = (m[:-1, :-1] << 3) | (m[:-1, 1:] << 2) | (m[1:, 1:] << 1) | m[1:, :-1]
    # edge midpoints: 0 top, 1 right, 2 bottom, 3 left
    table = {1: [(3, 2)], 2: [(2, 1)], 3: [(3, 1)], 4: [(1, 0)], 5: [(3, 0), (1, 2)], 6: [(2, 0)], 7: [(3, 0)],
             8: [(0, 3)], 9: [(0, 2)], 10: [(0, 1), (2, 3)], 11: [(0, 1)], 12: [(1, 3)], 13: [(1, 2)], 14: [(2, 3)]}
    off = {0: (0.5, 0.0), 1: (1.0, 0.5), 2: (0.5, 1.0), 3: (0.0, 0.5)}
    segs = []
    js, iis = np.nonzero((idx > 0) & (idx < 15))
    for j, i in zip(js, iis):
        for e0, e1 in table[int(idx[j, i])]:
            a = (i + off[e0][0], j + off[e0][1])
            b_ = (i + off[e1][0], j + off[e1][1])
            segs.append((a, b_))
    nxt = {}
    for a, b_ in segs:
        nxt.setdefault(a, []).append(b_)
        nxt.setdefault(b_, []).append(a)
    seen = set()
    lines = []
    for a, b_ in segs:
        if (a, b_) in seen:
            continue
        line = [a, b_]
        seen.add((a, b_))
        seen.add((b_, a))
        for end in (1, 0):
            while True:
                cur = line[-1] if end else line[0]
                prev = line[-2] if end else line[1]
                cands = [q for q in nxt.get(cur, ()) if q != prev and (cur, q) not in seen]
                if not cands:
                    break
                q = cands[0]
                seen.add((cur, q))
                seen.add((q, cur))
                if end:
                    line.append(q)
                else:
                    line.insert(0, q)
        lines.append(np.array(line))
    return lines


def fences_on_fields(b, max_runs=9, reach=1.8):
    """Post-and-rail fence runs (exactly-4 m segments, chained) along stretches of the Farmland field edges near the
    village; segments follow the ground (pitch) and skip paths and buildings."""
    G = b.G
    lay = G.farmland
    if lay is None:
        return 0
    t = G.terrain
    half = reach * b.R + 20.0
    i0 = max(0, int((b.cx - half * 100 - t.x0) / t.q))
    j0 = max(0, int((b.cy - half * 100 - t.y0) / t.q))
    i1 = min(t.nx, int((b.cx + half * 100 - t.x0) / t.q) + 1)
    j1 = min(t.ny, int((b.cy + half * 100 - t.y0) / t.q) + 1)
    sub = lay[j0:j1, i0:i1] > 128
    rng = b.frng
    fence = G.kit["SM_Rural_Fence_4m"]
    lines = []
    for ln in mask_contours(sub):
        xy = np.stack([(t.x0 + (i0 + ln[:, 0]) * t.q - b.cx) / 100.0, (t.y0 + (j0 + ln[:, 1]) * t.q - b.cy) / 100.0], 1)
        L = Line(xy)
        if L.L < 36.0:
            continue
        # smooth the staircase of the raster contour
        P = L.resample(6.0)
        if len(P) > 4:
            P = np.vstack([P[:1], 0.25 * P[:-2] + 0.5 * P[1:-1] + 0.25 * P[2:], P[-1:]])
        lines.append(Line(P))
    lines.sort(key=lambda l: float(np.min(np.hypot(l.p[:, 0], l.p[:, 1]))))
    runs = n_seg = 0
    for L in lines:
        if runs >= max_runs:
            break
        d_near = float(np.min(np.hypot(L.p[:, 0], L.p[:, 1])))
        if d_near > reach * b.R:
            continue
        run_len = float(rng.uniform(32.0, 72.0))
        s0 = float(rng.uniform(0.0, max(0.0, L.L - run_len)))
        s = s0
        p0 = L.at(s)
        placed = 0
        while s < min(L.L, s0 + run_len):
            # next point at exactly 4 m chord
            ss = np.arange(s + 3.0, s + 6.5, 0.1)
            q = L.at(ss)
            dd = np.hypot(q[:, 0] - p0[0], q[:, 1] - p0[1])
            k = np.flatnonzero(dd >= 4.0)
            if len(k) == 0:
                break
            p1 = q[k[0]]
            s = float(ss[k[0]])
            c = 0.5 * (p0 + p1)
            if math.hypot(c[0], c[1]) > reach * b.R:
                p0 = p1
                continue
            yaw = ang_of(p1 - p0)
            z0, z1 = float(b.tz(p0[0], p0[1])), float(b.tz(p1[0], p1[1]))
            pitch = max(-12.0, min(12.0, math.degrees(math.atan((z1 - z0) / 400.0))))
            o = obb_of(fence, float(c[0]), float(c[1]), yaw)
            if not b.grid.hit_obb(o, 0.3, F_STREET | F_PLAZA | F_WATER | F_BUILD | F_LANDMARK | F_RESERVED | F_PROP):
                b.commit(fence, float(c[0]), float(c[1]), 0.5 * (z0 + z1) - 6.0, yaw, "Props", pitch=pitch, obb=o,
                         solid=False, tag="fence", paint=F_PROP)
                placed += 1
            p0 = p1
        if placed >= 3:
            runs += 1
            n_seg += placed
    b.stats["fence_segments"] = n_seg
    b.stats["fence_runs"] = runs
    return n_seg


def wells_and_carts(b, sqc):
    spot = b.free_spot(sqc[0] + 4.0, sqc[1] - 3.0, 1.8, 10.0)
    if spot is not None:
        b.try_prop("SM_Rural_Well", spot[0], spot[1], 20.0, block=F_BUILD | F_WALL | F_LANDMARK | F_WATER | F_STREET)
    rng = b.frng
    n = 0
    for p in list(b.items):
        if p.cls == "Buildings" and p.asset.name.startswith("SM_Rural_House") and rng.random() < 0.18:
            c, s = math.cos(math.radians(p.yaw)), math.sin(math.radians(p.yaw))
            lx, ly = p.asset.x1 + 2.5, 0.0
            b.try_prop("SM_Rural_Well" if n % 2 == 0 else "SM_Rural_Cart", p.x + lx * c - ly * s, p.y + lx * s + ly * c,
                       float(rng.uniform(0, 360)), clear=0.4)
            n += 1


def build_north_village(b):
    """Sword Sanctuary: the Sword God dojo (North guild hall) and its training yard, a small North hamlet."""
    G = b.G
    P = north_palettes(G)
    b.jitter = 3.0
    b.collect_spawns()
    dojo = b.lm("Sword God Dojo")
    pd = b.place_fixed("SM_North_Guild_A", dojo["x"], dojo["y"], dojo["yaw"], "Buildings", margin=3.0, fit=(20.0, 25.0))
    df = np.array([-math.sin(math.radians(pd.yaw)), math.cos(math.radians(pd.yaw))])
    yard = np.array([pd.x, pd.y]) + df * (pd.asset.y1 + 16.0)
    b.add_plaza(float(yard[0]), float(yard[1]), 14.0, "training", "dirt", frontage=False)
    b.clear_spawns("dirt")
    mains = b.main_streets(2.6, "dirt")
    b.connector(float(yard[0]), float(yard[1]), 0.0, 0.0, 2.2, "path", "dirt")
    rng = b.rng
    used = [math.atan2(yard[1], yard[0])]
    for st in mains:
        q = st.line.p[np.argmax(np.hypot(st.line.p[:, 0], st.line.p[:, 1]))]
        used.append(math.atan2(q[1], q[0]))
    for k in range(3):
        best = None
        for a in np.linspace(-math.pi, math.pi, 72, endpoint=False):
            dmin = min(abs(wrap_deg(math.degrees(a - u))) for u in used)
            if best is None or dmin > best[0]:
                best = (dmin, a)
        a = best[1] + rng.normal(0, 0.1)
        used.append(a)
        t = np.array([math.cos(a), math.sin(a)]) * b.R * 0.85
        ts = np.linspace(0, 1, 24)
        nrm = np.array([-math.sin(a), math.cos(a)])
        pts = np.outer(ts, t) + np.outer(np.sin(math.pi * ts) * rng.normal(0, 6), nrm)
        b.add_street(pts, 1.8, "path", "dirt", 3.0, par_check=True, min_len=10, block=F_WATER | F_LANDMARK | F_RESERVED)
    b.prune_disconnected([(0.0, 0.0), (float(yard[0]), float(yard[1]))])
    hamlet = pal(G, ("SM_North_House_A", 3.0), ("SM_North_House_C", 3.0), ("SM_North_House_B", 2.0),
                 ("SM_North_Workshop_A", 0.5))
    b.palette = lambda p, kind: hamlet

    def make(k):
        return (lambda p, kind: (0.3, (8.0, 20.0), (3.0, 8.0), (2.0, 4.0), (1, 3))), 3.0

    def keep(p, kind, alpha):
        return max(0.2, 1.0 - alpha * math.hypot(*p) / b.R)
    lo, hi = budget_range(b)
    b.calibrate(target_for(b), lo, hi, make, keep_fn=keep)
    b.plaza_centrepiece(Plaza(float(yard[0]), float(yard[1]), 14.0, "tmp", None), "statue", benches=4, statues=0, lamps=False)
    tower = G.kit["SM_North_Tower_A"]
    b.frng = np.random.default_rng([SEED, b.crc, 31])
    for a in np.linspace(0, 2 * math.pi, 16, endpoint=False):
        q = np.array([math.cos(a), math.sin(a)]) * 0.7 * b.R
        if b.try_building(tower, float(q[0]), float(q[1]), float(rng.uniform(0, 360)), tol=1.5):
            break
    for p in list(b.items):
        if p.cls == "Buildings" and b.rng.random() < 0.3:
            c, s = math.cos(math.radians(p.yaw)), math.sin(math.radians(p.yaw))
            lx = p.asset.x1 + 1.6
            b.try_prop("SM_Prop_Crates_A" if b.rng.random() < 0.5 else "SM_Prop_Barrels_A", p.x + lx * c, p.y + lx * s,
                       float(b.rng.uniform(0, 360)), clear=0.3)


def build_labyrinth(b):
    """Labyrinth gate set into the rising ground: slid back toward the rock face (the site generator left a flat
    apron in front of it) until the facade stands on the apron and the block's back is buried in the slope, turned
    up to 30 deg toward the approach road."""
    G = b.G
    lm = b.lm("Labyrinth Gate")
    a = G.kit["SM_Landmark_LabyrinthGate"]
    rock_face = lm["yaw"]
    road_dir = None
    best_d = 1e9
    for road, p in b._roads_local():
        d = np.hypot(p[:, 0] - lm["x"], p[:, 1] - lm["y"])
        k = np.flatnonzero((d > 22) & (d < 70))
        if len(k) and d.min() < best_d:
            best_d = d.min()
            q = p[k[np.argmin(np.abs(d[k] - 45))]]
            road_dir = ang_of(q - np.array([lm["x"], lm["y"]]))
    xs = np.linspace(-a.hx + 2, a.hx - 2, 11)
    best = None
    for dyaw in np.arange(-30, 31, 5):
        face = rock_face + dyaw
        fv = np.array([math.cos(math.radians(face)), math.sin(math.radians(face))])
        sv = np.array([-fv[1], fv[0]])
        for back in np.arange(0.0, 50.0, 2.0):
            c = np.array([lm["x"], lm["y"]]) - fv * back

            def strip(ly, c=c, fv=fv, sv=sv):
                return b.tz(c[0] + xs * sv[0] + ly * fv[0], c[1] + xs * sv[1] + ly * fv[1]) / 100.0
            zf = strip(a.y1)
            apron = np.concatenate([strip(a.y1 + 5.0), strip(a.y1 + 12.0)])
            zb = np.concatenate([strip(a.y0 - 2.0), strip(a.y0 - 8.0)])
            zbase = float(min(zf.min(), apron.min()))
            rise = float(np.mean(zb) - zbase)
            flat = float(np.std(np.concatenate([zf, apron])) + abs(np.mean(apron) - np.mean(zf)))
            score = min(rise, 16.0) - 3.0 * flat - 0.08 * back - 0.08 * abs(dyaw)
            if road_dir is not None:
                score += 3.0 * math.cos(math.radians(face - road_dir))
            if best is None or score > best[0]:
                best = (score, c, face, rise, back, zbase)
    _, c, face, rise, back, zbase = best
    p = b.place_fixed("SM_Landmark_LabyrinthGate", float(c[0]), float(c[1]), face, "Landmark", margin=2.0, sink=40.0,
                      quiet=True)
    p.z = zbase * 100.0 - 40.0          # facade on the apron; the back part is buried in the slope
    b.stats.update(gate_rise_m=round(rise, 1), gate_back_m=round(back, 1), gate_facing=round(face, 1),
                   road_dir=None if road_dir is None else round(road_dir, 1))
    fv = np.array([math.cos(math.radians(face)), math.sin(math.radians(face))])
    q = c + fv * (a.y1 + 12.0) + np.array([-fv[1], fv[0]]) * 11.0
    b.try_prop("SM_Prop_Signpost_A", float(q[0]), float(q[1]), face + 90.0, clear=0.2)
    for k in range(3):
        q = c + fv * (a.y1 + 6.0 + 2.5 * k) - np.array([-fv[1], fv[0]]) * (14.0 + 1.5 * k)
        b.try_prop("SM_Prop_Crates_A" if k % 2 == 0 else "SM_Prop_Barrels_A", float(q[0]), float(q[1]),
                   float(b.rng.uniform(0, 360)), clear=0.2)


STYLE_BUILDERS = {
    "AsuraCapital": build_asura_capital,
    "AsuraTown": build_asura_town,
    "RuralVillage": build_village,
    "NorthernCity": build_north_city,
    "NorthernVillage": build_north_village,
    "DemonTown": build_demon,
    "DemonPort": build_demon,
    "MillisCapital": build_millis,
    "MillisPort": build_millis,
    "DesertCity": build_desert,
    "LabyrinthGate": build_labyrinth,
}


# ============================================================================================ bridges


def build_bridges(G):
    """Bridges[]: a stone bridge from Start to End at DeckZ, uniformly scaled to the span."""
    out = []
    for br in G.world.get("Bridges", []):
        sx, sy = br["Start"]["X"], br["Start"]["Y"]
        ex, ey = br["End"]["X"], br["End"]["Y"]
        L = math.hypot(ex - sx, ey - sy) / 100.0
        name = "SM_Bridge_Stone_12m" if L <= 16.0 else "SM_Bridge_Stone_24m"
        a = G.kit[name]
        span = 12.0 if name.endswith("12m") else 24.0
        scale = L / span
        yaw = math.degrees(math.atan2(ey - sy, ex - sx))
        cx, cy = 0.5 * (sx + ex), 0.5 * (sy + ey)
        out.append(dict(id=br["Id"], asset=a, X=cx, Y=cy, Z=float(br["DeckZ"]), yaw=yaw, scale=scale, length=L))
    return out


# ============================================================================================ validation


def validate(b):
    """Exact checks on the final layout (reported in the doc)."""
    rep = {}
    solids = [p for p in b.items if p.cls in ("Buildings", "Landmark")]
    walls = [p for p in b.items if p.cls == "Walls"]
    idx = ObbIndex(24.0)
    for p in solids + walls:
        idx.add(p.obb, p)
    close = 0
    overl = 0
    for p in solids:
        for q in idx.hits(p.obb, 0.9, skip=p):
            if q.cls in ("Buildings", "Landmark") and id(q) < id(p):
                continue
            if p.tag == "compound" and q.tag == "compound":
                continue
            if obb_overlap(p.obb, q.obb, 0.0):
                overl += 1
            else:
                close += 1
    rep["overlaps"] = overl
    rep["closer_than_0.9m"] = close
    # streets: distance from the building outline to street centrelines
    bad = 0
    for p in solids:
        if p.tag == "compound" or p.cls == "Landmark":
            continue
        cs = obb_corners(p.obb)
        pts = []
        for k in range(4):
            a, c = np.array(cs[k]), np.array(cs[(k + 1) % 4])
            for t in np.linspace(0, 1, 6):
                pts.append(a + (c - a) * t)
        pts = np.array(pts)
        for st in b.streets:
            q = st.line.p
            if np.min(np.hypot(q[:, 0] - p.x, q[:, 1] - p.y)) > 60:
                continue
            d = np.full(len(pts), 1e9)
            for u, v in zip(q[:-1], q[1:]):
                d = np.minimum(d, seg_dist(pts[:, 0], pts[:, 1], u[0], u[1], v[0], v[1]))
            if d.min() < st.hw + 0.3:
                bad += 1
                break
    rep["on_street"] = bad
    pbad = 0
    for p in b.items:
        if p.cls != "Props" or p.tag in ("pier", "boat", "blades"):
            continue
        cs = np.array(obb_corners(p.obb) + [(p.obb[0], p.obb[1])])
        for st in b.streets:
            q = st.line.p
            if np.min(np.hypot(q[:, 0] - p.x, q[:, 1] - p.y)) > 40:
                continue
            d = np.full(len(cs), 1e9)
            for u, v in zip(q[:-1], q[1:]):
                d = np.minimum(d, seg_dist(cs[:, 0], cs[:, 1], u[0], u[1], v[0], v[1]))
            if d.min() < st.hw - 0.05:
                pbad += 1
                break
    rep["props_on_street"] = pbad
    # water
    wet = 0
    for p in solids:
        if b.grid.hit_obb(p.obb, 0.0, F_WATER):
            wet += 1
    rep["in_water"] = wet
    # spawn clearance
    sp = []
    for s in b.spawns:
        dmin = 1e9
        for p in b.items:
            cx, cy, hx, hy, c, sn = p.obb
            dx, dy = s["x"] - cx, s["y"] - cy
            lx, ly = abs(dx * c + dy * sn) - hx, abs(-dx * sn + dy * c) - hy
            d = math.hypot(max(lx, 0.0), max(ly, 0.0))
            dmin = min(dmin, d)
        sp.append((s["id"], round(dmin, 1)))
    rep["spawn_clearance_m"] = sp
    # preview cameras: not inside a building and not behind a wall (nothing within 80 m along the view ray);
    # also report whether the sight line to the spawn point is open down to rooftop level near the target
    cams = []
    for s in b.spawns:
        cam = s.get("cam")
        if not cam:
            continue
        C = np.array([(cam["Location"]["X"] - b.cx) / 100.0, (cam["Location"]["Y"] - b.cy) / 100.0, cam["Location"]["Z"] / 100.0])
        yw, pt = math.radians(cam["Rotation"]["Yaw"]), math.radians(cam["Rotation"]["Pitch"])
        fwd = np.array([math.cos(pt) * math.cos(yw), math.cos(pt) * math.sin(yw), math.sin(pt)])
        T = np.array([s["x"], s["y"], float(b.tz(s["x"], s["y"])) / 100.0 + 1.7])
        inside = near = False
        roof = 0
        roof_d = 0.0
        for p in b.items:
            if p.cls == "Props":
                continue
            z0 = p.z / 100.0
            z1 = z0 + p.asset.h * p.scale
            cx, cy, hx, hy, c, sn = p.obb
            dx, dy = C[0] - cx, C[1] - cy
            if abs(dx * c + dy * sn) <= hx + 1 and abs(-dx * sn + dy * c) <= hy + 1 and z0 - 1 <= C[2] <= z1 + 1:
                inside = True
            if ray_hits_box(C, C + fwd * 80.0, p.obb, z0, z1):
                near = True
            th = ray_hits_box(C, T, p.obb, z0, z1, want_t=True)
            if th is not False:
                roof += 1
                roof_d = max(roof_d, (1.0 - th) * float(np.linalg.norm(T - C)))
        dist = float(np.hypot(C[0] - T[0], C[1] - T[1]))
        verdict = "inside a building" if inside else ("behind a wall / building" if near else "clear")
        if not inside and not near and roof:
            verdict += (" (%.0f m away; the last stretch of the sight line down to the spawn crosses %d building "
                        "bounding box%s, the farthest %.0f m from the spawn)" % (dist, roof, "es" if roof > 1 else "", roof_d))
        elif not inside and not near:
            verdict += " (%.0f m away, open sight line to the spawn)" % dist
        cams.append((s["id"], verdict))
    rep["preview_cameras"] = cams
    return rep


def ray_hits_box(A, B, o, z0, z1, want_t=False):
    """Segment A->B (3D, local m) against an oriented box footprint o extruded from z0 to z1 (slab test)."""
    cx, cy, hx, hy, c, s = o
    d = B - A
    # into box-local coordinates
    ax = (A[0] - cx) * c + (A[1] - cy) * s
    ay = -(A[0] - cx) * s + (A[1] - cy) * c
    dx = d[0] * c + d[1] * s
    dy = -d[0] * s + d[1] * c
    t0, t1 = 0.0, 1.0
    for p0, dp, lo, hi in ((ax, dx, -hx, hx), (ay, dy, -hy, hy), (A[2], d[2], z0, z1)):
        if abs(dp) < 1e-12:
            if p0 < lo or p0 > hi:
                return False
            continue
        ta, tb = (lo - p0) / dp, (hi - p0) / dp
        if ta > tb:
            ta, tb = tb, ta
        t0, t1 = max(t0, ta), min(t1, tb)
        if t0 > t1:
            return False
    return t0 if want_t else True


# ============================================================================================ outputs


def street_masks(G, builders):
    """Rasterise streets / plazas onto the landscape vertex grid (cobble and dirt), soft edges."""
    t = G.terrain
    cob = np.zeros((t.ny, t.nx), np.float32)
    dirt = np.zeros((t.ny, t.nx), np.float32)
    E = MASK_EDGE
    for b in builders:
        def window(xmin, xmax, ymin, ymax):
            i0 = max(0, int(math.floor((b.cx + 100 * xmin - t.x0) / t.q)))
            i1 = min(t.nx, int(math.ceil((b.cx + 100 * xmax - t.x0) / t.q)) + 1)
            j0 = max(0, int(math.floor((b.cy + 100 * ymin - t.y0) / t.q)))
            j1 = min(t.ny, int(math.ceil((b.cy + 100 * ymax - t.y0) / t.q)) + 1)
            if i0 >= i1 or j0 >= j1:
                return None
            xs = (t.x0 + np.arange(i0, i1) * t.q - b.cx) / 100.0
            ys = (t.y0 + np.arange(j0, j1) * t.q - b.cy) / 100.0
            return i0, i1, j0, j1, xs, ys

        for st in b.streets:
            if st.mat is None:
                continue
            tgt = cob if st.mat == "cobble" else dirt
            p = st.line.p
            for k in range(0, len(p) - 1, 8):
                q = p[k:k + 9]
                r = st.hw + E
                w = window(q[:, 0].min() - r, q[:, 0].max() + r, q[:, 1].min() - r, q[:, 1].max() + r)
                if w is None:
                    continue
                i0, i1, j0, j1, xs, ys = w
                d = np.full((j1 - j0, i1 - i0), 1e9)
                for a, c in zip(q[:-1], q[1:]):
                    d = np.minimum(d, seg_dist(xs[None, :], ys[:, None], a[0], a[1], c[0], c[1]))
                v = np.clip((st.hw + 0.5 * E - d) / E, 0.0, 1.0)
                sub = tgt[j0:j1, i0:i1]
                np.maximum(sub, v, out=sub)
        for pl in b.plazas:
            if pl.mat is None:
                continue
            tgt = cob if pl.mat == "cobble" else dirt
            if pl.poly is not None:
                poly = np.asarray(pl.poly)
                w = window(poly[:, 0].min() - E, poly[:, 0].max() + E, poly[:, 1].min() - E, poly[:, 1].max() + E)
                if w is None:
                    continue
                i0, i1, j0, j1, xs, ys = w
                X, Y = np.meshgrid(xs, ys)
                inside = points_in_poly(X, Y, [tuple(v) for v in poly])
                d = np.full(X.shape, 1e9)
                for k in range(len(poly)):
                    a, c = poly[k], poly[(k + 1) % len(poly)]
                    d = np.minimum(d, seg_dist(X, Y, a[0], a[1], c[0], c[1]))
                sd = np.where(inside, -d, d)
                v = np.clip((0.5 * E - sd) / E, 0.0, 1.0)
            else:
                w = window(pl.x - pl.r - E, pl.x + pl.r + E, pl.y - pl.r - E, pl.y + pl.r + E)
                if w is None:
                    continue
                i0, i1, j0, j1, xs, ys = w
                d = np.hypot(xs[None, :] - pl.x, ys[:, None] - pl.y)
                v = np.clip((pl.r + 0.5 * E - d) / E, 0.0, 1.0)
            sub = tgt[j0:j1, i0:i1]
            np.maximum(sub, v, out=sub)
    cob8 = np.round(cob * 255.0).astype(np.uint8)
    dirt8 = np.minimum(np.round(dirt * 255.0), 255 - cob8.astype(np.float32)).astype(np.uint8)
    return cob8, dirt8


def write_instances(G, builders, bridges):
    used = {}
    for b in builders:
        for p in b.items:
            used[p.asset.name] = p.asset
    for br in bridges:
        used[br["asset"].name] = br["asset"]
    names = sorted(used, key=lambda n: (used[n].category, n))
    mesh_index = {n: i for i, n in enumerate(names)}
    meshes = [used[n].path for n in names]
    records = []
    groups = []
    for b in builders:
        for cls in CLASSES:
            items = [p for p in b.items if p.cls == cls]
            if not items:
                continue
            items.sort(key=lambda p: (mesh_index[p.asset.name], p.x, p.y))
            off = len(records)
            for p in items:
                records.append((mesh_index[p.asset.name], b.cx + 100.0 * p.x, b.cy + 100.0 * p.y, p.z,
                                wrap_deg(p.yaw), p.pitch, p.roll, p.scale))
            groups.append(group_dict(b.id, cls, off, len(items)))
    for br in bridges:
        off = len(records)
        records.append((mesh_index[br["asset"].name], br["X"], br["Y"], br["Z"], wrap_deg(br["yaw"]), 0.0, 0.0, br["scale"]))
        groups.append(group_dict(br["id"], "Walls", off, 1))
    data = np.array(records, dtype="<f4").reshape(-1, 8)
    os.makedirs(OUT_SCATTER, exist_ok=True)
    data.tofile(os.path.join(OUT_SCATTER, "Cities.bin"))
    with open(os.path.join(OUT_SCATTER, "Cities.json"), "w", encoding="utf-8") as f:
        json.dump({"Generator": GENERATOR, "Seed": SEED, "Meshes": meshes, "Groups": groups}, f, indent=1)
    return data, meshes, groups


def group_dict(site_id, cls, off, count):
    g = {"Label": "City_%s_%s" % (site_id, cls), "Offset": int(off), "Count": int(count)}
    if cls in ("Buildings", "Walls"):
        g.update(CullStart=0, CullEnd=0, Collision=True, SpatiallyLoaded=True, CastShadow=True)
        hl = HLOD_CITY
    elif cls == "Props":
        g.update(CullStart=15000, CullEnd=25000, Collision=True, SpatiallyLoaded=True, CastShadow=True)
        hl = ""
    else:
        g.update(CullStart=0, CullEnd=0, Collision=True, SpatiallyLoaded=False, CastShadow=True)
        hl = ""
    g["Tags"] = ["MTCity", "MTCity_%s" % site_id, "MTCity_%s" % cls]
    g["HLODLayer"] = hl
    return g


CLASS_COLORS = {"Buildings": (214, 96, 58), "Walls": (58, 60, 72), "Props": (250, 214, 40), "Landmark": (170, 70, 210)}


def render_plan(G, b, cob, dirt, path, max_px=2200):
    half = b.half
    ppm = min(2.5, max_px / (2 * half))
    npx = int(2 * half * ppm)
    xs = -half + (np.arange(npx) + 0.5) / ppm
    X, Y = np.meshgrid(xs, xs)
    Z = b.tz(X, Y) / 100.0
    gy, gx = np.gradient(Z, 1.0 / ppm)
    slope = np.arctan(np.hypot(gx, gy))
    aspect = np.arctan2(-gx, gy)
    az, alt = math.radians(315), math.radians(45)
    hs = np.clip(np.sin(alt) * np.cos(slope) + np.cos(alt) * np.sin(slope) * np.cos(az - aspect), 0, 1)
    base = np.array({"AsuraCapital": (132, 158, 96), "AsuraTown": (132, 158, 96), "RuralVillage": (138, 164, 92),
                     "NorthernCity": (214, 220, 226), "NorthernVillage": (214, 220, 226), "DemonTown": (140, 92, 80),
                     "DemonPort": (140, 92, 80), "MillisCapital": (122, 160, 104), "MillisPort": (150, 160, 110),
                     "DesertCity": (206, 182, 132), "LabyrinthGate": (190, 168, 128)}.get(b.style, (140, 150, 110)), np.float32)
    img = base[None, None, :] * (0.45 + 0.55 * hs[..., None])
    wf = b.grid.get(X, Y)
    water = (wf & F_WATER) != 0
    img[water] = img[water] * 0.25 + np.array([50, 110, 190]) * 0.75
    # gardens (unpaved plazas) as darker lawns
    for pl in b.plazas:
        if pl.kind == "garden":
            gm = np.hypot(X - pl.x, Y - pl.y) < pl.r
            img[gm] = img[gm] * 0.55 + np.array([60, 120, 50], np.float32) * 0.45
    # street masks, bilinear; the 50 % contour of the mask is the paved edge, so show a narrow ramp around it
    t = G.terrain
    u = np.clip((b.cx + 100 * X - t.x0) / t.q, 0, t.nx - 1.001)
    v = np.clip((b.cy + 100 * Y - t.y0) / t.q, 0, t.ny - 1.001)
    iu = np.floor(u).astype(np.int64)
    jv = np.floor(v).astype(np.int64)
    fu = (u - iu)[..., None]
    fv = (v - jv)[..., None]

    def bil(m):
        a = m[jv, iu].astype(np.float32)[..., None]
        b_ = m[jv, iu + 1].astype(np.float32)[..., None]
        c = m[jv + 1, iu].astype(np.float32)[..., None]
        d_ = m[jv + 1, iu + 1].astype(np.float32)[..., None]
        return ((a * (1 - fu) + b_ * fu) * (1 - fv) + (c * (1 - fu) + d_ * fu) * fv) / 255.0
    cm = np.clip((bil(cob) - 0.3) / 0.4, 0, 1)
    dm = np.clip((bil(dirt) - 0.3) / 0.4, 0, 1)
    img = img * (1 - cm) + np.array([196, 190, 176], np.float32) * cm
    img = img * (1 - dm) + np.array([168, 128, 84], np.float32) * dm
    im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(im, "RGBA")

    def P(x, y):
        return ((x + half) * ppm, (y + half) * ppm)
    for road, p in b._roads_local():
        pts = [P(x, y) for x, y in p if abs(x) < half * 1.2 and abs(y) < half * 1.2]
        if len(pts) > 1:
            d.line(pts, fill=(110, 80, 50, 120), width=2)
    order = {"Landmark": 0, "Walls": 1, "Buildings": 2, "Props": 3}
    for p in sorted(b.items, key=lambda p: order[p.cls]):
        poly = [P(x, y) for x, y in obb_corners(p.obb)]
        col = CLASS_COLORS[p.cls]
        if p.tag in ("pier",):
            col = (150, 110, 70)
        d.polygon(poly, fill=col + (235,), outline=tuple(int(c * 0.45) for c in col) + (255,))
    font = ImageFont.load_default(size=max(12, int(npx / 110)))
    small = ImageFont.load_default(size=max(10, int(npx / 150)))
    for g in b.gates:
        x, y = P(g["x"], g["y"])
        r = 9 * ppm
        d.ellipse([x - r, y - r, x + r, y + r], outline=(0, 230, 255, 255), width=max(2, int(ppm * 1.2)))
        d.text((x + r + 3, y - r), "Gate (%s)" % "/".join(g["roads"]), fill=(0, 240, 255, 255), font=small)
    for s in b.spawns:
        x, y = P(s["x"], s["y"])
        r = 8 * ppm
        col = (255, 255, 0, 255) if s["spawn"] else (255, 160, 0, 255)
        d.ellipse([x - r, y - r, x + r, y + r], outline=col, width=max(2, int(ppm)))
        d.line([x - r * 0.6, y, x + r * 0.6, y], fill=col, width=2)
        d.line([x, y - r * 0.6, x, y + r * 0.6], fill=col, width=2)
        d.text((x + r + 3, y + 2), ("Spawn %s" if s["spawn"] else "Travel %s") % s["id"], fill=col, font=small)
    # legend, title, scale bar
    cnt = {c: sum(1 for p in b.items if p.cls == c) for c in CLASSES}
    title = "%s (%s)  buildings %d  walls %d  props %d  landmark %d" % (b.id, b.style, cnt["Buildings"], cnt["Walls"],
                                                                        cnt["Props"], cnt["Landmark"])
    d.rectangle([0, 0, npx, int(npx / 45) + 14], fill=(0, 0, 0, 150))
    d.text((10, 6), title, fill=(255, 255, 255, 255), font=font)
    ly = int(npx / 45) + 24
    for k, c in enumerate(CLASSES):
        d.rectangle([10, ly + k * 22, 28, ly + k * 22 + 14], fill=CLASS_COLORS[c] + (255,))
        d.text((34, ly + k * 22 - 2), c, fill=(255, 255, 255, 255), font=small)
    d.rectangle([10, ly + 4 * 22, 28, ly + 4 * 22 + 14], fill=(196, 190, 176, 255))
    d.text((34, ly + 4 * 22 - 2), "cobble", fill=(255, 255, 255, 255), font=small)
    d.rectangle([10, ly + 5 * 22, 28, ly + 5 * 22 + 14], fill=(168, 128, 84, 255))
    d.text((34, ly + 5 * 22 - 2), "dirt", fill=(255, 255, 255, 255), font=small)
    sb = 100.0 * ppm
    y0 = npx - 30
    d.rectangle([20, y0, 20 + sb, y0 + 8], fill=(255, 255, 255, 255))
    d.text((24 + sb, y0 - 6), "100 m", fill=(255, 255, 255, 255), font=small)
    im.save(path)
    return im


def contact_sheet(plans, path, cols=3, tile=900):
    if not plans:
        return
    rows = int(math.ceil(len(plans) / cols))
    sheet = Image.new("RGB", (cols * tile, rows * tile), (20, 20, 24))
    for k, (sid, im) in enumerate(plans):
        t = im.copy()
        t.thumbnail((tile, tile), Image.LANCZOS)
        x = (k % cols) * tile + (tile - t.size[0]) // 2
        y = (k // cols) * tile + (tile - t.size[1]) // 2
        sheet.paste(t, (x, y))
    q = 88
    while True:
        sheet.save(path, quality=q, optimize=True)
        if os.path.getsize(path) < 2.4e6 or q < 50:
            break
        q -= 6


def write_doc(G, builders, bridges, reports, runtime, path):
    lines = []
    A = lines.append
    A("# LA PLACE cities, towns and villages")
    A("")
    A("Generated by `%s` (seed %d) from the frozen world (`Content/Data/World.json`, `Content/Data/Locations.json`, "
      "`SourceArt/World/Height.r16`, `Layers/Farmland.png`) and the architecture kit "
      "(`SourceArt/Kit/manifest_architecture.json`). Deterministic (two runs give byte-identical outputs), numpy + Pillow "
      "only. This file is rewritten on every full run." % (GENERATOR, SEED))
    A("")
    A("![Main sites](../Images/LaPlace_Cities.jpg)")
    A("")
    A("## Rebuild")
    A("")
    A("```sh")
    A("~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_cities.py             # everything (~%.0f s)" % runtime)
    A("~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_cities.py --no-plans  # skip the review images")
    A("~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_cities.py --sites Ars,Roa  # preview: only their plans")
    A("```")
    A("")
    A("The Unreal side picks the set up with the existing importer: `Content/Python/mt_build_world.py` stage `instances` "
      "spawns every `SourceArt/World/Scatter/*.json` + `.bin` pair (tag `MTSet_Cities`).")
    A("")
    A("## Outputs")
    A("")
    A("| File | Content |")
    A("|---|---|")
    A("| `SourceArt/World/Scatter/Cities.json` + `Cities.bin` | instance set in the `Foliage.json` format: `Meshes` = "
      "`/Game/LaPlace/Kit/<Category>/<Asset>`; the bin is little-endian float32, 8 per record (MeshIndex, X, Y, Z cm, "
      "Yaw, Pitch, Roll deg, uniform Scale); each group is a contiguous record range |")
    A("| `SourceArt/World/Cities/StreetCobble.png`, `StreetDirt.png` | 6097 x 4573 8-bit greyscale on the landscape "
      "vertex grid (column = X, row = Y, as `Height.r16`); 255 = fully paved; the edge ramps linearly over %.1f m "
      "(~1.2 vertices) centred on the paved edge; `dirt <= 255 - cobble` at every vertex |" % MASK_EDGE)
    A("| `SourceArt/World/Cities/_Plan_<SiteId>.png` | review plan per site: hillshade, water, cobble / dirt masks, "
      "every footprint as its oriented rectangle (Buildings orange, Walls slate, Props yellow, Landmark purple), "
      "gardens green, the World.json roads as thin brown lines, gates (cyan rings, labelled with their roads) and spawn / "
      "fast-travel points (yellow / orange 8 m rings) |")
    A("| `Docs/Images/LaPlace_Cities.jpg` | contact sheet: Ars, Roa, Buena, Sharia, Rikarisu, Wenport, Millishion, "
      "King Dragon, Rapan |")
    A("")
    A("### Groups")
    A("")
    A("One group per site and class, `City_<SiteId>_<Class>`, tags `MTCity`, `MTCity_<SiteId>`, `MTCity_<Class>`:")
    A("")
    A("| Class | Collision | SpatiallyLoaded | Cull (cm) | HLODLayer | Content |")
    A("|---|---|---|---|---|---|")
    A("| Buildings | yes | yes | never | `%s` | houses, shops, inns, towers, manors, halls, keeps, chapels, barns, sheds, "
      "workshops, caravan yards, windmill body |" % HLOD_CITY)
    A("| Walls | yes | yes | never | `%s` | wall segments, wall towers, gates (city rings, the Ars palace ward, castle "
      "wards) |" % HLOD_CITY)
    A("| Props | yes | yes | 15000 - 25000 | none | lamps, fountains, statues, benches, stalls, crates, barrels, "
      "signposts, wells, haystacks, carts, fences, totems, piers, boats, windmill blades |")
    A("| Landmark | yes | **no** | never | none | `SM_Landmark_*` (Silver Palace, Ranoa University, cathedral, Roa keep, "
      "Rikarisu hall, Rapan guild, labyrinth gates): always loaded so they read from far away |")
    A("")
    A("Bridges from `World.json` `Bridges[]` lie on the highways between sites, so each is its own group "
      "`City_<BridgeId>_Walls` (same settings as Walls; tags `MTCity`, `MTCity_<BridgeId>`, `MTCity_Walls`).")
    A("")
    A("## Conventions")
    A("")
    A("- World X east, Y south, Z up, cm; Unreal yaw 0 = +X, 90 = +Y. Layout maths runs per site in metres around the "
      "site centre.")
    A("- A kit point (bx, by, bz) m lands at local (100 bx, -100 by, 100 bz) cm, so fronts, counters, bench seats, "
      "statues, boat bows and pier water ends (Blender -Y) face local +Y: facing (dx, dy) = yaw atan2(dy, dx) - 90. "
      "Footprints are the manifest bounds mapped (x, -y); wall segments run along local X with the outer face on +Y.")
    A("- Z = lowest terrain under the footprint (sample grid <= 3.5 m, edges included) minus 15-40 cm; houses need "
      "<= 1.2 m of relief under them, big buildings <= 2.5 m. Walls follow a fitted line per straight run.")
    A("")
    A("## Layout rules")
    A("")
    for r in RULES:
        A("- " + r)
    A("")
    A("## Per-site results")
    A("")
    A("| Site | Style | Buildings | Walls (seg / tower / gate) | Props | Landmark | Street km | Overlaps | On street "
      "(bld / props) | In water |")
    A("|---|---|---:|---|---:|---:|---:|---:|---|---:|")
    tot = {c: 0 for c in CLASSES}
    for b in builders:
        cnt = {c: sum(1 for p in b.items if p.cls == c) for c in CLASSES}
        for c in CLASSES:
            tot[c] += cnt[c]
        rep = reports.get(b.id, {})
        segs = sum(1 for p in b.items if p.cls == "Walls" and p.asset.name.endswith("_Wall_12m"))
        tws = sum(1 for p in b.items if p.cls == "Walls" and p.asset.name.endswith("WallTower"))
        gts = sum(1 for p in b.items if p.cls == "Walls" and p.asset.name.endswith("_Gate"))
        km = sum(st.line.L for st in b.streets) / 1000.0
        A("| %s | %s | %d | %d (%d / %d / %d) | %d | %d | %.1f | %d | %d / %d | %d |" % (
            b.id, b.style, cnt["Buildings"], cnt["Walls"], segs, tws, gts, cnt["Props"], cnt["Landmark"], km,
            rep.get("overlaps", 0), rep.get("on_street", 0), rep.get("props_on_street", 0), rep.get("in_water", 0)))
    A("| **Total** | | **%d** | **%d** | **%d** | **%d** | | | | |" % (tot["Buildings"], tot["Walls"], tot["Props"],
                                                                     tot["Landmark"]))
    A("")
    A("Checks are exact geometry on the final layout: *overlaps* = building / landmark footprints intersecting each "
      "other or a wall piece (compound pieces excepted); *on street* = a building outline within 0.3 m of a paved street "
      "band, or a prop (piers, boats and blades excepted) standing on one; *in water* = a footprint over sea, lake or "
      "river. Every building also keeps >= 1 m (separating-axis test) from every other building.")
    A("")
    A("### Gates")
    A("")
    for b in builders:
        if b.gates:
            A("- %s: %s" % (b.id, "; ".join("%s at (%.0f, %.0f) m" % ("/".join(g["roads"]), g["x"], g["y"]) for g in b.gates)))
    A("")
    A("### Bridges")
    A("")
    A("| Bridge | Mesh | Span (m) | Uniform scale | Deck Z (cm) |")
    A("|---|---|---:|---:|---:|")
    for br in bridges:
        A("| %s | `%s` | %.1f | %.3f | %.0f |" % (br["id"], br["asset"].name, br["length"], br["scale"], br["Z"]))
    A("")
    A("Each bridge runs from `Start` to `End` (local X along the span, pivot at mid-span) at `DeckZ`, with the deck ends "
      "on the banks; the 24 m three-arch bridge is scaled uniformly to the span (the 42 m Ars river crossing is 1.69x).")
    A("")
    A("## Spawn points and preview cameras")
    A("")
    for b in builders:
        rep = reports.get(b.id, {})
        for sid, dmin in rep.get("spawn_clearance_m", []):
            cam = dict(rep.get("preview_cameras", [])).get(sid, "-")
            A("- %s / `%s`: paved clearing r = 10 m, nearest placed object %.1f m away; preview camera %s." % (
                b.id, sid, dmin, cam))
    A("")
    notes = [n for b in builders for n in b.notes]
    A("## Site notes")
    A("")
    for n in SITE_NOTES:
        A("- " + n)
    A("")
    if notes:
        A("Generator log:")
        A("")
        for n in notes:
            A("- " + n)
        A("")
    used = set(p.asset.name for b in builders for p in b.items) | set(br["asset"].name for br in bridges)
    unused = sorted(n for n, a in G.kit.items() if n not in used and a.category != "VFX")
    A("Kit assets not placed: %s." % (", ".join("`%s`" % n for n in unused) if unused else "none"))
    A("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


RULES = [
    "**Walls**: a polar dynamic programme picks the wall line near the wall-district radius, avoiding water, keeping "
    "every landmark and keep-in area (the Silver Palace mound, the Millishion promenade) inside and preferring gentle "
    "ground. Towers stand on a chain of straight runs of 2-4 exactly-12 m segments (one uniform scale per arc, ~0.97-1.05, "
    "so every run closes exactly on a tower or gate joint); every change of direction is inside a tower. Runs are 2 "
    "segments on tight bends or grades over 8 %. Segments pitch with their run (<= 3 deg, up to 6 deg only where a run "
    "would otherwise be buried > 2.5 m); towers and gates stay upright. Ports get landward arcs that end in towers at the "
    "shore.",
    "**Gates**: every road that crosses the wall line gets the style's gate, centred on the crossing and turned toward the "
    "road (<= 30 deg off the wall normal); roads crossing within 19 m share one gate; gates stay >= 24 m apart so one 12 m "
    "segment links two neighbours (Rapan's twin caravan trails get two gates). The wall runs end at the gate joints, "
    "where the gate's own towers stand. A gate square sits just inside every gate, with a signpost and goods.",
    "**Streets**: the World.json roads inside the walls are the main streets (gate to centre; cobble, the road's width); "
    "then squares, avenues to the landmarks and the wall-side lane. The rest comes from a tensor field (radial round the "
    "centre, aligned with the main streets and avenues, radial round the squares and the palace mound; a grid field for "
    "Millis) traced as evenly spaced streamlines of two families: long curving ring and radial streets, T-junctions where "
    "a line stops at a crossing, spacing growing from ~35 m in the core to 110-150 m near the walls. Streets are clipped "
    "at water, landmarks and walls, split into edges at every junction, and must connect to the centre.",
    "**Buildings** line both sides of every street edge and square, fronts to the street, 1-3 m setback, 1-4 m gaps "
    "(tighter in the core), style palettes by district and street rank (shops and inns on main streets and squares, "
    "large houses in the core, small ones on lanes; towers capped per site). The frontage is placed at full density "
    "once, then thinned toward a radial density profile by removing whole runs of houses (gardens between rows; Millis "
    "may also drop whole minor grid edges), bisected to the site's budget. Separating-axis tests keep >= 1 m between "
    "buildings; a 0.5 m raster keeps them off streets (+0.35 m), squares, water (+4 m shore), walls (+2 m), landmarks and "
    "the outside of the wall.",
    "**Density**: continuous rows in the core, rows broken by gardens toward the walls, never below a floor (so there is "
    "no empty ring). Budgets: Ars ~680, Sharia / Millishion / Rapan ~330-350, towns 60-150, villages 25-45.",
    "**Spawn / fast-travel points** inside a site get a paved clearing of r = 10 m; nothing stands within 8.5 m.",
    "**Props**: lamp posts just outside the paved edge of main streets, avenues and rings; fountain, statues and benches "
    "on squares, forecourts and noble gardens (always off the street bands); rows of stalls with crates and barrels on "
    "market squares, leaving the through-road corridor clear; demon stalls and totems; wells, haystacks, carts, paddock "
    "fences and fences along the Farmland edges in the village; piers (deck at bank level, water end seaward) and moored "
    "boats (Z = 0) at harbour docks; windmill blades at the body's hub anchor.",
]

SITE_NOTES = [
    "Ars: the Silver Palace mound lies on the wall-district ring (418 m out), so the city wall bulges round the mound; "
    "the palace ward is closed by a white Noble wall ring at the foot of the mound with a Noble gate on the processional "
    "avenue; the noble quarter (Noble houses, gardens with fountains / statues, manors) fills the mound's surroundings "
    "and lines the avenue; the Royal Market and the Hall of Government sit at their landmarks.",
    "Sharia: Ranoa University slides ~24 m off its landmark to the flattest nearby ground; the 2.6 m left under its "
    "148 m length is the generator's 1.55 % site-plane tilt.",
    "Rikarisu: no wall district in World.json, so the Demon wall rings the 'Dense Streets' district (0.8 R) and the "
    "'Rock Dwellings' are huts in clusters along a path round the town outside the wall, on the flat crater floor.",
    "Labyrinth gates: the site generator left a flat apron ~30 m deep in front of each rock face, so each gate slides "
    "back toward the rock (32-48 m) until its facade stands on the apron and its back is buried 6-19 m in the slope; it "
    "turns at most 30 deg toward the approach road.",
    "Buena: the Greyrat house slides 14 m onto the flat top of its rise (the landmark point is at the foot of a natural "
    "hill); the windmill sits on its mound with the blades on the hub anchor. No walls.",
    "Wenport and the ports: the wall covers only the landward arcs; roads that reach the town from the water side "
    "(Wenport's Crater Road over Bridge_06) need no gate.",
]


# ============================================================================================ main


class Globals:
    pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sites", default="", help="comma list of site ids: preview only, rewrites just their _Plan_*.png "
                                                 "(the instance set, masks, sheet and doc need a full run)")
    ap.add_argument("--no-plans", action="store_true", help="skip the plan images and the contact sheet")
    args = ap.parse_args()
    t0 = time.time()
    G = Globals()
    G.world = load_json(WORLD_JSON)
    G.locations = load_json(LOCATIONS_JSON)
    man = load_json(KIT_MANIFEST)
    G.kit = {a["name"]: Asset(a) for a in man["assets"]}
    G.terrain = Terrain(G.world)
    try:
        G.farmland = np.asarray(Image.open(os.path.join(WORLD_DIR, "Layers", "Farmland.png")).convert("L"))
    except Exception:
        G.farmland = None
    log("loaded world + kit (%.1f s)" % (time.time() - t0))
    only = set(s for s in args.sites.split(",") if s)
    builders = []
    reports = {}
    for site in G.world["Sites"]:
        if only and site["Id"] not in only:
            continue
        ts = time.time()
        b = SiteBuilder(G, site)
        b.paint_water()
        b.paint_roads()
        STYLE_BUILDERS[site["Style"]](b)
        reports[b.id] = validate(b)
        builders.append(b)
        cnt = {c: sum(1 for p in b.items if p.cls == c) for c in CLASSES}
        log("%-15s %-16s bld %4d  walls %4d  props %4d  lm %d  gates %d  (%.1f s)  %s" % (
            b.id, b.style, cnt["Buildings"], cnt["Walls"], cnt["Props"], cnt["Landmark"], len(b.gates), time.time() - ts,
            {k: v for k, v in reports[b.id].items() if k in ("overlaps", "on_street", "props_on_street", "in_water")}))
    bridges = build_bridges(G)
    cob, dirt = street_masks(G, builders)
    os.makedirs(OUT_CITIES, exist_ok=True)
    if not only:
        data, meshes, groups = write_instances(G, builders, bridges)
        log("instances: %d records, %d groups, %d meshes" % (len(data), len(groups), len(meshes)))
        Image.fromarray(cob, "L").save(os.path.join(OUT_CITIES, "StreetCobble.png"), optimize=False, compress_level=6)
        Image.fromarray(dirt, "L").save(os.path.join(OUT_CITIES, "StreetDirt.png"), optimize=False, compress_level=6)
        log("masks written (%.1f s)" % (time.time() - t0))
    else:
        log("preview (--sites): Cities.json / .bin, the masks, the sheet and the doc are left untouched")
    plans = []
    if not args.no_plans:
        for b in builders:
            im = render_plan(G, b, cob, dirt, os.path.join(OUT_CITIES, "_Plan_%s.png" % b.id))
            if b.id in ("Ars", "Roa", "Buena", "Sharia", "Rikarisu", "Millishion", "Rapan", "KingDragon", "Wenport"):
                plans.append((b.id, im))
        if not only:
            os.makedirs(os.path.dirname(DOC_IMAGE), exist_ok=True)
            contact_sheet(plans, DOC_IMAGE)
    runtime = time.time() - t0
    if not only:
        write_doc(G, builders, bridges, reports, runtime, DOC_MD)
    for b in builders:
        for n in b.notes:
            log("note: " + n)
    log("done in %.1f s" % runtime)


if __name__ == "__main__":
    main()
