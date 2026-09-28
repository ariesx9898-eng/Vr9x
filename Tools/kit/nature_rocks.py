"""Procedural rocks for the LA PLACE nature kit.

Pipeline per rock: convex-hull chunks (random points on ellipsoids) -> voxel-remesh union (clean,
closed, no self-intersections, concave creases between chunks) -> light smoothing -> displacement
(fBm noise, sedimentary strata ledges, vertical cracks, wind flutes) -> collapse decimation to the
tri budget -> box-projected UVs (1 UV = 1 m) and per-face slots (mossy / snow). Pivot: base centre,
with the rock sunk 8-20 % of its height into the ground so it never floats on slopes.
Obsidian demon crags keep separate razor-sharp convex shards around a remeshed core.
"""
import math
import random

import numpy as np
from mathutils import Vector, Matrix

import nature_lib as L
from nature_lib import Geo, smoothstep, fbm, nz

UP = Vector((0.0, 0.0, 1.0))


# --------------------------------------------------------------------------------------------
# Building blocks
# --------------------------------------------------------------------------------------------
def rand_unit(rng):
    while True:
        v = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
        if 0.05 < v.length <= 1.0:
            return v.normalized()


def hull_chunk(rng, centre, radii, n_pts=18, rot=None, shell=(0.72, 1.0), flat_bottom=None, flat_top=None):
    """Convex hull of random points near an ellipsoid surface (big planar facets)."""
    pts = []
    for _ in range(n_pts):
        d = rand_unit(rng)
        p = Vector((d.x * radii[0], d.y * radii[1], d.z * radii[2])) * rng.uniform(*shell)
        if flat_bottom is not None:
            p.z = max(p.z, -radii[2] * flat_bottom)
        if flat_top is not None:
            p.z = min(p.z, radii[2] * flat_top)
        if rot is not None:
            p = rot @ p
        pts.append(Vector(centre) + p)
    return L.convex_hull_geo(pts)


def prism_chunk(rng, centre, rx, ry, h, n=9, jitter=0.18, top_scale=1.0, rot_z=0.0):
    """Irregular polygonal slab (strata layer): noisy polygon extruded by h, top scaled."""
    pts = []
    a0 = rng.uniform(0, 2 * math.pi)
    for i in range(n):
        a = a0 + 2 * math.pi * i / n + rng.uniform(-0.25, 0.25) * 2 * math.pi / n
        k = 1.0 + rng.uniform(-jitter, jitter)
        x, y = math.cos(a) * rx * k, math.sin(a) * ry * k
        c, s = math.cos(rot_z), math.sin(rot_z)
        x, y = x * c - y * s, x * s + y * c
        pts.append(Vector(centre) + Vector((x, y, 0.0)))
        pts.append(Vector(centre) + Vector((x * top_scale, y * top_scale, h)))
    return L.convex_hull_geo(pts)


def union(geos, voxel):
    g = Geo()
    for x in geos:
        g.merge(x)
    return L.remesh(g, voxel)


def to_np(geo):
    return np.asarray(geo.V, dtype=np.float64)


def set_np(geo, V):
    geo.V = [tuple(p) for p in V]


def displace(geo, fn, mask_below=None):
    """V += N * fn(p, n) for every vertex (fn returns a scalar or a 3-vector)."""
    N = L.vertex_normals_fast(geo)
    V = to_np(geo)
    out = V.copy()
    for i in range(len(V)):
        if mask_below is not None and V[i, 2] < mask_below:
            continue
        r = fn(V[i], N[i])
        if isinstance(r, (float, int)):
            out[i] = V[i] + N[i] * r
        else:
            out[i] = V[i] + np.asarray(r)
    set_np(geo, out)
    return geo


def noise_disp(amp, scale, off, octaves=4, ridge=0.0):
    def f(p, n):
        v = fbm(p, off, scale, octaves)
        if ridge:
            v = (1 - ridge) * v + ridge * (0.5 - abs(v)) * 2
        return amp * v
    return f


def strata_disp(z0, z1, rng, thick=(0.5, 1.4), amp=0.25, warp=0.15, off=(0, 0, 0), tilt=(0.0, 0.0),
                soft=0.14):
    """Sedimentary ledges: layers alternate between protruding (hard) and recessed (soft) bands.
    Pushes along the horizontal part of the normal so tops stay flat."""
    layers = []
    z = z0 - 1.0
    while z < z1 + 1.0:
        t = rng.uniform(*thick)
        hard = rng.random() < 0.55
        a = amp * (rng.uniform(0.6, 1.0) if hard else -rng.uniform(0.3, 0.7))
        layers.append((z, t, a))
        z += t
    zs = [l[0] for l in layers]

    def f(p, n):
        h = math.hypot(n[0], n[1])
        if h < 1e-3:
            return 0.0
        zz = p[2] + tilt[0] * p[0] + tilt[1] * p[1] + warp * fbm(p, off, 0.25, 2)
        # binary search layer
        lo, hi = 0, len(zs) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if zs[mid] <= zz:
                lo = mid
            else:
                hi = mid - 1
        zl, t, a = layers[lo]
        u = (zz - zl) / t
        prof = smoothstep(0.0, soft, u) * (1.0 - smoothstep(1.0 - soft, 1.0, u))
        # blend toward neighbours at the edges so the step is a chamfer, not a crack
        return a * prof * h
    return f


def crack_disp(centre, n_cracks, depth, width, rng, off=(0, 0, 0)):
    """Vertical fractures: narrow inward notches at a few azimuths (wander with height)."""
    az = [rng.uniform(0, 2 * math.pi) for _ in range(n_cracks)]
    ws = [width * rng.uniform(0.6, 1.3) for _ in range(n_cracks)]
    ds = [depth * rng.uniform(0.6, 1.2) for _ in range(n_cracks)]

    def f(p, n):
        h = math.hypot(n[0], n[1])
        th = math.atan2(p[1] - centre[1], p[0] - centre[0])
        s = 0.0
        for a, w, d in zip(az, ws, ds):
            aa = a + 0.12 * fbm((0, 0, p[2]), off, 0.2, 2)
            dd = abs((th - aa + math.pi) % (2 * math.pi) - math.pi)
            if dd < w:
                s += d * (1 - dd / w) ** 2
        return -s * h
    return f


def flute_disp(period, amp, off=(0, 0, 0)):
    """Horizontal wind-erosion grooves (tafoni-like), strongest on vertical faces."""
    def f(p, n):
        h = math.hypot(n[0], n[1])
        zz = p[2] + 0.35 * period * fbm(p, off, 0.3, 2)
        g = 0.5 - 0.5 * math.cos(2 * math.pi * zz / period)
        return -amp * (g ** 3) * h
    return f


def chisel(geo, rng, n_cuts, depth, min_z=-0.2, horiz_bias=0.0, centre=None):
    """Planar chips: vertices beyond a random plane near the surface are projected onto it,
    which carves crisp flat facets into a dense (remeshed) rock."""
    V = to_np(geo)
    c = V.mean(0) if centre is None else np.asarray(centre)
    for _ in range(n_cuts):
        while True:
            n = np.array([rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1)])
            n /= np.linalg.norm(n)
            n[2] *= (1.0 - horiz_bias)
            n /= np.linalg.norm(n)
            if n[2] > min_z:
                break
        d = (V - c) @ n
        cut = d.max() - rng.uniform(*depth)
        over = d > cut
        V[over] -= np.outer(d[over] - cut, n)
    set_np(geo, V)
    return geo


def fit_size(geo, longest=None, height=None):
    """Uniform scale about the pivot so the longest horizontal extent (or visible height) hits the target."""
    V = to_np(geo)
    lo, hi = V.min(0), V.max(0)
    if longest is not None:
        k = longest / max(hi[0] - lo[0], hi[1] - lo[1])
    else:
        k = height / max(1e-6, hi[2])
    V *= k
    set_np(geo, V)
    return geo


def superellipse_fn(a, b, p=3.0, noise_amp=0.0, off=(0, 0, 0), scale=0.5):
    """ring_fn for L.tube: rounded-rectangle cross-section (a along N, b along B) with noise."""
    def fn(i, th, pt, r):
        c, s_ = abs(math.cos(th)), abs(math.sin(th))
        rr = 1.0 / ((c / a) ** p + (s_ / b) ** p + 1e-12) ** (1.0 / p)
        if noise_amp:
            rr *= 1.0 + noise_amp * fbm((pt.x + math.cos(th), pt.y + math.sin(th), pt.z), off, scale, 3)
        return rr * r
    return fn


def clamp_bottom(geo, z_cut):
    V = to_np(geo)
    V[:, 2] = np.maximum(V[:, 2], z_cut)
    set_np(geo, V)
    return geo


def ground(geo, sink_frac, below_max=None):
    """Translate so z=0 sits sink_frac of the height above the lowest point, XY centred."""
    V = to_np(geo)
    lo, hi = V.min(0), V.max(0)
    zg = lo[2] + sink_frac * (hi[2] - lo[2])
    if below_max is not None:
        zg = min(zg, lo[2] + below_max)
    band = V[(V[:, 2] > zg - 0.1 * (hi[2] - lo[2])) & (V[:, 2] < zg + 0.25 * (hi[2] - lo[2]))]
    cx, cy = ((band.min(0) + band.max(0)) * 0.5)[:2] if len(band) else ((lo + hi) * 0.5)[:2]
    V -= np.array([cx, cy, zg])
    set_np(geo, V)
    return geo


def finish(geo, mat, sharp_deg=38):
    """Assign slot + box UVs to every face."""
    for i in range(len(geo.F)):
        geo.M[i] = mat
        geo.S[i] = True
    L.box_uv_all(geo)
    return geo, {"sharp_angle": math.radians(sharp_deg)}


def top_slot(geo, mat_top, threshold, off, noise_amp=0.25, scale=0.8):
    """Per-face slot swap for up-facing faces (moss), with a noisy boundary."""
    FN, CEN, _ = L.face_normals_fast(geo)
    for i in range(len(geo.F)):
        v = FN[i][2] + noise_amp * fbm(CEN[i], off, scale, 3)
        if v > threshold:
            geo.M[i] = mat_top
    return geo


def snow_cap(geo_hi, thickness, threshold, off, target_tris, sink=0.05, noise_amp=0.3, falloff=None,
             mat="MT_Snow"):
    """Snow sheet on the up-facing part of a (high-res) rock: offset along vertex normals,
    thickest in the middle of each snow field, edges tucked `sink` m under the rock surface."""
    FN, CEN, _ = L.face_normals_fast(geo_hi)
    VN = L.vertex_normals_fast(geo_hi)
    V = to_np(geo_hi)
    sel = []
    for i in range(len(geo_hi.F)):
        v = FN[i][2] + noise_amp * fbm(CEN[i], off, 0.5, 3)
        if v > threshold:
            sel.append(i)
    if not sel:
        return None
    # boundary distance via BFS over selected-face vertex graph
    edge_count = {}
    for i in sel:
        f = geo_hi.F[i]
        for k in range(len(f)):
            e = tuple(sorted((f[k], f[(k + 1) % len(f)])))
            edge_count[e] = edge_count.get(e, 0) + 1
    boundary = {v for e, c in edge_count.items() if c == 1 for v in e}
    adj = {}
    for (a, b) in edge_count:
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    import heapq
    dist = {v: 0.0 for v in boundary}
    pq = [(0.0, v) for v in boundary]
    heapq.heapify(pq)
    while pq:
        d, v = heapq.heappop(pq)
        if d > dist.get(v, 1e9):
            continue
        for w in adj.get(v, []):
            nd = d + float(np.linalg.norm(V[v] - V[w]))
            if nd < dist.get(w, 1e9):
                dist[w] = nd
                heapq.heappush(pq, (nd, w))
    fo = falloff or thickness * 2.5
    cap = Geo()
    remap = {}
    for i in sel:
        f = geo_hi.F[i]
        ids = []
        for v in f:
            if v not in remap:
                t = smoothstep(0.0, fo, dist.get(v, fo))
                k = 0.75 + 0.5 * (0.5 + 0.5 * fbm(V[v], off, 0.9, 2))
                off_n = thickness * k * t - sink * (1.0 - t) + 0.01
                remap[v] = cap.vert(V[v] + VN[v] * off_n)
            ids.append(remap[v])
        cap.face(tuple(ids), tuple((0.0, 0.0) for _ in ids), mat, True)
    cap = L.decimate(cap, target_tris, mat)
    for i in range(len(cap.F)):
        cap.M[i] = mat
    L.box_uv_all(cap)
    return cap


def chunk_rock(rng, size, n_chunks=3, aspect=(1.0, 0.85, 0.7), spread=0.35, pts=14, voxel_div=56,
               smooth_it=1, noise_amp=0.018, noise_scale=1.4, flat_bottom=0.55, cuts=7, cut_depth=(0.04, 0.12)):
    """Composite boulder: union of convex chunks, chiselled with planar chips (size ~ longest extent)."""
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geos = []
    R = size * 0.5
    for i in range(n_chunks):
        k = 1.0 if i == 0 else rng.uniform(0.5, 0.8)
        c = Vector((rng.uniform(-1, 1) * spread * R, rng.uniform(-1, 1) * spread * R * 0.8,
                    rng.uniform(-0.2, 0.25) * R * aspect[2])) if i else Vector((0, 0, 0))
        radii = (R * aspect[0] * k * rng.uniform(0.85, 1.05), R * aspect[1] * k * rng.uniform(0.85, 1.05),
                 R * aspect[2] * k * rng.uniform(0.85, 1.1))
        rot = Matrix.Rotation(rng.uniform(0, 2 * math.pi), 3, "Z") @ Matrix.Rotation(rng.uniform(-0.25, 0.25), 3, "X")
        geos.append(hull_chunk(rng, c, radii, pts, rot, shell=(0.9, 1.0), flat_bottom=flat_bottom))
    g = union(geos, size / voxel_div)
    if cuts:
        chisel(g, rng, cuts, (cut_depth[0] * size, cut_depth[1] * size), min_z=-0.1)
    if smooth_it:
        g = L.smooth(g, 0.4, smooth_it)
    if noise_amp:
        g = displace(g, noise_disp(noise_amp * size, noise_scale / size * 2, off, 3, ridge=0.35))
    return g, off


# --------------------------------------------------------------------------------------------
# Generic rocks
# --------------------------------------------------------------------------------------------
def rock_generic(name, variant):
    rng = random.Random(L.seed_of(name))
    cfg = {
        "SM_Rock_Small_A": dict(L=0.45, n=2, aspect=(1.0, 0.8, 0.65), tris=320, sink=0.2),
        "SM_Rock_Small_B": dict(L=0.75, n=3, aspect=(0.9, 0.75, 0.85), tris=420, sink=0.18),
        "SM_Rock_Small_C": dict(L=1.0, n=3, aspect=(1.0, 0.9, 0.78), tris=520, sink=0.2, cluster=True),
        "SM_Rock_Medium_A": dict(L=1.5, n=3, aspect=(1.0, 0.85, 0.7), tris=700, sink=0.15),
        "SM_Rock_Medium_B": dict(L=2.2, n=3, aspect=(0.8, 0.7, 1.05), tris=900, sink=0.12),
        "SM_Rock_Medium_C": dict(L=2.9, n=4, aspect=(1.0, 0.8, 0.6), tris=1100, sink=0.14, moss=True),
        "SM_Rock_Large_A": dict(L=5.2, n=4, aspect=(1.0, 0.8, 0.75), tris=1700, sink=0.12, cuts=10),
        "SM_Rock_Large_B": dict(L=7.6, n=5, aspect=(1.0, 0.75, 0.62), tris=2400, sink=0.12, moss=True, cuts=12),
    }[name]
    if cfg.get("cluster"):
        # three rocks of decreasing size, touching, for scattering along paths and streams
        g = Geo()
        specs = [(0.62, (0.0, 0.0)), (0.44, (0.44, 0.22)), (0.32, (-0.2, 0.45))]
        for s_, (x, y) in specs:
            gi, _ = chunk_rock(rng, 1.0, n_chunks=2, aspect=cfg["aspect"], voxel_div=40, cuts=5)
            ground(gi, 0.2)
            fit_size(gi, longest=s_ * cfg["L"])
            gi.transform(Matrix.Translation((x * cfg["L"], y * cfg["L"], 0.0)))
            g.merge(gi)
        g = L.decimate(g, cfg["tris"])
        ground_xy(g)
        return finish(g, "MT_Rock")
    g, off = chunk_rock(rng, 1.0, cfg["n"], cfg["aspect"], cuts=cfg.get("cuts", 7))
    ground(g, cfg["sink"])
    clamp_bottom(g, -0.3 * cfg["aspect"][2])
    fit_size(g, longest=cfg["L"])
    g = L.decimate(g, cfg["tris"])
    g, info = finish(g, "MT_Rock")
    if cfg.get("moss"):
        top_slot(g, "MT_RockMossy", 0.55, off, 0.3, 1.2 / cfg["L"])
    return g, info


def ground_xy(g):
    V = to_np(g)
    lo, hi = V.min(0), V.max(0)
    V[:, 0] -= (lo[0] + hi[0]) * 0.5
    V[:, 1] -= (lo[1] + hi[1]) * 0.5
    set_np(g, V)


# --------------------------------------------------------------------------------------------
# Cliffs
# --------------------------------------------------------------------------------------------
def cliff(name, variant):
    """Cliff chunk: an irregular massif of blocky chunks, chiselled flat faces, vertical fractures
    and subtle strata; wide enough to line mountain sides and canyon walls."""
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    W, D, H, tris, nblk = {"A": (20.0, 12.0, 14.0, 2600, 7), "B": (16.0, 11.0, 24.0, 2900, 8),
                           "C": (32.0, 16.0, 28.0, 3000, 10)}[variant]
    geos = []
    for i in range(nblk):
        f = i / max(1, nblk - 1)
        x = (f - 0.5) * W * 0.62 + rng.uniform(-0.08, 0.08) * W
        y = rng.uniform(-0.15, 0.15) * D
        hh = H * rng.uniform(0.55, 1.0) * (1.0 - 0.35 * abs(f - 0.5) * 2 if variant != "B" else 1.0)
        rx = W * rng.uniform(0.16, 0.24)
        ry = D * rng.uniform(0.34, 0.46)
        geos.append(hull_chunk(rng, Vector((x, y, hh * 0.5 - 0.12 * H)), (rx, ry, hh * 0.5 + 0.12 * H), 16,
                               Matrix.Rotation(rng.uniform(-0.25, 0.25), 3, "Z") @
                               Matrix.Rotation(rng.uniform(-0.08, 0.08), 3, "Y"), shell=(0.92, 1.0)))
    # low apron blocks at the foot
    for i in range(3):
        x = rng.uniform(-0.4, 0.4) * W
        geos.append(hull_chunk(rng, Vector((x, rng.choice((-1, 1)) * D * 0.35, 0.05 * H)),
                               (W * 0.14, D * 0.25, H * 0.12), 12))
    g = union(geos, max(W, H) / 120)
    chisel(g, rng, 16, (0.02 * H, 0.07 * H), min_z=-0.05, horiz_bias=0.5)
    g = L.smooth(g, 0.4, 1)
    g = displace(g, strata_disp(-0.2 * H, 1.2 * H, rng, thick=(0.03 * H, 0.07 * H), amp=0.012 * H,
                                warp=0.02 * H, off=off, soft=0.2))
    g = displace(g, crack_disp((0, 0), 9 if variant != "B" else 12, 0.03 * W, 0.05, rng, off))
    g = displace(g, noise_disp(0.012 * H, 0.3, off, 4, ridge=0.5))
    ground(g, 0.0)
    g.transform(Matrix.Translation((0, 0, -0.07 * H)))
    fit_size(g, height=H)
    clamp_bottom(g, -0.1 * H)
    g = L.decimate(g, tris)
    return finish(g, "MT_Rock", 40)


# --------------------------------------------------------------------------------------------
# Snow rocks
# --------------------------------------------------------------------------------------------
def rock_snow(name, variant):
    rng = random.Random(L.seed_of(name))
    if variant == "A":
        size, n, aspect, tris, snow_tris = 2.6, 3, (1.0, 0.85, 0.75), 800, 450
    else:
        size, n, aspect, tris, snow_tris = 6.2, 4, (1.0, 0.8, 0.8), 1800, 900
    g, off = chunk_rock(rng, 1.0, n, aspect, voxel_div=64, cuts=9)
    if variant == "B":
        g = displace(g, strata_disp(-1.0, 1.0, rng, thick=(0.08, 0.16), amp=0.025, warp=0.03, off=off))
    ground(g, 0.14)
    clamp_bottom(g, -0.25 * aspect[2])
    fit_size(g, longest=size)
    cap = snow_cap(g, 0.05 * size + 0.04, 0.5 if variant == "A" else 0.36, off, snow_tris, sink=0.02 * size + 0.02,
                   noise_amp=0.3 if variant == "A" else 0.22)
    g = L.decimate(g, tris)
    g, info = finish(g, "MT_RockSnow")
    if cap is not None:
        g.merge(cap)
    return g, info


# --------------------------------------------------------------------------------------------
# Desert sandstone
# --------------------------------------------------------------------------------------------
def stacked(rng, profile, W, D, n_side=11, jitter=0.16, drift=0.05, rot=0.25):
    """Stack of slabs following a radius profile [(z0, thickness, scale)]."""
    geos = []
    cx = cy = 0.0
    for z0, t, sc in profile:
        cx += rng.uniform(-drift, drift) * W
        cy += rng.uniform(-drift, drift) * D
        geos.append(prism_chunk(rng, Vector((cx, cy, z0)), W * 0.5 * sc * rng.uniform(0.92, 1.05),
                                D * 0.5 * sc * rng.uniform(0.92, 1.05), t * 1.06, n=n_side, jitter=jitter,
                                top_scale=rng.uniform(0.92, 1.03), rot_z=rng.uniform(-rot, rot)))
    return geos


def rock_desert(name, variant):
    """Layered sandstone: one weathered mass (chunk union + chisel) banded by strata ledges."""
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    if variant == "A":      # layered sandstone boulder, ~3 m
        Lx, tris, tilt = 3.2, 900, (0.0, 0.0)
        g, off = chunk_rock(rng, 1.0, 3, (1.0, 0.85, 0.85), voxel_div=64, cuts=8, noise_amp=0.01)
        thick, amp = (0.07, 0.12), 0.03
    elif variant == "B":    # flat-topped mesa outcrop, ~8.5 m wide
        Lx, tris, tilt = 7.8, 1500, (0.0, 0.0)
        geos = [hull_chunk(rng, Vector((0, 0, 0.12)), (0.5, 0.4, 0.4), 22, flat_top=0.62, flat_bottom=0.6,
                           shell=(0.92, 1.0)),
                hull_chunk(rng, Vector((0.22, 0.12, 0.05)), (0.3, 0.26, 0.25), 16, flat_top=0.5),
                hull_chunk(rng, Vector((-0.25, -0.1, 0.0)), (0.26, 0.24, 0.2), 16, flat_top=0.5)]
        g = union(geos, 1.0 / 70)
        chisel(g, rng, 8, (0.02, 0.06), min_z=-0.1, horiz_bias=0.8)
        g = L.smooth(g, 0.4, 1)
        thick, amp = (0.045, 0.08), 0.022
    else:                   # tilted strata slab, ~5 m
        Lx, tris, tilt = 5.5, 1300, (0.45, 0.0)
        geos = [hull_chunk(rng, Vector((0, 0, 0.3)), (0.5, 0.32, 0.55), 22, Matrix.Rotation(0.35, 3, "Y"),
                           flat_bottom=0.8, shell=(0.92, 1.0)),
                hull_chunk(rng, Vector((0.25, 0.05, 0.1)), (0.3, 0.3, 0.28), 16)]
        g = union(geos, 1.0 / 70)
        chisel(g, rng, 7, (0.02, 0.07), min_z=-0.1)
        g = L.smooth(g, 0.4, 1)
        thick, amp = (0.035, 0.07), 0.018
    g = displace(g, strata_disp(-1.0, 2.0, rng, thick=thick, amp=amp, warp=0.02, off=off, tilt=tilt, soft=0.18))
    g = displace(g, noise_disp(0.006, 2.5, off, 3))
    ground(g, 0.12)
    clamp_bottom(g, -0.12)
    fit_size(g, longest=Lx)
    g = L.decimate(g, tris)
    return finish(g, "MT_RockDesert", 42)


def rock_arch(name):
    """~20 m natural sandstone arch: a swept rounded-rectangle band over two footing masses,
    voxel-unioned, chiselled and banded with strata."""
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    span, rise = 16.0, 15.0
    n = 28
    pts, radii = [], []
    for i in range(n + 1):
        t = i / n
        a_ = math.pi * (1.0 - t)
        x = span / 2 * math.cos(a_)
        z = -2.0 + (rise + 2.0) * math.sin(a_) ** 0.9
        pts.append(Vector((x, 0.25 * math.sin(3 * t), z)))
        radii.append(1.0)
    geo = Geo()
    thick_fn = lambda t: 2.3 + 2.4 * (1 - math.sin(math.pi * t)) ** 1.6       # half band thickness
    depth_fn = lambda t: 2.6 + 0.9 * (1 - math.sin(math.pi * t))              # half depth

    def rf(i, th, p, r):
        t = i / n
        return superellipse_fn(thick_fn(t), depth_fn(t), 3.0, 0.12, off, 0.35)(i, th, p, r)
    L.tube(geo, pts, radii, "MT_RockDesert", 24, ring_fn=rf, tip="cap", cap_base=True)
    geos = [geo]
    for sgn in (-1, 1):   # footing masses
        geos.append(hull_chunk(rng, Vector((sgn * (span / 2 + 1.2), rng.uniform(-0.5, 0.5), 1.0)), (4.8, 4.4, 4.0),
                               20, Matrix.Rotation(rng.uniform(-0.3, 0.3), 3, "Z"), flat_bottom=0.7,
                               shell=(0.9, 1.0)))
    # hard cap layer over the crown
    geos.append(hull_chunk(rng, Vector((0.4, 0.0, rise + 2.2)), (5.5, 3.2, 1.2), 16, flat_top=0.7))
    g = union(geos, 0.2)
    chisel(g, rng, 10, (0.3, 0.9), min_z=-0.05, horiz_bias=0.4)
    g = L.smooth(g, 0.4, 2)
    g = displace(g, strata_disp(-3, rise + 6, rng, thick=(0.6, 1.3), amp=0.3, warp=0.25, off=off, soft=0.2))
    g = displace(g, noise_disp(0.1, 0.35, off, 3))
    ground(g, 0.0)
    g.transform(Matrix.Translation((0, 0, -1.0)))
    clamp_bottom(g, -1.3)
    g = L.decimate(g, 3000)
    return finish(g, "MT_RockDesert", 42)


def rock_pillar(name, variant):
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geo = Geo()
    if variant == "A":      # sandstone hoodoo, 18 m: flared base, thin neck, hard cap rock
        H = 18.0
        n = 30
        pts = [Vector((0.25 * math.sin(i * 0.4), 0.2 * math.cos(i * 0.3), -2.0 + (H + 1.2) * i / n)) for i in range(n + 1)]

        def prof(t):
            return 3.6 * (1.0 - t) ** 1.7 + 1.15 + 0.15 * math.sin(t * 7.0 + 0.8) * (1 - t)
        radii = [prof(i / n) for i in range(n + 1)]
        L.tube(geo, pts, radii, "MT_RockDesert", 20, ring_fn=superellipse_fn(1.0, 0.85, 2.4, 0.12, off, 0.4),
               tip="cap", cap_base=True)
        cap_c = pts[-1] + Vector((0.3, 0.1, -0.2))
        geos = [geo, hull_chunk(rng, cap_c, (3.2, 2.8, 1.35), 18, flat_top=0.75, flat_bottom=0.7, shell=(0.9, 1.0)),
              hull_chunk(rng, Vector((1.5, 0.6, 0.6)), (3.0, 2.6, 2.4), 16, flat_bottom=0.6)]
        mat, amp, thick, tris = "MT_RockDesert", 0.26, (0.5, 1.9), 2200
    else:                   # 38 m weathered granite spire
        H = 38.0
        n = 40
        pts = [Vector((0.9 * math.sin(i * 0.12), 0.6 * math.sin(i * 0.09 + 1), -2.5 + (H + 1.0) * i / n))
               for i in range(n + 1)]
        radii = [6.2 * (1.0 - i / n) ** 1.25 + 0.6 for i in range(n + 1)]

        def rf(i, th, p, r):
            lobes = 1.0 + 0.16 * math.cos(5 * th + 0.08 * p.z) + 0.08 * math.cos(9 * th + 1.3)
            return r * lobes * (1.0 + 0.1 * fbm((p.x + math.cos(th), p.y + math.sin(th), p.z), off, 0.3, 3))
        L.tube(geo, pts, radii, "MT_Rock", 24, ring_fn=rf, tip="point", tip_len=2.5, cap_base=True)
        geos = [geo]
        for i in range(4):   # buttresses
            a_ = rng.uniform(0, 6.28)
            geos.append(hull_chunk(rng, Vector((math.cos(a_) * 5.0, math.sin(a_) * 5.0, 3.0)), (3.6, 3.0, 6.0), 14,
                                   Matrix.Rotation(a_, 3, "Z"), shell=(0.9, 1.0)))
        mat, amp, thick, tris = "MT_Rock", 0.22, (1.2, 2.6), 2600
    g = union(geos, H / 130)
    chisel(g, rng, 12, (0.02 * H, 0.05 * H), min_z=-0.05, horiz_bias=0.7)
    g = L.smooth(g, 0.4, 1)
    g = displace(g, strata_disp(-3, H + 2, rng, thick=thick, amp=amp, warp=0.2, off=off, soft=0.2))
    if variant == "B":
        g = displace(g, crack_disp((0, 0), 7, 0.35, 0.1, rng, off))
    g = displace(g, noise_disp(0.01 * H, 0.35, off, 3))
    ground(g, 0.0)
    g.transform(Matrix.Translation((0, 0, -1.2)))
    clamp_bottom(g, -1.6)
    g = L.decimate(g, tris)
    return finish(g, mat, 42)


# --------------------------------------------------------------------------------------------
# Demon obsidian crags
# --------------------------------------------------------------------------------------------
def shard(rng, base, direction, length, width):
    """Elongated faceted crystal/obsidian blade (closed convex hull, sharp tip)."""
    d = direction.normalized()
    s1 = L.perp(d)
    s2 = d.cross(s1)
    pts = [base - d * (width * 0.3)]
    rot = rng.uniform(0, 6.28)
    for i in range(6):
        a = rot + 2 * math.pi * i / 6 + rng.uniform(-0.3, 0.3)
        w = width * rng.uniform(0.6, 1.0) * (0.6 if i % 2 else 1.0)
        pts.append(base + (s1 * math.cos(a) + s2 * math.sin(a)) * w)
        pts.append(base + d * length * rng.uniform(0.45, 0.65) + (s1 * math.cos(a) + s2 * math.sin(a)) * w * 0.55)
    pts.append(base + d * length + s1 * rng.uniform(-0.1, 0.1) * width)
    return L.convex_hull_geo(pts)


def rock_demon(name, variant):
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    if variant == "A":      # cluster of tall obsidian shards, ~7 m
        core_size, core_tris, shards = 4.5, 500, [(7.2, 0.9), (5.8, 0.8), (5.0, 0.75), (4.2, 0.6), (3.4, 0.55),
                                                  (2.6, 0.45), (2.2, 0.4), (1.6, 0.35)]
        spread, tilt = 1.2, (0.1, 0.45)
    elif variant == "B":    # jagged crag with leaning blades, ~12 m
        core_size, core_tris, shards = 9.0, 1100, [(12.0, 1.6), (10.5, 1.5), (9.0, 1.3), (8.0, 1.2), (7.0, 1.1),
                                                   (6.0, 1.0), (5.0, 0.9), (4.2, 0.8), (3.5, 0.7), (2.8, 0.6),
                                                   (2.2, 0.5)]
        spread, tilt = 2.4, (0.15, 0.6)
    else:                   # low jagged outcrop, ~3.5 m
        core_size, core_tris, shards = 4.2, 450, [(3.6, 0.6), (3.0, 0.55), (2.6, 0.5), (2.2, 0.45), (1.8, 0.4),
                                                  (1.4, 0.35), (1.2, 0.3)]
        spread, tilt = 1.3, (0.35, 0.9)
    core, _ = chunk_rock(rng, core_size, 4, (1.0, 0.85, 0.5), voxel_div=40, smooth_it=0, noise_amp=0.05,
                         noise_scale=1.2)
    core = displace(core, noise_disp(0.06 * core_size, 0.8 / core_size * 2, off, 2, ridge=0.8))
    ground(core, 0.2)
    clamp_bottom(core, -0.25 * core_size * 0.5)
    core = L.decimate(core, core_tris)
    g = Geo()
    g.merge(core)
    a0 = rng.uniform(0, 6.28)
    for i, (ln, w) in enumerate(shards):
        a = a0 + i * 2.39996
        r = spread * (0.2 + 0.8 * i / len(shards)) * rng.uniform(0.6, 1.1)
        base = Vector((math.cos(a) * r, math.sin(a) * r, -0.35))
        lean = rng.uniform(*tilt) * (0.4 + 0.6 * i / len(shards))
        d = (UP + Vector((math.cos(a), math.sin(a), 0.0)) * math.tan(lean)).normalized()
        g.merge(shard(rng, base, d, ln + 0.6, w))
    ground_xy(g)
    return finish(g, "MT_RockDemon", 25)


# --------------------------------------------------------------------------------------------
# Heaven plateau pale rocks
# --------------------------------------------------------------------------------------------
def rock_pale(name, variant):
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    if variant == "A":      # wind-rounded boulder with flutes, 4.5 m
        g, off = chunk_rock(rng, 1.0, 4, (1.0, 0.8, 0.85), voxel_div=64, smooth_it=4, noise_amp=0.01, cuts=6)
        g = displace(g, flute_disp(0.16, 0.012, off))
        ground(g, 0.12)
        clamp_bottom(g, -0.15)
        fit_size(g, longest=4.6)
        g = L.decimate(g, 1400)
    else:                   # tall wind-carved slab, ~8 m
        g, off = chunk_rock(rng, 1.0, 4, (0.75, 0.42, 1.25), spread=0.3, voxel_div=72, smooth_it=1, noise_amp=0.01,
                            cuts=14, cut_depth=(0.03, 0.1))
        g = displace(g, flute_disp(0.14, 0.01, off))
        ground(g, 0.1)
        clamp_bottom(g, -0.12)
        fit_size(g, height=8.0)
        g = L.decimate(g, 2000)
    return finish(g, "MT_RockPale", 50)


# --------------------------------------------------------------------------------------------
# Log and stump
# --------------------------------------------------------------------------------------------
def jagged_end(geo, ring_ids, centre, axis, depth, rng, off, mat="MT_BarkDead"):
    """Close a tube end with splintered wood: rim pushed along the axis by noise, recessed centre."""
    n = len(ring_ids)
    V = geo.V
    for k, vid in enumerate(ring_ids):
        p = Vector(V[vid])
        a = 2 * math.pi * k / n
        s = depth * (0.35 + 0.65 * abs(math.sin(a * rng.choice([2, 3]) + off[0])) *
                     (0.7 + 0.3 * fbm((math.cos(a), math.sin(a), 0), off, 1.5, 2)))
        geo.V[vid] = tuple(p + axis * s)
    c = geo.vert(centre + axis * depth * 0.15)
    for k in range(n):
        a, b = ring_ids[k], ring_ids[(k + 1) % n]
        f = (a, b, c)
        pa, pb, pc = Vector(geo.V[a]), Vector(geo.V[b]), Vector(geo.V[c])
        if (pb - pa).cross(pc - pa).dot(axis) < 0:
            f = (b, a, c)
        geo.face(f, L.box_uv_face([geo.V[x] for x in f]), mat, False)


def log(name):
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geo = Geo()
    Lg, r0 = 7.2, 0.42
    n = 16
    pts = []
    for i in range(n + 1):
        t = i / n
        x = -Lg / 2 + Lg * t
        pts.append(Vector((x, 0.18 * math.sin(t * 2.6 + 0.5), 0.0)))
    radii = [r0 * (1.0 - 0.25 * (i / n)) for i in range(n + 1)]
    # sink ~20 % of the diameter and follow a gentle sag
    pts = [p + Vector((0, 0, radii[i] * 0.62 - 0.05 * math.sin(math.pi * i / n))) for i, p in enumerate(pts)]

    def rf(i, th, p, r):
        return r * (1.0 + 0.07 * fbm((p.x * 0.6, math.cos(th) * 1.2, math.sin(th) * 1.2), off, 1.0, 3))
    rings = L.tube(geo, pts, radii, "MT_BarkOak", 12, ring_fn=rf, tip="open")
    T, _, _ = L.transport_frames(pts)
    jagged_end(geo, list(reversed(rings[0])), pts[0], -T[0], 0.35, rng, off)
    jagged_end(geo, rings[-1], pts[-1], T[-1], 0.45, rng, off)
    # two broken branch stubs
    for t, az, ln in ((0.35, 1.0, 0.9), (0.7, 2.3, 0.6)):
        i = int(t * n)
        p = pts[i]
        d = (Vector((0.3, math.cos(az), math.sin(az) + 0.4))).normalized()
        bp = [p, p + d * ln * 0.5, p + d * ln]
        L.tube(geo, bp, [radii[i] * 0.35, radii[i] * 0.28, radii[i] * 0.22], "MT_BarkOak", 6, tip="cap",
               tip_len=0.0)
    return geo, {}


def stump(name):
    rng = random.Random(L.seed_of(name))
    off = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geo = Geo()
    H, r0 = 0.95, 0.46
    pts = [Vector((0.0, 0.0, -0.3 + (H + 0.3) * i / 8)) for i in range(9)]
    radii = [r0 * (1.0 + 0.55 * math.exp(-max(0.0, p.z) / 0.25)) for p in pts]
    import nature_trees as TR
    collar = TR.root_collar(5, 0.55, 4, 0.35, rng)

    def rf(i, th, p, r):
        return collar(i, th, p, r) * (1.0 + 0.04 * fbm((math.cos(th), math.sin(th), p.z), off, 1.5, 2))
    rings = L.tube(geo, pts, radii, "MT_BarkOak", 16, ring_fn=rf, tip="open", cap_base=True)
    jagged_end(geo, rings[-1], pts[-1], UP, 0.32, rng, off)
    TR.surface_roots(geo, rng, "MT_BarkOak", r0, 5, (0.9, 1.5), (0.13, 0.2), 0.3, 0.22, sides=6)
    return geo, {}


# --------------------------------------------------------------------------------------------
# Registry
# --------------------------------------------------------------------------------------------
SPECIES = {}
META = {}


def _add(name, fn, style, regions, scatter, budget, hrange, rule="max_extent"):
    SPECIES[name] = fn
    META[name] = dict(style=style, regions=regions, scatter=scatter, budget=budget, hrange=hrange, rule=rule)


ALL_LAND = [1, 2, 3, 4, 5, 6, 10, 11, 12, 15]
for _n, _h in (("Small_A", (0.3, 1.0)), ("Small_B", (0.3, 1.0)), ("Small_C", (0.2, 1.0)),
               ("Medium_A", (1.0, 3.0)), ("Medium_B", (1.0, 3.0)), ("Medium_C", (1.0, 3.0)),
               ("Large_A", (4.0, 8.0)), ("Large_B", (4.0, 8.0))):
    _add("SM_Rock_" + _n, (lambda n: lambda nm: rock_generic(nm, n[-1]))(_n), "Temperate", ALL_LAND, "Rocks",
         (300, 3000), _h)
for _v in "ABC":
    _add("SM_Rock_Cliff_" + _v, (lambda v: lambda n: cliff(n, v))(_v), "Mountain", [3, 4, 7, 12, 14, 6], "Cliffs",
         (300, 3000), (10.0, 30.0), rule="height")
for _v in "AB":
    _add("SM_Rock_Snow_" + _v, (lambda v: lambda n: rock_snow(n, v))(_v), "Northern", [3, 4, 7, 12], "Rocks",
         (300, 3000), (1.0, 8.0))
for _v in "ABC":
    _add("SM_Rock_Desert_" + _v, (lambda v: lambda n: rock_desert(n, v))(_v), "Desert", [13, 14, 5], "Rocks",
         (300, 3000), (1.5, 8.0))
_add("SM_Rock_Arch_A", rock_arch, "Desert", [13, 14], "Landmark", (300, 3000), (16.0, 24.0), rule="height")
_add("SM_Rock_Pillar_A", lambda n: rock_pillar(n, "A"), "Desert", [13, 14], "Landmark", (300, 3000), (15.0, 40.0),
     rule="height")
_add("SM_Rock_Pillar_B", lambda n: rock_pillar(n, "B"), "Mountain", [3, 12, 14, 7], "Landmark", (300, 3000),
     (15.0, 40.0), rule="height")
for _v in "ABC":
    _add("SM_Rock_Demon_" + _v, (lambda v: lambda n: rock_demon(n, v))(_v), "Demon", [8, 9], "Rocks", (300, 3000),
         (2.5, 14.0))
for _v in "AB":
    _add("SM_Rock_Pale_" + _v, (lambda v: lambda n: rock_pale(n, v))(_v), "Heaven", [7], "Rocks", (300, 3000),
         (2.0, 12.0))
_add("SM_Log_A", log, "Temperate", [1, 2, 4, 10, 11, 6], "Rocks", (300, 3000), (0.4, 1.2), rule="height")
_add("SM_Stump_A", stump, "Temperate", [1, 2, 4, 10, 11, 5], "Rocks", (300, 3000), (0.6, 1.6), rule="height")
