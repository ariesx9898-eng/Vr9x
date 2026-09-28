"""Procedural plants for the LA PLACE nature kit.

Bushes reuse the broadleaf grower from nature_trees (multi-stem, low envelope). Grass, wheat,
flower and reed clumps are crossed / fanned blade cards (UVs cover the whole 0..1 texture, V up).
Ferns and the giant demon plants use arched strip cards; mushrooms are lathed solids with a gill
underside. Everything is seeded from the asset name.
"""
import math
import random

from mathutils import Vector, Matrix

import nature_lib as L
from nature_lib import Geo, smoothstep, fbm, dir_from_angles
import nature_trees as TR

UP = Vector((0.0, 0.0, 1.0))


# --------------------------------------------------------------------------------------------
# Blade cards
# --------------------------------------------------------------------------------------------
def blade(geo, base, yaw, lean, w, h, mat, segs=2, curl=0.25, shear=None, taper=0.1):
    """Card anchored at `base`, facing the horizontal direction `yaw` (front face), leaning
    `lean` rad toward its front and curling a further `curl` rad toward the tip.
    UV: u 0..1 across, v 0..1 bottom->top (the whole texture)."""
    n0 = Vector((math.cos(yaw), math.sin(yaw), 0.0))
    side = UP.cross(n0).normalized()
    p = Vector(base)
    rows = []
    step = h / segs
    for j in range(segs + 1):
        t = j / segs
        c = p.copy()
        if shear is not None:
            c += shear * (t * t)
        ww = w * (1.0 - taper * t)
        rows.append([geo.vert(c - side * (ww * 0.5)), geo.vert(c + side * (ww * 0.5))])
        a = lean + curl * (t + 0.5 / segs)
        p = p + (UP * math.cos(a) + n0 * math.sin(a)) * step
    for j in range(segs):
        a, b = rows[j]
        d, c = rows[j + 1]
        geo.face((a, b, c, d), ((0.0, j / segs), (1.0, j / segs), (1.0, (j + 1) / segs), (0.0, (j + 1) / segs)),
                 mat, True)


def fan_clump(geo, rng, centre, n, mat, h, w, spread, lean=(0.1, 0.45), curl=(0.1, 0.35), segs=2,
              h_jit=(0.75, 1.1), centre_cards=3, shear=None):
    """Crossed centre cards + a fan of outward-facing cards around them."""
    golden = 2.39996
    cnt = 0
    for i in range(centre_cards):
        yaw = math.pi * i / centre_cards + rng.uniform(-0.15, 0.15)
        blade(geo, centre + Vector((rng.uniform(-0.04, 0.04), rng.uniform(-0.04, 0.04), -0.02)), yaw,
              rng.uniform(0.0, 0.08), w * rng.uniform(0.9, 1.15), h * rng.uniform(0.95, 1.12), mat, segs,
              rng.uniform(0.02, 0.12), shear=shear)
        cnt += 1
    off = rng.uniform(0, 2 * math.pi)
    for i in range(n - centre_cards):
        az = off + golden * i
        r = spread * math.sqrt((i + 0.5) / max(1, n - centre_cards)) * rng.uniform(0.7, 1.0)
        pos = centre + Vector((math.cos(az) * r, math.sin(az) * r, -0.02))
        yaw = az + rng.uniform(-0.7, 0.7)
        blade(geo, pos, yaw, rng.uniform(*lean), w * rng.uniform(0.8, 1.2), h * rng.uniform(*h_jit), mat, segs,
              rng.uniform(*curl), shear=shear)
        cnt += 1
    return cnt


# --------------------------------------------------------------------------------------------
# Grass family
# --------------------------------------------------------------------------------------------
def grass(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    c = Vector((0, 0, 0))
    if variant == "A":      # medium meadow clump
        fan_clump(geo, rng, c, 14, "MT_Grass", 0.6, 0.55, 0.32)
    elif variant == "B":    # tall grass
        fan_clump(geo, rng, c, 14, "MT_Grass", 1.0, 0.6, 0.35, lean=(0.08, 0.35), curl=(0.15, 0.4), segs=3)
    else:                   # low, wide lawn patch
        fan_clump(geo, rng, c, 20, "MT_Grass", 0.36, 0.6, 0.7, lean=(0.25, 0.7), curl=(0.1, 0.3), centre_cards=2)
    return geo, {}


def grass_dry(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    if variant == "A":      # dry tuft
        fan_clump(geo, rng, Vector((0, 0, 0)), 14, "MT_GrassDry", 0.62, 0.55, 0.3, lean=(0.15, 0.55))
    else:                   # tall steppe grass
        fan_clump(geo, rng, Vector((0, 0, 0)), 16, "MT_GrassDry", 1.05, 0.62, 0.4, lean=(0.1, 0.4),
                  curl=(0.2, 0.5), segs=3)
    return geo, {}


def snow_mound(geo, rng, centre, radius, height, segs=12):
    """Low lens of snow (closed), sunk slightly into the ground; the rim feathers out at ground level."""
    off = (rng.uniform(0, 50), rng.uniform(0, 50), 0)
    prof = [(0.0, -0.06), (radius, -0.02), (radius * 0.92, 0.01), (radius * 0.72, height * 0.42),
            (radius * 0.42, height * 0.84), (0.0, height)]

    def rf(j, th, r, z):
        k = 1.0 + 0.18 * fbm((math.cos(th), math.sin(th), 0.0), off, 1.3, 2)
        return r * k, z
    L.lathe(geo, [(r, z + centre.z) for r, z in prof], "MT_Snow", segs, ring_fn=rf, centre=(centre.x, centre.y))


def grass_snow(name):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    snow_mound(geo, rng, Vector((0, 0, 0)), 0.42, 0.13)
    fan_clump(geo, rng, Vector((0, 0, 0.02)), 12, "MT_GrassDry", 0.52, 0.45, 0.24, lean=(0.1, 0.45))
    return geo, {}


def wheat(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    grid = 5
    size = 2.0
    margin = 0.27
    h = 1.05 if variant == "A" else 0.95
    wind = None
    if variant == "B":
        a = rng.uniform(0, 2 * math.pi)
        wind = Vector((math.cos(a), math.sin(a), 0.0)) * 0.22
    span = size - 2 * margin
    for gx in range(grid):
        for gy in range(grid):
            x = -span / 2 + span * (gx + 0.5) / grid + rng.uniform(-0.1, 0.1)
            y = -span / 2 + span * (gy + 0.5) / grid + rng.uniform(-0.1, 0.1)
            yaw = rng.uniform(0, math.pi)
            for k in range(2):
                blade(geo, Vector((x, y, -0.03)), yaw + k * math.pi / 2 + rng.uniform(-0.2, 0.2),
                      rng.uniform(-0.05, 0.12), rng.uniform(0.58, 0.68), h * rng.uniform(0.9, 1.06), "MT_Wheat", 2,
                      rng.uniform(0.0, 0.12), shear=wind)
    return geo, {}


def flowers(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    c = Vector((0, 0, 0))
    if variant == "A":      # low wildflower clump
        fan_clump(geo, rng, c, 8, "MT_Grass", 0.3, 0.45, 0.25, lean=(0.2, 0.55))
        fan_clump(geo, rng, c, 8, "MT_Flowers", 0.38, 0.45, 0.28, lean=(0.05, 0.35), centre_cards=2)
    elif variant == "B":    # tall wildflowers
        fan_clump(geo, rng, c, 8, "MT_Grass", 0.45, 0.5, 0.3, lean=(0.15, 0.45))
        fan_clump(geo, rng, c, 10, "MT_Flowers", 0.8, 0.55, 0.32, lean=(0.03, 0.25), segs=2, centre_cards=3)
    else:                   # wide meadow patch
        for k in range(3):
            a = 2 * math.pi * k / 3 + rng.uniform(-0.3, 0.3)
            cc = Vector((math.cos(a) * 0.45, math.sin(a) * 0.45, 0.0))
            fan_clump(geo, rng, cc, 5, "MT_Grass", 0.35, 0.5, 0.25, lean=(0.2, 0.5), centre_cards=1)
            fan_clump(geo, rng, cc, 6, "MT_Flowers", 0.5, 0.5, 0.28, lean=(0.05, 0.35), centre_cards=2)
    return geo, {}


def reeds(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    if variant == "A":
        fan_clump(geo, rng, Vector((0, 0, 0)), 12, "MT_Reeds", 1.8, 0.65, 0.35, lean=(0.03, 0.2),
                  curl=(0.05, 0.25), segs=3)
    else:   # denser, taller bed of cattails
        for k in range(2):
            a = rng.uniform(0, 2 * math.pi)
            cc = Vector((math.cos(a), math.sin(a), 0.0)) * 0.35 * k
            fan_clump(geo, rng, cc, 10, "MT_Reeds", 2.4, 0.75, 0.4, lean=(0.03, 0.18), curl=(0.05, 0.22),
                      segs=3)
    return geo, {}


# --------------------------------------------------------------------------------------------
# Ferns
# --------------------------------------------------------------------------------------------
def frond_card(geo, rng, base, az, el, length, width, mat, segs=4, gravity=0.3, fold=-0.1, twist=0.0,
               normal_fn=None, taper=(0.5, 1.0)):
    d = dir_from_angles(az, el)
    pts = TR.grow(base, d, length, length / segs, rng, gravity_bend=gravity, min_segs=segs)
    sides = []
    for j in range(len(pts)):
        tan = (pts[min(j + 1, len(pts) - 1)] - pts[max(j - 1, 0)]).normalized()
        s = tan.cross(UP)
        if s.length < 1e-3:
            s = L.perp(tan)
        if twist:
            s = Matrix.Rotation(twist * j / len(pts), 3, tan) @ s
        sides.append(s.normalized())
    widths = [width * (taper[0] + (taper[1] - taper[0]) * math.sin(math.pi * min(1.0, 0.15 + 0.9 * j / (len(pts) - 1))))
              for j in range(len(pts))]
    L.strip_card(geo, pts, sides, widths, mat, normal_fn=normal_fn,
                 fold=[w * fold for w in widths] if fold else None)
    return pts


def fern(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    n, length, width = (11, 0.95, 0.42) if variant == "A" else (15, 1.45, 0.6)
    golden = 2.39996
    off = rng.uniform(0, 6.28)
    for i in range(n):
        f = i / (n - 1)
        el = math.radians(72 - 42 * f + rng.uniform(-6, 6))
        frond_card(geo, rng, Vector((0, 0, -0.02)), off + golden * i, el, length * rng.uniform(0.8, 1.1),
                   width * rng.uniform(0.9, 1.1), "MT_Fern", segs=4, gravity=0.55 + 0.4 * f, fold=-0.12,
                   twist=rng.uniform(-0.3, 0.3))
    return geo, {}


# --------------------------------------------------------------------------------------------
# Bushes (broadleaf grower at shrub scale)
# --------------------------------------------------------------------------------------------
def _stems(rng, n, lean=(0.35, 0.8), spread=0.12, h=(0.8, 1.0)):
    out = []
    a0 = rng.uniform(0, 2 * math.pi)
    for i in range(n):
        a = a0 + 2 * math.pi * i / n + rng.uniform(-0.3, 0.3)
        r = spread * rng.uniform(0.2, 1.0)
        out.append(dict(off=(math.cos(a) * r, math.sin(a) * r), lean=rng.uniform(*lean), az=a, h=rng.uniform(*h),
                        r=rng.uniform(0.8, 1.1)))
    return out


def bush_params(rng, height, width, depth, leaves, bark, n_stems, R, n, size, fill, stem_r=0.045):
    return dict(
        bark=bark, leaves=leaves, sides=[5, 4, 3], height=height, crown_base=0.06, crown_w=width, crown_d=depth,
        trunk_h=0.5, trunk_r=stem_r, below=0.12, flare=0.0, trunk_taper=0.5, trunk_seg=0.25, wiggle=0.03,
        lump=0.18, lump_scale=1.6, flat_bottom=0.5, stems=_stems(rng, n_stems),
        levels=[dict(n=3, t=(0.35, 1.0), angle=(25, 55), len_env=(0.6, 0.95), r_rel=0.6, seg=0.22, tropism=0.1,
                     wander=0.5, tip_r=0.4, max_len=1.3, min_len=0.15, min_r=0.012, min_tip_r=0.008,
                     outward_bias=0.4)],
        foliage=dict(R=R, n=n, size=size, along=[1.0], up_bias=0.3, tilt=0.55, fill=fill, fill_gap=0.95,
                     min_gap=0.7, max_env=1.2, normal_w=(0.62, 0.23, 0.15), fill_min_z=-0.55, tip_levels=[0, 1],
                     wire_r=0.012, wire_max=1.5, min_z=0.18))


def bush(name, variant):
    rng = random.Random(L.seed_of(name) + 1)
    if variant == "A":      # round leafy bush
        P = bush_params(rng, 1.6, 2.2, 2.0, "MT_LeavesOak", "MT_BarkOak", 5, 0.42, 6, 0.8, 55)
    elif variant == "B":    # low, wide spreading bush
        P = bush_params(rng, 1.0, 2.8, 2.2, "MT_LeavesBirch", "MT_BarkOak", 6, 0.38, 6, 0.72, 60)
        P["stems"] = _stems(rng, 6, lean=(0.9, 1.6), spread=0.15)
    elif variant == "C":    # flowering bush
        P = bush_params(rng, 1.4, 1.9, 1.8, "MT_LeavesOak", "MT_BarkOak", 5, 0.4, 6, 0.75, 50)
    else:                   # tall upright shrub
        P = bush_params(rng, 2.5, 1.7, 1.5, "MT_LeavesOak", "MT_BarkOak", 4, 0.42, 6, 0.8, 60)
        P["stems"] = _stems(rng, 4, lean=(0.12, 0.3), spread=0.1)
        P["trunk_h"] = 0.75
    geo, info = TR.broadleaf(name, P)
    if variant == "C":
        _sprinkle(geo, rng, P, "MT_Flowers", 22, 0.5)
    return geo, info


def _sprinkle(geo, rng, P, mat, count, size):
    """Cards facing outward on the upper shell of the bush envelope (flowers / snow tufts)."""
    H, cb = P["height"], P["crown_base"] * P["height"]
    c = Vector((0, 0, (cb + H) * 0.5))
    r = Vector((P["crown_w"] * 0.5, P["crown_d"] * 0.5, (H - cb) * 0.5))
    golden = math.pi * (3 - math.sqrt(5))
    for i in range(count):
        z = 1 - 2 * (i + 0.5) / count
        z = z * 0.8 + 0.15
        rr = math.sqrt(max(0.0, 1 - z * z))
        a = golden * i + rng.uniform(-0.3, 0.3)
        d = Vector((rr * math.cos(a), rr * math.sin(a), z))
        p = c + Vector((d.x * r.x, d.y * r.y, d.z * r.z)) * rng.uniform(0.78, 0.92)
        nrm = (d + Vector((rng.gauss(0, 0.3), rng.gauss(0, 0.3), 0.2))).normalized()
        TR.quad(geo, p, nrm, UP + d * 0.3, size * rng.uniform(0.8, 1.2), size * rng.uniform(0.8, 1.2), mat)


def snow_lump(geo, rng, centre, rx, ry, h, segs=10, drape=None):
    """Closed, irregular cap of snow draped over foliage: domed middle, thin rim sagging `drape` m."""
    off = (rng.uniform(0, 50), rng.uniform(0, 50), 0)
    drape = h * 0.9 if drape is None else drape
    prof = [(0.0, -h * 0.3), (0.62, -h * 0.28), (1.0, -h * 0.1), (0.96, h * 0.08), (0.8, h * 0.45), (0.52, h * 0.8),
            (0.24, h * 0.96), (0.0, h)]

    def rf(j, th, r, z):
        k = 1.0 + 0.22 * fbm((math.cos(th), math.sin(th), 0.0), off, 1.2, 2)
        sag = drape * (min(1.0, r) ** 2) * (0.8 + 0.4 * (0.5 + 0.5 * fbm((math.cos(th), math.sin(th), 3.0), off, 1.5, 2)))
        return r * k, z - sag
    g = Geo()
    L.lathe(g, prof, "MT_Snow", segs, ring_fn=rf)
    m = Matrix.Translation(centre) @ Matrix.Diagonal((rx, ry, 1.0, 1.0))
    g.transform(m)
    L.box_uv_all(g)
    geo.merge(g)


def _top_at(geo, x, y, radius, below=None):
    """Highest vertex z within `radius` of (x, y) (optionally only vertices under `below`)."""
    best = None
    for vx, vy, vz in geo.V:
        if (vx - x) ** 2 + (vy - y) ** 2 <= radius * radius and (below is None or vz < below):
            best = vz if best is None else max(best, vz)
    return best


def bush_snow(name):
    rng = random.Random(L.seed_of(name) + 1)
    P = bush_params(rng, 1.5, 2.0, 1.8, "MT_NeedlesSnow", "MT_BarkPine", 5, 0.42, 7, 0.8, 55)
    P["foliage"]["normal_w"] = (0.65, 0.2, 0.15)
    geo, info = TR.broadleaf(name, P)
    # snow resting on top of the shrub: one broad lump on the crown + two smaller ones on the shoulders,
    # each seated a little below the local top of the foliage cards so it reads as lying *on* the needles
    spots = [(0.05, -0.05, 0.9, 0.8, 0.26)]
    for k in range(2):
        a = rng.uniform(0, 2 * math.pi) + k * math.pi
        spots.append((math.cos(a) * 0.62, math.sin(a) * 0.55, 0.45, 0.4, 0.16))
    for x, y, rx, ry, h in spots:
        zt = _top_at(geo, x, y, 0.35) or P["height"]
        snow_lump(geo, rng, Vector((x, y, zt - 0.22)), rx, ry, h)
    return geo, info


def bush_desert(name, variant):
    rng = random.Random(L.seed_of(name) + 1)
    if variant == "A":      # round dry scrub
        P = bush_params(rng, 1.0, 1.5, 1.4, "MT_GrassDry", "MT_BarkDead", 6, 0.26, 3, 0.5, 0, stem_r=0.035)
    else:                   # wider creosote-like scrub with upright stems
        P = bush_params(rng, 1.5, 2.2, 1.8, "MT_GrassDry", "MT_BarkDead", 7, 0.28, 3, 0.55, 0, stem_r=0.04)
        P["stems"] = _stems(rng, 7, lean=(0.25, 0.6), spread=0.14)
    P["levels"] = [dict(P["levels"][0], n=4, wander=1.0, max_len=0.9),
                   dict(n=1, t=(0.4, 1.0), angle=(30, 60), to_env=False, len_rel=(0.3, 0.6), r_rel=0.6, seg=0.15,
                        wander=1.1, tip_r=0.4, max_len=0.5, min_len=0.1, min_r=0.008, min_tip_r=0.006)]
    P["foliage"] = dict(P["foliage"], keep_p=0.8, tip_levels=[1, 2], min_gap=0.9, tuft=True, n=4)
    return TR.broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Giant demon plants
# --------------------------------------------------------------------------------------------
def _curl_stalk(rng, base, height, lean_az, lean, curl_r, curl_turns, seg=0.25):
    """Stalk rising (with lean) then curling into a crook / fiddlehead at the top."""
    d_lean = Vector((math.cos(lean_az), math.sin(lean_az), 0.0))
    pts = []
    n = max(6, int(height / seg))
    for i in range(n + 1):
        t = i / n
        pts.append(base + UP * (height * t) + d_lean * (lean * height * t * t))
    # crook: continue along a shrinking circle in the vertical plane of d_lean
    top = pts[-1]
    tan = (pts[-1] - pts[-2]).normalized()
    side = tan.cross(d_lean).normalized() if tan.cross(d_lean).length > 1e-3 else L.perp(tan)
    k = int(12 * curl_turns)
    ang = 0.0
    p = top
    dirv = tan
    step = 2 * math.pi * curl_r / 12
    for i in range(k):
        f = i / max(1, k)
        ang = math.radians(30) * (1.0 + 0.9 * f)
        dirv = (Matrix.Rotation(-ang, 3, side) @ dirv).normalized()
        p = p + dirv * step * (1.0 - 0.55 * f)
        pts.append(p)
    return pts


def demon_plant(name, variant):
    rng = random.Random(L.seed_of(name))
    noff = (rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(0, 50))
    geo = Geo()
    golden = 2.39996

    def nfn_from(c):
        def fn(p, n):
            q = p - c
            return (q.normalized() * 0.55 + UP * 0.25 + n * 0.3).normalized() if q.length > 1e-6 else n
        return fn

    if variant == "A":      # tentacle-frond rosette with a fiddlehead stalk, ~3.4 m
        # swollen base bulb
        L.lathe(geo, [(0.0, -0.3), (0.62, -0.2), (0.7, 0.2), (0.5, 0.55), (0.22, 0.8), (0.0, 0.86)], "MT_BarkDemon",
                12)
        off = rng.uniform(0, 6.28)
        for i in range(10):
            f = i / 9
            frond_card(geo, rng, Vector((0, 0, 0.45)), off + golden * i, math.radians(70 - 35 * f),
                       rng.uniform(3.0, 3.9), rng.uniform(1.0, 1.3), "MT_LeavesDemon", segs=8, gravity=0.42 + 0.25 * f,
                       fold=-0.16, twist=rng.uniform(-0.5, 0.5), normal_fn=nfn_from(Vector((0, 0, 1.2))))
        pts = _curl_stalk(rng, Vector((0, 0, 0.3)), 2.6, rng.uniform(0, 6.28), 0.12, 0.35, 1.3)
        TR.tube(geo, pts, TR.taper(len(pts), 0.13, 0.03), "MT_BarkDemon", 8, tip="point")
    elif variant == "B":    # lantern stalks with drooping purple tassels, ~4.8 m
        env = TR.Envelope((0, 0, 3.0), (2.5, 2.5, 2.2))
        a0 = rng.uniform(0, 6.28)
        for k in range(4):
            az = a0 + 2 * math.pi * k / 4 + rng.uniform(-0.4, 0.4)
            h = rng.uniform(3.2, 4.4)
            base = Vector((math.cos(az) * 0.25, math.sin(az) * 0.25, -0.2))
            pts = _curl_stalk(rng, base, h, az, rng.uniform(0.15, 0.3), rng.uniform(0.35, 0.5), 0.55)
            TR.tube(geo, pts, TR.taper(len(pts), rng.uniform(0.11, 0.15), 0.04), "MT_BarkDemon", 7, tip="point")
            tip = pts[-1]
            TR.leaf_clump(geo, tip + Vector((0, 0, -0.2)), 0.55, 6, 1.1, "MT_LeavesDemon", rng, env, up_bias=-0.2,
                          tilt=0.5, droop=0.8, normal_w=(0.3, 0.5, 0.2))
            # hanging tassels
            for t in range(3):
                a = rng.uniform(0, 6.28)
                nrm = Vector((math.cos(a), math.sin(a), 0.0))
                TR.quad(geo, tip + Vector((0, 0, -0.35)), nrm, -UP, 0.55, rng.uniform(1.2, 1.8), "MT_LeavesDemon",
                        base_anchor=True, normal_fn=nfn_from(tip))
        # basal leaves
        for i in range(7):
            frond_card(geo, rng, Vector((0, 0, 0.0)), a0 + golden * i, math.radians(rng.uniform(25, 45)),
                       rng.uniform(1.4, 1.9), rng.uniform(0.7, 0.9), "MT_LeavesDemon", segs=4, gravity=0.5,
                       fold=-0.14, normal_fn=nfn_from(Vector((0, 0, 0.5))))
    else:                   # spiky rosette with a tall flowering spike, ~4.6 m
        off = rng.uniform(0, 6.28)
        for i in range(16):
            f = i / 15
            frond_card(geo, rng, Vector((0, 0, -0.05)), off + golden * i, math.radians(68 - 45 * f),
                       rng.uniform(1.9, 2.7), rng.uniform(0.55, 0.75), "MT_LeavesDemon", segs=5, gravity=0.12 + 0.2 * f,
                       fold=-0.2, taper=(0.9, 1.0), normal_fn=nfn_from(Vector((0, 0, 0.6))))
        pts = TR.grow(Vector((0, 0, -0.2)), (UP + Vector((0.08, 0.05, 0))).normalized(), 4.4, 0.3, rng, wander=0.3,
                      noise_off=noff, min_segs=12)
        TR.tube(geo, pts, TR.taper(len(pts), 0.16, 0.035), "MT_BarkDemon", 8, tip="point")
        env = TR.Envelope((0, 0, 3.6), (1.0, 1.0, 1.2))
        spike = TR.Branch(pts, TR.taper(len(pts), 0.16, 0.035), 1)
        for j, t in enumerate((0.55, 0.68, 0.8, 0.92, 1.0)):
            p, d, _ = spike.point_at(t)
            TR.leaf_clump(geo, p, 0.35 + 0.1 * (1 - t), 4, 0.8, "MT_LeavesDemon", rng, env, up_bias=0.2, tilt=0.5,
                          normal_w=(0.5, 0.3, 0.2))
    return geo, {}


# --------------------------------------------------------------------------------------------
# Giant mushrooms
# --------------------------------------------------------------------------------------------
def mushroom(geo, rng, base, height, cap_r, stem_r, lean_az, lean, cap_h=0.42, skirt=True, segs=20):
    noff = (rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(0, 50))
    d_lean = Vector((math.cos(lean_az), math.sin(lean_az), 0.0))
    Hc = cap_h * cap_r
    z_under = height - Hc * 0.95
    n = 10
    pts = []
    for i in range(n + 1):
        t = i / n
        z = -0.3 + (z_under + Hc * 0.4 + 0.3) * t
        pts.append(base + UP * z + d_lean * (lean * max(0.0, z) ** 1.6 / max(1.0, height) ** 0.6))
    radii = []
    for p in pts:
        h = max(0.0, p.z - base.z)
        radii.append(stem_r * (1.0 - 0.22 * h / height) * (1.0 + 0.75 * math.exp(-h / (0.12 * height + 0.1))))

    def stem_rf(i, th, p, r):
        return r * (1.0 + 0.06 * fbm((math.cos(th) * 1.5, math.sin(th) * 1.5, p.z * 0.6), noff, 1.0, 2))
    L.tube(geo, pts, radii, "MT_MushroomStem", 14, ring_fn=stem_rf, tip="point", tip_len=0.1, cap_base=True)
    # cap frame at the stem top
    T, _, _ = L.transport_frames(pts)
    top_i = max(i for i, p in enumerate(pts) if p.z - base.z <= z_under + 1e-6)
    origin = pts[top_i]
    az = (T[-1] * 0.6 + UP * 0.4).normalized()
    ax = L.perp(az)
    ay = az.cross(ax).normalized()
    R = cap_r
    prof = [(stem_r * 0.6, Hc * 0.12), (0.45 * R, Hc * 0.03), (0.82 * R, -0.02 * Hc), (0.97 * R, 0.0),
            (1.0 * R, 0.14 * Hc), (0.93 * R, 0.4 * Hc), (0.74 * R, 0.72 * Hc), (0.42 * R, 0.94 * Hc), (0.0, Hc)]
    mats = ["MT_MushroomStem"] * 3 + ["MT_MushroomCap"] * 5

    def cap_rf(j, th, r, z):
        k = 1.0 + 0.06 * fbm((math.cos(th), math.sin(th), 0.0), noff, 1.4, 2) if j >= 3 else 1.0
        wob = 0.05 * Hc * fbm((math.cos(th) * 2, math.sin(th) * 2, 1.0), noff, 1.0, 2) if 3 <= j <= 5 else 0.0
        return r * k, z + wob
    L.lathe(geo, prof, mats, segs, ring_fn=cap_rf, frame=(origin, ax, ay, az))
    if skirt:
        zs = height * 0.62
        # stem axis point at that height
        k = min(range(len(pts)), key=lambda i: abs(pts[i].z - base.z - zs))
        o = pts[k]
        rs = radii[k]
        sp = [(rs * 0.95, -0.1 * rs), (rs * 1.5, -0.3 * rs), (rs * 1.72, -0.36 * rs), (rs * 1.62, -0.22 * rs),
              (rs * 1.3, -0.08 * rs), (rs * 0.95, 0.02 * rs)]
        L.lathe(geo, sp, "MT_MushroomStem", 14, close_top=False, frame=(o, Vector((1, 0, 0)), Vector((0, 1, 0)),
                                                                        T[k]))
    return origin


def mushroom_giant(name, variant):
    rng = random.Random(L.seed_of(name))
    geo = Geo()
    if variant == "A":
        mushroom(geo, rng, Vector((0, 0, 0)), 5.6, 2.7, 0.46, rng.uniform(0, 6.28), 0.35, cap_h=0.42)
    else:
        specs = [(3.9, 1.7, 0.33, 0.0, 0.0, 0.25), (2.9, 1.3, 0.26, 1.35, 0.4, 0.5), (2.2, 1.0, 0.21, 1.1, 2.3, 0.55),
                 (1.5, 0.7, 0.16, 1.2, 4.2, 0.6)]
        for h, cr, sr, dist, az, lean in specs:
            b = Vector((math.cos(az) * dist, math.sin(az) * dist, 0.0))
            mushroom(geo, rng, b, h, cr, sr, az if dist else rng.uniform(0, 6.28), lean, cap_h=0.45,
                     skirt=h > 2.5, segs=16)
    return geo, {}


# --------------------------------------------------------------------------------------------
# Registry
# --------------------------------------------------------------------------------------------
SPECIES = {}
META = {}


def _add(name, fn, style, regions, scatter, budget, hrange):
    SPECIES[name] = fn
    META[name] = dict(style=style, regions=regions, scatter=scatter, budget=budget, hrange=hrange)


TEMP = [1, 2, 5, 11, 10, 6]
for _v in "ABCD":
    _add("SM_Bush_" + _v, (lambda v: lambda n: bush(n, v))(_v), "Temperate", TEMP, "Bushes", (150, 2000), (0.8, 3.0))
_add("SM_Bush_Snow_A", bush_snow, "Northern", [3, 4, 7, 12], "Bushes", (150, 2000), (0.8, 2.5))
for _v in "AB":
    _add("SM_Bush_Desert_" + _v, (lambda v: lambda n: bush_desert(n, v))(_v), "Desert", [5, 13, 14, 8], "Bushes",
         (150, 2000), (0.6, 2.0))
for _v in "ABC":
    _add("SM_Plant_DemonGiant_" + _v, (lambda v: lambda n: demon_plant(n, v))(_v), "Demon", [8, 9], "Bushes",
         (150, 3000), (2.0, 5.2))
for _v in "AB":
    _add("SM_Mushroom_Giant_" + _v, (lambda v: lambda n: mushroom_giant(n, v))(_v), "Demon", [8, 9, 10], "Bushes",
         (150, 3000), (2.0, 6.0))
for _v in "AB":
    _add("SM_Fern_" + _v, (lambda v: lambda n: fern(n, v))(_v), "GreatForest", [10, 6, 11, 2], "Grass", (8, 399),
         (0.4, 1.6))
for _v in "ABC":
    _add("SM_Grass_" + _v, (lambda v: lambda n: grass(n, v))(_v), "Temperate", [1, 2, 3, 5, 6, 10, 11, 12, 15],
         "Grass", (8, 399), (0.25, 1.2))
for _v in "AB":
    _add("SM_Grass_Dry_" + _v, (lambda v: lambda n: grass_dry(n, v))(_v), "Desert", [5, 8, 13, 14], "Grass",
         (8, 399), (0.3, 1.2))
_add("SM_Grass_Snow_A", grass_snow, "Northern", [3, 4, 7, 12], "Grass", (8, 399), (0.3, 0.8))
for _v in "AB":
    _add("SM_Wheat_" + _v, (lambda v: lambda n: wheat(n, v))(_v), "Farmland", [1, 2, 11], "Grass", (8, 399),
         (0.8, 1.2))
for _v in "ABC":
    _add("SM_Flowers_" + _v, (lambda v: lambda n: flowers(n, v))(_v), "Temperate", [1, 2, 11, 10], "Flowers",
         (8, 399), (0.25, 1.0))
for _v in "AB":
    _add("SM_Reeds_" + _v, (lambda v: lambda n: reeds(n, v))(_v), "Wetland", [1, 2, 6, 10, 11], "Grass", (8, 399),
         (1.2, 2.8))
