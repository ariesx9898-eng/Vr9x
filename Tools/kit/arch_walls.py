"""City walls for six styles: SM_<Style>_Wall_12m, SM_<Style>_WallTower, SM_<Style>_Gate.

Conventions (all styles):
  * Wall_12m runs along X from x = -6 to +6, centred on its axis (origin on the wall centreline at ground level),
    outer (enemy) face toward -Y. The body, outer talus, walkway and both parapets are one extruded cross-section, so
    consecutive segments placed 12 m apart meet edge-to-edge with continuous faces; merlons sit in 12/n m cells so the
    rhythm continues across joints. For a run of length L use n = round(L / 12) segments scaled along X by L / (12 n)
    (never overlap segments: overlapping coplanar faces would z-fight).
  * WallTower is centred on a joint (origin = tower axis); its radius (or half size) covers wall ends meeting at any
    angle. Walkway doors face +/-X at walkway height.
  * Gate replaces one 12 m segment: its flanking towers are centred at x = +/-6 (on the joints to the neighbouring
    segments); the open arched passage is centred on the origin.
"""
import math

from mathutils import Vector

from arch_geo import Frame
from arch_parts import (Opening, _extrude_x, arcade_outline, arch_poly, circle_poly, cornice, dome, door_trim,
                        finial_spike, poly_body, rect, roof_cone, window_trim)
from arch_registry import asset

STYLES = {
    "Asura": dict(mat="MT_Stone", H=8.0, T=3.4, merlon="rect", tower="round", r=4.2, roof=("cone", "MT_RoofRed"),
                  banner="MT_ClothRed", corbel=0.06, buttresses=True),
    "Noble": dict(mat="MT_StoneWhite", H=10.0, T=4.0, merlon="rect", tower="round", r=4.6, roof=("dome", "MT_RoofBlue"),
                  banner="MT_ClothBlue", corbel=0.08, pilasters=True, gold=True),
    "North": dict(mat="MT_Stone", H=8.0, T=3.4, merlon="rect", tower="round", r=4.2, roof=("cone", "MT_RoofDark"),
                  banner="MT_ClothBlue", corbel=0.06, snow=True, buttresses=True),
    "Millis": dict(mat="MT_StoneWhite", H=9.0, T=3.6, merlon="swallow", tower="round", r=4.3,
                   roof=("spire", "MT_RoofBlue"), banner="MT_ClothBlue", corbel=0.42, machicolation=True),
    "Demon": dict(mat="MT_DemonRock", H=8.0, T=3.8, merlon="tri", tower="square", r=4.2, roof=("crystal", None),
                  banner="MT_Hide", corbel=0.06, bone=True),
    "Desert": dict(mat="MT_Sandstone", H=7.0, T=3.2, merlon="round", tower="square", r=4.0,
                   roof=("dome", "MT_PlasterTan"), banner="MT_ClothRed", corbel=0.06, vigas=True),
}


def merlon_profile(shape, w, hm):
    """Merlon outline in (u, v) from (0, 0) to (w, 0) with height hm."""
    if shape == "tri":
        return [(0, 0), (w, 0), (w * 0.62, hm * 0.55), (w / 2, hm), (w * 0.38, hm * 0.55)]
    if shape == "round":
        r = w / 2
        return [(0, 0), (w, 0), (w, hm - r)] + [(r + r * math.cos(math.pi * i / 6), hm - r + r * math.sin(math.pi * i / 6))
                                                for i in range(1, 6)] + [(0, hm - r)]
    if shape == "swallow":
        return [(0, 0), (w, 0), (w, hm), (w * 0.5, hm * 0.62), (0, hm)]
    return [(0, 0), (w, 0), (w, hm), (0, hm)]


def merlon_row(g, x0, x1, y_out, y_in, z, S, cell=1.5, w=0.85, hm=0.9, snow=False):
    """Merlons along X between x0 and x1 on a parapet whose faces are at y_out / y_in (merlons inset 3 cm)."""
    L = x1 - x0
    n = max(1, int(round(L / cell)))
    c = L / n
    prof = merlon_profile(S["merlon"], w, hm)
    for k in range(n):
        u = x0 + c * (k + 0.5) - w / 2
        fr = Frame((u, y_out + 0.03, z - 0.04), (1, 0, 0), (0, 0, 1))
        g.plate(fr, prof, -(y_in - y_out - 0.06), 0.0, S["mat"])
        if snow and S["merlon"] == "rect":
            g.box(u - 0.03, y_out, z + hm - 0.03, u + w + 0.03, y_in, z + hm + 0.09, "MT_Snow")


def wall_section(S):
    T, H = S["T"], S["H"]
    yo, yi = -T / 2, T / 2
    cb = S.get("corbel", 0.06)
    return yo, yi, [(yo - 0.9, -1.0), (yi, -1.0), (yi, H + 1.0), (yi - 0.42, H + 1.0), (yi - 0.42, H),
                    (yo + 0.62, H), (yo + 0.62, H + 1.1), (yo - cb, H + 1.1), (yo - cb, H - 0.32), (yo, H - 0.38),
                    (yo, 2.3)]


def build_wall(g, S, x0=-6.0, x1=6.0):
    yo, yi, sec = wall_section(S)
    zb = g.buried(-1.0)
    sec = [(y, zb if z == -1.0 else z) for (y, z) in sec]
    mat = S["mat"]
    _extrude_x(g, sec, x0, x1, [mat] * len(sec), ["box"] * len(sec), mat)
    H = S["H"]
    cb = S.get("corbel", 0.06)
    merlon_row(g, x0, x1, yo - cb, yo + 0.62, H + 1.1, S, snow=S.get("snow"))
    if S.get("machicolation"):
        n = int(round((x1 - x0) / 0.75))
        for k in range(n):
            x = x0 + (x1 - x0) * (k + 0.5) / n
            g.box(x - 0.14, yo - cb + 0.04, H - 1.1, x + 0.14, yo + 0.12, H - 0.3 + 0.02, mat)
    if S.get("buttresses"):
        from arch_parts import buttress
        fr = Frame((x0, yo, 0.0), (1, 0, 0), (0, 0, 1))
        L = x1 - x0
        for u in ((L / 4, 3 * L / 4) if L > 8.0 else (L / 2,)):
            buttress(g, fr, u, 1.2, H - 2.2, 1.05, width=0.9, mat=mat, steps=2)
    if S.get("vigas"):
        from arch_parts import viga_row
        fr = Frame((x0, yo, 0.0), (1, 0, 0), (0, 0, 1))
        viga_row(g, fr, x1 - x0, H - 1.25, n_out=0.42, spacing=1.0, margin=0.5)
        g.box(x0 + 0.01, yo - 0.05, H - 1.62, x1 - 0.01, yo + 0.1, H - 1.42, mat)
    if S.get("pilasters"):
        for x in (-4.0, 0.0, 4.0):
            if x0 + 0.5 < x < x1 - 0.5:
                g.box(x - 0.35, yo - 0.09, 2.0, x + 0.35, yo + 0.2, H - 0.5, mat)
    if S.get("snow"):
        g.box(x0 + 0.01, yo + 0.64, H - 0.03, x1 - 0.01, yi - 0.44, H + 0.12, "MT_Snow")
    if S.get("bone"):
        from arch_demon import tusk
        for x in (x0 + 3.0, x1 - 3.0):
            tusk(g, (x, yo - 0.2, H - 0.2), (x, yo - 1.1, H - 0.6), (x, yo - 1.5, H + 0.3), (x, yo - 1.3, H + 1.3), 0.16)
    return yo, yi


def tower_body(g, S, cx=0.0, cy=0.0, doors=True, height_extra=4.5, windows=True):
    """Tower centred at (cx, cy): round (20-gon) or square with chamfers; talus, walkway doors, crowning."""
    H = S["H"]
    r = S["r"]
    Ht = H + height_extra
    mat = S["mat"]
    zb = g.buried(-1.0)
    if S["tower"] == "round":
        segs = 20
        poly = circle_poly(cx, cy, r, segs, a0=math.pi / segs)
    else:
        c = 0.9 if S["mat"] == "MT_DemonRock" else 0.0
        if c:
            poly = [(cx - r + c, cy - r), (cx + r - c, cy - r), (cx + r, cy - r + c), (cx + r, cy + r - c),
                    (cx + r - c, cy + r), (cx - r + c, cy + r), (cx - r, cy + r - c), (cx - r, cy - r + c)]
        else:
            poly = [(cx - r, cy - r), (cx + r, cy - r), (cx + r, cy + r), (cx - r, cy + r)]
    # openings: walkway doors on the faces pointing +/-X, slits on the outer (-Y) faces
    ops = {}
    k = len(poly)
    for e in range(k):
        a, b = poly[e], poly[(e + 1) % k]
        mx, my = (a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy
        Lf = math.dist(a, b)
        nrm = Vector((b[1] - a[1], -(b[0] - a[0]), 0)).normalized()
        lst = []
        if doors and abs(nrm.x) > 0.95:
            dw = min(1.2, Lf - 0.5)
            lst.append(Opening(arch_poly(Lf / 2 - dw / 2, H + 0.05 - zb, dw, 2.3, 8), depth=0.4, back="MT_WoodPlanks",
                               kind="door"))
        elif windows and nrm.y < -0.9:
            lst.append(Opening(arch_poly(Lf / 2 - 0.22, H * 0.5 - zb, 0.44, 1.3, 6), depth=0.4, kind="arch"))
            lst.append(Opening(arch_poly(Lf / 2 - 0.3, H + 1.6 - zb, 0.6, 1.5, 6), depth=0.4, kind="arch"))
        if lst:
            ops[e] = lst
    frames = poly_body(g, poly, zb, Ht, mat, ops=ops)
    for e, lst in ops.items():
        fr, _ = frames[e]
        for op in lst:
            if op.kind == "door":
                door_trim(g, fr, op, dict(door_frame_mat=mat, door_frame_w=0.14, straps=True))
            else:
                window_trim(g, fr, op, dict(trim=mat, frame_w=0.1, sill=False, mullions=False, shutters=False))
    # talus
    if S["tower"] == "round":
        g.lathe([(r + 0.95, g.buried(-1.05)), (r + 0.02, 2.4)], 20, mat, cx, cy, a0=math.pi / 20, smooth=False)
    else:
        rings = []
        for (d, z) in ((0.95, g.buried(-1.05)), (0.02, 2.4)):
            rings.append([(p[0] + (d if p[0] > cx else -d), p[1] + (d if p[1] > cy else -d), z) for p in poly])
        g.loft(rings, mat, cap0=True, cap1=True)
    return poly, Ht


def tower_crown(g, S, cx, cy, Ht, poly):
    """Corbelled parapet with merlons and the style's roof on a drum."""
    mat = S["mat"]
    r = S["r"]
    snow = S.get("snow")
    if S["tower"] == "round":
        segs = 20
        g.lathe([(r - 0.6, Ht - 1.0), (r + 0.02, Ht - 1.0), (r + 0.45, Ht - 0.2), (r + 0.45, Ht + 0.2),
                 (r - 0.6, Ht + 0.2)], segs, mat, cx, cy, a0=math.pi / segs, smooth=False)
        ring_o = circle_poly(cx, cy, r + 0.42, segs, a0=math.pi / segs)
        ring_i = circle_poly(cx, cy, r - 0.2, segs, a0=math.pi / segs)
        g.prism_holes(ring_o, [ring_i], Ht + 0.1, Ht + 1.1, mat)
        nm = 14
        prof = merlon_profile(S["merlon"], 0.9, 0.85)
        for k in range(nm):
            a = math.tau * (k + 0.5) / nm
            t = Vector((-math.sin(a), math.cos(a), 0))
            rv = Vector((math.cos(a), math.sin(a), 0))
            o = Vector((cx, cy, Ht + 1.06)) + rv * (r + 0.39) - t * 0.45
            fr = Frame(o, t, (0, 0, 1), -rv)
            # frame normal points inward; extrude from the outer face inward by the parapet thickness
            g.plate(fr, prof, 0.0, 0.56, mat)
            if snow and S["merlon"] == "rect":
                c = Vector((cx, cy, 0)) + rv * (r + 0.11)
                g.obox(c + Vector((0, 0, Ht + 1.06 + 0.85 + 0.03)), t, rv, Vector((0, 0, 1)), 0.48, 0.32, 0.06,
                       "MT_Snow")
        drum_r = r - 0.9
    else:
        # square: corbel band + parapet ring + merlons (drum on the roof terrace)
        xs = [p[0] for p in poly]
        ys = [p[1] for p in poly]
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        cornice(g, x0, y0, x1, y1, Ht - 0.6, 0.62, 0.3, mat)
        outer = [(x0 - 0.28, y0 - 0.28), (x1 + 0.28, y0 - 0.28), (x1 + 0.28, y1 + 0.28), (x0 - 0.28, y1 + 0.28)]
        inner = [(x0 + 0.3, y0 + 0.3), (x1 - 0.3, y0 + 0.3), (x1 - 0.3, y1 - 0.3), (x0 + 0.3, y1 - 0.3)]
        g.prism_holes(outer, [inner], Ht - 0.05, Ht + 1.0, mat)
        prof = merlon_profile(S["merlon"], 0.85, 0.8)
        for (a, b, fixed, axis, inward) in ((x0, x1, y0 - 0.28, "x", 1), (x0, x1, y1 + 0.28, "x", -1),
                                           (y0, y1, x0 - 0.28, "y", 1), (y0, y1, x1 + 0.28, "y", -1)):
            L = b - a - 1.2
            n = max(1, int(L / 1.35))
            for k in range(n):
                c = a + 0.6 + L * (k + 0.5) / n
                if axis == "x":
                    fr = Frame((c - 0.425, fixed + inward * 0.03, Ht + 0.96), (1, 0, 0), (0, 0, 1))
                    g.plate(fr, prof, -0.52 if inward > 0 else 0.0, 0.0 if inward > 0 else 0.52, mat)
                else:
                    fr = Frame((fixed + inward * 0.03, c - 0.425, Ht + 0.96), (0, 1, 0), (0, 0, 1))
                    g.plate(fr, prof, 0.0 if inward > 0 else -0.52, 0.52 if inward > 0 else 0.0, mat)
        drum_r = min(x1 - x0, y1 - y0) / 2 - 1.0
    kind, rmat = S["roof"]
    if kind in ("cone", "spire", "dome"):
        dz = 2.2 if kind != "dome" else 1.6
        g.cylinder(cx, cy, drum_r, Ht - 0.2, Ht + dz, 16, mat, smooth=False)
        if kind == "cone":
            roof_cone(g, cx, cy, drum_r + 0.05, Ht + dz, drum_r * 1.9, rmat, ov=0.45, th=0.2, course=0.45, step=0.045,
                      segs=16, under="MT_WoodPlanks", finial=dict(mat="MT_Iron", h=1.2), snow=("MT_Snow" if snow else None))
        elif kind == "spire":
            roof_cone(g, cx, cy, drum_r + 0.05, Ht + dz, drum_r * 3.4, rmat, ov=0.35, th=0.2, course=0.7, step=0.05,
                      segs=16, under="MT_WoodPlanks", finial=dict(mat="MT_Iron", h=1.6))
        else:
            top = dome(g, cx, cy, drum_r + 0.08, Ht + dz, rmat, segs=16, rings=7,
                       shape="pointed" if S["mat"] == "MT_Sandstone" else "hemi")
            finial_spike(g, cx, cy, top - 0.1, dict(mat="MT_Gold", h=1.1, r=0.07))
    elif kind == "crystal":
        from arch_demon import crystal
        crystal(g, cx, cy, Ht - 0.1, 3.4, 0.55)
        for a in (0, 90, 180, 270):
            ax, ay = math.cos(math.radians(a + 45)), math.sin(math.radians(a + 45))
            crystal(g, cx + ax * 0.8, cy + ay * 0.8, Ht - 0.1, 1.5, 0.26, tilt=(-20 * ay, 20 * ax))


def banner(g, x, y, z_top, w, h, mat, rod="MT_Iron"):
    """Hanging cloth banner (thin plate with a swallow-tail bottom) on a rod, against a wall facing -Y at y."""
    fr = Frame((x - w / 2, y, 0.0), (1, 0, 0), (0, 0, 1))
    prof = [(0.0, z_top - h), (w / 2, z_top - h + 0.35), (w, z_top - h), (w, z_top), (0.0, z_top)]
    g.plate(fr, prof, 0.08, 0.13, mat)
    g.plate(fr, rect(-0.12, z_top - 0.05, w + 0.24, 0.07), -0.2, 0.18, rod)


# ---------------------------------------------------------------------------------------------- assets

def build_gate(g, S):
    H = S["H"]
    T = S["T"]
    mat = S["mat"]
    gw, gh = 5.0, 6.4
    hx = 4.8
    y0, y1 = -T / 2 - 1.1, T / 2 + 1.1
    zb = g.buried(-1.0)
    zt = H + 3.4
    outline = arcade_outline(2 * hx, zt - zb, [(hx - gw / 2, gw, gh - zb)], segs=12)
    g.plate(Frame((-hx, y0, zb), (1, 0, 0), (0, 0, 1)), outline, -(y1 - y0), 0.0, mat)
    cornice(g, -hx, y0, hx, y1, zt - 0.55, 0.5, 0.25, mat)
    # parapet + merlons on the gatehouse roof
    outer = [(-hx - 0.2, y0 - 0.2), (hx + 0.2, y0 - 0.2), (hx + 0.2, y1 + 0.2), (-hx - 0.2, y1 + 0.2)]
    inner = [(-hx + 0.35, y0 + 0.35), (hx - 0.35, y0 + 0.35), (hx - 0.35, y1 - 0.35), (-hx + 0.35, y1 - 0.35)]
    g.prism_holes(outer, [inner], zt - 0.1, zt + 1.0, mat)
    merlon_row(g, -hx + 0.3, hx - 0.3, y0 - 0.2, y0 + 0.35, zt + 1.0, S, cell=1.4, snow=S.get("snow"))
    # passage vault ceiling line: portcullis bars (raised) near the outer face
    for k in range(9):
        x = -gw / 2 + 0.35 + (gw - 0.7) * k / 8
        g.box(x - 0.045, y0 + 0.55, 3.8, x + 0.045, y0 + 0.64, gh + 0.6, "MT_Iron")
        g.lathe([(0.0, 3.4), (0.06, 3.82)], 4, "MT_Iron", x, y0 + 0.595, a0=math.pi / 4, smooth=False)
    for z in (4.3, 5.1):
        g.box(-gw / 2 + 0.05, y0 + 0.52, z - 0.05, gw / 2 - 0.05, y0 + 0.67, z + 0.05, "MT_Iron")
    # flanking towers on the joints
    for sx in (-1, 1):
        poly, Ht = tower_body(g, S, cx=sx * 6.0, cy=0.0, doors=False, height_extra=5.5)
        tower_crown(g, S, sx * 6.0, 0.0, Ht, poly)
    for sx in (-1, 1):
        banner(g, sx * 2.0, y0, zt - 0.8, 1.3, 3.6, S["banner"])
    if S.get("gold"):
        finial_spike(g, 0.0, (y0 + y1) / 2, zt + 0.95, dict(mat="MT_Gold", h=1.6, r=0.1))
    return {"attach": {"opening_width": gw, "opening_height": gh, "portcullis_clearance": 3.8,
                       "tower_centres_x": [-6.0, 6.0]}}


def _register(style):
    S = STYLES[style]
    pre = style

    @asset(f"SM_{pre}_Wall_12m", "Walls", style, kind="wall", pivot="base_point", recentre=False, foundation=1.1,
           notes=f"{style} city wall segment: 12.0 m along X (x = -6..+6), walkway at {S['H']:.1f} m, outer face -Y, "
                 f"{S['merlon']} merlons in 12/n m cells. Chain end to end every 12 m; cover joints with "
                 f"SM_{pre}_WallTower.")
    def _wall(g, rng):
        build_wall(g, S)
        return {"attach": {"segment_length": 12.0, "walkway_height": S["H"], "wall_thickness": S["T"],
                           "outer_face": "-Y"}}

    @asset(f"SM_{pre}_WallTower", "Walls", style, kind="wall", pivot="base_point", recentre=False, foundation=1.1,
           notes=f"{style} wall tower centred on a wall joint (origin = tower axis); covers wall ends meeting at any "
                 f"angle; walkway doors on the +/-X faces at {S['H']:.1f} m.")
    def _tower(g, rng):
        poly, Ht = tower_body(g, S)
        tower_crown(g, S, 0.0, 0.0, Ht, poly)
        return {"attach": {"covers_joint": True, "tower_radius": S["r"], "walkway_height": S["H"]}}

    @asset(f"SM_{pre}_Gate", "Walls", style, kind="wall", pivot="base_point", recentre=False, foundation=1.1,
           budget=(1000, 12000),
           notes=f"{style} gatehouse replacing one 12 m wall segment: flanking towers centred at x = +/-6 (on the "
                 f"joints), open arched passage 5.0 m wide on the origin, raised portcullis.")
    def _gate(g, rng):
        return build_gate(g, S)


for _s in ("Asura", "Noble", "North", "Millis", "Demon", "Desert"):
    _register(_s)
