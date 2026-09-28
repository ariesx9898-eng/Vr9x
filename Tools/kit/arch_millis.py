"""Millis set (Millishion, Zant Port): white stone, steep blue slate gables, round arches everywhere (arcaded
loggias, arched and paired windows, oculi), round towers with tall slender spires and iron finials, a chapel with
a rose window, buttresses and an apse. Distinct from the AsuraNoble set (classical mansards, gold crests)."""
import math

from arch_geo import Frame
from arch_house import House, gable_heights
from arch_parts import (Opening, arcade_outline, arch_poly, block, buttress, circle_poly, cornice, corner_posts,
                        door_trim, finial_spike, poly_body, rect, roof_cone, roof_gable, rose_tracery, side_frame,
                        stair, window_trim)
from arch_registry import asset

M_WIN = dict(trim="MT_StoneWhite", frame_w=0.1, kind="arch", shutters=False, sill_mat="MT_StoneWhite",
             mullion_mat="MT_StoneWhite", win_w=1.0, win_h=1.9, win_sill=0.8)
M_WIN_UP = dict(M_WIN, win_h=1.7, win_sill=0.7)
M_DOOR = dict(door_frame_mat="MT_StoneWhite", door_frame_w=0.16, straps=True, battens=True)
M_ROOF = dict(roof_mat="MT_RoofBlue", roof_under="MT_WoodPlanks", fascia="MT_StoneWhite", verge="MT_StoneWhite",
              course=0.36, step=0.035, ridge_mat="MT_RoofBlue")
IRON_FINIAL = dict(mat="MT_Iron", h=1.6, r=0.07)


def millis_house(W, D, heights, **kw):
    base = dict(walls=["MT_StoneWhite"], timber=[False], jetty=[(-0.04, -0.04, -0.04)] * 3, cornices="MT_StoneWhite",
                plinth="MT_Stone", floor0=0.4, win_styles=[M_WIN, M_WIN_UP], door_style=M_DOOR, pitch=56,
                win_spacing=2.4, dormer_wall="MT_StoneWhite", step_mat="MT_Stone", ov=0.4, ovv=0.25)
    base.update(M_ROOF)
    base.update(kw)
    return House(W, D, heights, **base)


def _oculus(h, g, side="front", r=0.55):
    """Round window high in a gable face."""
    top = h.levels[-1]
    fr, Lf = top["frames"][side]
    H = top["H"]
    vc = H + (Lf / 2) * h.ta * 0.45
    op = Opening(circle_poly(Lf / 2, vc, r, 16), depth=0.2, kind="window")
    return op


def _loggia(h, g, arches, depth):
    """Arcade slab on the upper storey's front plane over a recessed ground floor (loggia)."""
    L0, L1 = h.levels[0], h.levels[1]
    x0, y0, x1, y1 = L1["fp"]
    fr, Lf = side_frame(x0, y0, x1, y1, 0.0, "front")
    zb = g.buried(-0.35)
    Hs = L1["z0"] + 0.1
    # 4 cm wider than the storey on both ends so its end faces never share the storey's side planes
    outline = arcade_outline(Lf + 0.08, Hs - zb, [(u0 + 0.04, w, hc - zb) for (u0, w, hc) in arches])
    g.plate(Frame(fr.p(-0.04, zb, 0), fr.u, fr.v), outline, -0.45, 0.04, "MT_StoneWhite")
    cornice(g, x0 - 0.01, y0 - 0.01, x1 + 0.01, y1 + 0.01, L1["z0"] - 0.09, 0.2, 0.12, "MT_StoneWhite")


# ---------------------------------------------------------------------------------------------- houses

@asset("SM_Millis_House_A", "Millis", "Millis",
       notes="Gable-fronted Millis town house with an arcaded ground-floor loggia, arched windows and a round oculus "
             "in the steep blue gable.")
def millis_house_a(g, rng):
    W = 8.4
    Lup = W - 0.08

    def extras(h, g):
        _loggia(h, g, [(0.55, 2.2, 3.05), (Lup / 2 - 1.1, 2.2, 3.05), (Lup - 0.55 - 2.2, 2.2, 3.05)], 1.9)

    h = millis_house(W, 9.0, [3.6, 3.2, 3.0], ridge="y", pitch=58, jetty=[(1.9, -0.04, -0.04), (-0.04, -0.04, -0.04)],
                     door=dict(side="front", at=0.5, w=1.2, h=2.4, kind="arch"),
                     window_counts={(0, "front"): 2, (1, "front"): 3, (2, "front"): 3},
                     window_positions={(0, "front"): [1.6, 6.8]}, gable_windows=False,
                     chimneys=[dict(x=0.3, y=0.25, pots=1, mat="MT_StoneWhite", cap="MT_StoneWhite",
                                    pot_mat="MT_Stone")],
                     hooks=[extras])
    # oculus in the front gable
    h.extra_openings[(2, "front")] = [Opening(circle_poly(Lup / 2 - 0.04, 3.0 + 1.35, 0.55, 16), depth=0.2,
                                              kind="window")]
    h.build(g)


@asset("SM_Millis_House_B", "Millis", "Millis",
       notes="Millis house with a full-height round corner turret and a tall slender spire, arched windows and "
             "blue gable roof with dormers.")
def millis_house_b(g, rng):
    def turret(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        cx, cy = x0 - 0.35, y0 - 0.35
        R = 1.65
        zb = g.buried(-0.4)
        z1 = h.zw + 1.8
        ops = {}
        for e in (6, 8):
            ops[e] = [Opening(arch_poly(0.14, L["z0"] + 0.9 - zb, 0.5, 1.4, 6), depth=0.2, kind="arch")
                      for L in h.levels]
        frames = poly_body(g, circle_poly(cx, cy, R, 12, a0=math.pi / 12), zb, z1, "MT_StoneWhite", ops=ops)
        for e, lst in ops.items():
            fr, _ = frames[e]
            for op in lst:
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.07, sill=False, mullions=False))
        g.lathe([(R - 0.3, z1 - 0.3), (R + 0.03, z1 - 0.3), (R + 0.22, z1 - 0.06), (R + 0.22, z1 + 0.1),
                 (R - 0.3, z1 + 0.1)], 12, "MT_StoneWhite", cx, cy, a0=math.pi / 12, smooth=False)
        g.lathe([(R + 0.1, g.buried(-0.45)), (R + 0.06, 0.3), (R + 0.02, 0.42)], 12, "MT_Stone", cx, cy,
                a0=math.pi / 12, smooth=False)
        roof_cone(g, cx, cy, R + 0.22, z1 + 0.1, 7.5, "MT_RoofBlue", ov=0.2, th=0.14, course=0.55, step=0.04,
                  segs=12, under="MT_WoodPlanks", finial=IRON_FINIAL)

    millis_house(10.6, 8.2, [3.5, 3.2], ridge="x", pitch=55, door=dict(side="front", at=0.6, w=1.2, h=2.4, kind="arch"),
                 window_positions={(0, "front"): [3.2, 8.8], (1, "front"): [3.2, 5.8, 8.6]},
                 window_counts={(0, "left"): 0, (1, "left"): 1},
                 dormers=[dict(u=-0.05, w=1.4), dict(u=0.28, w=1.4)],
                 chimneys=[dict(x=0.3, y=0.2, pots=1, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_Stone")],
                 hooks=[turret]).build(g)


@asset("SM_Millis_House_C", "Millis", "Millis",
       notes="Tall narrow gable-fronted Millis house, four storeys, with a two-storey oriel bay on the front and a "
             "round oculus in the gable.")
def millis_house_c(g, rng):
    W = 6.6

    def oriel(h, g):
        L1, L2 = h.levels[1], h.levels[2]
        x0, y0, x1, y1 = L1["fp"]
        ow, od = 3.0, 0.75
        ox0, ox1 = -ow / 2, ow / 2
        oy0, oy1 = y0 - od, y0 + 0.4
        zb = L1["z0"] - 0.35
        zt = L2["z1"] - 0.2
        ops = []
        for L in (L1, L2):
            ops.append(Opening(arch_poly(ow / 2 - 0.6, L["z0"] + 0.5 - zb, 1.2, 1.9, 8), depth=0.16, kind="arch"))
        frames = block(g, ox0, oy0, ox1, oy1, zb, zt, "MT_StoneWhite", ops={"front": ops})
        for op in ops:
            window_trim(g, frames["front"], op, dict(M_WIN, sill=False))
        rings = []
        for (f, z) in ((0.25, zb - 1.3), (0.6, zb - 0.7), (0.98, zb + 0.08)):
            hx, hy = ow / 2 * f, (oy1 - oy0) / 2 * f
            cy = (oy0 + oy1) / 2
            rings.append([(-hx, cy - hy, z), (hx, cy - hy, z), (hx, cy + hy, z), (-hx, cy + hy, z)])
        g.loft(rings, "MT_StoneWhite", cap0=True, cap1=True)
        # little hipped roof on the oriel
        from arch_parts import roof_hip
        roof_hip(g, ox0, ox1, oy0, oy1, zt, 50, "MT_RoofBlue", ov=0.12, th=0.12, course=0.3, step=0.03,
                 under="MT_WoodPlanks", fascia="MT_StoneWhite", hips=True)

    h = millis_house(W, 10.0, [3.4, 3.0, 3.0, 2.8], ridge="y", pitch=62,
                     door=dict(side="front", at=0.3, w=1.1, h=2.3, kind="arch"),
                     window_counts={(0, "front"): 1, (1, "front"): 0, (2, "front"): 0, (3, "front"): 2},
                     window_positions={(0, "front"): [4.6]}, gable_windows=False,
                     chimneys=[dict(x=-0.25, y=0.3, pots=1, mat="MT_StoneWhite", cap="MT_StoneWhite",
                                    pot_mat="MT_Stone")], hooks=[oriel])
    h.extra_openings[(3, "front")] = [Opening(circle_poly((W - 0.12) / 2, 2.8 + 1.5, 0.5, 16), depth=0.2,
                                              kind="window")]
    h.build(g)


# ---------------------------------------------------------------------------------------------- shop

@asset("SM_Millis_Shop_A", "Millis", "Millis",
       notes="Arcaded Millis shop: three open arches over a loggia with two shop counters under blue awnings, "
             "arched windows above, blue roof.")
def millis_shop_a(g, rng):
    W = 10.4
    Lup = W - 0.08

    def extras(h, g):
        _loggia(h, g, [(0.6, 2.7, 3.35), (Lup / 2 - 1.35, 2.7, 3.35), (Lup - 0.6 - 2.7, 2.7, 3.35)], 2.0)

    shops = [Opening(rect(0.6, 0.85, 2.4, 1.45), depth=0.4, back="MT_WoodPlanks", kind="shop", awning="MT_ClothBlue"),
             Opening(rect(W - 3.0, 0.85, 2.4, 1.45), depth=0.4, back="MT_WoodPlanks", kind="shop",
                     awning="MT_ClothBlue")]
    millis_house(W, 9.0, [3.9, 3.3], ridge="x", pitch=54, jetty=[(2.0, -0.04, -0.04)],
                 door=dict(side="front", at=0.5, w=1.2, h=2.4, kind="arch"), skip_windows={(0, "front")},
                 extra_openings={(0, "front"): shops}, window_counts={(1, "front"): 4},
                 dormers=[dict(u=0.0, w=1.5)],
                 chimneys=[dict(x=-0.35, y=0.2, pots=1, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_Stone")],
                 hooks=[extras]).build(g)


# ---------------------------------------------------------------------------------------------- tower

@asset("SM_Millis_Tower_A", "Millis", "Millis", budget=(1500, 8000),
       notes="Round white Millis tower (radius 3.2 m) with arched windows, a corbelled gallery and a tall slender "
             "blue spire with an iron finial.")
def millis_tower_a(g, rng):
    r = 3.2
    segs = 16
    zt = 21.0
    zb = g.buried(-0.5)
    Lf = 2 * r * math.sin(math.pi / segs)
    front = None
    for e in range(segs):
        a = math.pi / segs + (e + 0.5) * math.tau / segs
        if abs(math.cos(a)) < 0.25 and math.sin(a) < 0:
            front = e
    ops = {}
    for e in range(segs):
        lst = []
        if e == front:
            lst.append(Opening(arch_poly(Lf / 2 - 0.5, 0.45 - zb, 1.0, 2.3, 8), depth=0.4, back="MT_WoodPlanks",
                               kind="door"))
        for k, zw in enumerate((5.0, 9.5, 14.0, 18.2)):
            if (e + 2 * k) % 4 == 0 and not (e == front and k == 0):
                lst.append(Opening(arch_poly(Lf / 2 - 0.3, zw - zb, 0.6, 1.4, 6), depth=0.3, kind="arch"))
        ops[e] = lst
    frames = poly_body(g, circle_poly(0, 0, r, segs, a0=math.pi / segs), zb, zt, "MT_StoneWhite", ops=ops)
    for e, lst in ops.items():
        fr, _ = frames[e]
        for op in lst:
            if op.back == "MT_WoodPlanks":
                door_trim(g, fr, op, M_DOOR)
                stair(g, Frame((fr.o.x, fr.o.y, 0.0), fr.u, (0, 0, 1)), Lf / 2 - 0.8, Lf / 2 + 0.8, 0.435, "MT_Stone",
                      embed=0.35)
            else:
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.08, sill_mat="MT_StoneWhite",
                                            mullions=False))
    g.lathe([(r + 0.3, g.buried(-0.55)), (r + 0.18, 0.25), (r + 0.05, 0.4)], segs, "MT_Stone", a0=math.pi / segs,
            smooth=False)
    for z in (7.6, 12.1, 16.6):
        g.lathe([(r - 0.3, z), (r + 0.1, z), (r + 0.1, z + 0.2), (r - 0.3, z + 0.2)], segs, "MT_StoneWhite",
                a0=math.pi / segs, smooth=False)
    # corbelled gallery with a parapet ring
    g.lathe([(r - 0.3, zt - 1.2), (r + 0.02, zt - 1.2), (r + 0.75, zt - 0.2), (r + 0.75, zt + 0.1),
             (r - 0.3, zt + 0.1)], segs, "MT_StoneWhite", a0=math.pi / segs, smooth=False)
    ring_o = circle_poly(0, 0, r + 0.72, segs, a0=math.pi / segs)
    ring_i = circle_poly(0, 0, r + 0.5, segs, a0=math.pi / segs)
    g.prism_holes(ring_o, [ring_i], zt + 0.05, zt + 1.1, "MT_StoneWhite")
    # upper drum and spire
    r2 = r - 0.5
    g.cylinder(0, 0, r2, zt - 0.1, zt + 3.2, segs, "MT_StoneWhite", smooth=False, a0=math.pi / segs)
    roof_cone(g, 0, 0, r2 + 0.1, zt + 3.2, 13.0, "MT_RoofBlue", ov=0.3, th=0.18, course=0.7, step=0.05, segs=segs,
              under="MT_WoodPlanks", finial=dict(mat="MT_Iron", h=2.2, r=0.09), a0=math.pi / segs)


# ---------------------------------------------------------------------------------------------- chapel

@asset("SM_Millis_Chapel_A", "Millis", "Millis", budget=(3000, 15000),
       notes="Millis chapel: buttressed nave with pointed windows, a rose window over the pointed-arch door on the "
             "west (-Y) front, a square bell tower with a spire on the front-left corner and an apse at the back.")
def millis_chapel_a(g, rng):
    W, D = 9.4, 18.0
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    H = 7.6
    pitch = 56
    th = 0.24
    ze, zr, ta, ca = gable_heights(H, W, pitch, th)
    zb = g.buried(-0.5)
    ops = {"front": [], "left": [], "right": []}
    ops["front"].append(Opening(arch_poly(W / 2 - 1.1, 0.35 - zb, 2.2, 3.8, 10, pointed=True), depth=0.45,
                                back="MT_WoodPlanks", kind="door"))
    rose_c = (W / 2, H + 1.0 - zb)
    ops["front"].append(Opening(circle_poly(rose_c[0], rose_c[1], 1.35, 24), depth=0.3, kind="rose"))
    bays = [2.2 + (D - 4.4) * k / 4 for k in range(5)]
    for side in ("left", "right"):
        for k in range(4):
            uc = (bays[k] + bays[k + 1]) / 2
            ops[side].append(Opening(arch_poly(uc - 0.55, 2.3 - zb, 1.1, 4.0, 8, pointed=True), depth=0.35,
                                     kind="arch"))
    # nave (gable ends front/back)
    frames = block(g, x0, y0, x1, y1, zb, H, "MT_StoneWhite", ops=ops, gable=("y", ze, zr))
    for side, lst in ops.items():
        fr = frames[side]
        for op in lst:
            if op.kind == "door":
                door_trim(g, fr, op, dict(M_DOOR, door_frame_w=0.25))
            elif op.kind == "rose":
                from arch_parts import frame_ring
                frame_ring(g, fr, op, width=0.22, mat="MT_StoneWhite")
                rose_tracery(g, fr, rose_c[0], rose_c[1], 1.35, 0.3)
            else:
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.12, sill_mat="MT_StoneWhite",
                                            mullion_mat="MT_StoneWhite"))
    frf, _ = side_frame(x0, y0, x1, y1, 0.0, "front")
    stair(g, frf, W / 2 - 1.8, W / 2 + 1.8, 0.335, "MT_Stone")
    g.box(x0 - 0.12, y0 - 0.12, g.buried(-0.55), x1 + 0.12, y1 + 0.12, 0.3, "MT_Stone")
    # buttresses between the side windows
    for side in ("left", "right"):
        fr, L = side_frame(x0, y0, x1, y1, 0.0, side)
        for u in bays:
            uu = u if side == "right" else L - u
            buttress(g, fr, uu, g.buried(-0.3), H - 0.1, 1.1, width=0.65, mat="MT_StoneWhite")
    roof_gable(g, x0, x1, y0, y1, H, pitch, "MT_RoofBlue", ov=0.4, ovv=0.3, th=th, course=0.38, step=0.04,
               under="MT_WoodPlanks", fascia="MT_StoneWhite", verge="MT_StoneWhite", axis="y", ridge_mat="MT_RoofBlue")
    # apse: half octagon against the back wall with a conical roof
    ar = 3.6
    apoly = [(ar * math.cos(math.radians(a)), y1 - 0.6 + ar * math.sin(math.radians(a))) for a in
             (-22.5, 22.5, 67.5, 112.5, 157.5, 202.5)]
    azb = g.buried(-0.45)
    ael = 2 * ar * math.sin(math.radians(22.5))
    aops = {e: [Opening(arch_poly(ael / 2 - 0.4, 2.4 - azb, 0.8, 2.8, 6, pointed=True), depth=0.3, kind="arch")]
            for e in (1, 2, 3)}
    afr = poly_body(g, apoly, azb, 6.2, "MT_StoneWhite", ops=aops)
    for e, lst in aops.items():
        fr, _ = afr[e]
        for op in lst:
            window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.1, sill_mat="MT_StoneWhite", mullions=False))
    roof_cone(g, 0.0, y1 - 0.6, ar + 0.05, 6.2, 3.6, "MT_RoofBlue", ov=0.35, th=0.18, course=0.4, step=0.04, segs=8,
              under="MT_WoodPlanks", a0=math.radians(-22.5))
    # bell tower on the front-left corner
    S = 2.2
    tx, ty = x0 - 0.6, y0 + 0.2
    tzb = g.buried(-0.5)
    tz = 17.0
    tops = {}
    for side in ("front", "left"):
        tops[side] = [Opening(arch_poly(S - 0.35, z - tzb, 0.7, 1.6, 6, pointed=True), depth=0.3, kind="arch")
                      for z in (4.0, 8.5)]
    for side in ("front", "right", "back", "left"):
        tops.setdefault(side, [])
        tops[side].append(Opening(arch_poly(S - 0.6, 12.8 - tzb, 1.2, 2.8, 8, pointed=True), depth=0.35,
                                  kind="arch"))
    tfr = block(g, tx - S, ty - S, tx + S, ty + S, tzb, tz, "MT_StoneWhite", ops=tops)
    for side, lst in tops.items():
        for op in lst:
            window_trim(g, tfr[side], op, dict(trim="MT_StoneWhite", frame_w=0.1, sill_mat="MT_StoneWhite",
                                                 mullion_mat="MT_StoneWhite"))
    corner_posts(g, tx - S, ty - S, tx + S, ty + S, 0.2, tz - 0.3, "MT_StoneWhite", s=0.55, p=0.08)
    cornice(g, tx - S, ty - S, tx + S, ty + S, tz - 0.35, 0.3, 0.16, "MT_StoneWhite")
    g.lathe([(S + 0.3, tz - 0.1), (S + 0.3, tz + 0.25), (0.0, tz + 10.5)], 8, "MT_RoofBlue", tx, ty, a0=math.pi / 8,
            smooth=False)
    finial_spike(g, tx, ty, tz + 10.3, dict(mat="MT_Iron", h=1.8, r=0.08))
    return {"attach": {"door_width": 2.2}}
