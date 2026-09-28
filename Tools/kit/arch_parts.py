"""Architectural building blocks for the LA PLACE kit (built on arch_geo.Geo).

Every attachment follows the contact rule from arch_geo: it is sunk into the surface it sits on (embed) and stands
proud of it by an amount that differs from any overlapping attachment, so no two faces ever share a plane where they
overlap. Typical proud depths used here (metres in front of a wall face):
  corner posts 0.07, posts 0.06, rails 0.05, braces 0.04 / 0.035, window/door frames 0.03,
  shutters 0.035 (never overlapping frames), sills 0.08, lintels 0.045, pilasters 0.10, cornices 0.12-0.25.
"""
import math

from mathutils import Vector

from arch_geo import TAU, Frame, ccw, newell, offset_poly2d, vec

# ====================================================================== openings


def bar3(g, fr, u0, u1, v0, v1, n0, n1, mat):
    """Thin bar whose ends are buried: only its front and two side faces (6 triangles), open behind."""
    P = fr.p
    with g.part(closed=False):
        g.face([P(u0, v0, n1), P(u1, v0, n1), P(u1, v1, n1), P(u0, v1, n1)], mat)
        g.face([P(u0, v0, n0), P(u0, v0, n1), P(u0, v1, n1), P(u0, v1, n0)], mat)
        g.face([P(u1, v0, n1), P(u1, v0, n0), P(u1, v1, n0), P(u1, v1, n1)], mat)


def wplate(g, fr, poly, n0, n1, mat, **kw):
    """Wall-mounted plate: its back face is buried in the wall (n0 < 0) and never visible, so it is omitted."""
    g.plate(fr, poly, n0, n1, mat, back=not (n0 < 0), **kw)


class Opening:
    """A recessed opening in a wall face. poly: polygon in the face frame (u right, v up, metres)."""

    def __init__(self, poly, depth=0.18, back="MT_Glass", reveal=None, kind="window", **data):
        self.poly = ccw(poly)
        self.depth = depth
        self.back = back
        self.reveal = reveal
        self.kind = kind
        self.data = data

    @property
    def box(self):
        us = [p[0] for p in self.poly]
        vs = [p[1] for p in self.poly]
        return min(us), min(vs), max(us), max(vs)


def rect(u0, v0, w, h):
    return [(u0, v0), (u0 + w, v0), (u0 + w, v0 + h), (u0, v0 + h)]


def arch_poly(u0, v0, w, h, segs=8, pointed=False, k=1.0):
    """Arched opening polygon: width w, total height h to the crown. pointed: two-centred arch of radius k*w."""
    r = w / 2
    uc = u0 + r
    pts = [(u0, v0), (u0 + w, v0)]
    if not pointed and h < r - 1e-6:
        # segmental arch: circle through both springing points (at v0) and the crown (v0 + h)
        R = (r * r + h * h) / (2 * h)
        cv = v0 + h - R
        a = math.asin(min(1.0, r / R))
        for i in range(1, segs):
            t = math.pi / 2 - a + 2 * a * i / segs
            pts.append((uc + R * math.cos(t), cv + R * math.sin(t)))
        return pts
    if not pointed:
        vs = v0 + h - r
        for i in range(segs + 1):
            a = math.pi * i / segs
            pts.append((uc + r * math.cos(a), vs + r * math.sin(a)))
        return pts
    R = k * w
    dz = math.sqrt(max(w * R - w * w / 4, 1e-6))
    vs = v0 + h - dz
    half = max(2, segs // 2)
    cxr = u0 + w - R
    ta = math.atan2(dz, uc - cxr)
    right = [(cxr + R * math.cos(ta * i / half), vs + R * math.sin(ta * i / half)) for i in range(half + 1)]
    cxl = u0 + R
    left = [(cxl - R * math.cos(ta * i / half), vs + R * math.sin(ta * i / half)) for i in range(half, -1, -1)]
    pts += right + left[1:]
    return pts


def circle_poly(uc, vc, r, n=16, a0=None):
    if a0 is None:
        a0 = math.pi / n
    return [(uc + r * math.cos(a0 + TAU * i / n), vc + r * math.sin(a0 + TAU * i / n)) for i in range(n)]


# ====================================================================== bodies


def face_frame(pts):
    P = [vec(p) for p in pts]
    n = newell(P).normalized()
    u = (P[1] - P[0]).normalized()
    v = n.cross(u).normalized()
    return Frame(P[0], u, v, n)


def solid(g, faces):
    """One closed part from faces [(pts, mat, openings)], openings recessed into the solid.
    Returns the list of face frames (None for faces without openings)."""
    frames = []
    with g.part(closed=True):
        for pts, mat, ops in faces:
            if not ops:
                g.face(pts, mat)
                frames.append(face_frame(pts) if len(pts) >= 3 else None)
                continue
            fr = face_frame(pts)
            frames.append(fr)
            holes = [[fr.p(u, v) for (u, v) in op.poly] for op in ops]
            g.poly_holes(pts, holes, mat)
            for op in ops:
                d = op.depth
                rm = op.reveal or mat
                P = op.poly
                k = len(P)
                for i in range(k):
                    a = P[i]
                    b = P[(i + 1) % k]
                    g.face([fr.p(a[0], a[1], 0), fr.p(b[0], b[1], 0), fr.p(b[0], b[1], -d), fr.p(a[0], a[1], -d)],
                           rm)
                g.face([fr.p(u, v, -d) for (u, v) in P], op.back)
    return frames


def box_faces(x0, y0, x1, y1, z0, z1):
    return {
        "front": [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],
        "right": [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],
        "back": [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)],
        "left": [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)],
        "top": [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],
        "bottom": [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)],
    }


def gable_faces(x0, y0, x1, y1, z0, ze, zr, ridge="x"):
    """Closed house body with gable ends. ridge 'x': gables on left/right, 'y': gables on front/back."""
    xc = (x0 + x1) / 2
    yc = (y0 + y1) / 2
    f = {"bottom": [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)]}
    if ridge == "x":
        f["front"] = [(x0, y0, z0), (x1, y0, z0), (x1, y0, ze), (x0, y0, ze)]
        f["back"] = [(x1, y1, z0), (x0, y1, z0), (x0, y1, ze), (x1, y1, ze)]
        f["right"] = [(x1, y0, z0), (x1, y1, z0), (x1, y1, ze), (x1, yc, zr), (x1, y0, ze)]
        f["left"] = [(x0, y1, z0), (x0, y0, z0), (x0, y0, ze), (x0, yc, zr), (x0, y1, ze)]
        f["slope1"] = [(x0, y0, ze), (x1, y0, ze), (x1, yc, zr), (x0, yc, zr)]
        f["slope2"] = [(x1, y1, ze), (x0, y1, ze), (x0, yc, zr), (x1, yc, zr)]
    else:
        f["front"] = [(x0, y0, z0), (x1, y0, z0), (x1, y0, ze), (xc, y0, zr), (x0, y0, ze)]
        f["back"] = [(x1, y1, z0), (x0, y1, z0), (x0, y1, ze), (xc, y1, zr), (x1, y1, ze)]
        f["right"] = [(x1, y0, z0), (x1, y1, z0), (x1, y1, ze), (x1, y0, ze)]
        f["left"] = [(x0, y1, z0), (x0, y0, z0), (x0, y0, ze), (x0, y1, ze)]
        f["slope1"] = [(x1, y0, ze), (x1, y1, ze), (xc, y1, zr), (xc, y0, zr)]
        f["slope2"] = [(x0, y1, ze), (x0, y0, ze), (xc, y0, zr), (xc, y1, zr)]
    return f


def block(g, x0, y0, x1, y1, z0, z1, mat, ops=None, mats=None, gable=None):
    """Rectangular (optionally gabled) body with openings. ops: dict side -> [Opening].
    gable: None or (ridge_axis, ze, zr) -> body top follows the roof mid-plane.
    Returns dict side -> Frame (origin at the bottom-left of the face seen from outside)."""
    ops = ops or {}
    mats = mats or {}
    if gable:
        axis, ze, zr = gable
        F = gable_faces(x0, y0, x1, y1, z0, ze, zr, axis)
    else:
        F = box_faces(x0, y0, x1, y1, z0, z1)
    names = list(F.keys())
    frames = solid(g, [(F[k], mats.get(k, mat), ops.get(k)) for k in names])
    return {k: fr for k, fr in zip(names, frames)}


def side_frame(x0, y0, x1, y1, z0, side):
    """Frame of a vertical face of the box [x0,x1]x[y0,y1] at height z0 (origin bottom-left from outside)."""
    if side == "front":
        return Frame((x0, y0, z0), (1, 0, 0), (0, 0, 1)), x1 - x0
    if side == "right":
        return Frame((x1, y0, z0), (0, 1, 0), (0, 0, 1)), y1 - y0
    if side == "back":
        return Frame((x1, y1, z0), (-1, 0, 0), (0, 0, 1)), x1 - x0
    return Frame((x0, y1, z0), (0, -1, 0), (0, 0, 1)), y1 - y0


# ====================================================================== trims


def frame_ring(g, fr, op, width=0.09, inset=0.03, n0=-0.06, n1=0.03, mat="MT_Timber", open_bottom=False,
               bottom_drop=0.02):
    """Trim around an opening. open_bottom: U-shaped (doors), jambs end bottom_drop below the threshold."""
    P = op.poly
    outer = offset_poly2d(P, width)
    inner = offset_poly2d(P, -inset)
    if not open_bottom:
        g.ring_plate(fr, outer, inner, n0, n1, mat, back=not (n0 < 0))
        return
    v0 = min(p[1] for p in P) - bottom_drop
    # P starts with the bottom edge (bottom-left, bottom-right)
    k = len(P)
    U = [(outer[1][0], v0)] + outer[2:] + [(outer[0][0], v0), (inner[0][0], v0)] + \
        [inner[i] for i in range(k - 1, 1, -1)] + [(inner[1][0], v0)]
    wplate(g, fr, U, n0, n1, mat)


def window_trim(g, fr, op, style):
    """Frame, sill, mullions, shutters and lintel for a rectangular or arched window, driven by style dict."""
    u0, v0, u1, v1 = op.box
    w = u1 - u0
    h = v1 - v0
    tm = style.get("trim", "MT_Timber")
    fw = style.get("frame_w", 0.08)
    if style.get("frame", True):
        frame_ring(g, fr, op, width=fw, mat=style.get("frame_mat", tm))
    d = op.depth
    mm = style.get("mullion_mat", tm)
    if style.get("mullions", True) and w > 0.45:
        # vertical bar (sits in front of the glass, ends buried in the reveals)
        wplate(g, fr, rect(u0 + w / 2 - 0.035, v0 - 0.03, 0.07, h + 0.06), -d - 0.03, -d + 0.05, mm)
        if h > 0.8 and op.kind != "arch":
            vm = v0 + h * 0.62
            wplate(g, fr, rect(u0 - 0.03, vm - 0.03, w + 0.06, 0.06), -d - 0.025, -d + 0.04, mm)
        elif op.kind == "arch":
            vm = v0 + (h - w / 2) * 0.9
            wplate(g, fr, rect(u0 - 0.03, vm - 0.03, w + 0.06, 0.06), -d - 0.025, -d + 0.04, mm)
    if style.get("sill", True):
        sm = style.get("sill_mat", "MT_Stone")
        so = style.get("sill_over", fw + 0.045)
        wplate(g, fr, rect(u0 - so, v0 - fw - 0.07, w + 2 * so, 0.085), -0.08, style.get("sill_proud", 0.08), sm)
    if style.get("shutters") and op.kind != "arch":
        shm = style.get("shutter_mat", "MT_WoodPlanks")
        sw = w / 2 + 0.02
        gap = fw + 0.02
        if not op.data.get("no_shutter_left"):
            wplate(g, fr, rect(u0 - gap - sw, v0 - 0.02, sw, h + 0.04), -0.02, 0.035, shm)
        if not op.data.get("no_shutter_right"):
            wplate(g, fr, rect(u1 + gap, v0 - 0.02, sw, h + 0.04), -0.02, 0.035, shm)
    if style.get("lintel"):
        lm = style.get("lintel_mat", "MT_Stone")
        lo = style.get("lintel_over", fw + 0.065)
        wplate(g, fr, rect(u0 - lo, v1 + fw - 0.02, w + 2 * lo, style.get("lintel_h", 0.16)), -0.055,
                style.get("lintel_proud", 0.045), lm)
    if style.get("pediment") and op.kind != "arch":
        pm = style.get("pediment_mat", "MT_StoneWhite")
        base = v1 + fw + 0.03
        ext = fw + 0.13
        wplate(g, fr, rect(u0 - ext, base - 0.05, w + 2 * ext, 0.17), -0.055, 0.075, pm)
        uc = (u0 + u1) / 2
        if style["pediment"] == "tri":
            tri = [(u0 - ext - 0.03, base + 0.1), (u1 + ext + 0.03, base + 0.1), (uc, base + 0.1 + 0.2 + w * 0.22)]
            wplate(g, fr, tri, -0.05, 0.095, pm)
        else:
            half = w / 2 + ext + 0.03
            sag = 0.18 + w * 0.08
            R = (half * half + sag * sag) / (2 * sag)
            cz = base + 0.1 + sag - R
            a0 = math.asin(half / R)
            arc = [(uc + R * math.sin(-a0 + 2 * a0 * i / 8), cz + R * math.cos(-a0 + 2 * a0 * i / 8)) for i in range(9)]
            seg = [(uc - half, base + 0.1), (uc + half, base + 0.1)] + arc[::-1]
            wplate(g, fr, seg, -0.05, 0.095, pm)
    if style.get("balconette") and op.kind != "arch":
        bm = style.get("balconette_mat", "MT_StoneWhite")
        rm = style.get("rail_mat", "MT_Iron")
        vs = v0 - fw - 0.1
        wplate(g, fr, rect(u0 - fw - 0.12, vs, w + 2 * fw + 0.24, 0.12), -0.09, 0.42, bm)
        top = vs + 0.12 + 0.82
        nb = max(3, int((w + 2 * fw + 0.1) / 0.17))
        for k in range(nb + 1):
            u = u0 - fw - 0.04 + (w + 2 * fw + 0.08) * k / nb
            bar3(g, fr, u - 0.012, u + 0.012, vs + 0.1, top - 0.02, 0.345, 0.369, rm)
        wplate(g, fr, rect(u0 - fw - 0.07, top - 0.045, w + 2 * fw + 0.14, 0.045), 0.335, 0.382, rm)
        for u in (u0 - fw - 0.05, u1 + fw + 0.02):
            wplate(g, fr, rect(u, top - 0.055, 0.03, 0.04), -0.04, 0.35, rm)
    if style.get("keystone") and op.kind == "arch":
        km = style.get("keystone_mat", "MT_StoneWhite")
        uc = (u0 + u1) / 2
        wplate(g, fr, [(uc - 0.09, v1 - 0.05), (uc + 0.09, v1 - 0.05), (uc + 0.13, v1 + fw + 0.14),
                     (uc - 0.13, v1 + fw + 0.14)], -0.04, 0.06, km)
    if style.get("flowerbox") and op.kind != "arch" and w > 0.6:
        wplate(g, fr, rect(u0 - 0.02, v0 - fw - 0.07 - 0.2, w + 0.04, 0.22), -0.05, 0.24, "MT_WoodPlanks")


def door_trim(g, fr, op, style):
    u0, v0, u1, v1 = op.box
    w = u1 - u0
    tm = style.get("door_frame_mat", style.get("trim", "MT_Timber"))
    fw = style.get("door_frame_w", 0.1)
    frame_ring(g, fr, op, width=fw, mat=tm, open_bottom=True, bottom_drop=op.data.get("jamb_drop", 0.02))
    d = op.depth
    # iron straps on the leaf
    if style.get("straps", True):
        for fv in (0.25, 0.72):
            vv = v0 + (v1 - v0) * fv
            if op.kind == "arch" and vv > v1 - w / 2:
                continue
            wplate(g, fr, rect(u0 - 0.03, vv, w * 0.8 + 0.03, 0.06), -d - 0.03, -d + 0.025, "MT_Iron")
    # vertical plank battens
    if style.get("battens", False):
        for fu in (0.33, 0.66):
            uu = u0 + w * fu
            wplate(g, fr, rect(uu - 0.02, v0 - 0.025, 0.04, (v1 - v0) * (0.7 if op.kind == "arch" else 1.0) + 0.055),
                    -d - 0.035, -d + 0.015, "MT_Timber")
    if style.get("door_lintel"):
        lm = style.get("lintel_mat", "MT_Stone")
        wplate(g, fr, rect(u0 - fw - 0.1, v1 + fw - 0.02, w + 2 * fw + 0.2, 0.18), -0.05, 0.05, lm)


def steps(g, x0, x1, y_front, y_wall, z_top, mat="MT_Stone", n=None, embed=0.25, zb=-0.5):
    """Straight stair (sawtooth section extruded along X) climbing +Y from y_front to the wall line y_wall,
    top tread at z_top; the block continues `embed` into the building."""
    rise = 0.17
    if n is None:
        n = max(1, int(round(z_top / rise)))
    rise = z_top / n
    run = (y_wall - y_front) / n
    pts = [(y_front, zb), (y_wall + embed, zb), (y_wall + embed, z_top)]
    for k in range(n, 0, -1):
        yk = y_front + run * (k - 1)
        pts.append((yk, rise * k))
        pts.append((yk, rise * (k - 1))) if k > 1 else None
    pts = [p for p in pts if p is not None]
    fr = Frame((x0, 0, 0), (0, 1, 0), (0, 0, 1))
    wplate(g, fr, pts, 0, x1 - x0, mat)


def chimney(g, x, y, w, d, z0, z1, mat="MT_Stone", cap="MT_Stone", pots=0, pot_mat="MT_RoofRed"):
    g.box(x - w / 2, y - d / 2, z0, x + w / 2, y + d / 2, z1, mat)
    g.box(x - w / 2 - 0.07, y - d / 2 - 0.07, z1 - 0.12, x + w / 2 + 0.07, y + d / 2 + 0.07, z1 + 0.08, cap)
    for i in range(pots):
        px = x + (i - (pots - 1) / 2) * min(0.28, w / max(pots, 1))
        g.cylinder(px, y, 0.08, z1 + 0.05, z1 + 0.38, 8, pot_mat)


def merlon_wall(g, fr, L, h_wall, h_merlon, merlon_w, gap_w, thick, mat, n0=0.0, end_gap=None):
    """Parapet with crenellations as one closed prism: outline in the (u, v) plane of fr, extruded thick
    along -n (from n0 to n0 - thick). Merlons are centred in cells of width merlon_w + gap_w."""
    cell = merlon_w + gap_w
    n = max(1, int(round(L / cell)))
    cell = L / n
    pts = [(0, 0), (L, 0), (L, h_wall)]
    for k in range(n - 1, -1, -1):
        c = cell * (k + 0.5)
        pts += [(c + merlon_w / 2, h_wall), (c + merlon_w / 2, h_wall + h_merlon),
                (c - merlon_w / 2, h_wall + h_merlon), (c - merlon_w / 2, h_wall)]
    pts.append((0, h_wall))
    # remove duplicates at the ends
    out = []
    for p in pts:
        if not out or (abs(out[-1][0] - p[0]) > 1e-6 or abs(out[-1][1] - p[1]) > 1e-6):
            out.append(p)
    g.plate(fr, out, n0 - thick, n0, mat)


# ====================================================================== timber framing


def _split_span(a, b, cuts):
    """Split [a,b] removing the cut intervals; returns list of (s,e)."""
    segs = [(a, b)]
    for c0, c1 in cuts:
        nxt = []
        for s, e in segs:
            if c1 <= s or c0 >= e:
                nxt.append((s, e))
                continue
            if c0 > s:
                nxt.append((s, c0))
            if c1 < e:
                nxt.append((c1, e))
        segs = nxt
    return [(s, e) for s, e in segs if e - s > 0.12]


# depth classes (back = embed into the wall, front = proud of the wall); all distinct so overlapping
# attachments never share a plane
N_POST = (-0.065, 0.06)
N_RAIL = (-0.05, 0.05)
N_BRACE = (-0.04, 0.04)
N_BRACE2 = (-0.035, 0.033)


def timber_facade(g, fr, L, H, ops=(), mat="MT_Timber", post=0.2, rail=0.2, corner=0.15, braces=True,
                  extra_rails=(), rail_bottom=True, rail_top=True, margin=0.06, brace_w=0.17, max_panel=1.5,
                  brace_mode="diag", flip=False, under_braces=True):
    """Half-timbering on a wall face region u in [0, L], v in [0, H] of frame fr.
    Corner posts (corner_posts) occupy `corner` metres at each end. Rails are cut around openings, posts
    flank openings and subdivide long blank panels, braces fill blank panels (between the pair of rails with
    the tallest gap) and form a V under windows."""
    boxes = [op.box for op in ops]
    fws = [op.data.get("fw", 0.08) for op in ops]
    rails = []
    if rail_bottom:
        rails.append((0.02, rail + 0.02))
    if rail_top:
        rails.append((H - rail - 0.02, H - 0.02))
    for v in extra_rails:
        rails.append((v, v + rail * 0.8))
    rails.sort()
    ua, ub = corner - 0.04, L - corner + 0.04
    for r0, r1 in rails:
        # rails stop 2 cm inside the opening's trim; openings whose trim clears the rail do not cut it
        cuts = [(b[0] - f + 0.02, b[2] + f - 0.02) for b, f in zip(boxes, fws)
                if b[1] - f + 0.03 < r1 and b[3] + f - 0.03 > r0]
        for s, e in _split_span(ua, ub, cuts):
            wplate(g, fr, rect(s, r0, e - s, r1 - r0), N_RAIL[0], N_RAIL[1], mat)
    vb = rails[0][1] - 0.05 if rail_bottom else 0.03
    vt = rails[-1][0] + 0.05 if rail_top else H - 0.03
    pos = []
    for b, f in zip(boxes, fws):
        # posts overlap the trim by 1.5 cm
        pos.append(b[0] - f + 0.015 - post / 2)
        pos.append(b[2] + f - 0.015 + post / 2)
    pos = sorted(p for p in pos if corner + post / 2 + 0.05 < p < L - corner - post / 2 - 0.05)
    merged = []
    for p in pos:
        if merged and p - merged[-1] < post + 0.12:
            merged[-1] = (merged[-1] + p) / 2
        else:
            merged.append(p)
    anchors = [corner - post / 2] + merged + [L - corner + post / 2]
    allp = []
    for i in range(len(anchors) - 1):
        a, b = anchors[i], anchors[i + 1]
        span = b - a
        has_op = any(bx[0] > a and bx[2] < b for bx in boxes)
        if not has_op and span > max_panel:
            n = int(math.ceil(span / max_panel))
            for k in range(1, n):
                allp.append(a + span * k / n)
    posts = sorted(merged + allp)
    real_posts = []
    for p in posts:
        blocked = any(bx[0] - f + 0.03 < p + post / 2 and bx[2] + f - 0.03 > p - post / 2 and
                      bx[1] - f + 0.03 < vt and bx[3] + f - 0.03 > vb for bx, f in zip(boxes, fws))
        if blocked:
            continue
        wplate(g, fr, rect(p - post / 2, vb, post, vt - vb), N_POST[0], N_POST[1], mat)
        real_posts.append(p)
    if not braces:
        return
    # vertical gaps between consecutive rails
    gaps = [(rails[k][1], rails[k + 1][0]) for k in range(len(rails) - 1) if rails[k + 1][0] - rails[k][1] > 0.4]
    if not gaps:
        return
    edges = [corner] + real_posts + [L - corner]
    pw = [0.0] + [post / 2] * len(real_posts) + [0.0]
    for i in range(len(edges) - 1):
        a = edges[i] + pw[i]
        b = edges[i + 1] - pw[i + 1]
        if b - a < 0.45:
            continue
        inside = [bx for bx, f in zip(boxes, fws) if bx[0] - f < b and bx[2] + f > a]
        if inside:
            for bx in (inside if under_braces else []):
                if bx[2] - bx[0] < 0.5:
                    continue
                # rail directly under the window, if any
                below = [r for r in rails if r[1] <= bx[1] + 1e-6]
                if not below:
                    continue
                r_top = max(below, key=lambda r: r[1])
                r_bot = max([r for r in rails if r[1] <= r_top[0] + 1e-6], key=lambda r: r[1], default=None)
                if r_bot is None or r_top[0] - r_bot[1] < 0.4:
                    continue
                mid = (bx[0] + bx[2]) / 2
                _brace(g, fr, bx[0], r_bot[1], mid - 0.01, r_top[0], brace_w * 0.9, N_BRACE, mat, False)
                _brace(g, fr, mid + 0.01, r_bot[1], bx[2], r_top[0], brace_w * 0.9, N_BRACE2, mat, True,
                       top_extra=0.012)
            continue
        mirror = ((i % 2) == 1) ^ flip
        v0, v1 = max(gaps, key=lambda gp: gp[1] - gp[0])
        if brace_mode == "cross" and b - a > 0.9:
            _brace(g, fr, a, v0, b, v1, brace_w, N_BRACE, mat, False)
            _brace(g, fr, a, v0, b, v1, brace_w, N_BRACE2, mat, True, top_extra=0.012)
        else:
            top = v0 + min(v1 - v0, (b - a) * 1.7)
            if top < v1 - 0.25:
                top = v1
            _brace(g, fr, a, v0, b, top, brace_w, N_BRACE, mat, mirror)


def _brace(g, fr, a, v0, b, v1, bw, nn, mat, mirror, top_extra=0.0):
    """Parallelogram brace in panel [a,b]x[v0,v1] with horizontal ends sunk 3 cm into the rails."""
    e = 0.03
    eb = 0.035
    dx = b - a
    dy = v1 - v0
    ln = math.hypot(dx, dy)
    if ln < 0.3 or dy < 0.2:
        return
    bwh = bw * ln / dy
    bwh = min(bwh, dx * 0.45)
    vt = v1 + e + top_extra
    if not mirror:
        pts = [(a - 0.02, v0 - eb), (a - 0.02 + bwh, v0 - eb), (b + 0.02, vt), (b + 0.02 - bwh, vt)]
    else:
        pts = [(b + 0.02 - bwh, v0 - eb), (b + 0.02, v0 - eb), (a - 0.02 + bwh, vt), (a - 0.02, vt)]
    wplate(g, fr, pts, nn[0], nn[1], mat)


def corner_posts(g, x0, y0, x1, y1, z0, z1, mat="MT_Timber", s=0.22, p=0.07):
    """Square posts wrapping the four vertical corners of a box, proud by p on both faces."""
    for (cx, cy, sx, sy) in ((x0, y0, -1, -1), (x1, y0, 1, -1), (x1, y1, 1, 1), (x0, y1, -1, 1)):
        xa = cx + sx * p
        xb = cx - sx * (s - p)
        ya = cy + sy * p
        yb = cy - sy * (s - p)
        g.box(min(xa, xb), min(ya, yb), z0, max(xa, xb), max(ya, yb), z1, mat)


def jetty_beam(g, x0, y0, x1, y1, z, h, mat="MT_Timber", proud=0.09):
    """Sill beam ring (closed band) wrapping a storey base: from z to z+h, proud of the walls."""
    outer = [(x0 - proud, y0 - proud), (x1 + proud, y0 - proud), (x1 + proud, y1 + proud), (x0 - proud, y1 + proud)]
    inner = [(x0 + 0.1, y0 + 0.1), (x1 - 0.1, y0 + 0.1), (x1 - 0.1, y1 - 0.1), (x0 + 0.1, y1 - 0.1)]
    g.prism_holes(outer, [inner], z, z + h, mat)


def joist_ends(g, fr, L, v, n_out=0.28, size=0.14, spacing=0.6, mat="MT_Timber", margin=0.3):
    """Row of protruding joist ends under a jetty (boxes sticking out of the face)."""
    n = max(1, int((L - 2 * margin) / spacing))
    for k in range(n + 1):
        u = margin + (L - 2 * margin) * k / max(n, 1)
        wplate(g, fr, rect(u - size / 2, v - size, size, size), -0.08, n_out, mat)


# ====================================================================== roofs


class RoofInfo:
    pass


def _sawtooth(run, slope_len, course, step, courses=True):
    """Profile along a roof slope: [(h, off)] from the eave (h=0) to the top (h=run), excluding (0,0)."""
    if not courses or course <= 0:
        return [(0.0, 0.0), (run, 0.0)]
    n = max(1, int(round(slope_len / course)))
    prof = []
    for k in range(n):
        prof.append((run * k / n, step))
        prof.append((run * (k + 1) / n, 0.0))
    return prof


def roof_gable(g, x0, x1, y0, y1, zw, pitch, mat, ov=0.5, ovv=0.35, th=0.22, course=0.34, step=0.045,
               under="MT_WoodPlanks", fascia="MT_Timber", verge=None, ridge_mat=None, ridge=True, axis="x",
               courses=True, snow=None, snow_from=0.25, ridge_w=0.17):
    """Gable roof over the wall rectangle [x0,x1]x[y0,y1] (wall tops at zw). axis: ridge direction.
    Returns RoofInfo with ze/zr (body mid-plane eave / apex heights) and helpers."""
    if axis == "y":
        # build canonical (ridge along local X) and rotate +90 deg about Z: world (x, y) = (-ly, lx)
        with g.xf(rotz=90):
            info = roof_gable(g, y0, y1, -x1, -x0, zw, pitch, mat, ov, ovv, th, course, step, under, fascia,
                              verge, ridge_mat, ridge, "x", courses, snow, snow_from, ridge_w)
        info.axis = "y"
        return info
    ta = math.tan(math.radians(pitch))
    ca = math.cos(math.radians(pitch))
    w = (y1 - y0) / 2
    yc = (y0 + y1) / 2
    run = w + ov
    z_eb = zw - ov * ta
    z_et = z_eb + th / ca
    z_ur = zw + w * ta
    z_tr = z_ur + th / ca
    prof = _sawtooth(run, run / ca, course, step, courses)
    back = [(y1 + ov - h, z_et + h * ta + o) for (h, o) in prof]
    front = [(y0 - ov + h, z_et + h * ta + o) for (h, o) in reversed(prof)][1:]
    sec = [(y0 - ov, z_eb), (yc, z_ur), (y1 + ov, z_eb)] + back + front
    emat = [under, under, fascia] + [mat] * (len(back) - 1 + len(front)) + [fascia]
    euv = ["slope", "slope", "box"] + ["slope"] * (len(back) - 1 + len(front)) + ["box"]
    xa, xb = x0 - ovv, x1 + ovv
    _extrude_x(g, sec, xa, xb, emat, euv, verge or mat)
    if ridge:
        rw = ridge_w
        rm = ridge_mat or mat
        rs = [(yc - rw, z_tr - rw * ta - 0.04), (yc + rw, z_tr - rw * ta - 0.04), (yc + rw * 0.95, z_tr + 0.03),
              (yc + rw * 0.55, z_tr + 0.1), (yc - rw * 0.55, z_tr + 0.1), (yc - rw * 0.95, z_tr + 0.03)]
        _extrude_x(g, rs, xa - 0.03, xb + 0.03, [rm] * len(rs), ["box"] * len(rs), rm)
    if snow:
        _snow_gable(g, xa, xb, y0 - ov, y1 + ov, yc, z_et, ta, ca, run, step, snow_from, snow, ridge_w)
    info = RoofInfo()
    info.axis = "x"
    # horizontal distances (from the wall line) of the vertical tile-course risers; other vertical faces must avoid
    info.risers = [h - ov for (h, o) in prof if o > 0]
    info.ze = zw + th / (2 * ca)
    info.zr = z_ur + th / (2 * ca)
    info.z_top_ridge = z_tr + (0.1 if ridge else 0.0)
    info.ta = ta
    info.zw = zw
    info.z_under = lambda d: zw + d * ta          # underside height at horizontal distance d in from the wall
    info.z_top = lambda d: z_et + (d + ov) * ta   # tile base height at distance d in from the wall line
    info.xa, info.xb = xa, xb
    return info


def _extrude_x(g, sec, xa, xb, emats, euvs, cap_mat):
    """Extrude a (y, z) section polygon (CCW seen from +X) along X from xa to xb with per-edge materials."""
    if _area(sec) < 0:
        sec = list(reversed(sec))
        emats = list(reversed(emats[:-1])) + [emats[-1]]
        euvs = list(reversed(euvs[:-1])) + [euvs[-1]]
        # after reversing, edge i of the new polygon corresponds to edge (k-2-i) of the old one; the closing
        # edge stays last. This keeps materials aligned for reversed inputs.
    k = len(sec)
    with g.part():
        for i in range(k):
            a = sec[i]
            b = sec[(i + 1) % k]
            # edge a->b is CCW seen from +X, so (a@xa, b@xa, b@xb, a@xb) faces outward
            g.face([(xa, a[0], a[1]), (xa, b[0], b[1]), (xb, b[0], b[1]), (xb, a[0], a[1])], emats[i], euvs[i])
        g.face([(xb, p[0], p[1]) for p in sec], cap_mat, "box")
        g.face([(xa, p[0], p[1]) for p in reversed(sec)], cap_mat, "box")


def _area(p):
    s = 0.0
    for i in range(len(p)):
        a = p[i]
        b = p[(i + 1) % len(p)]
        s += a[0] * b[1] - b[0] * a[1]
    return s / 2


def _snow_gable(g, xa, xb, ye0, ye1, yc, z_et, ta, ca, run, step, frac, mat, ridge_w=0.17):
    """Snow blanket over both slopes: bottom buried 2 cm under the tile valleys, top 14 cm above the lips, with a
    drift over the ridge so the ridge cap stays covered on steep pitches."""
    t_top = step + 0.14
    t_bot = -0.02
    h0 = run * frac

    def P(h, off, side):
        y = ye0 + h if side == 0 else ye1 - h
        return (y, z_et + h * ta + off)

    z_apex = z_et + run * ta
    ds = ridge_w + 0.05
    lip = 0.05
    bottom = [P(h0 + lip, t_bot, 0), (yc, z_apex + t_bot), P(h0 + lip, t_bot, 1)]
    top = [P(h0, t_top * 0.55, 1), P(h0 + 0.12, t_top, 1), (yc + ds, z_apex + 0.16), (yc, z_apex + 0.21),
           (yc - ds, z_apex + 0.16), P(h0 + 0.12, t_top, 0), P(h0, t_top * 0.55, 0)]
    sec = [P(h0 - 0.02, t_bot - 0.03, 0)] + bottom + [P(h0 - 0.02, t_bot - 0.03, 1)] + top
    _extrude_x(g, sec, xa - 0.04, xb + 0.04, [mat] * len(sec), ["box"] * len(sec), mat)


def roof_hip(g, x0, x1, y0, y1, zw, pitch, mat, ov=0.5, th=0.22, course=0.34, step=0.045, under="MT_WoodPlanks",
             fascia="MT_Timber", courses=True, hips=True, hip_mat=None, finial=None, snow=None, snow_from=0.3,
             cut=None, crest=None, crest_spacing=0.85):
    """Hip (or pyramid, if square) roof as a solid: flat soffit at the eave, stepped tile courses.
    cut: horizontal inset (m, from the eave edge) where the roof stops with a flat top (lower part of a mansard).
    crest: material for an ornamental crest (spikes) along the ridge."""
    X0, X1, Y0, Y1 = x0 - ov, x1 + ov, y0 - ov, y1 + ov
    ta = math.tan(math.radians(pitch))
    ca = math.cos(math.radians(pitch))
    z_eb = zw - ov * ta
    z_et = z_eb + th / ca
    dfull = min(X1 - X0, Y1 - Y0) / 2
    dmax = min(cut, dfull) if cut else dfull
    is_cut = dmax < dfull - 1e-6
    prof = _sawtooth(dmax, dmax / ca, course, step, courses)
    rings = []
    for (d, o) in prof:
        z = z_et + d * ta + o
        rings.append([(X0 + d, Y0 + d, z), (X1 - d, Y0 + d, z), (X1 - d, Y1 - d, z), (X0 + d, Y1 - d, z)])
    base = [(X0, Y0, z_eb), (X1, Y0, z_eb), (X1, Y1, z_eb), (X0, Y1, z_eb)]
    with g.part():
        for i in range(4):
            j = (i + 1) % 4
            g.face([base[i], base[j], rings[0][j], rings[0][i]], fascia, "box")
        for r in range(len(rings) - 1):
            A, B = rings[r], rings[r + 1]
            for i in range(4):
                j = (i + 1) % 4
                g.face([A[i], A[j], B[j], B[i]], mat, "slope")
        if is_cut:
            g.face(list(rings[-1]), mat, "box")
        g.face(list(reversed(base)), under, "box")
    z_ridge = z_et + dmax * ta
    info = RoofInfo()
    info.cut_rect = (X0 + dmax, Y0 + dmax, X1 - dmax, Y1 - dmax) if is_cut else None
    info.z_cut = z_ridge if is_cut else None
    info.risers = [d - ov for (d, o) in prof if o > 0]
    info.z_ridge = z_ridge
    info.ta = ta
    info.zw = zw
    info.z_top = lambda d: z_et + (d + ov) * ta
    info.ridge = ((X0 + dmax, (Y0 + Y1) / 2), (X1 - dmax, (Y0 + Y1) / 2)) if (X1 - X0) >= (Y1 - Y0) else \
        (((X0 + X1) / 2, Y0 + dmax), ((X0 + X1) / 2, Y1 - dmax))
    if hips and is_cut:
        hm = hip_mat or mat
        for (cx, cy), (ex, ey) in zip([(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)],
                                      [(X0 + dmax, Y0 + dmax), (X1 - dmax, Y0 + dmax), (X1 - dmax, Y1 - dmax),
                                       (X0 + dmax, Y1 - dmax)]):
            p0 = Vector((cx, cy, z_et + step + 0.02))
            p1 = Vector((ex, ey, z_ridge + 0.02))
            p1 = p0 + (p1 - p0) * (1.0 - 0.06 / max((p1 - p0).length, 0.5))
            g.beam(p0, p1, 0.2, 0.14, hm, up=(0, 0, 1))
        hips = False
    if hips:
        hm = hip_mat or mat
        (rx0, ry0), (rx1, ry1) = info.ridge
        zt = z_ridge + 0.02
        corners = [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)]
        ends = [(rx0, ry0), (rx1, ry1), (rx1, ry1), (rx0, ry0)]
        if (X1 - X0) < (Y1 - Y0):
            ends = [(rx0, ry0), (rx0, ry0), (rx1, ry1), (rx1, ry1)]
        pyramid = abs(rx1 - rx0) + abs(ry1 - ry0) <= 0.05
        for (cx, cy), (ex, ey) in zip(corners, ends):
            p0 = Vector((cx, cy, z_et + step + 0.02))
            p1 = Vector((ex, ey, zt))
            if pyramid:
                p1 = p0 + (p1 - p0) * (1.0 - 0.3 / max((p1 - p0).length, 0.5))
            g.beam(p0, p1, 0.2, 0.14, hm, up=(0, 0, 1))
        if not pyramid:
            g.beam((rx0, ry0, zt), (rx1, ry1, zt), 0.24, 0.16, hm, ext=0.06)
        else:
            # small pyramid cap over the apex covering the hip ends
            cx, cy = rx0, ry0
            c = 0.34
            g.lathe([(c * 1.41, z_ridge - c * ta - 0.12), (c * 1.41, z_ridge - c * ta + 0.08), (0.0, z_ridge + 0.28)],
                    4, hm, cx, cy, a0=math.pi / 4, smooth=False)
    if finial and not is_cut:
        (rx0, ry0), (rx1, ry1) = info.ridge
        if abs(rx1 - rx0) + abs(ry1 - ry0) <= 0.05:
            finial_spike(g, rx0, ry0, z_ridge - 0.1, finial)
        else:
            for (fx, fy) in ((rx0, ry0), (rx1, ry1)):
                finial_spike(g, fx, fy, z_ridge + 0.02, finial)
    if crest and not is_cut:
        (rx0, ry0), (rx1, ry1) = info.ridge
        L = math.hypot(rx1 - rx0, ry1 - ry0)
        if L > 0.8:
            n = max(1, int(L / crest_spacing))
            for k in range(1, n):
                t = k / n
                cx, cy = rx0 + (rx1 - rx0) * t, ry0 + (ry1 - ry0) * t
                g.lathe([(0.045, z_ridge + 0.06), (0.07, z_ridge + 0.2), (0.0, z_ridge + 0.5)], 4, crest, cx, cy,
                        a0=math.pi / 4, smooth=False)
    if snow:
        _snow_hip(g, X0, X1, Y0, Y1, z_et, ta, dmax, step, snow_from, snow)
    return info


def _snow_hip(g, X0, X1, Y0, Y1, z_et, ta, dmax, step, frac, mat):
    d0 = dmax * frac
    rings = []
    for d, off in ((d0 - 0.02, -0.05), (d0, 0.09), (d0 + 0.12, step + 0.14), (dmax - 0.25, step + 0.4),
                   (dmax, step + 0.45)):
        z = z_et + d * ta + off
        rings.append([(X0 + d, Y0 + d, z), (X1 - d, Y0 + d, z), (X1 - d, Y1 - d, z), (X0 + d, Y1 - d, z)])
    zb = z_et + (d0 + 0.05) * ta - 0.02
    dbot = d0 + 0.05
    bot = [(X0 + dbot, Y0 + dbot, zb), (X1 - dbot, Y0 + dbot, zb), (X1 - dbot, Y1 - dbot, zb), (X0 + dbot, Y1 - dbot, zb)]
    with g.part():
        for i in range(4):
            j = (i + 1) % 4
            g.face([bot[i], bot[j], rings[0][j], rings[0][i]], mat)
        for r in range(len(rings) - 1):
            A, B = rings[r], rings[r + 1]
            for i in range(4):
                j = (i + 1) % 4
                g.face([A[i], A[j], B[j], B[i]], mat)
        g.face(list(reversed(bot)), mat)


def roof_cone(g, cx, cy, r, zw, height, mat, ov=0.4, th=0.2, course=0.4, step=0.05, segs=16, under="MT_WoodPlanks",
              fascia=None, courses=True, finial=None, snow=None, flare=0.0, a0=None):
    """Conical roof over a round tower of radius r (wall top zw); apex at zw + height."""
    R = r + ov
    slope = math.atan2(height, r)
    ta = math.tan(slope)
    z_eb = zw - ov * ta * (1.0 - flare)
    z_et = z_eb + th / math.cos(slope)
    apex = zw + height + th / math.cos(slope)
    run = R
    slen = math.hypot(run, apex - z_et)
    prof = [(R, z_eb)]
    n = max(1, int(round(slen / course))) if courses else 1
    for k in range(n):
        d = run * k / n
        rr = R - d
        z = z_et + (apex - z_et) * (k / n)
        prof.append((rr, z + (step if courses else 0.0)))
        d1 = run * (k + 1) / n
        if k + 1 < n:
            prof.append((R - d1, z_et + (apex - z_et) * ((k + 1) / n)))
    prof.append((0.0, apex))
    mats = [fascia or mat] + [mat] * (len(prof) - 2)
    a0 = (math.pi / segs) if a0 is None else a0
    g.lathe(prof, segs, mat, cx, cy, mats=mats, flat_caps_mat=under, smooth=False, a0=a0)
    if finial:
        finial_spike(g, cx, cy, apex - 0.25, finial)
    if snow:
        # snow cone over the upper 65 %: follows the tile base line, its buried bottom disc sits inside the roof
        def zbase(rad):
            return z_et + (apex - z_et) * (1.0 - rad / R)
        rr = R * 0.65
        t = step + 0.13
        sp = [(rr, zbase(rr) - 0.06), (rr + 0.035, zbase(rr) + 0.06), (rr * 0.93, zbase(rr * 0.93) + t),
              (rr * 0.5, zbase(rr * 0.5) + t), (0.0, apex + t)]
        g.lathe(sp, segs, snow, cx, cy, smooth=True, flat_caps_mat=snow, a0=a0)
    return apex


def finial_spike(g, cx, cy, z0, spec):
    """spec: dict(mat, h, r) ball + spike finial on a roof apex."""
    mat = spec.get("mat", "MT_Iron") if isinstance(spec, dict) else spec
    h = spec.get("h", 1.2) if isinstance(spec, dict) else 1.2
    r = spec.get("r", 0.08) if isinstance(spec, dict) else 0.08
    g.lathe([(r * 1.6, z0), (r * 1.6, z0 + 0.12), (r, z0 + 0.2), (r * 2.2, z0 + 0.35), (r * 2.2, z0 + 0.42),
             (r * 0.8, z0 + 0.55), (0.0, z0 + h)], 8, mat, cx, cy, smooth=True)


def dome(g, cx, cy, r, z0, mat, segs=20, rings=8, shape="hemi", lantern=None, drum_mat=None, ribs=None):
    """Dome sitting on z0 (its base ring buried 5 cm). shape: hemi, onion, pointed, low."""
    prof = []
    for i in range(rings + 1):
        t = i / rings
        a = t * math.pi / 2
        if shape == "hemi":
            rr, zz = r * math.cos(a), r * math.sin(a)
        elif shape == "low":
            rr, zz = r * math.cos(a), 0.6 * r * math.sin(a)
        elif shape == "pointed":
            rr = r * math.cos(a) ** 0.85
            zz = r * 1.35 * math.sin(a) ** 1.3
        else:  # onion
            s = math.sin(a * 2.0 - 0.0)
            rr = r * (1.0 + 0.22 * math.sin(t * math.pi * 1.25)) * math.cos(a) ** 0.9
            zz = r * 1.5 * t
        prof.append((max(rr, 0.0), z0 + zz))
    prof[0] = (prof[0][0], z0 - 0.05)
    prof[-1] = (0.0, prof[-1][1])
    g.lathe(prof, segs, mat, cx, cy, smooth=True, flat_caps_mat=mat)
    top = prof[-1][1]
    if lantern:
        finial_spike(g, cx, cy, top - 0.1, lantern)
    return top


# ====================================================================== misc


def column(g, cx, cy, z0, h, r, mat, segs=12, base=True, capital=True, cap_mat=None):
    prof = []
    if base:
        prof += [(r * 1.45, z0 - 0.05), (r * 1.45, z0 + 0.12), (r * 1.2, z0 + 0.2)]
    else:
        prof += [(r, z0 - 0.05)]
    prof += [(r, z0 + 0.28), (r * 0.9, z0 + h - 0.35)]
    if capital:
        prof += [(r * 1.25, z0 + h - 0.18), (r * 1.5, z0 + h - 0.1), (r * 1.5, z0 + h + 0.03)]
    else:
        prof += [(r * 0.9, z0 + h + 0.03)]
    g.lathe(prof, segs, mat, cx, cy, smooth=True)


def cornice(g, x0, y0, x1, y1, z, h, proud, mat):
    """Closed band wrapping a box footprint at height z..z+h, proud of the walls."""
    outer = [(x0 - proud, y0 - proud), (x1 + proud, y0 - proud), (x1 + proud, y1 + proud), (x0 - proud, y1 + proud)]
    inner = [(x0 + 0.15, y0 + 0.15), (x1 - 0.15, y0 + 0.15), (x1 - 0.15, y1 - 0.15), (x0 + 0.15, y1 - 0.15)]
    g.prism_holes(outer, [inner], z, z + h, mat)


def awning(g, fr, u0, u1, v_top, depth, drop, mat, poles=True, pole_mat="MT_Timber", scallop=True, th=0.04):
    """Sloped cloth awning on a wall face: from v_top at the wall down to v_top - drop at `depth` out."""
    w = u1 - u0
    # cloth sheet as thin slab, sunk into the wall at the back
    back_n = -0.04
    pts_sec = [(back_n, v_top + 0.02), (depth, v_top - drop), (depth, v_top - drop - th), (back_n, v_top + 0.02 - th)]
    # build via frame: section in (n, v), extruded along u
    for (a, b) in [(0, 1)]:
        pass
    o = fr.p(u0, 0, 0)
    sec_fr = Frame(o, fr.n, fr.v, -fr.u)  # (u'=n, v'=v) extrude along -(-u) ... handled by plate below
    # plate extrudes along sec_fr.n = -(u) from n0 to n1 -> use negative range to go along +u
    g.plate(sec_fr, pts_sec, -w, 0.0, mat)
    if scallop:
        k = max(2, int(w / 0.5))
        for i in range(k):
            ua = u0 + w * i / k + 0.02
            ub = u0 + w * (i + 1) / k - 0.02
            um = (ua + ub) / 2
            tri = [(ua, v_top - drop - th + 0.03), (ub, v_top - drop - th + 0.03), (um, v_top - drop - 0.2)]
            wplate(g, fr, tri, depth - 0.03, depth - 0.008, mat)
    if poles:
        for u in (u0 + 0.1, u1 - 0.1):
            pb = fr.p(u, 0, depth - 0.06)
            pt = fr.p(u, v_top - drop - th + 0.025, depth - 0.06)
            g.cylinder(pb.x, pb.y, 0.035, pb.z - 0.05, pt.z, 6, pole_mat)


def stair(g, fr, u0, u1, z_top, mat="MT_Stone", run_per=0.3, embed=0.12, z_bot=-0.3, rise=0.17):
    """Straight flight of steps against the wall face fr (vertical frame), rising from the ground (z=0) to
    z_top at the wall; spans u0..u1 along the face. Built as one sawtooth prism, buried below ground."""
    n = max(1, int(round(z_top / rise)))
    run = run_per * n
    h = [z_top * k / n for k in range(n + 1)]
    nk = [run * (n - k + 1) / n for k in range(n + 1)]  # front edge of step k (k = 1..n)
    z_bot = g.buried(z_bot)
    pts = [(run, z_bot), (-embed, z_bot), (-embed, h[n])]
    for k in range(n, 0, -1):
        pts.append((nk[k], h[k]))
        if k > 1:
            pts.append((nk[k], h[k - 1]))
    o = Frame((fr.o.x, fr.o.y, 0.0), fr.n, (0, 0, 1))
    # extrusion axis = fr.n x Z = -fr.u, so the range [-u1, -u0] along it covers u0..u1 of the face
    base = fr.p(0, 0, 0)
    o = Frame((base.x, base.y, 0.0), fr.n, (0, 0, 1))
    g.plate(o, pts, -u1, -u0, mat)


def poly_body(g, poly, z0, z1, mat, ops=None, top=True, bottom=True, top_mat=None):
    """Closed prism over a CCW polygon with recessed openings per side: ops = {edge_index: [Opening]}.
    Face frames: origin at vertex i (z0), u along edge i. Returns the list of (Frame, length) per edge."""
    ops = ops or {}
    poly = [tuple(p) for p in poly]
    from arch_geo import ccw as _ccw
    poly = _ccw(poly)
    k = len(poly)
    faces = []
    frames = []
    for i in range(k):
        a = poly[i]
        b = poly[(i + 1) % k]
        pts = [(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)]
        faces.append((pts, mat, ops.get(i)))
        L = math.hypot(b[0] - a[0], b[1] - a[1])
        frames.append((Frame((a[0], a[1], z0), (b[0] - a[0], b[1] - a[1], 0), (0, 0, 1)), L))
    if top:
        faces.append(([(p[0], p[1], z1) for p in poly], top_mat or mat, None))
    if bottom:
        faces.append(([(p[0], p[1], z0) for p in reversed(poly)], mat, None))
    solid(g, faces)
    return frames


def hanging_sign(g, fr, u, v, board="MT_WoodPlanks", bracket="MT_Iron", w=0.7, h=0.5, out=1.0):
    """Iron bracket sticking out of the wall at (u, v) with a board hanging under it, perpendicular to the wall."""
    wplate(g, fr, rect(u - 0.03, v - 0.05, 0.06, 0.07), -0.1, out, bracket)
    # diagonal stay
    wplate(g, fr, [(u - 0.02, v - 0.55), (u + 0.02, v - 0.55), (u + 0.02, v - 0.03), (u - 0.02, v - 0.03)],
            -0.08, 0.06, bracket)
    stay = [(0.02, -0.5), (0.1, -0.5), (out * 0.72, -0.03), (out * 0.64, -0.03)]
    o = fr.p(u, v, 0)
    sfr = Frame(o, fr.n, fr.v, None)
    g.plate(sfr, stay, -0.018, 0.018, bracket)
    # chains + board
    bf = Frame(fr.p(u, 0, 0), fr.n, fr.v, None)
    g.plate(bf, rect(out * 0.3, v - 0.12 - h, w, h), -0.025, 0.025, board)
    for f in (0.36, 0.3 + w - 0.06):
        g.plate(bf, rect(out * f - 0.012, v - 0.14, 0.024, 0.1), -0.01, 0.01, bracket)


def balcony(g, fr, u0, u1, v, depth, floor_mat="MT_WoodPlanks", rail_mat="MT_Timber", bracket_mat="MT_Timber",
            rail_h=1.0, posts=True, solid_rail=None, bracket_us=None, bracket_back=0.2):
    """Balcony on a wall face: slab top at v - 0.02, brackets below, balustrade posts + top rail."""
    w = u1 - u0
    wplate(g, fr, rect(u0, v - 0.2, w, 0.18), -0.12, depth, floor_mat)
    n = max(2, int(w / 1.2) + 1)
    us = bracket_us if bracket_us is not None else [u0 + 0.12 + (w - 0.24) * k / (n - 1) for k in range(n)]
    for u in us:
        bfr = Frame(fr.p(u, 0, 0), fr.n, fr.v, None)
        g.plate(bfr, [(-bracket_back, v - 0.75), (0.0, v - 0.75), (depth * 0.8, v - 0.19), (-bracket_back, v - 0.19)],
                -0.06, 0.06, bracket_mat)
    if solid_rail:
        wplate(g, fr, rect(u0 + 0.02, v - 0.03, w - 0.04, rail_h - 0.05), depth - 0.14, depth - 0.04, solid_rail)
        return
    # balusters: front row + sides
    nb = max(3, int(w / 0.22))
    for k in range(nb + 1):
        u = u0 + 0.06 + (w - 0.12) * k / nb
        wplate(g, fr, rect(u - 0.025, v - 0.04, 0.05, rail_h - 0.07), depth - 0.1, depth - 0.05, rail_mat)
    for u in (u0 + 0.06, u1 - 0.06):
        for f in (0.35, 0.65):
            wplate(g, fr, rect(u - 0.025, v - 0.04, 0.05, rail_h - 0.07), depth * f - 0.025, depth * f + 0.025,
                    rail_mat)
    # top rails (front and sides)
    wplate(g, fr, rect(u0 + 0.02, v + rail_h - 0.13, w - 0.04, 0.09), depth - 0.12, depth - 0.03, rail_mat)
    for u in (u0 + 0.045, u1 - 0.095):
        wplate(g, fr, rect(u, v + rail_h - 0.12, 0.05, 0.07), -0.05, depth - 0.06, rail_mat)


def shop_front(g, fr, op, awning_mat, counter_mat="MT_WoodPlanks", awning_depth=1.3, awning_drop=0.55):
    """Counter under a shop opening plus a cloth awning above it."""
    u0, v0, u1, v1 = op.box
    wplate(g, fr, rect(u0 - 0.14, v0 - 0.135, u1 - u0 + 0.28, 0.1), -0.1, 0.42, counter_mat)
    wplate(g, fr, rect(u0 - 0.05, v0 - 0.7, u1 - u0 + 0.1, 0.61), -0.08, 0.36, counter_mat)
    awning(g, fr, u0 - 0.15, u1 + 0.15, v1 + 0.35, awning_depth, awning_drop, awning_mat, poles=False)


def hoist_beam(g, fr, u, v, out=0.9, mat="MT_Timber"):
    wplate(g, fr, rect(u - 0.09, v - 0.09, 0.18, 0.18), -0.3, out, mat)
    p = fr.p(u, v - 0.1, out - 0.12)
    g.cylinder(p.x, p.y, 0.03, p.z - 0.27, p.z + 0.03, 6, "MT_Iron")
    g.cylinder(p.x, p.y, 0.1, p.z - 0.45, p.z - 0.25, 8, "MT_WoodPlanks")


def roof_shed(g, x0, x1, y0, y1, z_low, z_high, mat, ov=0.3, ovv=0.25, th=0.16, under="MT_WoodPlanks",
              fascia="MT_Timber", verge=None, courses=True, course=0.34, step=0.04, ov_back=None):
    """Mono-pitch (lean-to) roof over [x0,x1]x[y0,y1]: underside passes through (y0, z_low) and (y1, z_high).
    Returns the underside height function z_under(y) and the slab thickness."""
    ta = (z_high - z_low) / (y1 - y0)
    ca = math.cos(math.atan(ta))
    ob = ov if ov_back is None else ov_back
    yb0, yb1 = y0 - ov, y1 + ob
    zb0 = z_low - ov * ta
    zb1 = z_high + ob * ta
    run = yb1 - yb0
    prof = _sawtooth(run, run / ca, course, step, courses)
    top = [(yb0 + h, zb0 + th / ca + h * ta + o) for (h, o) in reversed(prof)]
    sec = [(yb0, zb0), (yb1, zb1)] + top
    emat = [under, fascia] + [mat] * (len(top) - 1) + [fascia]
    euv = ["slope", "box"] + ["slope"] * (len(top) - 1) + ["box"]
    _extrude_x(g, sec, x0 - ovv, x1 + ovv, emat, euv, verge or mat)
    return (lambda y: z_low + (y - y0) * ta), th / ca


def shed_body(g, x0, y0, x1, y1, zb, z_front, z_back, mat, ops=None):
    """Closed body whose top follows a mono-pitch roof plane (front lower at y0, back higher at y1)."""
    F = {
        "front": [(x0, y0, zb), (x1, y0, zb), (x1, y0, z_front), (x0, y0, z_front)],
        "right": [(x1, y0, zb), (x1, y1, zb), (x1, y1, z_back), (x1, y0, z_front)],
        "back": [(x1, y1, zb), (x0, y1, zb), (x0, y1, z_back), (x1, y1, z_back)],
        "left": [(x0, y1, zb), (x0, y0, zb), (x0, y0, z_front), (x0, y1, z_back)],
        "top": [(x0, y0, z_front), (x1, y0, z_front), (x1, y1, z_back), (x0, y1, z_back)],
        "bottom": [(x0, y0, zb), (x0, y1, zb), (x1, y1, zb), (x1, y0, zb)],
    }
    ops = ops or {}
    frames = solid(g, [(F[k], mat, ops.get(k)) for k in F])
    return dict(zip(F.keys(), frames))


def porch(g, x0, x1, y_wall, depth, z_floor, z_eave, z_wall_top, roof_mat="MT_RoofRed", post_mat="MT_Timber",
          deck_mat="MT_WoodPlanks", step_mat="MT_Stone", posts=2, deck=True, courses=True):
    """Lean-to porch on the front (-Y) wall: posts, a raised deck with steps and a mono-pitch roof whose
    high edge is sunk into the wall."""
    yf = y_wall - depth
    if deck and z_floor > 0.08:
        g.box(x0 + 0.05, yf + 0.05, g.buried(-0.3), x1 - 0.05, y_wall + 0.14, z_floor - 0.045, deck_mat)
        fr = Frame((x0, yf + 0.05, 0.0), (1, 0, 0), (0, 0, 1))
        cx = (x0 + x1) / 2
        stair(g, fr, cx - x0 - 0.7, cx - x0 + 0.7, z_floor - 0.055, step_mat)
    xs = [x0 + 0.12 + (x1 - x0 - 0.24) * k / (posts - 1) for k in range(posts)]
    zp0 = (z_floor - 0.06) if (deck and z_floor > 0.08) else g.buried(-0.25)
    for x in xs:
        g.box(x - 0.09, yf + 0.13, zp0, x + 0.09, yf + 0.31, z_eave + 0.06, post_mat)
    # beam along the front on top of the posts
    g.box(x0 + 0.02, yf + 0.11, z_eave - 0.14, x1 - 0.02, yf + 0.33, z_eave + 0.035, post_mat)
    roof_shed(g, x0, x1, yf + 0.22, y_wall + 0.05, z_eave + 0.035, z_wall_top, roof_mat, ov=0.3, ovv=0.2,
              th=0.14, courses=courses, ov_back=0.05)


def exterior_chimney(g, fr, u, w, depth, z_top, mat="MT_Stone", cap="MT_Stone", pots=1, pot_mat="MT_RoofRed",
                     base_w=None, base_h=2.2):
    """Chimney stack standing against a wall face (frame fr), from below ground to z_top."""
    bw = base_w or w + 0.5
    o = fr.p(u, 0, 0)
    # wide fireplace base, then the stack; both sink 0.12 m into the wall
    zb = g.buried(-0.4)
    wplate(g, fr, rect(u - bw / 2, zb - fr.o.z, bw, base_h + 0.02 - zb), -0.12, depth + 0.15, mat)
    wplate(g, fr, rect(u - w / 2, base_h - 0.1 - fr.o.z, w, z_top - base_h + 0.1), -0.12, depth, mat)
    # sloped shoulder between base and stack
    sh = Frame(fr.p(u - bw / 2, base_h - fr.o.z, 0), fr.u, fr.v, fr.n)
    wplate(g, sh, [(0.0, 0.0), (bw, 0.0), (bw - (bw - w) / 2, 0.45), ((bw - w) / 2, 0.45)], -0.12, depth + 0.08, mat)
    wplate(g, fr, rect(u - w / 2 - 0.07, z_top - 0.12 - fr.o.z, w + 0.14, 0.2), -0.12, depth + 0.07, cap)
    for i in range(pots):
        c = fr.p(u + (i - (pots - 1) / 2) * 0.28, z_top - fr.o.z, depth / 2)
        g.cylinder(c.x, c.y, 0.08, z_top + 0.04, z_top + 0.36, 8, pot_mat)


def crystal_lantern(g, fr, u, v, out=0.55, h=0.5, bracket=True):
    """Wall lantern: iron bracket arm from the wall at (u, v) and a caged glowing crystal hanging from it."""
    if bracket:
        wplate(g, fr, rect(u - 0.025, v - 0.03, 0.05, 0.06), -0.08, out + 0.03, "MT_Iron")
        wplate(g, fr, rect(u - 0.02, v - 0.42, 0.04, 0.4), -0.06, 0.04, "MT_Iron")
        sfr = Frame(fr.p(u, v, 0), fr.n, fr.v, None)
        g.plate(sfr, [(0.02, -0.38), (0.07, -0.38), (out * 0.62, -0.02), (out * 0.55, -0.02)], -0.012, 0.012,
                "MT_Iron")
    c = fr.p(u, v, out)
    zt = c.z - 0.01
    lantern_cage(g, c.x, c.y, zt - h, zt, 0.12)


def lantern_cage(g, cx, cy, z0, z1, r):
    """Iron lantern (square cage with a pyramid hood) around an elongated glowing crystal; z0..z1 overall."""
    h = z1 - z0
    g.lathe([(r * 1.7, z1 - 0.12), (r * 1.75, z1 - 0.1), (0.0, z1)], 4, "MT_Iron", cx, cy, a0=math.pi / 4,
            smooth=False)
    g.box(cx - r, cy - r, z0, cx + r, cy + r, z0 + 0.04, "MT_Iron")
    for sx in (-1, 1):
        for sy in (-1, 1):
            bx, by = cx + sx * (r - 0.02), cy + sy * (r - 0.02)
            g.box(bx - 0.015, by - 0.015, z0 + 0.02, bx + 0.015, by + 0.015, z1 - 0.1, "MT_Iron")
    g.lathe([(0.0, z0 + 0.03), (r * 0.62, z0 + h * 0.4), (r * 0.62, z0 + h * 0.58), (0.0, z1 - 0.11)], 6,
            "MT_Crystal", cx, cy, smooth=False)


def emblem_shield(g, fr, u, v, w=1.2, h=1.5, mat="MT_WoodPlanks", trim="MT_Iron", swords=True, out=0.06):
    """Shield-shaped board on a wall with crossed swords (guild emblem); (u, v) = top centre."""
    pts = [(u - w / 2, v), (u + w / 2, v), (u + w / 2, v - h * 0.55), (u + w * 0.28, v - h * 0.85), (u, v - h),
           (u - w * 0.28, v - h * 0.85), (u - w / 2, v - h * 0.55)]
    wplate(g, fr, pts, -0.04, out, mat)
    rim_o = offset_poly2d(ccw(pts), 0.05)
    wplate(g, fr, rim_o, -0.03, out - 0.03, trim)
    if swords:
        for s in (-1, 1):
            a = math.radians(40) * s
            cu, cv = u, v - h * 0.48
            d = (math.sin(a), math.cos(a))
            L = h * 0.95
            p0 = (cu - d[0] * L / 2, cv - d[1] * L / 2)
            p1 = (cu + d[0] * L / 2, cv + d[1] * L / 2)
            nrm = (-d[1], d[0])
            bw = 0.035
            blade = [(p0[0] + nrm[0] * bw, p0[1] + nrm[1] * bw), (p0[0] - nrm[0] * bw, p0[1] - nrm[1] * bw),
                     (p1[0] - nrm[0] * 0.01, p1[1] - nrm[1] * 0.01), (p1[0] + nrm[0] * 0.01, p1[1] + nrm[1] * 0.01)]
            z = out + (0.012 if s > 0 else 0.028)
            wplate(g, fr, blade, out - (0.02 if s > 0 else 0.025), z, "MT_Iron")
            gu = (p0[0] + d[0] * L * 0.22, p0[1] + d[1] * L * 0.22)
            guard = [(gu[0] + nrm[0] * 0.14 - d[0] * 0.025, gu[1] + nrm[1] * 0.14 - d[1] * 0.025),
                     (gu[0] - nrm[0] * 0.14 - d[0] * 0.025, gu[1] - nrm[1] * 0.14 - d[1] * 0.025),
                     (gu[0] - nrm[0] * 0.14 + d[0] * 0.025, gu[1] - nrm[1] * 0.14 + d[1] * 0.025),
                     (gu[0] + nrm[0] * 0.14 + d[0] * 0.025, gu[1] + nrm[1] * 0.14 + d[1] * 0.025)]
            wplate(g, fr, guard, out - 0.015, z + 0.02, "MT_Gold")


def gable_block(g, fr, u0, u1, v0, h, n0, n1, side_mat, cap_mat):
    """Triangular prism on a wall face (a pediment block / portico roof): triangle base u0..u1 at height v0, apex
    h above, extruded along the face normal from n0 to n1. Sloped faces get side_mat, triangles cap_mat."""
    uc = (u0 + u1) / 2
    tri = [(u0, v0), (u1, v0), (uc, v0 + h)]
    P = lambda u, v, n: fr.p(u, v, n)
    with g.part():
        g.face([P(*t, n1) for t in tri], cap_mat)
        g.face([P(*t, n0) for t in reversed(tri)], cap_mat)
        for (a, b) in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
            m = cap_mat if a[1] == b[1] else side_mat
            g.face([P(a[0], a[1], n0), P(b[0], b[1], n0), P(b[0], b[1], n1), P(a[0], a[1], n1)], m,
                   "slope" if m == side_mat else "box")


def portico(g, fr, uc, width, depth, col_h, ncols, mat="MT_StoneWhite", roof_mat="MT_RoofBlue", col_r=0.22,
            floor_h=0.0, pediment=True, gold=None):
    """Classical portico on a wall face: a raised floor, a row of columns along the front edge, an entablature
    beam and a pediment roof (triangular prism) running back into the wall."""
    u0, u1 = uc - width / 2, uc + width / 2
    if floor_h > 0.05:
        wplate(g, fr, rect(u0 - 0.1, g.buried(-0.4) - fr.o.z, width + 0.2, floor_h + 0.4 - 0.02), -0.2, depth + 0.1,
               mat)
    zf = floor_h
    xs = [u0 + col_r + 0.1 + (width - 2 * col_r - 0.2) * k / (ncols - 1) for k in range(ncols)]
    for u in xs:
        c = fr.p(u, 0, depth - col_r - 0.12)
        column(g, c.x, c.y, fr.o.z + zf - 0.05, col_h + 0.03, col_r, mat, segs=12)
    ent_v = zf + col_h
    wplate(g, fr, rect(u0 - 0.05, ent_v - 0.02, width + 0.1, 0.62), -0.15, depth + 0.02, mat)
    wplate(g, fr, rect(u0 - 0.15, ent_v + 0.58, width + 0.3, 0.14), -0.15, depth + 0.12, mat)
    if pediment:
        ph = width * 0.2
        gable_block(g, fr, u0 - 0.12, u1 + 0.12, ent_v + 0.7, ph, -0.25, depth + 0.1, roof_mat, mat)
        # raking cornice (tympanum frame) proud of the pediment face
        o = fr.shifted(dn=depth + 0.1)
        ringo = [(u0 - 0.2, ent_v + 0.66), (u1 + 0.2, ent_v + 0.66), (uc, ent_v + 0.66 + ph + 0.12)]
        ringi = [(u0 + 0.35, ent_v + 0.84), (u1 - 0.35, ent_v + 0.84), (uc, ent_v + 0.7 + ph - 0.18)]
        g.ring_plate(o, ringo, ringi, -0.06, 0.08, mat)
        if gold:
            top = fr.p(uc, ent_v + 0.66 + ph + 0.12, depth + 0.02)
            finial_spike(g, top.x, top.y, top.z - 0.12, dict(mat=gold, h=0.9, r=0.07))
    return ent_v + 0.7


def balustrade(g, fr, u0, u1, v, h=1.0, n0=-0.05, n1=0.3, mat="MT_StoneWhite", spacing=0.28, posts_every=6,
               segs=6, solid=False):
    """Balustrade on top of a wall / terrace edge: plinth rail, top rail, balusters and posts."""
    w = n1 - n0
    g.plate(fr, rect(u0, v - 0.02, u1 - u0, 0.14), n0, n1, mat)
    g.plate(fr, rect(u0 - 0.03, v + h - 0.12, u1 - u0 + 0.06, 0.14), n0 - 0.03, n1 + 0.03, mat)
    if solid:
        g.plate(fr, rect(u0 + 0.02, v + 0.1, u1 - u0 - 0.04, h - 0.2), n0 + 0.04, n1 - 0.04, mat)
        return
    n = max(2, int((u1 - u0) / spacing))
    for k in range(n + 1):
        u = u0 + 0.16 + (u1 - u0 - 0.32) * k / n
        if k % posts_every == 0:
            g.plate(fr, rect(u - 0.12, v + 0.1, 0.24, h - 0.2), n0 + 0.02 - 0.0, n1 - 0.02, mat)
        else:
            cu = u
            c = fr.p(cu, 0, (n0 + n1) / 2)
            if segs <= 4:
                g.lathe([(0.075, fr.o.z + v + 0.1), (0.1, fr.o.z + v + 0.32), (0.06, fr.o.z + v + h - 0.1)], 4, mat,
                        c.x, c.y, smooth=False, a0=math.pi / 4)
            else:
                g.lathe([(0.055, fr.o.z + v + 0.1), (0.085, fr.o.z + v + 0.3), (0.05, fr.o.z + v + h - 0.3),
                         (0.07, fr.o.z + v + h - 0.19), (0.07, fr.o.z + v + h - 0.1)], segs, mat, c.x, c.y,
                        smooth=False)


def drum_dome(g, cx, cy, z0, r, drum_h, dome_mat, wall_mat="MT_StoneWhite", segs=16, windows=True, shape="hemi",
              lantern_mat="MT_Gold", ribs=None):
    """Cylindrical drum with arched windows, a cornice ring, a dome and a small lantern with a finial."""
    poly = circle_poly(cx, cy, r, segs, a0=math.pi / segs)
    Lf = 2 * r * math.sin(math.pi / segs)
    ops = {}
    if windows:
        for e in range(0, segs, 2):
            ops[e] = [Opening(arch_poly(Lf / 2 - Lf * 0.22, drum_h * 0.25, Lf * 0.44, drum_h * 0.55, 6), depth=0.2,
                              kind="arch")]
    frames = poly_body(g, poly, z0, z0 + drum_h, wall_mat, ops=ops)
    for e, lst in ops.items():
        fr, _ = frames[e]
        for op in lst:
            window_trim(g, fr, op, dict(trim=wall_mat, frame_w=0.08, sill=False, mullions=False, shutters=False))
    g.lathe([(r - 0.3, z0 + drum_h - 0.3), (r + 0.06, z0 + drum_h - 0.3), (r + 0.28, z0 + drum_h - 0.05),
             (r + 0.28, z0 + drum_h + 0.12), (r - 0.3, z0 + drum_h + 0.12)], segs, wall_mat, cx, cy,
            a0=math.pi / segs, smooth=False)
    top = dome(g, cx, cy, r + 0.1, z0 + drum_h + 0.1, dome_mat, segs=segs * 2 if segs < 20 else segs, rings=8,
               shape=shape)
    lr = max(0.35, r * 0.18)
    g.cylinder(cx, cy, lr, top - 0.15, top + lr * 1.6, 8, wall_mat, smooth=False)
    ltop = dome(g, cx, cy, lr * 1.12, top + lr * 1.6, dome_mat, segs=8, rings=4)
    finial_spike(g, cx, cy, ltop - 0.05, dict(mat=lantern_mat, h=max(0.8, r * 0.35), r=0.06 + r * 0.01))
    return ltop


def arcade_outline(L, H, arches, segs=8, pointed=False):
    """Outline of a wall slab 0..L x 0..H whose bottom edge is notched by open arches.
    arches: list of (u0, width, crown_height) sorted by u0."""
    pts = [(0.0, 0.0)]
    for (u0, w, hc) in arches:
        a = arch_poly(u0, 0.0, w, hc, segs, pointed=pointed)
        # a = [bottom-left, bottom-right, arc from right springing to left springing]
        arc = a[2:]
        pts.append((u0, 0.0))
        pts += list(reversed(arc))
        pts.append((u0 + w, 0.0))
    pts += [(L, 0.0), (L, H), (0.0, H)]
    out = []
    for p in pts:
        if not out or abs(out[-1][0] - p[0]) > 1e-6 or abs(out[-1][1] - p[1]) > 1e-6:
            out.append(p)
    if abs(out[0][0] - out[-1][0]) < 1e-6 and abs(out[0][1] - out[-1][1]) < 1e-6:
        out.pop()
    return out


def buttress(g, fr, u, v0, h, depth, width=0.6, mat="MT_StoneWhite", steps=2):
    """Stepped buttress against a wall face at u (centre), from v0 up h, projecting `depth` at its foot."""
    prof = [(-0.25, v0), (depth, v0)]
    for k in range(steps):
        t = (k + 1) / steps
        d = depth * (1 - 0.45 * t)
        vt = v0 + h * t
        prof += [(d, vt - 0.35), (d * 0.8, vt)] if k < steps - 1 else [(d, vt - 0.6), (0.12, vt)]
    prof += [(-0.25, v0 + h)]
    sfr = Frame(fr.p(u, 0, 0), fr.n, fr.v, None)
    g.plate(sfr, prof, -width / 2, width / 2, mat)


def rose_tracery(g, fr, uc, vc, r, depth, mat="MT_StoneWhite"):
    """Tracery inside a round window pocket: inner ring and four crossing bars at distinct depths."""
    back = -depth
    inner_o = [(uc + r * 0.42 * math.cos(math.tau * i / 16), vc + r * 0.42 * math.sin(math.tau * i / 16)) for i in range(16)]
    inner_i = [(uc + r * 0.32 * math.cos(math.tau * i / 16), vc + r * 0.32 * math.sin(math.tau * i / 16)) for i in range(16)]
    g.ring_plate(fr, inner_o, inner_i, back - 0.02, back + 0.075, mat)
    for k in range(4):
        a = math.pi * k / 4
        d = (math.cos(a), math.sin(a))
        nrm = (-d[1], d[0])
        L = r + 0.03
        bw = 0.035
        bar = [(uc - d[0] * L + nrm[0] * bw, vc - d[1] * L + nrm[1] * bw), (uc - d[0] * L - nrm[0] * bw, vc - d[1] * L - nrm[1] * bw),
               (uc + d[0] * L - nrm[0] * bw, vc + d[1] * L - nrm[1] * bw), (uc + d[0] * L + nrm[0] * bw, vc + d[1] * L + nrm[1] * bw)]
        g.plate(fr, bar, back - 0.03 - 0.004 * k, back + 0.05 - 0.004 * k, mat)


def parapet_ring(g, x0, y0, x1, y1, z_roof, h, mat, proud=0.05, thick=0.35, coping=None, merlons=None):
    """Parapet around a flat roof: a closed ring from 0.1 below the roof top up h, proud of the walls; optional
    coping lip and rounded / stepped merlons ('round' | 'step') along its top."""
    outer = [(x0 - proud, y0 - proud), (x1 + proud, y0 - proud), (x1 + proud, y1 + proud), (x0 - proud, y1 + proud)]
    inner = [(x0 + thick, y0 + thick), (x1 - thick, y0 + thick), (x1 - thick, y1 - thick), (x0 + thick, y1 - thick)]
    g.prism_holes(outer, [inner], z_roof - 0.1, z_roof + h, mat)
    if coping:
        o2 = [(x0 - proud - 0.04, y0 - proud - 0.04), (x1 + proud + 0.04, y0 - proud - 0.04),
              (x1 + proud + 0.04, y1 + proud + 0.04), (x0 - proud - 0.04, y1 + proud + 0.04)]
        i2 = [(x0 + thick - 0.04, y0 + thick - 0.04), (x1 - thick + 0.04, y0 + thick - 0.04),
              (x1 - thick + 0.04, y1 - thick + 0.04), (x0 + thick - 0.04, y1 - thick + 0.04)]
        g.prism_holes(o2, [i2], z_roof + h - 0.06, z_roof + h + 0.07, coping)
    if merlons:
        zt = z_roof + h + (0.07 if coping else 0.0)
        for (a, b, fixed, axis, inward) in ((x0, x1, y0, "x", 1), (x0, x1, y1, "x", -1), (y0, y1, x0, "y", 1),
                                           (y0, y1, x1, "y", -1)):
            m = thick + 0.3          # keep merlons clear of the corners (rows would overlap there)
            L = b - a - 2 * m
            if L < 0.6:
                continue
            n = max(1, int(L / 1.1))
            for k in range(n):
                c = a + m + L * (k + 0.5) / n
                f0 = fixed - inward * (proud - 0.02)
                f1 = fixed + inward * (thick - 0.02)
                w = 0.5
                d = abs(f1 - f0)
                if merlons == "round":
                    prof = [(0, 0), (w, 0), (w, 0.3)] + [(w / 2 + w / 2 * math.cos(math.pi * i / 6),
                                                          0.3 + w / 2 * math.sin(math.pi * i / 6)) for i in range(1, 6)] + [(0, 0.3)]
                else:
                    prof = [(0, 0), (w, 0), (w, 0.25), (w * 0.8, 0.25), (w * 0.8, 0.45), (w * 0.2, 0.45), (w * 0.2, 0.25),
                            (0, 0.25)]
                if axis == "x":
                    # frame normal = X x Z = -Y, so the range [-d, 0] extrudes from min(f0,f1) toward +Y
                    g.plate(Frame((c - w / 2, min(f0, f1), zt - 0.03), (1, 0, 0), (0, 0, 1)), prof, -d, 0.0, mat)
                else:
                    # frame normal = Y x Z = +X
                    g.plate(Frame((min(f0, f1), c - w / 2, zt - 0.03), (0, 1, 0), (0, 0, 1)), prof, 0.0, d, mat)


def viga_row(g, fr, L, v, n_out=0.4, size=0.16, spacing=0.9, mat="MT_Timber", margin=0.5):
    """Row of protruding round-ish roof beam ends (vigas) under a flat roof."""
    n = max(1, int((L - 2 * margin) / spacing))
    for k in range(n + 1):
        u = margin + (L - 2 * margin) * k / n
        wplate(g, fr, [(u - size / 2, v), (u + size / 2, v), (u + size / 2, v + size * 0.7), (u, v + size),
                       (u - size / 2, v + size * 0.7)], -0.1, n_out, mat)
