"""VFX meshes for spells (Docs/LaPlace/Spec.md section 6: slot 'VFX' unless noted). Unit sizes so the game scales
them. Every mesh is a closed, outward-facing shell (the 'open' funnel and beam are thin double-walled shells so
both sides render with back-face culling)."""
import math

import bmesh
from mathutils import Matrix, Vector

from arch_registry import asset

TAU = math.tau


def surf(g, P, UV, ni, nj, mat, wrap=True, flip=False, smooth=True):
    """Quad grid. P[i][j] (i across the wrap direction, j along), UV[i][j] with ni+1 columns when wrapping."""
    for i in range(ni):
        i1 = (i + 1) % len(P) if wrap else i + 1
        for j in range(nj):
            q = [P[i][j], P[i1][j], P[i1][j + 1], P[i][j + 1]]
            t = [UV[i][j], UV[i + 1][j], UV[i + 1][j + 1], UV[i][j + 1]]
            if flip:
                q = q[::-1]
                t = t[::-1]
            g.face(q, mat, uv=("uv", t), smooth=smooth)


def fan(g, center, ring, uvc, uvr, mat, flip=False, smooth=False):
    n = len(ring)
    for i in range(n):
        j = (i + 1) % n
        q = [center, ring[i], ring[j]]
        t = [uvc, uvr[i], uvr[i + 1] if len(uvr) > n else uvr[j]]
        if flip:
            q = q[::-1]
            t = t[::-1]
        g.face(q, mat, uv=("uv", t), smooth=smooth)


def frames_along(path):
    """Parallel-transport frames (tangent, normal, binormal) along a polyline."""
    P = [Vector(p) for p in path]
    T = []
    for k in range(len(P)):
        a = P[max(k - 1, 0)]
        b = P[min(k + 1, len(P) - 1)]
        T.append((b - a).normalized())
    ref = Vector((0, 0, 1)) if abs(T[0].z) < 0.9 else Vector((1, 0, 0))
    N = [(ref - T[0] * ref.dot(T[0])).normalized()]
    for k in range(1, len(P)):
        n = N[-1] - T[k] * N[-1].dot(T[k])
        if n.length < 1e-6:
            n = N[-1]
        N.append(n.normalized())
    B = [T[k].cross(N[k]) for k in range(len(P))]
    return P, T, N, B


def tube(g, path, radii, segs, mat, smooth=True, squash=None, v0=0.0, v1=1.0, twist=0.0):
    """Closed tube along a path. radii[k] may be 0 at the ends (pointed tips); otherwise ends are capped."""
    P, T, N, B = frames_along(path)
    rings = []
    for k in range(len(P)):
        r = radii[k]
        sq = squash[k] if squash else 1.0
        ring = []
        for i in range(segs):
            a = TAU * i / segs + twist * k / max(len(P) - 1, 1)
            ring.append(P[k] + N[k] * (math.cos(a) * r) + B[k] * (math.sin(a) * r * sq))
        rings.append(ring)
    Pm = [[rings[k][i] for k in range(len(P))] for i in range(segs)]
    UV = [[(i / segs, v0 + (v1 - v0) * k / (len(P) - 1)) for k in range(len(P))] for i in range(segs + 1)]
    with g.part():
        # ring orientation: N x B = T, so increasing angle runs counter-clockwise around T
        surf(g, Pm, UV, segs, len(P) - 1, mat, wrap=True, flip=False, smooth=smooth)
        if radii[0] > 1e-6:
            uvr = [(0.5 + 0.5 * math.cos(TAU * i / segs), 0.5 + 0.5 * math.sin(TAU * i / segs)) for i in range(segs + 1)]
            fan(g, P[0], rings[0], (0.5, 0.5), uvr, mat, flip=True)
        if radii[-1] > 1e-6:
            uvr = [(0.5 + 0.5 * math.cos(TAU * i / segs), 0.5 + 0.5 * math.sin(TAU * i / segs)) for i in range(segs + 1)]
            fan(g, P[-1], rings[-1], (0.5, 0.5), uvr, mat, flip=False)


def bez(p0, p1, p2, p3, n):
    out = []
    for k in range(n + 1):
        t = k / n
        a = (1 - t) ** 3
        b = 3 * (1 - t) ** 2 * t
        c = 3 * (1 - t) * t * t
        d = t ** 3
        out.append(tuple(a * p0[i] + b * p1[i] + c * p2[i] + d * p3[i] for i in range(3)))
    return out


def hull_rock(g, rng, center, radii, npts, mat, jitter=0.25, flat_bottom=None):
    """Convex hull of jittered points on an ellipsoid (faceted rock)."""
    bm = bmesh.new()
    cx, cy, cz = center
    for k in range(npts):
        # stratified directions
        z = 1 - 2 * (k + 0.5) / npts
        r = math.sqrt(max(0.0, 1 - z * z))
        a = k * 2.399963 + rng.uniform(-0.3, 0.3)
        d = Vector((r * math.cos(a), r * math.sin(a), z))
        s = 1.0 + rng.uniform(-jitter, jitter * 0.6)
        p = Vector((cx + d.x * radii[0] * s, cy + d.y * radii[1] * s, cz + d.z * radii[2] * s))
        if flat_bottom is not None and p.z < flat_bottom:
            p.z = flat_bottom + rng.uniform(0.0, 0.004)
        bm.verts.new(p)
    res = bmesh.ops.convex_hull(bm, input=list(bm.verts))
    for v in [v for v in bm.verts if not v.link_faces]:
        bm.verts.remove(v)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    g.add_bmesh(bm, mat, uv="box", smooth=False)
    bm.free()


# ============================================================================ rings and discs

@asset("SM_VFX_Ring", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False, view=(-30, 35),
       notes="Thin torus, major radius 1.0 m, tube radius 0.035 m, lying in the XY plane, centred on the origin. "
             "UV: u = 0..1 around the ring, v = 0..1 around the tube.")
def vfx_ring(g, rng):
    R, r = 1.0, 0.035
    ni, nj = 72, 10
    P = [[((R + r * math.cos(TAU * j / nj)) * math.cos(TAU * i / ni), (R + r * math.cos(TAU * j / nj)) *
           math.sin(TAU * i / ni), r * math.sin(TAU * j / nj)) for j in range(nj + 1)] for i in range(ni)]
    for i in range(ni):
        P[i][nj] = P[i][0]
    UV = [[(i / ni, j / nj) for j in range(nj + 1)] for i in range(ni + 1)]
    with g.part():
        surf(g, P, UV, ni, nj, "VFX")


@asset("SM_VFX_ShockRing", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False, view=(-30, 35),
       notes="Flat annulus in the XY plane: inner radius 0.8 m, outer 1.0 m, 6 mm thick, centred on the origin. "
             "UV: u = 0..1 around, v = 0 at the inner edge to 1 at the outer edge (top and bottom faces).")
def vfx_shockring(g, rng):
    ri, ro, h = 0.8, 1.0, 0.003
    n = 72
    ca = [math.cos(TAU * i / n) for i in range(n + 1)]
    sa = [math.sin(TAU * i / n) for i in range(n + 1)]
    with g.part():
        for i in range(n):
            j = i + 1
            u0, u1 = i / n, j / n
            # top (+Z): radial x tangential = +Z
            g.face([(ri * ca[i], ri * sa[i], h), (ro * ca[i], ro * sa[i], h), (ro * ca[j], ro * sa[j], h),
                    (ri * ca[j], ri * sa[j], h)], "VFX", uv=("uv", [(u0, 0.0), (u0, 1.0), (u1, 1.0), (u1, 0.0)]))
            # bottom (-Z)
            g.face([(ri * ca[i], ri * sa[i], -h), (ro * ca[i], ro * sa[i], -h), (ro * ca[j], ro * sa[j], -h),
                    (ri * ca[j], ri * sa[j], -h)][::-1], "VFX",
                   uv=("uv", [(u0, 0.0), (u0, 1.0), (u1, 1.0), (u1, 0.0)][::-1]))
            # outer rim
            g.face([(ro * ca[i], ro * sa[i], -h), (ro * ca[j], ro * sa[j], -h), (ro * ca[j], ro * sa[j], h),
                    (ro * ca[i], ro * sa[i], h)], "VFX", uv=("uv", [(u0, 0.985), (u1, 0.985), (u1, 1.0), (u0, 1.0)]))
            # inner rim
            g.face([(ri * ca[j], ri * sa[j], -h), (ri * ca[i], ri * sa[i], -h), (ri * ca[i], ri * sa[i], h),
                    (ri * ca[j], ri * sa[j], h)], "VFX", uv=("uv", [(u1, 0.015), (u0, 0.015), (u0, 0.0), (u1, 0.0)]))


@asset("SM_VFX_Disc", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False, view=(-30, 35),
       notes="Flat disc of radius 1.0 m in the XY plane, 4 mm thick, centred on the origin. Planar UV 0..1 "
             "(u = 0.5 + x/2, v = 0.5 + y/2) on both faces.")
def vfx_disc(g, rng):
    r, h = 1.0, 0.002
    n = 64
    ring_t = [(r * math.cos(TAU * i / n), r * math.sin(TAU * i / n), h) for i in range(n)]
    ring_b = [(x, y, -h) for (x, y, _) in ring_t]
    uvr = [(0.5 + 0.5 * math.cos(TAU * i / n), 0.5 + 0.5 * math.sin(TAU * i / n)) for i in range(n + 1)]
    uvr2 = [(0.5 + 0.505 * math.cos(TAU * i / n), 0.5 + 0.505 * math.sin(TAU * i / n)) for i in range(n + 1)]
    with g.part():
        fan(g, (0, 0, h), ring_t, (0.5, 0.5), uvr, "VFX")
        fan(g, (0, 0, -h), ring_b, (0.5, 0.5), uvr, "VFX", flip=True)
        for i in range(n):
            j = (i + 1) % n
            g.face([ring_b[i], ring_b[j], ring_t[j], ring_t[i]], "VFX",
                   uv=("uv", [uvr2[i], uvr2[i + 1], uvr[i + 1], uvr[i]]))


# ============================================================================ blades, flames, funnels

def _arc(sag, t):
    """Circular arc through (0,-1), (sag, 0), (0, 1); t in 0..1 from tip to tip."""
    c = (sag * sag - 1) / (2 * sag)
    R = math.sqrt(c * c + 1)
    a0 = math.atan2(1.0, -c)
    a = -a0 + 2 * a0 * t
    return (c + R * math.cos(a), R * math.sin(a))


@asset("SM_VFX_Crescent", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False, view=(-20, 55),
       notes="Air-blade crescent lying flat in the XY plane: 2.0 m chord along Y, curving toward +X, 0.27 m wide "
             "at the middle, lens-shaped 5 cm thick section with sharp edges; bounding-box centre on the origin. "
             "UV: u = 0..1 along the arc (tip at -Y to tip at +Y), v = 0 inner (concave) edge to 1 outer edge.")
def vfx_crescent(g, rng):
    nt, ns = 36, 6
    so, si = 0.55, 0.28
    top = []
    bot = []
    for k in range(nt + 1):
        t = k / nt
        xo, yo = _arc(so, t)
        xi, yi = _arc(si, t)
        rt, rb = [], []
        for j in range(ns + 1):
            s = j / ns
            x = xi + (xo - xi) * s
            y = yi + (yo - yi) * s
            hgt = 0.025 * (math.sin(math.pi * s) ** 0.8) * (math.sin(math.pi * t) ** 0.6)
            rt.append((x, y, hgt))
            rb.append((x, y, -hgt))
        top.append(rt)
        bot.append(rb)
    with g.part():
        for k in range(nt):
            for j in range(ns):
                uv = [(k / nt, j / ns), (k / nt, (j + 1) / ns), ((k + 1) / nt, (j + 1) / ns), ((k + 1) / nt, j / ns)]
                g.face([top[k][j], top[k][j + 1], top[k + 1][j + 1], top[k + 1][j]], "VFX", uv=("uv", uv),
                       smooth=True)
                g.face([bot[k][j], bot[k][j + 1], bot[k + 1][j + 1], bot[k + 1][j]][::-1], "VFX",
                       uv=("uv", uv[::-1]), smooth=True)


@asset("SM_VFX_Flame", "VFX", "VFX", kind="vfx", pivot="base_point", foundation=0.0, ground=False,
       notes="Teardrop flame tongue 1.0 m tall; origin at the base point (z = 0), tip curling toward +X. "
             "UV: u = 0..1 around, v = 0 at the base to 1 at the tip.")
def vfx_flame(g, rng):
    segs, nz = 24, 18
    P = []
    for i in range(segs):
        a = TAU * i / segs
        col = []
        for k in range(nz + 1):
            t = k / nz
            r = 0.2 * (math.sin(math.pi * min(1.0, t ** 0.55)) ** 1.1) * (1.0 - 0.3 * t)
            r *= 1.0 + 0.08 * math.sin(3 * a + 5.0 * t)
            x = 0.13 * t ** 2.2
            col.append((x + r * math.cos(a), r * math.sin(a) * 0.9, t * 1.0))
        P.append(col)
    UV = [[(i / segs, k / nz) for k in range(nz + 1)] for i in range(segs + 1)]
    with g.part():
        surf(g, P, UV, segs, nz, "VFX")


@asset("SM_VFX_Funnel", "VFX", "VFX", kind="vfx", pivot="base_point", foundation=0.0, ground=False, view=(-30, 18),
       notes="Tornado funnel: open twisted funnel 8.0 m tall, origin at the centre of the bottom ring (radius 1.0 m, "
             "z = 0), flaring to radius 3.0 m at the top; "
             "at the top, five helical lobes (0.35 turns over the height), axis leaning gently; a 3 cm double wall "
             "so both sides render. UV: u = 0..1 around (following the helix), v = 0 at the bottom to 1 at the top, "
             "on both the outer and inner surfaces.")
def vfx_funnel(g, rng):
    segs, nz = 40, 16
    H = 8.0
    wall = 0.03
    turns = 0.35

    def pt(i, k, inner):
        t = k / nz
        z = t * H
        a = TAU * i / segs + TAU * turns * t
        r = 1.0 + 2.0 * t ** 1.6
        r *= 1.0 + 0.07 * math.sin(5 * (TAU * i / segs))
        if inner:
            r -= wall
        cx = 0.35 * math.sin(math.pi * t) + 0.15 * t
        return (cx + r * math.cos(a), r * math.sin(a), z)

    Po = [[pt(i, k, False) for k in range(nz + 1)] for i in range(segs)]
    Pi = [[pt(i, k, True) for k in range(nz + 1)] for i in range(segs)]
    UV = [[(i / segs, k / nz) for k in range(nz + 1)] for i in range(segs + 1)]
    with g.part():
        surf(g, Po, UV, segs, nz, "VFX")
        surf(g, Pi, UV, segs, nz, "VFX", flip=True)
        for i in range(segs):
            j = (i + 1) % segs
            u0, u1 = i / segs, (i + 1) / segs
            # bottom rim (faces down)
            g.face([Po[i][0], Pi[i][0], Pi[j][0], Po[j][0]], "VFX",
                   uv=("uv", [(u0, 0.0), (u0, 0.004), (u1, 0.004), (u1, 0.0)]), smooth=False)
            # top rim (faces up)
            g.face([Po[j][nz], Pi[j][nz], Pi[i][nz], Po[i][nz]], "VFX",
                   uv=("uv", [(u1, 1.0), (u1, 0.996), (u0, 0.996), (u0, 1.0)]), smooth=False)


@asset("SM_VFX_Beam", "VFX", "VFX", kind="vfx", pivot="base", foundation=0.0, ground=False, view=(-30, 25),
       notes="Open cylinder (a 1.5 cm double wall so both sides render), radius 1.0 m, from z = 0 to z = 1.0 m; "
             "scale Z for the beam length. UV: u = 0..1 around, v = 0 at the base to 1 at the top.")
def vfx_beam(g, rng):
    segs = 48
    ro, ri = 1.0, 0.985
    Po = [[(ro * math.cos(TAU * i / segs), ro * math.sin(TAU * i / segs), z) for z in (0.0, 0.5, 1.0)]
          for i in range(segs)]
    Pi = [[(ri * math.cos(TAU * i / segs), ri * math.sin(TAU * i / segs), z) for z in (0.0, 0.5, 1.0)]
          for i in range(segs)]
    UV = [[(i / segs, v) for v in (0.0, 0.5, 1.0)] for i in range(segs + 1)]
    with g.part():
        surf(g, Po, UV, segs, 2, "VFX")
        surf(g, Pi, UV, segs, 2, "VFX", flip=True)
        for i in range(segs):
            j = (i + 1) % segs
            u0, u1 = i / segs, (i + 1) / segs
            g.face([Po[i][0], Pi[i][0], Pi[j][0], Po[j][0]], "VFX",
                   uv=("uv", [(u0, 0.0), (u0, 0.004), (u1, 0.004), (u1, 0.0)]), smooth=False)
            g.face([Po[j][2], Pi[j][2], Pi[i][2], Po[i][2]], "VFX",
                   uv=("uv", [(u1, 1.0), (u1, 0.996), (u0, 0.996), (u0, 1.0)]), smooth=False)


# ============================================================================ dragon

@asset("SM_VFX_DragonSegment", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False,
       view=(-40, 25),
       notes="Smooth tube body segment for the water dragon: radius 0.5 m, 1.0 m long along Y (y = -0.5..0.5), "
             "capped ends, centred on the origin; chain segments 1 m apart along the body curve (the head's neck "
             "ring is also 0.5 m). UV: u = 0..1 around, v = 0 at y = -0.5 to 1 at y = +0.5.")
def vfx_dragon_segment(g, rng):
    segs, nl = 24, 8
    P = [[(0.5 * math.cos(TAU * i / segs), -0.5 + k / nl, 0.5 * math.sin(TAU * i / segs)) for k in range(nl + 1)]
         for i in range(segs)]
    UV = [[(i / segs, k / nl) for k in range(nl + 1)] for i in range(segs + 1)]
    uvr = [(0.5 + 0.5 * math.cos(TAU * i / segs), 0.5 + 0.5 * math.sin(TAU * i / segs)) for i in range(segs + 1)]
    with g.part():
        # angle runs from +X toward +Z; with the tube along +Y this ordering faces inward, so flip
        surf(g, P, UV, segs, nl, "VFX", flip=True)
        fan(g, (0, -0.5, 0), [P[i][0] for i in range(segs)], (0.5, 0.5), uvr, "VFX")
        fan(g, (0, 0.5, 0), [P[i][nl] for i in range(segs)], (0.5, 0.5), uvr, "VFX", flip=True)


def _superellipse_ring(cx, y, cz, hw, hh, n, e=2.6):
    out = []
    for i in range(n):
        a = TAU * i / n
        c, s = math.cos(a), math.sin(a)
        x = hw * math.copysign(abs(c) ** (2 / e), c)
        z = hh * math.copysign(abs(s) ** (2 / e), s)
        out.append((cx + x, y, cz + z))
    return out


def _loft_y(g, stations, n, mat, tip=None, back_cap=True, v_len=None):
    """Loft superelliptic rings along +Y. stations: (y, half_w, half_h, z_centre[, e]). tip: point closing the end."""
    rings = [_superellipse_ring(0.0, s[0], s[3], s[1], s[2], n, s[4] if len(s) > 4 else 2.6) for s in stations]
    y0 = stations[0][0]
    y1 = tip[1] if tip else stations[-1][0]
    L = y1 - y0
    P = [[rings[k][i] for k in range(len(rings))] for i in range(n)]
    UV = [[(i / n, (stations[k][0] - y0) / L) for k in range(len(rings))] for i in range(n + 1)]
    with g.part():
        surf(g, P, UV, n, len(rings) - 1, mat, flip=True)
        if tip:
            fan(g, tip, rings[-1], (0.5, 1.0), [(i / n, (stations[-1][0] - y0) / L) for i in range(n + 1)], mat,
                flip=True, smooth=True)
        if back_cap:
            c = (0.0, stations[0][0], stations[0][3])
            fan(g, c, rings[0], (0.5, 0.0), [(i / n, 0.0) for i in range(n + 1)], mat, smooth=False)


@asset("SM_VFX_DragonHead", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False, view=(-55, 18),
       notes="Stylised eastern water-dragon head, 3.0 m long, facing +Y (snout at +Y), smooth, with a slightly open "
             "jaw, swept-back horns, long whiskers, brow ridges, eyes and fangs; bounding-box centre on the origin. "
             "The neck ring (radius ~0.5 m, matching SM_VFX_DragonSegment) is at the 'neck' anchor. UV: u around, "
             "v along +Y on the head and jaw; u around / v along each horn and whisker.")
def vfx_dragon_head(g, rng):
    n = 24
    # upper head (neck -> snout)
    st = [(-1.5, 0.50, 0.50, 0.0), (-1.2, 0.56, 0.56, 0.06), (-0.85, 0.64, 0.60, 0.14), (-0.45, 0.62, 0.54, 0.16),
          (-0.1, 0.50, 0.40, 0.08), (0.3, 0.40, 0.30, 0.0), (0.75, 0.37, 0.27, -0.04), (1.05, 0.38, 0.27, -0.05),
          (1.3, 0.30, 0.22, -0.07)]
    _loft_y(g, st, n, "VFX", tip=(0.0, 1.47, -0.1))
    g.anchor("neck", (0.0, -1.5, 0.0))
    # lower jaw (slightly open), hinged under the cheeks
    jaw = [(-0.55, 0.40, 0.20, -0.28), (-0.2, 0.38, 0.16, -0.34), (0.3, 0.32, 0.12, -0.44), (0.8, 0.28, 0.10, -0.53),
           (1.12, 0.22, 0.08, -0.59)]
    _loft_y(g, jaw, 16, "VFX", tip=(0.0, 1.27, -0.62))
    # eyes and brow ridges
    for sx in (-1, 1):
        g.sphere(sx * 0.49, -0.42, 0.33, 0.12, 12, 8, "VFX")
        brow = bez((sx * 0.25, -0.1, 0.52), (sx * 0.5, -0.3, 0.62), (sx * 0.62, -0.6, 0.6), (sx * 0.6, -0.95, 0.5), 8)
        tube(g, brow, [0.05, 0.09, 0.1, 0.1, 0.09, 0.08, 0.06, 0.04, 0.0], 8, "VFX", squash=[0.6] * 9)
        # horns: sweep back and up
        horn = bez((sx * 0.32, -0.75, 0.62), (sx * 0.45, -1.2, 1.0), (sx * 0.62, -1.7, 1.1), (sx * 0.78, -2.1, 1.35), 12)
        tube(g, horn, [0.13 * (1 - k / 12) ** 0.8 for k in range(13)], 10, "VFX")
        # whiskers from the snout sides, trailing back
        wh = bez((sx * 0.34, 1.02, -0.08), (sx * 1.1, 1.05, -0.25), (sx * 1.35, 0.1, -0.1), (sx * 1.6, -0.9, 0.2), 16)
        tube(g, wh, [0.045 * (1 - k / 16) ** 0.6 for k in range(17)], 6, "VFX")
        # cheek frills: flat tapered blades sweeping back from the jaw hinge
        for (p0, p1, p2, p3, r0) in (((sx * 0.52, -0.75, -0.12), (sx * 0.85, -1.1, -0.25), (sx * 1.0, -1.5, -0.2),
                                      (sx * 1.15, -1.95, -0.05), 0.11),
                                     ((sx * 0.55, -0.95, 0.18), (sx * 0.85, -1.35, 0.2), (sx * 0.95, -1.7, 0.3),
                                      (sx * 1.02, -2.05, 0.5), 0.085)):
            fr_path = bez(p0, p1, p2, p3, 9)
            tube(g, fr_path, [r0 * (1 - k / 9) ** 0.7 for k in range(10)], 5, "VFX", squash=[0.35] * 10)
        # fangs
        f0 = (sx * 0.2, 1.15, -0.27)
        tube(g, [f0, (sx * 0.2, 1.17, -0.4), (sx * 0.19, 1.18, -0.5)], [0.045, 0.03, 0.0], 6, "VFX")
        # nostril bumps
        g.sphere(sx * 0.16, 1.28, 0.08, 0.07, 10, 6, "VFX")
    # dorsal fins along the top of the head and neck
    for k, (y, h) in enumerate(((-0.5, 0.3), (-0.95, 0.38), (-1.35, 0.3))):
        z = 0.62 if k == 0 else (0.68 if k == 1 else 0.58)
        path = [(0.0, y + 0.1, z - 0.1), (0.0, y - 0.05, z + h * 0.6), (0.0, y - 0.25, z + h)]
        tube(g, path, [0.09, 0.05, 0.0], 6, "VFX", squash=[0.35, 0.35, 0.35])


# ============================================================================ earth magic

@asset("SM_VFX_Spike_A", "VFX", "VFX", kind="vfx", pivot="base_point", foundation=0.0, ground=False,
       notes="Jagged rock spike (earth magic), 3.0 m tall, base radius 0.6 m centred on the origin at z = 0, "
             "slot MT_Rock, faceted; "
             "box-projected UVs (1 UV = 1 m).")
def vfx_spike_a(g, rng):
    _spike(g, rng, 0.0, 0.0, 0.0, 3.0, 0.6, 7, lean=(0.08, 0.05))


@asset("SM_VFX_Spike_B", "VFX", "VFX", kind="vfx", pivot="base_point", foundation=0.0, ground=False,
       notes="Rock spike cluster: a 3.0 m main spike (base radius 0.55 m centred on the origin at z = 0) with two "
             "smaller shards fused at its base; "
             "slot MT_Rock, box-projected UVs.")
def vfx_spike_b(g, rng):
    _spike(g, rng, 0.0, 0.0, 0.0, 3.0, 0.55, 8, lean=(-0.05, 0.1))
    _spike(g, rng, 0.42, -0.25, -0.003, 1.5, 0.32, 6, lean=(0.45, -0.25))
    _spike(g, rng, -0.35, 0.3, -0.006, 1.05, 0.26, 6, lean=(-0.4, 0.3))


def _spike(g, rng, cx, cy, z0, H, R, sides, lean=(0.0, 0.0)):
    nr = 7
    rings = []
    twist = rng.uniform(0.3, 0.7)
    for k in range(nr):
        t = k / nr
        z = z0 + H * t
        rr = R * (1 - t) ** 0.95 * (1.0 + rng.uniform(-0.08, 0.08))
        if k in (2, 4) and rng.random() < 0.8:
            rr *= 0.86  # chipped ledge
        ox = cx + lean[0] * t * H * 0.35
        oy = cy + lean[1] * t * H * 0.35
        ring = []
        for i in range(sides):
            a = TAU * i / sides + twist * t + rng.uniform(-0.12, 0.12)
            ri = rr * (1.0 + rng.uniform(-0.12, 0.12))
            ring.append((ox + ri * math.cos(a), oy + ri * math.sin(a), z))
        rings.append(ring)
    tip = (cx + lean[0] * H * 0.35 + rng.uniform(-0.03, 0.03), cy + lean[1] * H * 0.35, z0 + H)
    with g.part():
        for k in range(nr - 1):
            A, Bn = rings[k], rings[k + 1]
            for i in range(sides):
                j = (i + 1) % sides
                g.face([A[i], A[j], Bn[j]], "MT_Rock", "box", False)
                g.face([A[i], Bn[j], Bn[i]], "MT_Rock", "box", False)
        top = rings[-1]
        for i in range(sides):
            g.face([top[i], top[(i + 1) % sides], tip], "MT_Rock", "box", False)
        g.face(list(reversed(rings[0])), "MT_Rock", "box", False)


@asset("SM_VFX_RockChunk_A", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False,
       notes="Faceted rock chunk ~0.3 m (convex, closed), centred on the origin; slot MT_Rock, box UVs.")
def vfx_chunk_a(g, rng):
    hull_rock(g, rng, (0, 0, 0), (0.16, 0.13, 0.11), 22, "MT_Rock")


@asset("SM_VFX_RockChunk_B", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False,
       notes="Faceted rock chunk ~0.4 m, elongated; slot MT_Rock.")
def vfx_chunk_b(g, rng):
    hull_rock(g, rng, (0, 0, 0), (0.22, 0.14, 0.12), 24, "MT_Rock")


@asset("SM_VFX_RockChunk_C", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False,
       notes="Faceted rock chunk ~0.5 m, flattened slab; slot MT_Rock.")
def vfx_chunk_c(g, rng):
    hull_rock(g, rng, (0, 0, 0), (0.26, 0.22, 0.12), 26, "MT_Rock")


@asset("SM_VFX_RockChunk_D", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False,
       notes="Faceted rock chunk ~0.6 m, blocky; slot MT_Rock.")
def vfx_chunk_d(g, rng):
    hull_rock(g, rng, (0, 0, 0), (0.3, 0.27, 0.24), 30, "MT_Rock", jitter=0.3)


@asset("SM_VFX_EarthWall", "VFX", "VFX", kind="vfx", pivot="base", foundation=0.0, ground=False, view=(-30, 18),
       notes="Earth wall: thick rough stone slab 6.0 m (X) x 1.5 m (Y) x 4.0 m (Z) with a crumbly top edge, "
             "base at z = 0, faces -Y; slot MT_Rock, faceted, box-projected UVs.")
def vfx_earth_wall(g, rng):
    nx = 22
    W, T, H = 6.0, 1.5, 4.0
    # crumbly top profile
    tops = []
    h = 0.0
    for k in range(nx + 1):
        h = 0.6 * h + rng.uniform(-0.25, 0.25)
        notch = 0.45 if rng.random() < 0.18 else 0.0
        tops.append(H - 0.35 - abs(h) - notch + (0.35 if k in (0, nx) else 0.0) * 0.0)
    tops = [min(t, H) for t in tops]
    tops[nx // 2] = H
    secs = []
    for k in range(nx + 1):
        x = -W / 2 + W * k / nx
        zt = tops[k]
        # section in (y, z), counter-clockwise seen from +X: bottom front -> bottom back -> up back -> top -> down front
        def j(a):
            return rng.uniform(-a, a) if 0 < k < nx else 0.0
        fy = -T / 2
        by = T / 2
        pts = [(fy + j(0.03), 0.0), (by + j(0.03), 0.0)]
        for f in (0.3, 0.62):
            pts.append((by - 0.08 * f + j(0.07), zt * f))
        pts.append((by - 0.18 + j(0.06), zt - 0.12 + j(0.05)))
        pts.append((0.25 + j(0.1), zt + j(0.08)))
        pts.append((-0.3 + j(0.1), zt - 0.05 + j(0.08)))
        pts.append((fy + 0.2 + j(0.06), zt - 0.2 + j(0.05)))
        for f in (0.62, 0.3):
            pts.append((fy + 0.08 * f + j(0.07), zt * f))
        secs.append([(x + (j(0.06) if z > 0 else 0.0), y, z) for (y, z) in pts])
    m = len(secs[0])
    with g.part():
        for k in range(nx):
            A, Bn = secs[k], secs[k + 1]
            for i in range(m):
                i1 = (i + 1) % m
                # sections are CCW seen from +X (y right, z up) -> quad A_i, A_i1, B_i1, B_i faces outward
                g.face([A[i], A[i1], Bn[i1]], "MT_Rock", "box", False)
                g.face([A[i], Bn[i1], Bn[i]], "MT_Rock", "box", False)
        g.face(list(reversed(secs[0])), "MT_Rock", "box", False)
        g.face(list(secs[-1]), "MT_Rock", "box", False)


@asset("SM_VFX_Crystal", "VFX", "VFX", kind="vfx", pivot="center", foundation=0.0, ground=False, view=(-35, 20),
       notes="Stone bullet (Stone Cannon): compressed six-sided stone slug 0.5 m long pointing +Z, radius ~0.1 m, "
             "faceted; bounding-box centre on the origin; slot MT_Rock, box-projected UVs.")
def vfx_crystal(g, rng):
    sides = 6
    prof = [(0.07, -0.25), (0.1, -0.215), (0.103, -0.05), (0.098, 0.06), (0.05, 0.19), (0.0, 0.25)]
    rings = []
    for (r, z) in prof:
        ring = []
        for i in range(sides):
            a = TAU * i / sides + (0.08 if z > 0 else 0.0)
            rr = r * (1.0 + rng.uniform(-0.05, 0.05)) if r > 0 else 0.0
            ring.append((rr * math.cos(a), rr * math.sin(a), z))
        rings.append(ring)
    with g.part():
        for k in range(len(rings) - 1):
            A, Bn = rings[k], rings[k + 1]
            for i in range(sides):
                j = (i + 1) % sides
                g.face([A[i], A[j], Bn[j], Bn[i]], "MT_Rock", "box", False)
        g.face(list(reversed(rings[0])), "MT_Rock", "box", False)
