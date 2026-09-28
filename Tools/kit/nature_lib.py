"""Shared helpers for the LA PLACE nature kit (Blender 5.0 used as a Python module).

Geometry is accumulated in a small `Geo` container (vertices, polygons, per-corner UVs, per-face
material slot, optional per-corner custom normals) and turned into one Blender object per asset,
which is exported as GLB. Conventions follow Docs/LaPlace/Spec.md sections 6 and 7:
metres, Z-up (the glTF exporter converts), pivot at the base centre on the ground, transforms
applied, slot names from the palette, solids UV'd at 1 UV unit = 1 m, cards UV'd over 0..1.
"""
import math
import os
import zlib

import numpy as np
import bpy
from mathutils import Vector, Matrix, noise

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KIT_DIR = os.path.join(ROOT, "SourceArt", "Kit")

# --------------------------------------------------------------------------------------------
# Palette (Spec section 6). Base colours are sRGB; they only drive previews / default shading,
# the game re-maps every slot by name to its own material instance.
# --------------------------------------------------------------------------------------------
NATURE_MATERIALS = {
    #  name               sRGB base colour        rough  card
    "MT_BarkOak":       ((0.36, 0.28, 0.21), 0.90, False),
    "MT_BarkBirch":     ((0.86, 0.84, 0.79), 0.80, False),
    "MT_BarkPine":      ((0.46, 0.29, 0.20), 0.90, False),
    "MT_BarkDead":      ((0.56, 0.51, 0.46), 0.90, False),
    "MT_BarkGiant":     ((0.36, 0.34, 0.23), 0.90, False),
    "MT_BarkDemon":     ((0.21, 0.13, 0.17), 0.75, False),
    "MT_LeavesOak":     ((0.29, 0.45, 0.16), 0.80, True),
    "MT_LeavesBirch":   ((0.50, 0.63, 0.22), 0.80, True),
    "MT_NeedlesPine":   ((0.15, 0.30, 0.18), 0.85, True),
    "MT_NeedlesSnow":   ((0.72, 0.78, 0.80), 0.80, True),
    "MT_LeavesGiant":   ((0.20, 0.38, 0.17), 0.80, True),
    "MT_LeavesDemon":   ((0.46, 0.21, 0.54), 0.70, True),
    "MT_LeavesPalm":    ((0.35, 0.50, 0.19), 0.80, True),
    "MT_Grass":         ((0.33, 0.52, 0.18), 0.85, True),
    "MT_GrassDry":      ((0.70, 0.60, 0.37), 0.90, True),
    "MT_Wheat":         ((0.84, 0.69, 0.35), 0.85, True),
    "MT_Flowers":       ((0.86, 0.58, 0.66), 0.80, True),
    "MT_Fern":          ((0.24, 0.45, 0.19), 0.85, True),
    "MT_Reeds":         ((0.50, 0.55, 0.30), 0.85, True),
    "MT_MushroomCap":   ((0.62, 0.27, 0.24), 0.60, False),
    "MT_MushroomStem":  ((0.86, 0.81, 0.69), 0.70, False),
    "MT_Rock":          ((0.47, 0.46, 0.44), 0.90, False),
    "MT_RockMossy":     ((0.38, 0.43, 0.29), 0.90, False),
    "MT_RockSnow":      ((0.40, 0.42, 0.45), 0.85, False),
    "MT_RockDesert":    ((0.74, 0.52, 0.35), 0.90, False),
    "MT_RockDemon":     ((0.17, 0.12, 0.13), 0.35, False),
    "MT_RockPale":      ((0.80, 0.78, 0.73), 0.85, False),
    "MT_Snow":          ((0.92, 0.94, 0.97), 0.60, False),   # architecture palette, used for snow caps
}
ARCH_MATERIALS = [
    "MT_Plaster", "MT_PlasterTan", "MT_Timber", "MT_WoodPlanks", "MT_Stone", "MT_StoneWhite", "MT_Cobble",
    "MT_Sandstone", "MT_DemonRock", "MT_RoofRed", "MT_RoofBlue", "MT_RoofDark", "MT_RoofThatch",
    "MT_RoofSilver", "MT_RoofGreen", "MT_Snow", "MT_ClothRed", "MT_ClothBlue", "MT_ClothGreen",
    "MT_ClothTan", "MT_Hide", "MT_Iron", "MT_Gold", "MT_Glass", "MT_Crystal", "MT_Bone",
]
SPEC_PALETTE = set(NATURE_MATERIALS) | set(ARCH_MATERIALS) | {"VFX"}
CARD_MATERIALS = {k for k, v in NATURE_MATERIALS.items() if v[2]}


def srgb_to_linear(c):
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


def seed_of(name, salt=0):
    return (zlib.crc32(name.encode("utf8")) + 7919 * salt) & 0x7FFFFFFF


# --------------------------------------------------------------------------------------------
# Geometry container
# --------------------------------------------------------------------------------------------
class Geo:
    """Polygon soup with per-corner UVs, per-face material slot and optional custom normals."""

    def __init__(self):
        self.V = []    # [(x, y, z)]
        self.F = []    # [(i0, i1, ...)]
        self.UV = []   # [((u, v), ...)] per face corner
        self.M = []    # [slot name] per face
        self.S = []    # [smooth flag] per face
        self.N = []    # [None | ((nx, ny, nz), ...)] per face corner

    # -- building --------------------------------------------------------------------------
    def vert(self, p):
        self.V.append((float(p[0]), float(p[1]), float(p[2])))
        return len(self.V) - 1

    def face(self, idx, uv, mat, smooth=True, normals=None):
        assert len(idx) == len(uv) >= 3
        self.F.append(tuple(idx))
        self.UV.append(tuple((float(a), float(b)) for a, b in uv))
        self.M.append(mat)
        self.S.append(bool(smooth))
        self.N.append(None if normals is None else tuple(tuple(float(c) for c in n) for n in normals))

    def merge(self, other):
        off = len(self.V)
        self.V.extend(other.V)
        self.F.extend(tuple(i + off for i in f) for f in other.F)
        self.UV.extend(other.UV)
        self.M.extend(other.M)
        self.S.extend(other.S)
        self.N.extend(other.N)
        return self

    def tris(self):
        return sum(len(f) - 2 for f in self.F)

    def tris_by_mat(self):
        out = {}
        for f, m in zip(self.F, self.M):
            out[m] = out.get(m, 0) + len(f) - 2
        return out

    def bounds(self):
        a = np.asarray(self.V)
        return a.min(0), a.max(0)

    def transform(self, mat4):
        """Apply a mathutils Matrix to positions (and rotate custom normals)."""
        m = np.array(mat4)
        a = np.asarray(self.V, dtype=np.float64)
        a = a @ m[:3, :3].T + m[:3, 3]
        self.V = [tuple(p) for p in a]
        r = m[:3, :3]
        det = np.linalg.det(r)
        if det < 0:  # mirrored: keep outward winding
            self.F = [tuple(reversed(f)) for f in self.F]
            self.UV = [tuple(reversed(u)) for u in self.UV]
            self.N = [None if n is None else tuple(reversed(n)) for n in self.N]
        inv_t = np.linalg.inv(r).T
        newN = []
        for n in self.N:
            if n is None:
                newN.append(None)
            else:
                nn = np.asarray(n) @ inv_t.T
                nn /= np.linalg.norm(nn, axis=1, keepdims=True) + 1e-12
                newN.append(tuple(tuple(x) for x in nn))
        self.N = newN
        return self

    def remove_unused_verts(self):
        used = sorted({i for f in self.F for i in f})
        remap = {o: n for n, o in enumerate(used)}
        self.V = [self.V[o] for o in used]
        self.F = [tuple(remap[i] for i in f) for f in self.F]
        return self


# --------------------------------------------------------------------------------------------
# Small vector helpers
# --------------------------------------------------------------------------------------------
def V3(*a):
    if len(a) == 1:
        return Vector(a[0])
    return Vector(a)


def perp(v):
    """A unit vector perpendicular to v."""
    ref = Vector((0, 0, 1)) if abs(v.z) < 0.9 else Vector((1, 0, 0))
    p = v.cross(ref)
    return p.normalized()


def rotate_about(v, axis, ang):
    return Matrix.Rotation(ang, 3, axis.normalized()) @ v


def dir_from_angles(azimuth, elevation):
    ce = math.cos(elevation)
    return Vector((ce * math.cos(azimuth), ce * math.sin(azimuth), math.sin(elevation)))


def lerp(a, b, t):
    return a + (b - a) * t


def smoothstep(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0))) if e1 != e0 else (1.0 if x >= e1 else 0.0)
    return t * t * (3 - 2 * t)


def nz(p, off=(0.0, 0.0, 0.0), scale=1.0):
    """Perlin noise in [-1, 1] (mathutils default permutation -> deterministic)."""
    return noise.noise(Vector(((p[0] + off[0]) * scale, (p[1] + off[1]) * scale, (p[2] + off[2]) * scale)))


def fbm(p, off=(0.0, 0.0, 0.0), scale=1.0, octaves=4, gain=0.5, lac=2.0):
    s = 0.0
    a = 1.0
    f = scale
    norm = 0.0
    for _ in range(octaves):
        s += a * noise.noise(Vector(((p[0] + off[0]) * f, (p[1] + off[1]) * f, (p[2] + off[2]) * f)))
        norm += a
        a *= gain
        f *= lac
    return s / norm


_NV_OFF = ((0.0, 0.0, 0.0), (31.416, 47.853, 12.793), (73.519, 5.237, 91.121))


def nvec(p, off=(0.0, 0.0, 0.0), scale=1.0):
    """Vector noise from three offset scalar Perlin lookups. (mathutils.noise.noise_vector is NOT used:
    its internal offsets are seeded per process, which breaks deterministic builds.)"""
    q = ((p[0] + off[0]) * scale, (p[1] + off[1]) * scale, (p[2] + off[2]) * scale)
    return Vector(tuple(noise.noise(Vector((q[0] + o[0], q[1] + o[1], q[2] + o[2]))) for o in _NV_OFF))


# --------------------------------------------------------------------------------------------
# Primitive builders
# --------------------------------------------------------------------------------------------
def transport_frames(pts):
    """Tangents and parallel-transported normals/binormals along a polyline."""
    n = len(pts)
    T = []
    for i in range(n):
        if i == 0:
            t = pts[1] - pts[0]
        elif i == n - 1:
            t = pts[-1] - pts[-2]
        else:
            a = pts[i] - pts[i - 1]
            b = pts[i + 1] - pts[i]
            t = a.normalized() + b.normalized()
            if t.length < 1e-6:
                t = b
        T.append(t.normalized())
    ref = Vector((1, 0, 0)) if abs(T[0].x) < 0.9 else Vector((0, 1, 0))
    N0 = (ref - T[0] * ref.dot(T[0])).normalized()
    Ns = [N0]
    for i in range(1, n):
        q = T[i - 1].rotation_difference(T[i])
        ni = q @ Ns[-1]
        ni = (ni - T[i] * ni.dot(T[i]))
        if ni.length < 1e-8:
            ni = perp(T[i])
        Ns.append(ni.normalized())
    Bs = [T[i].cross(Ns[i]).normalized() for i in range(n)]
    return T, Ns, Bs


def tube(geo, pts, radii, mat, sides, u_repeat=None, v0=0.0, tip="point", tip_len=None,
         cap_base=False, ring_fn=None, twist=0.0, smooth=True, frames=None, uv_scale=1.0):
    """Tapered tube along `pts` (mathutils Vectors). ring_fn(i, theta, point, radius) -> radius.
    tip: 'point' (apex vertex), 'cap' (flat fan), 'open'. UVs: v = metres along the tube,
    u wraps an integer number (>=1) of 1 m tiles around the base circumference."""
    n = len(pts)
    assert n >= 2 and len(radii) == n
    T, Ns, Bs = frames if frames is not None else transport_frames(pts)
    ring_u = None
    if u_repeat is None:
        circ = 2 * math.pi * radii[0] * uv_scale
        if circ >= 0.75:
            u_repeat = float(round(circ))          # thick limbs: seamless integer number of 1 m tiles
        else:                                      # thin limbs: exact cone unwrap, 1 UV = 1 m everywhere
            ring_u = [max(0.01, 2 * math.pi * r * uv_scale) for r in radii]
            u_repeat = ring_u[0]
    if ring_u is None:
        ring_u = [u_repeat] * n
    rings = []
    vcoord = [v0]
    for i in range(1, n):
        vcoord.append(vcoord[-1] + (pts[i] - pts[i - 1]).length * uv_scale)
    for i in range(n):
        ring = []
        for k in range(sides):
            th = 2 * math.pi * k / sides + twist * i
            off = Ns[i] * math.cos(th) + Bs[i] * math.sin(th)
            r = radii[i]
            if ring_fn is not None:
                r = ring_fn(i, th, pts[i], r)
            ring.append(geo.vert(pts[i] + off * r))
        rings.append(ring)
    for i in range(n - 1):
        ua, ub = ring_u[i], ring_u[i + 1]
        for k in range(sides):
            k2 = (k + 1) % sides
            geo.face((rings[i][k], rings[i][k2], rings[i + 1][k2], rings[i + 1][k]),
                     ((ua * k / sides, vcoord[i]), (ua * (k + 1) / sides, vcoord[i]),
                      (ub * (k + 1) / sides, vcoord[i + 1]), (ub * k / sides, vcoord[i + 1])),
                     mat, smooth)
    if tip == "point":
        L = tip_len if tip_len is not None else max(radii[-1] * 2.0, 0.02)
        apex = geo.vert(pts[-1] + T[-1] * L)
        va = vcoord[-1] + L * uv_scale
        for k in range(sides):
            k2 = (k + 1) % sides
            u0 = ring_u[-1] * k / sides
            u1 = ring_u[-1] * (k + 1) / sides
            # apex fan is flat-shaded: on thin twigs averaged normals would run along the axis
            geo.face((rings[-1][k], rings[-1][k2], apex), ((u0, vcoord[-1]), (u1, vcoord[-1]), ((u0 + u1) / 2, va)),
                     mat, False)
    elif tip == "cap":
        c = geo.vert(pts[-1] + T[-1] * (tip_len or 0.0))
        for k in range(sides):
            k2 = (k + 1) % sides
            a = geo.V[rings[-1][k]]
            b = geo.V[rings[-1][k2]]
            geo.face((rings[-1][k], rings[-1][k2], c), ((a[0], a[1]), (b[0], b[1]),
                                                         (geo.V[c][0], geo.V[c][1])), mat, False)
    if cap_base:
        c = geo.vert(pts[0] - T[0] * 0.0)
        for k in range(sides):
            k2 = (k + 1) % sides
            a = geo.V[rings[0][k]]
            b = geo.V[rings[0][k2]]
            geo.face((rings[0][k2], rings[0][k], c), ((b[0], b[1]), (a[0], a[1]), (geo.V[c][0], geo.V[c][1])),
                     mat, False)
    return rings


def clamp_to_front(n, front, min_dot=0.25):
    """Tilt a shading normal just enough that it stays on the front side of its card."""
    d = n.dot(front)
    if d < min_dot:
        n = n + front * (min_dot - d)
    return n.normalized()


def card(geo, base, up, side, width, height, mat, segs=1, bend=0.0, normal_fn=None, sway=0.0,
         u0=0.0, u1=1.0, v0=0.0, v1=1.0, width_taper=0.0, fold=0.0):
    """Alpha card anchored at `base` (bottom centre), extending along `up` by `height`, `width`
    along `side`. bend: displacement of the tip along the card normal (m), quadratic.
    Front face normal = side x up. UVs span u0..u1, v0..v1 (default whole texture).
    fold: V-fold along the centre line (raises the centre along the normal, m) -> 2 columns."""
    up = up.normalized()
    side = side.normalized()
    nrm = side.cross(up).normalized()
    cols = [0.0, 0.5, 1.0] if fold != 0.0 else [0.0, 1.0]
    rows = []
    for j in range(segs + 1):
        t = j / segs
        centre = base + up * (height * t) + nrm * (bend * t * t) + side * (sway * t * t)
        w = width * (1.0 - width_taper * t)
        row = []
        for c in cols:
            p = centre + side * (w * (c - 0.5))
            if fold != 0.0 and c == 0.5:
                p = p + nrm * fold
            row.append(p)
        rows.append(row)
    ids = [[geo.vert(p) for p in row] for row in rows]
    for j in range(segs):
        for ci in range(len(cols) - 1):
            a, b = ids[j][ci], ids[j][ci + 1]
            c, d = ids[j + 1][ci + 1], ids[j + 1][ci]
            uva = (lerp(u0, u1, cols[ci]), lerp(v0, v1, j / segs))
            uvb = (lerp(u0, u1, cols[ci + 1]), lerp(v0, v1, j / segs))
            uvc = (lerp(u0, u1, cols[ci + 1]), lerp(v0, v1, (j + 1) / segs))
            uvd = (lerp(u0, u1, cols[ci]), lerp(v0, v1, (j + 1) / segs))
            nr = None
            idx, uvs = (a, b, c, d), (uva, uvb, uvc, uvd)
            if normal_fn is not None:
                nr = [normal_fn(Vector(geo.V[x]), nrm) for x in idx]
                front = nrm
                if sum((n_ for n_ in nr), Vector((0, 0, 0))).dot(nrm) < 0:
                    # keep the winding consistent with the shading normals (front face = lit side)
                    idx, uvs, nr = idx[::-1], uvs[::-1], nr[::-1]
                    front = -nrm
                nr = [clamp_to_front(n_, front) for n_ in nr]
            geo.face(idx, uvs, mat, True, nr)
    return nrm


def strip_card(geo, pts, sides, widths, mat, normal_fn=None, fold=None, v0=0.0, v1=1.0):
    """Card following a polyline `pts` (centre line) with per-point side vectors and widths.
    UV u across 0..1, v along v0..v1 (by arc length). Optional fold (list of centre offsets
    along the card normal) makes a V-shaped frond with two columns."""
    n = len(pts)
    lens = [0.0]
    for i in range(1, n):
        lens.append(lens[-1] + (pts[i] - pts[i - 1]).length)
    total = lens[-1] if lens[-1] > 0 else 1.0
    cols = [0.0, 0.5, 1.0] if fold is not None else [0.0, 1.0]
    ids = []
    nrms = []
    for i in range(n):
        tan = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
        s = sides[i].normalized()
        nr = s.cross(tan).normalized()
        nrms.append(nr)
        row = []
        for c in cols:
            p = pts[i] + s * (widths[i] * (c - 0.5))
            if fold is not None and c == 0.5:
                p = p + nr * fold[i]
            row.append(geo.vert(p))
        ids.append(row)
    for i in range(n - 1):
        va = lerp(v0, v1, lens[i] / total)
        vb = lerp(v0, v1, lens[i + 1] / total)
        for ci in range(len(cols) - 1):
            a, b = ids[i][ci], ids[i][ci + 1]
            c, d = ids[i + 1][ci + 1], ids[i + 1][ci]
            nr = None
            idx = (a, b, c, d)
            uvs = ((cols[ci], va), (cols[ci + 1], va), (cols[ci + 1], vb), (cols[ci], vb))
            if normal_fn is not None:
                nr = [normal_fn(Vector(geo.V[x]), nrms[i if x in (a, b) else i + 1]) for x in idx]
                pa, pb, pc = Vector(geo.V[a]), Vector(geo.V[b]), Vector(geo.V[c])
                fnrm = (pb - pa).cross(pc - pa).normalized()
                if sum((n_ for n_ in nr), Vector((0, 0, 0))).dot(fnrm) < 0:
                    idx, uvs, nr = idx[::-1], uvs[::-1], nr[::-1]
                    fnrm = -fnrm
                nr = [clamp_to_front(n_, fnrm) for n_ in nr]
            geo.face(idx, uvs, mat, True, nr)


def lathe(geo, profile, mat, segs, ring_fn=None, close_top=True, close_bottom=False, uv="box",
          centre=(0.0, 0.0), smooth=True, flip=False, frame=None):
    """Surface of revolution around Z (or around `frame` = (origin, x, y, z) axes).
    profile: [(r, z)]; walking the profile with the solid on the left gives outward normals
    (e.g. bottom-to-top along the outside of a dome, or outward along an underside).
    A first/last point with r == 0 becomes an apex (fan), otherwise close_top / close_bottom add a
    flat fan. mat: slot name or list with one slot per profile segment.
    ring_fn(j, theta, r, z) -> (r, z) perturbs rings. UV: box projection (1 UV = 1 m)."""
    cx, cy = centre
    nseg = max(1, len(profile) - 1)
    mats = list(mat) if isinstance(mat, (list, tuple)) else [mat] * nseg
    mats += [mats[-1]] * (nseg - len(mats))
    prof = list(profile)
    j0 = 0
    top_apex = bot_apex = None
    if prof[-1][0] <= 1e-9:
        top_apex = prof.pop()[1]
        close_top = True
    if prof[0][0] <= 1e-9:
        bot_apex = prof.pop(0)[1]
        close_bottom = True
        j0 = 1

    def place(r, th, z):
        if frame is None:
            return (cx + r * math.cos(th), cy + r * math.sin(th), z)
        o, ax, ay, az = frame
        return tuple(o + ax * (r * math.cos(th)) + ay * (r * math.sin(th)) + az * z)
    rings = []
    for j, (r, z) in enumerate(prof):
        ring = []
        for k in range(segs):
            th = 2 * math.pi * k / segs
            rr, zz = (r, z) if ring_fn is None else ring_fn(j + j0, th, r, z)
            ring.append(geo.vert(place(rr, th, zz)))
        rings.append(ring)
    faces = []
    for j in range(len(prof) - 1):
        for k in range(segs):
            k2 = (k + 1) % segs
            f = (rings[j][k], rings[j][k2], rings[j + 1][k2], rings[j + 1][k])
            faces.append((f if not flip else tuple(reversed(f)), mats[min(j + j0, nseg - 1)]))
    if close_top:
        c = geo.vert(place(0.0, 0.0, top_apex if top_apex is not None else prof[-1][1]))
        for k in range(segs):
            k2 = (k + 1) % segs
            f = (rings[-1][k], rings[-1][k2], c)
            faces.append((f if not flip else tuple(reversed(f)), mats[-1]))
    if close_bottom:
        c = geo.vert(place(0.0, 0.0, bot_apex if bot_apex is not None else prof[0][1]))
        for k in range(segs):
            k2 = (k + 1) % segs
            f = (rings[0][k2], rings[0][k], c)
            faces.append((f if not flip else tuple(reversed(f)), mats[0]))
    for f, m in faces:
        geo.face(f, box_uv_face([geo.V[i] for i in f]), m, smooth)
    return rings


def box_uv_face(pts, scale=1.0):
    """Box projection (1 UV unit = 1 m) chosen by the face's dominant normal axis."""
    p = [Vector(x) for x in pts]
    n = Vector((0, 0, 0))
    for i in range(len(p)):
        a, b = p[i], p[(i + 1) % len(p)]
        n.x += (a.y - b.y) * (a.z + b.z)
        n.y += (a.z - b.z) * (a.x + b.x)
        n.z += (a.x - b.x) * (a.y + b.y)
    ax = max(range(3), key=lambda i: abs(n[i]))
    out = []
    for q in p:
        if ax == 0:
            u, v = (q.y if n.x > 0 else -q.y), q.z
        elif ax == 1:
            u, v = (-q.x if n.y > 0 else q.x), q.z
        else:
            u, v = q.x, (q.y if n.z > 0 else -q.y)
        out.append((u * scale, v * scale))
    return out


def box_uv_all(geo, mats=None):
    """Recompute box-projected UVs for all faces (optionally only for faces with slot in mats)."""
    for i, f in enumerate(geo.F):
        if mats is None or geo.M[i] in mats:
            geo.UV[i] = tuple(box_uv_face([geo.V[j] for j in f]))


# --------------------------------------------------------------------------------------------
# Blender round-trips (remesh / smooth / decimate) through evaluated modifier stacks
# --------------------------------------------------------------------------------------------
def geo_to_mesh(geo, name="tmp"):
    me = bpy.data.meshes.new(name)
    me.from_pydata(geo.V, [], geo.F)
    me.update()
    return me


def mesh_to_geo(me, mat="MT_Rock"):
    g = Geo()
    n = len(me.vertices)
    co = np.empty(n * 3)
    me.vertices.foreach_get("co", co)
    g.V = [tuple(x) for x in co.reshape(n, 3)]
    for p in me.polygons:
        vs = tuple(p.vertices)
        g.F.append(vs)
        g.UV.append(tuple((0.0, 0.0) for _ in vs))
        g.M.append(mat)
        g.S.append(True)
        g.N.append(None)
    return g


def apply_modifiers(geo, mods, mat="MT_Rock"):
    """mods: list of (type, {props}). Returns a new Geo with the evaluated result."""
    me = geo_to_mesh(geo, "mod_tmp")
    ob = bpy.data.objects.new("mod_tmp", me)
    bpy.context.scene.collection.objects.link(ob)
    for typ, props in mods:
        m = ob.modifiers.new(typ.lower(), typ)
        for k, v in props.items():
            setattr(m, k, v)
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    me2 = bpy.data.meshes.new_from_object(ev, preserve_all_data_layers=False, depsgraph=dg)
    out = mesh_to_geo(me2, mat)
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.meshes.remove(me)
    bpy.data.meshes.remove(me2)
    return out


def remesh(geo, voxel, mat="MT_Rock", adaptivity=0.0):
    return apply_modifiers(geo, [("REMESH", {"mode": "VOXEL", "voxel_size": voxel, "adaptivity": adaptivity})], mat)


def decimate(geo, target_tris, mat="MT_Rock"):
    cur = geo.tris()
    if cur <= target_tris:
        return geo
    ratio = max(0.001, min(1.0, target_tris / cur))
    return apply_modifiers(geo, [("DECIMATE", {"decimate_type": "COLLAPSE", "ratio": ratio,
                                              "use_collapse_triangulate": True})], mat)


def smooth(geo, factor=0.5, iterations=2, mat="MT_Rock"):
    return apply_modifiers(geo, [("SMOOTH", {"factor": factor, "iterations": iterations})], mat)


def convex_hull_geo(points, mat="MT_Rock"):
    import bmesh
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts[:])
    # drop anything that is not part of the hull surface
    loose = [v for v in bm.verts if not v.link_faces]
    if loose:
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new("hull")
    bm.to_mesh(me)
    bm.free()
    g = mesh_to_geo(me, mat)
    bpy.data.meshes.remove(me)
    return g


def recalc_outward(geo):
    """Make face winding consistent and outward for closed shells (bmesh)."""
    import bmesh
    me = geo_to_mesh(geo)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(me)
    bm.free()
    # rebuild faces preserving per-face data order (bmesh keeps face order)
    newF = [tuple(p.vertices) for p in me.polygons]
    for i, f in enumerate(newF):
        if f != geo.F[i]:
            geo.F[i] = f
            if geo.UV[i] is not None:
                geo.UV[i] = tuple(box_uv_face([geo.V[j] for j in f]))
    bpy.data.meshes.remove(me)
    return geo


def vertex_normals(geo):
    """Area-weighted vertex normals (numpy)."""
    V = np.asarray(geo.V)
    N = np.zeros_like(V)
    for f in geo.F:
        p0 = V[f[0]]
        for k in range(1, len(f) - 1):
            n = np.cross(V[f[k]] - p0, V[f[k + 1]] - p0)
            for idx in (f[0], f[k], f[k + 1]):
                N[idx] += n
    N /= np.linalg.norm(N, axis=1, keepdims=True) + 1e-12
    return N


def vertex_normals_fast(geo):
    """Vertex normals computed by Blender (much faster than the Python loop for big meshes)."""
    me = geo_to_mesh(geo, "vn_tmp")
    n = len(me.vertices)
    arr = np.empty(n * 3)
    me.vertex_normals.foreach_get("vector", arr)
    bpy.data.meshes.remove(me)
    return arr.reshape(n, 3)


def face_normals_fast(geo):
    me = geo_to_mesh(geo, "fn_tmp")
    n = len(me.polygons)
    arr = np.empty(n * 3)
    me.polygon_normals.foreach_get("vector", arr)
    cen = np.empty(n * 3)
    me.polygons.foreach_get("center", cen)
    area = np.empty(n)
    me.polygons.foreach_get("area", area)
    bpy.data.meshes.remove(me)
    return arr.reshape(n, 3), cen.reshape(n, 3), area


def face_normals(geo):
    V = np.asarray(geo.V)
    out = []
    for f in geo.F:
        n = np.zeros(3)
        p0 = V[f[0]]
        for k in range(1, len(f) - 1):
            n += np.cross(V[f[k]] - p0, V[f[k + 1]] - p0)
        a = np.linalg.norm(n)
        out.append(n / a if a > 0 else n)
    return np.asarray(out)


def face_areas(geo):
    V = np.asarray(geo.V)
    out = []
    for f in geo.F:
        n = np.zeros(3)
        p0 = V[f[0]]
        for k in range(1, len(f) - 1):
            n += np.cross(V[f[k]] - p0, V[f[k + 1]] - p0)
        out.append(0.5 * np.linalg.norm(n))
    return np.asarray(out)


def triangulate(geo):
    """Fan-triangulate every polygon (keeps UVs / normals / materials)."""
    g = Geo()
    g.V = list(geo.V)
    for f, uv, m, s, n in zip(geo.F, geo.UV, geo.M, geo.S, geo.N):
        for k in range(1, len(f) - 1):
            g.F.append((f[0], f[k], f[k + 1]))
            g.UV.append((uv[0], uv[k], uv[k + 1]))
            g.M.append(m)
            g.S.append(s)
            g.N.append(None if n is None else (n[0], n[k], n[k + 1]))
    return g


def drop_degenerate(geo, min_area=1e-7):
    keep = [i for i, a in enumerate(face_areas(geo)) if a > min_area and len(set(geo.F[i])) == len(geo.F[i])]
    g = Geo()
    g.V = geo.V
    g.F = [geo.F[i] for i in keep]
    g.UV = [geo.UV[i] for i in keep]
    g.M = [geo.M[i] for i in keep]
    g.S = [geo.S[i] for i in keep]
    g.N = [geo.N[i] for i in keep]
    return g.remove_unused_verts()


# --------------------------------------------------------------------------------------------
# Scene / materials / export
# --------------------------------------------------------------------------------------------
def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.objects):
        for b in list(coll):
            try:
                coll.remove(b)
            except Exception:
                pass


def export_material(name):
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    col, rough, is_card = NATURE_MATERIALS.get(name, ((0.5, 0.5, 0.5), 0.8, False))
    m = bpy.data.materials.new(name)
    lin = srgb_to_linear(col)
    m.diffuse_color = (*lin, 1.0)
    m.roughness = rough
    m.use_backface_culling = not is_card     # cards export as doubleSided
    nt = m.node_tree
    bsdf = next((n for n in nt.nodes if n.bl_idname == "ShaderNodeBsdfPrincipled"), None)
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = (*lin, 1.0)
        bsdf.inputs["Roughness"].default_value = rough
        bsdf.inputs["Metallic"].default_value = 0.0
    return m


def flatten_folded(geo, min_dot=0.12):
    """Safety net: a smooth face whose averaged vertex normals swing (almost) behind the face -
    sharp bends on thin tubes - is flat-shaded so its exported normals agree with its winding.
    Faces with custom normals (cards) are left alone."""
    V = np.asarray(geo.V)
    VN = np.zeros_like(V)
    FN = []
    for f, sm in zip(geo.F, geo.S):
        p0 = V[f[0]]
        n = np.zeros(3)
        for k in range(1, len(f) - 1):
            n += np.cross(V[f[k]] - p0, V[f[k + 1]] - p0)
        FN.append(n)
        if sm:
            for i in f:
                VN[i] += n
    VN /= np.linalg.norm(VN, axis=1, keepdims=True) + 1e-12
    count = 0
    for k, (f, sm, cn) in enumerate(zip(geo.F, geo.S, geo.N)):
        if not sm or cn is not None:
            continue
        n = FN[k] / (np.linalg.norm(FN[k]) + 1e-12)
        if min(float(VN[i] @ n) for i in f) < min_dot:
            geo.S[k] = False
            count += 1
    return count


def clamp_cards_floor(geo, floor=-0.03):
    """Card vertices never go deeper than `floor` (frond / leaf tips rest on the ground instead of
    cutting into it). Cards own their vertices, so solids are never touched."""
    idx = {i for f, m in zip(geo.F, geo.M) if m in CARD_MATERIALS for i in f}
    n = 0
    for i in idx:
        x, y, z = geo.V[i]
        if z < floor:
            geo.V[i] = (x, y, floor)
            n += 1
    return n


def build_object(geo, name, sharp_angle=None):
    """Create a Blender mesh object from Geo (materials assigned by slot name)."""
    geo = drop_degenerate(geo)
    flatten_folded(geo)
    me = bpy.data.meshes.new(name)
    me.from_pydata(geo.V, [], geo.F)
    # material slots in first-use order
    slots = []
    for m in geo.M:
        if m not in slots:
            slots.append(m)
    for s in slots:
        me.materials.append(export_material(s))
    me.polygons.foreach_set("material_index", [slots.index(m) for m in geo.M])
    me.polygons.foreach_set("use_smooth", geo.S)
    uvl = me.uv_layers.new(name="UVMap")
    flat = [c for uv in geo.UV for pair in uv for c in pair]
    uvl.data.foreach_set("uv", flat)
    me.update()
    if sharp_angle is not None:
        me.set_sharp_from_angle(angle=sharp_angle)
    if any(n is not None for n in geo.N):
        loops = []
        for f, n in zip(geo.F, geo.N):
            if n is None:
                loops.extend([(0.0, 0.0, 0.0)] * len(f))
            else:
                loops.extend(n)
        me.normals_split_custom_set(loops)
    me.validate(clean_customdata=False)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, geo


def export_glb(ob, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLB", use_selection=True, export_apply=True, export_yup=True,
        export_materials="EXPORT", export_normals=True, export_texcoords=True, export_tangents=False,
        export_vertex_color="NONE", export_animations=False, export_cameras=False, export_lights=False,
        export_extras=False, export_skins=False, export_morph=False,
    )


def mesh_stats(geo):
    lo, hi = geo.bounds()
    return {
        "tris": geo.tris(),
        "min": [round(float(x), 3) for x in lo],
        "max": [round(float(x), 3) for x in hi],
        "footprint": [round(float(hi[0] - lo[0]), 2), round(float(hi[1] - lo[1]), 2)],
        "height": round(float(hi[2]), 2),
        "slots": sorted(set(geo.M)),
    }
