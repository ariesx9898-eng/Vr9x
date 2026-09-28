"""Procedural mesh builder for the LA PLACE architecture and VFX kit.

`Geo` accumulates polygons for one asset. Geometry is organised in *parts*: each part is welded on its own and is
normally a closed, outward-facing shell. Parts are allowed to interpenetrate (a beam sunk into a wall, a chimney
through a roof) but must never share a plane with another part where they overlap: every contact between two parts is
made by embedding one into the other by a few centimetres, so no two faces are ever coincident (no z-fighting).

Conventions: metres, Z up, building fronts face -Y. Polygons are listed counter-clockwise as seen from outside.
UV modes per face: 'box' (world box projection, 1 UV = 1 m), 'slope' (planar along roof slopes, u horizontal,
v up-slope, 1 UV = 1 m), ('cyl', centre, axis) cylindrical (u = arc length, v = height), ('uv', [(u, v), ...])
explicit per-corner UVs, ('planar', o, U, V) planar projection.
"""
import math
from contextlib import contextmanager

from mathutils import Matrix, Vector
from mathutils import geometry as mgeo

WELD = 1e-5
TAU = math.tau


def vec(p):
    if isinstance(p, Vector) and len(p) == 3:
        return p.copy()
    return Vector((p[0], p[1], p[2] if len(p) > 2 else 0.0))


def newell(pts):
    n = Vector((0.0, 0.0, 0.0))
    k = len(pts)
    for i in range(k):
        a = pts[i]
        b = pts[(i + 1) % k]
        n.x += (a.y - b.y) * (a.z + b.z)
        n.y += (a.z - b.z) * (a.x + b.x)
        n.z += (a.x - b.x) * (a.y + b.y)
    return n


def plane_basis(n):
    n = n.normalized()
    ref = Vector((0, 0, 1)) if abs(n.z) < 0.9 else Vector((1, 0, 0))
    u = ref.cross(n).normalized()
    v = n.cross(u).normalized()
    return u, v


def area2d(p):
    s = 0.0
    for i in range(len(p)):
        a = p[i]
        b = p[(i + 1) % len(p)]
        s += a[0] * b[1] - b[0] * a[1]
    return 0.5 * s


def is_convex2d(p):
    sign = 0
    k = len(p)
    for i in range(k):
        a, b, c = p[i], p[(i + 1) % k], p[(i + 2) % k]
        cr = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
        if abs(cr) < 1e-12:
            continue
        s = 1 if cr > 0 else -1
        if sign == 0:
            sign = s
        elif s != sign:
            return False
    return True


def ccw(p):
    """Return polygon (2D) in counter-clockwise order."""
    return list(p) if area2d(p) > 0 else list(reversed(p))


def cw(p):
    return list(reversed(ccw(p)))


def circle2d(cx, cy, r, n, a0=0.0, a1=None, closed=True):
    if a1 is None:
        return [(cx + r * math.cos(a0 + TAU * i / n), cy + r * math.sin(a0 + TAU * i / n)) for i in range(n)]
    cnt = n if not closed else n
    return [(cx + r * math.cos(a0 + (a1 - a0) * i / cnt), cy + r * math.sin(a0 + (a1 - a0) * i / cnt))
            for i in range(cnt + 1)]


def offset_poly2d(p, d):
    """Offset a CCW polygon outward by d (miter joins). Negative d shrinks."""
    k = len(p)
    out = []
    for i in range(k):
        a = Vector((p[i - 1][0], p[i - 1][1]))
        b = Vector((p[i][0], p[i][1]))
        c = Vector((p[(i + 1) % k][0], p[(i + 1) % k][1]))
        e1 = (b - a).normalized()
        e2 = (c - b).normalized()
        n1 = Vector((e1.y, -e1.x))
        n2 = Vector((e2.y, -e2.x))
        m = n1 + n2
        if m.length < 1e-9:
            m = n1
        m.normalize()
        cosh = m.dot(n1)
        s = d / max(cosh, 0.2)
        q = b + m * s
        out.append((q.x, q.y))
    return out


class Frame:
    """Local planar frame: origin o, in-plane axes u (right) and v (up), normal n = u x v (outward)."""

    __slots__ = ("o", "u", "v", "n")

    def __init__(self, o, u, v, n=None):
        self.o = vec(o)
        self.u = vec(u).normalized()
        self.v = vec(v).normalized()
        self.n = vec(n).normalized() if n is not None else self.u.cross(self.v).normalized()

    def p(self, a, b, c=0.0):
        return self.o + self.u * a + self.v * b + self.n * c

    def shifted(self, du=0.0, dv=0.0, dn=0.0):
        return Frame(self.p(du, dv, dn), self.u, self.v, self.n)

    def matrix(self):
        m = Matrix.Identity(4)
        for r in range(3):
            m[r][0] = self.u[r]
            m[r][1] = self.v[r]
            m[r][2] = self.n[r]
            m[r][3] = self.o[r]
        return m


class Geo:
    def __init__(self, name="asset"):
        self.name = name
        self.V = []
        self.F = []
        self.FM = []
        self.FU = []
        self.FS = []
        self.parts = []
        self._M = [Matrix.Identity(4)]
        self._flip = [False]
        self._uv = ["box"]
        self._smooth = [False]
        self._cur = None
        self.anchors = {}
        self._bury = 0

    def buried(self, z):
        """A foundation depth slightly different on every call (2 mm steps), so buried bottom faces of
        overlapping parts never share a plane."""
        self._bury += 1
        return z - 0.002 * self._bury

    # ------------------------------------------------------------------ state
    @contextmanager
    def xf(self, loc=(0, 0, 0), rotz=0.0, rotx=0.0, roty=0.0, scale=None, M=None):
        """Push a local transform (degrees). Order: translate, rotate Z, Y, X, scale."""
        T = Matrix.Translation(vec(loc))
        if rotz:
            T = T @ Matrix.Rotation(math.radians(rotz), 4, "Z")
        if roty:
            T = T @ Matrix.Rotation(math.radians(roty), 4, "Y")
        if rotx:
            T = T @ Matrix.Rotation(math.radians(rotx), 4, "X")
        if scale is not None:
            if isinstance(scale, (int, float)):
                scale = (scale, scale, scale)
            S = Matrix.Diagonal((scale[0], scale[1], scale[2], 1.0))
            T = T @ S
        if M is not None:
            T = T @ M
        Mn = self._M[-1] @ T
        self._M.append(Mn)
        self._flip.append(Mn.to_3x3().determinant() < 0)
        try:
            yield
        finally:
            self._M.pop()
            self._flip.pop()

    @contextmanager
    def style(self, uv=None, smooth=None):
        self._uv.append(uv if uv is not None else self._uv[-1])
        self._smooth.append(smooth if smooth is not None else self._smooth[-1])
        try:
            yield
        finally:
            self._uv.pop()
            self._smooth.pop()

    @contextmanager
    def part(self, closed=True):
        if self._cur is not None:
            yield
            return
        self._cur = (len(self.V), len(self.F), closed)
        try:
            yield
        finally:
            v0, f0, cl = self._cur
            self._cur = None
            self._weld(v0, f0)
            if len(self.F) > f0:
                self.parts.append((v0, len(self.V), f0, len(self.F), cl))

    def pt(self, p):
        q = self._M[-1] @ vec(p)
        return q

    def anchor(self, name, p):
        self.anchors[name] = tuple(round(c, 4) for c in self.pt(p))

    # ------------------------------------------------------------------ low level
    def _uvspec(self, uv):
        uv = uv if uv is not None else self._uv[-1]
        if isinstance(uv, tuple) and uv and uv[0] == "cyl":
            c = self.pt(uv[1])
            a = (self._M[-1].to_3x3() @ vec(uv[2])).normalized()
            return ("cyl", c, a)
        if isinstance(uv, tuple) and uv and uv[0] == "planar":
            o = self.pt(uv[1])
            m3 = self._M[-1].to_3x3()
            return ("planar", o, m3 @ vec(uv[2]), m3 @ vec(uv[3]))
        return uv

    def face(self, pts, mat, uv=None, smooth=None):
        """Add one polygon (list of local points, CCW from outside). Concave polygons are triangulated."""
        P = [self.pt(p) for p in pts]
        self._face_world(P, mat, self._uvspec(uv), smooth)

    def _face_world(self, P, mat, uvs, smooth, force_ngon=False):
        if self._flip[-1]:
            P = list(reversed(P))
            if isinstance(uvs, tuple) and uvs and uvs[0] == "uv":
                uvs = ("uv", list(reversed(uvs[1])))
        n = len(P)
        if n < 3:
            return
        sm = smooth if smooth is not None else self._smooth[-1]
        if n > 3 and not force_ngon:
            nrm = newell(P)
            if nrm.length < 1e-12:
                return
            bu, bv = plane_basis(nrm)
            p2 = [(q.dot(bu), q.dot(bv)) for q in P]
            if not is_convex2d(p2):
                self._tess_world([P], mat, uvs, sm, nrm)
                return
        if self._cur is None:
            with self.part(closed=False):
                self._emit(P, mat, uvs, sm)
        else:
            self._emit(P, mat, uvs, sm)

    def _emit(self, P, mat, uvs, sm):
        base = len(self.V)
        for q in P:
            self.V.append((q.x, q.y, q.z))
        self.F.append(list(range(base, base + len(P))))
        self.FM.append(mat)
        self.FU.append(uvs)
        self.FS.append(bool(sm))

    def _tess_world(self, loops, mat, uvs, sm, nrm=None):
        """Triangulate polygon-with-holes given as world-space loops; emit triangles oriented along nrm."""
        if nrm is None:
            nrm = newell(loops[0])
        bu, bv = plane_basis(nrm)
        flat = []
        l2 = []
        for lp in loops:
            l2.append([(q.dot(bu), q.dot(bv)) for q in lp])
            flat.extend(lp)
        tris = mgeo.tessellate_polygon(l2)
        explicit = isinstance(uvs, tuple) and uvs and uvs[0] == "uv"
        flat_uv = None
        if explicit:
            flat_uv = list(uvs[1])
        for t in tris:
            a, b, c = flat[t[0]], flat[t[1]], flat[t[2]]
            tn = (b - a).cross(c - a)
            if tn.length < 1e-12:
                continue
            idx = list(t)
            if tn.dot(nrm) < 0:
                idx = [t[0], t[2], t[1]]
            spec = ("uv", [flat_uv[i] for i in idx]) if explicit else uvs
            P = [flat[i] for i in idx]
            if self._cur is None:
                with self.part(closed=False):
                    self._emit(P, mat, spec, sm)
            else:
                self._emit(P, mat, spec, sm)

    def poly_holes(self, outer, holes, mat, uv=None, smooth=None):
        """Planar polygon with holes (local 3D points). Outer CCW from outside."""
        O = [self.pt(p) for p in outer]
        H = [[self.pt(p) for p in h] for h in holes]
        nrm = newell(O)
        if self._flip[-1]:
            nrm = -nrm
        # _tess_world orients by nrm directly, so no flip handling needed for vertex order
        self._tess_world([O] + H, mat, self._uvspec(uv), smooth if smooth is not None else self._smooth[-1], nrm)

    def _weld(self, v0, f0):
        key_to = {}
        remap = {}
        newV = []
        for i in range(v0, len(self.V)):
            x, y, z = self.V[i]
            k = (round(x / WELD), round(y / WELD), round(z / WELD))
            j = key_to.get(k)
            if j is None:
                j = v0 + len(newV)
                key_to[k] = j
                newV.append(self.V[i])
            remap[i] = j
        del self.V[v0:]
        self.V.extend(newV)
        keepF, keepM, keepU, keepS = [], [], [], []
        for fi in range(f0, len(self.F)):
            idx = [remap[i] for i in self.F[fi]]
            uvs = self.FU[fi]
            uvl = list(uvs[1]) if (isinstance(uvs, tuple) and uvs and uvs[0] == "uv") else None
            # drop consecutive duplicates (with wrap-around)
            out = []
            out_uv = []
            for k, i in enumerate(idx):
                if out and out[-1] == i:
                    continue
                out.append(i)
                if uvl is not None:
                    out_uv.append(uvl[k])
            while len(out) > 1 and out[0] == out[-1]:
                out.pop()
                if uvl is not None:
                    out_uv.pop()
            if len(set(out)) < 3 or len(set(out)) != len(out):
                continue
            P = [Vector(self.V[i]) for i in out]
            if newell(P).length < 1e-10:
                continue
            keepF.append(out)
            keepM.append(self.FM[fi])
            keepU.append(("uv", out_uv) if uvl is not None else uvs)
            keepS.append(self.FS[fi])
        del self.F[f0:], self.FM[f0:], self.FU[f0:], self.FS[f0:]
        self.F.extend(keepF)
        self.FM.extend(keepM)
        self.FU.extend(keepU)
        self.FS.extend(keepS)

    # ------------------------------------------------------------------ primitives
    def box(self, x0, y0, z0, x1, y1, z1, mat, uv=None, skip=(), mats=None):
        """Axis-aligned box in local coordinates. mats: optional dict side->material
        (sides: 'bottom','top','front'(-Y),'back'(+Y),'left'(-X),'right'(+X))."""
        if x0 > x1:
            x0, x1 = x1, x0
        if y0 > y1:
            y0, y1 = y1, y0
        if z0 > z1:
            z0, z1 = z1, z0
        c = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        faces = {
            "bottom": (0, 3, 2, 1), "top": (4, 5, 6, 7), "front": (0, 1, 5, 4),
            "right": (1, 2, 6, 5), "back": (2, 3, 7, 6), "left": (3, 0, 4, 7)}
        mats = mats or {}
        with self.part(closed=not skip):
            for k, f in faces.items():
                if k in skip:
                    continue
                self.face([c[i] for i in f], mats.get(k, mat), uv)

    def cbox(self, cx, cy, z0, sx, sy, h, mat, **kw):
        """Box centred at (cx, cy) with size (sx, sy), from z0 to z0 + h."""
        self.box(cx - sx / 2, cy - sy / 2, z0, cx + sx / 2, cy + sy / 2, z0 + h, mat, **kw)

    def obox(self, center, ax, ay, az, hx, hy, hz, mat, uv=None):
        """Oriented box: centre, orthonormal axes, half sizes."""
        c = vec(center)
        ax, ay, az = vec(ax), vec(ay), vec(az)
        pts = []
        for sz in (-1, 1):
            for sy, sx in ((-1, -1), (-1, 1), (1, 1), (1, -1)):
                pts.append(c + ax * (sx * hx) + ay * (sy * hy) + az * (sz * hz))
        # pts: bottom 0..3 ccw (as x,y), top 4..7
        if ax.cross(ay).dot(az) < 0:
            az = -az
            pts = pts[4:] + pts[:4]
        faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        with self.part():
            for f in faces:
                self.face([pts[i] for i in f], mat, uv)

    def beam(self, p0, p1, w, h, mat, up=(0, 0, 1), uv=None, ext=0.0):
        """Rectangular beam between two points; w = width (side), h = height (along `up`)."""
        p0, p1 = vec(p0), vec(p1)
        a = p1 - p0
        L = a.length
        if L < 1e-6:
            return
        a /= L
        upv = vec(up)
        side = a.cross(upv)
        if side.length < 1e-6:
            side = a.cross(Vector((1, 0, 0)))
            if side.length < 1e-6:
                side = a.cross(Vector((0, 1, 0)))
        side.normalize()
        upv = side.cross(a).normalized()
        c = (p0 + p1) * 0.5
        self.obox(c, a, side, upv, L / 2 + ext, w / 2, h / 2, mat, uv)

    def prism(self, poly, z0, z1, mat, top=True, bottom=True, side_mat=None, top_mat=None, bot_mat=None,
              uv=None, side_uv=None, smooth=None):
        """Extrude a 2D polygon (CCW from above) along local Z."""
        poly = ccw(poly)
        k = len(poly)
        with self.part(closed=top and bottom):
            for i in range(k):
                a = poly[i]
                b = poly[(i + 1) % k]
                self.face([(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)],
                          side_mat or mat, side_uv if side_uv is not None else uv, smooth)
            if top:
                self.face([(p[0], p[1], z1) for p in poly], top_mat or mat, uv, False)
            if bottom:
                self.face([(p[0], p[1], z0) for p in reversed(poly)], bot_mat or mat, uv, False)

    def prism_holes(self, outer, holes, z0, z1, mat, side_mat=None, uv=None, bottom=True):
        """Extrude polygon with holes along Z (closed unless bottom=False)."""
        outer = ccw(outer)
        holes = [cw(h) for h in holes]
        with self.part(closed=bottom):
            for loop in [outer] + holes:
                k = len(loop)
                for i in range(k):
                    a = loop[i]
                    b = loop[(i + 1) % k]
                    self.face([(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)],
                              side_mat or mat, uv)
            self.poly_holes([(p[0], p[1], z1) for p in outer], [[(p[0], p[1], z1) for p in h] for h in holes],
                            mat, uv)
            if bottom:
                self.poly_holes([(p[0], p[1], z0) for p in reversed(outer)],
                                [[(p[0], p[1], z0) for p in h] for h in holes], mat, uv)

    def plate(self, fr, poly, n0, n1, mat, holes=None, side_mat=None, uv=None, back=True, front_mat=None):
        """Polygon given in frame (u, v) coordinates, extruded along the frame normal from n0 to n1."""
        with self.xf(M=fr.matrix()):
            if holes:
                self.prism_holes(poly, holes, n0, n1, front_mat or mat, side_mat=side_mat or mat, uv=uv, bottom=back)
            else:
                self.prism(poly, n0, n1, mat, bottom=back, side_mat=side_mat, top_mat=front_mat, uv=uv)

    def ring_plate(self, fr, outer, inner, n0, n1, mat, uv=None, back=True):
        self.plate(fr, outer, n0, n1, mat, holes=[inner], uv=uv, back=back)

    def loft(self, rings, mat, closed=True, cap0=False, cap1=False, uv=None, smooth=None, part_closed=None):
        """Connect rings (lists of local 3D points with equal counts) with quads. Rings run counter-clockwise
        when looking down the loft direction from its end (so faces point outward)."""
        n = len(rings[0])
        pc = part_closed if part_closed is not None else (closed and cap0 and cap1)
        with self.part(closed=pc):
            for r in range(len(rings) - 1):
                A = rings[r]
                B = rings[r + 1]
                for i in range(n if closed else n - 1):
                    j = (i + 1) % n
                    self.face([A[i], A[j], B[j], B[i]], mat, uv, smooth)
            if cap0:
                self.face(list(reversed(rings[0])), mat, uv, False)
            if cap1:
                self.face(list(rings[-1]), mat, uv, False)

    def lathe(self, prof, segs, mat, cx=0.0, cy=0.0, a0=0.0, uv="cyl", smooth=True, caps=True, mats=None,
              flat_caps_mat=None):
        """Surface of revolution around the local Z axis at (cx, cy). prof: [(r, z), ...] bottom to top.
        r == 0 at an end makes a pointed apex. mats: optional per-profile-segment material list."""
        if uv == "cyl":
            uv = ("cyl", (cx, cy, 0.0), (0, 0, 1))
        ring = []
        for r, z in prof:
            if r <= 1e-9:
                ring.append([(cx, cy, z)] * segs)
            else:
                ring.append([(cx + r * math.cos(a0 + TAU * i / segs), cy + r * math.sin(a0 + TAU * i / segs), z)
                             for i in range(segs)])
        closed_part = caps or (prof[0][0] <= 1e-9 and prof[-1][0] <= 1e-9)
        with self.part(closed=closed_part):
            for k in range(len(prof) - 1):
                m = mats[k] if mats else mat
                A = ring[k]
                B = ring[k + 1]
                for i in range(segs):
                    j = (i + 1) % segs
                    self.face([A[i], A[j], B[j], B[i]], m, uv, smooth)
            if caps:
                if prof[0][0] > 1e-9:
                    self.face(list(reversed(ring[0])), flat_caps_mat or (mats[0] if mats else mat), "box", False)
                if prof[-1][0] > 1e-9:
                    self.face(list(ring[-1]), flat_caps_mat or (mats[-1] if mats else mat), "box", False)

    def cylinder(self, cx, cy, r, z0, z1, segs, mat, smooth=True, caps=True, a0=None):
        if a0 is None:
            a0 = math.pi / segs
        self.lathe([(r, z0), (r, z1)], segs, mat, cx, cy, a0=a0, smooth=smooth, caps=caps)

    def cone(self, cx, cy, r0, z0, z1, segs, mat, r1=0.0, smooth=True, a0=None):
        if a0 is None:
            a0 = math.pi / segs
        self.lathe([(r0, z0), (r1, z1)], segs, mat, cx, cy, a0=a0, smooth=smooth)

    def sphere(self, cx, cy, cz, r, segs, rings, mat, smooth=True, zmin=None):
        prof = []
        for i in range(rings + 1):
            t = -math.pi / 2 + math.pi * i / rings
            prof.append((r * math.cos(t) if 0 < i < rings else 0.0, cz + r * math.sin(t)))
        self.lathe(prof, segs, mat, cx, cy, smooth=smooth, uv="box")

    def add_bmesh(self, bm, mat, uv=None, smooth=None, closed=True, mat_of_face=None):
        """Import all faces of a bmesh (local coordinates) as one part."""
        with self.part(closed=closed):
            for f in bm.faces:
                m = mat_of_face(f) if mat_of_face else mat
                self.face([tuple(v.co) for v in f.verts], m, uv, smooth)

    # ------------------------------------------------------------------ queries / finishing
    def bounds(self):
        if not self.V:
            return (0, 0, 0), (0, 0, 0)
        xs = [v[0] for v in self.V]
        ys = [v[1] for v in self.V]
        zs = [v[2] for v in self.V]
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def translate_all(self, d):
        dx, dy, dz = d
        self.V = [(x + dx, y + dy, z + dz) for (x, y, z) in self.V]
        for f in range(len(self.FU)):
            u = self.FU[f]
            if isinstance(u, tuple) and u and u[0] in ("cyl", "planar"):
                self.FU[f] = (u[0], u[1] + Vector(d)) + tuple(u[2:])
        self.anchors = {k: (v[0] + dx, v[1] + dy, v[2] + dz) for k, v in self.anchors.items()}

    def tri_count(self):
        return sum(len(f) - 2 for f in self.F)


# ---------------------------------------------------------------------- UVs and Blender mesh

def _uv_box(P, n):
    ax = max(range(3), key=lambda i: abs(n[i]))
    out = []
    for p in P:
        if ax == 0:
            out.append((p.y if n.x > 0 else -p.y, p.z))
        elif ax == 1:
            out.append((-p.x if n.y > 0 else p.x, p.z))
        else:
            out.append((p.x, p.y if n.z > 0 else -p.y))
    return out


def _uv_slope(P, n):
    if abs(n.z) > 0.985 or abs(n.z) < 0.05:
        return _uv_box(P, n)
    th = Vector((0, 0, 1)).cross(n).normalized()
    tu = n.cross(th).normalized()
    return [(p.dot(th), p.dot(tu)) for p in P]


def _uv_cyl(P, n, c, a):
    if abs(n.dot(a)) > 0.9:
        return _uv_box(P, n)
    e1 = Vector((1, 0, 0)) if abs(a.x) < 0.9 else Vector((0, 1, 0))
    e1 = (e1 - a * e1.dot(a)).normalized()
    e2 = a.cross(e1)
    cen = sum((p for p in P), Vector((0, 0, 0))) / len(P)
    dc = cen - c
    rc = dc - a * dc.dot(a)
    R = max(rc.length, 0.05)
    tc = math.atan2(rc.dot(e2), rc.dot(e1))
    out = []
    for p in P:
        d = p - c
        r = d - a * d.dot(a)
        if r.length < 1e-6:
            t = tc
        else:
            t = math.atan2(r.dot(e2), r.dot(e1))
            while t - tc > math.pi:
                t -= TAU
            while t - tc < -math.pi:
                t += TAU
        out.append((t * R, d.dot(a)))
    return out


def compute_uvs(geo):
    """Per-face list of per-corner UVs."""
    res = []
    for fi, f in enumerate(geo.F):
        P = [Vector(geo.V[i]) for i in f]
        n = newell(P)
        n = n.normalized() if n.length > 0 else Vector((0, 0, 1))
        spec = geo.FU[fi]
        if isinstance(spec, tuple) and spec:
            kind = spec[0]
            if kind == "uv":
                res.append(list(spec[1]))
                continue
            if kind == "cyl":
                res.append(_uv_cyl(P, n, spec[1], spec[2]))
                continue
            if kind == "planar":
                o, U, Vv = spec[1], spec[2], spec[3]
                res.append([((p - o).dot(U), (p - o).dot(Vv)) for p in P])
                continue
        if spec == "slope":
            res.append(_uv_slope(P, n))
        else:
            res.append(_uv_box(P, n))
    return res


def to_blender_object(geo, name, sharp_angle=35.0):
    import bpy
    from arch_palette import get_material
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in geo.V], [], [list(f) for f in geo.F])
    if len(me.polygons) != len(geo.F):
        raise RuntimeError(f"{name}: polygon count changed on import ({len(me.polygons)} vs {len(geo.F)})")
    order = []
    for m in geo.FM:
        if m not in order:
            order.append(m)
    for m in order:
        me.materials.append(get_material(m))
    mi = {m: i for i, m in enumerate(order)}
    me.polygons.foreach_set("material_index", [mi[m] for m in geo.FM])
    me.polygons.foreach_set("use_smooth", [bool(s) for s in geo.FS])
    uvl = me.uv_layers.new(name="UVMap")
    uvs = compute_uvs(geo)
    flat = []
    for poly, fu in zip(me.polygons, uvs):
        if len(fu) != poly.loop_total:
            raise RuntimeError(f"{name}: uv count mismatch")
        for u, v in fu:
            flat.extend((u, v))
    uvl.data.foreach_set("uv", flat)
    if any(geo.FS):
        me.set_sharp_from_angle(angle=math.radians(sharp_angle))
    me.update()
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj
