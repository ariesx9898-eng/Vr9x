"""Desert set (Rapan, the labyrinth city): tan plaster and sandstone cubes with flat roofs behind parapets
(rounded or stepped merlons), projecting roof-beam ends, deep small windows with wooden lattices, arched doors,
small domes, cloth awnings on poles, an inn built around a courtyard, a tapering tower and a walled caravan yard."""
import math

from arch_geo import Frame
from arch_house import House
from arch_parts import (Opening, arcade_outline, arch_poly, awning, block, circle_poly, cornice, dome, door_trim,
                        finial_spike, parapet_ring, rect, roof_shed, side_frame, solid, stair, viga_row, window_trim,
                        wplate)
from arch_registry import asset

D_WIN = dict(trim="MT_WoodPlanks", frame_w=0.09, shutters=False, sill_mat="MT_Sandstone", mullion_mat="MT_WoodPlanks",
             win_w=0.8, win_h=1.05, win_sill=1.0, depth=0.3)
D_WIN_A = dict(D_WIN, kind="arch", win_h=1.3)
D_DOOR = dict(door_frame_mat="MT_Sandstone", door_frame_w=0.16, straps=True, battens=True)
PARAPET = dict(h=0.85, mat="MT_PlasterTan", coping="MT_Sandstone", merlons="round")


def desert_house(W, D, heights, **kw):
    base = dict(walls=["MT_PlasterTan"], timber=[False], jetty=[(-0.04, -0.04, -0.04)] * 3, cornices=None,
                plinth="MT_Sandstone", plinth_proud=0.06, floor0=0.25, roof="flat", parapet=PARAPET,
                win_styles=[D_WIN_A, D_WIN], door_style=D_DOOR, win_spacing=2.4, step_mat="MT_Sandstone")
    base.update(kw)
    return House(W, D, heights, **base)


def _vigas(h, g, sides=("front", "back", "left", "right"), mat="MT_Timber"):
    top = h.levels[-1]
    for s in sides:
        fr, Lf = top["frames"][s]
        viga_row(g, fr, Lf, top["H"] - 0.45, n_out=0.38, spacing=1.0)


def _kiosk(g, cx, cy, z, s=2.2, h=2.3, dome_mat="MT_PlasterTan", door_side="front"):
    """Roof kiosk (stair exit) with a small dome."""
    zb = z - 0.2
    op = Opening(arch_poly(s / 2 - 0.45, 0.2 + 0.05, 0.9, 1.9, 8), depth=0.25, back="MT_WoodPlanks", kind="door")
    frs = block(g, cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2, zb, z + h, "MT_PlasterTan", ops={door_side: [op]})
    door_trim(g, frs[door_side], op, dict(D_DOOR, door_frame_w=0.1))
    cornice(g, cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2, z + h - 0.25, 0.3, 0.1, "MT_Sandstone")
    top = dome(g, cx, cy, s / 2 - 0.05, z + h + 0.02, dome_mat, segs=16, rings=6)
    finial_spike(g, cx, cy, top - 0.08, dict(mat="MT_Gold", h=0.7, r=0.05))


@asset("SM_Desert_House_A", "Desert", "Desert",
       notes="Two-storey Rapan house: tan plaster cube, rounded parapet merlons, roof-beam ends, deep windows with "
             "wooden lattices, domed roof kiosk.")
def desert_house_a(g, rng):
    def extras(h, g):
        _vigas(h, g, ("front", "left", "right"))
        x0, y0, x1, y1 = h.levels[-1]["fp"]
        _kiosk(g, x1 - 1.6, y1 - 1.6, h.zw, door_side="front")

    desert_house(8.2, 7.2, [3.3, 3.0], door=dict(side="front", at=0.35, w=1.1, h=2.3, kind="arch"),
                 window_counts={(0, "front"): 2, (1, "front"): 3}, window_positions={(0, "front"): [5.2, 6.9]},
                 hooks=[extras]).build(g)


@asset("SM_Desert_House_B", "Desert", "Desert",
       notes="Stepped Rapan house: single-storey block with a roof terrace behind a stepped parapet, a setback upper "
             "room, an outside stair and a cloth awning on poles.")
def desert_house_b(g, rng):
    W, D = 10.4, 8.4
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    H0 = 3.4
    z0 = 0.2
    zb = g.buried(-0.4)
    ops = {"front": [Opening(arch_poly(2.2, z0 - zb, 1.1, 2.3, 8), depth=0.3, back="MT_WoodPlanks", kind="door"),
                     Opening(rect(5.0, z0 + 1.0 - zb, 0.8, 1.05), depth=0.3, kind="window"),
                     Opening(rect(7.6, z0 + 1.0 - zb, 0.8, 1.05), depth=0.3, kind="window")],
           "left": [Opening(rect(D / 2 - 0.4, z0 + 1.0 - zb, 0.8, 1.05), depth=0.3, kind="window")],
           "back": [Opening(rect(W / 2 - 0.4, z0 + 1.0 - zb, 0.8, 1.05), depth=0.3, kind="window")]}
    frs = block(g, x0, y0, x1, y1, zb, z0 + H0, "MT_PlasterTan", ops=ops)
    for s, lst in ops.items():
        for op in lst:
            if op.kind == "door":
                door_trim(g, frs[s], op, D_DOOR)
            else:
                window_trim(g, frs[s], op, D_WIN)
    g.box(x0 - 0.06, y0 - 0.06, g.buried(-0.45), x1 + 0.06, y1 + 0.06, z0 - 0.045, "MT_Sandstone")
    parapet_ring(g, x0, y0, x1, y1, z0 + H0, 0.8, "MT_PlasterTan", coping="MT_Sandstone", merlons="step")
    # upper room at the back-left
    ux0, uy0, ux1, uy1 = x0 + 0.5, y1 - 5.0, x0 + 5.5, y1 - 0.5
    uz = z0 + H0
    uops = {"front": [Opening(arch_poly(1.4, 0.15 + 0.2, 1.0, 2.2, 8), depth=0.25, back="MT_WoodPlanks", kind="door"),
                      Opening(rect(3.2, 1.0 + 0.2, 0.8, 1.0), depth=0.28, kind="window")],
            "left": [Opening(rect(2.1, 1.0 + 0.2, 0.8, 1.0), depth=0.28, kind="window")]}
    ufrs = block(g, ux0, uy0, ux1, uy1, uz - 0.2, uz + 3.0, "MT_PlasterTan", ops=uops)
    for s, lst in uops.items():
        for op in lst:
            if op.kind == "door":
                door_trim(g, ufrs[s], op, dict(D_DOOR, door_frame_w=0.12))
            else:
                window_trim(g, ufrs[s], op, D_WIN)
    parapet_ring(g, ux0, uy0, ux1, uy1, uz + 3.0, 0.7, "MT_PlasterTan", coping="MT_Sandstone", merlons="round")
    fr_u, Lu = side_frame(ux0, uy0, ux1, uy1, uz, "front")
    viga_row(g, fr_u, Lu, 2.45, n_out=0.35, spacing=1.0)
    # awning on poles over the terrace in front of the upper room
    awning(g, fr_u, 0.3, Lu - 0.3, 2.35, 2.2, 0.55, "MT_ClothTan", poles=True, pole_mat="MT_Timber")
    # outside stair along the right wall, climbing toward the back
    stair(g, Frame((x1, y1 - 0.4, 0.0), (1, 0, 0), (0, 0, 1)), -0.1, 1.0, uz - 0.015, "MT_Sandstone", run_per=0.28)


@asset("SM_Desert_House_C", "Desert", "Desert",
       notes="Domed Rapan house: sandstone cube with a large central dome on an octagonal drum and a flat-roofed "
             "side room.")
def desert_house_c(g, rng):
    def extras(h, g):
        _vigas(h, g, ("front", "back"))
        top = h.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        r = 2.4
        # octagonal drum with small windows
        from arch_parts import poly_body
        poly = circle_poly(cx, cy, r, 8, a0=math.pi / 8)
        Lf = 2 * r * math.sin(math.pi / 8)
        ops = {e: [Opening(arch_poly(Lf / 2 - 0.3, 0.45, 0.6, 0.9, 6), depth=0.2, kind="arch")] for e in range(0, 8, 2)}
        frames = poly_body(g, poly, h.zw - 0.25, h.zw + 1.6, "MT_Sandstone", ops=ops)
        for e, lst in ops.items():
            for op in lst:
                window_trim(g, frames[e][0], op, dict(D_WIN, sill=False, mullions=False))
        tz = dome(g, cx, cy, r * 0.98, h.zw + 1.6, "MT_PlasterTan", segs=16, rings=8, shape="pointed")
        finial_spike(g, cx, cy, tz - 0.1, dict(mat="MT_Gold", h=0.9, r=0.06))
        # side room on the right
        L0 = h.levels[0]
        bx0, by0, bx1, by1 = L0["fp"]
        sx0, sx1 = bx1 - 0.2, bx1 + 3.6
        sy0, sy1 = by0 + 1.0, by1 - 1.2
        sops = {"front": [Opening(rect(1.2, 1.25, 0.8, 1.0), depth=0.28, kind="window")]}
        sfrs = block(g, sx0, sy0, sx1, sy1, g.buried(-0.4), 3.1, "MT_PlasterTan", ops=sops)
        for op in sops["front"]:
            window_trim(g, sfrs["front"], op, D_WIN)
        parapet_ring(g, sx0, sy0, sx1, sy1, 3.1, 0.6, "MT_PlasterTan", coping="MT_Sandstone")

    desert_house(8.0, 8.0, [4.4], walls=["MT_Sandstone"], door=dict(side="front", at=0.5, w=1.3, h=2.6, kind="arch"),
                 win_styles=[dict(D_WIN_A, win_h=1.5, win_sill=1.1)], window_counts={(0, "front"): 2},
                 parapet=dict(PARAPET, mat="MT_Sandstone", merlons="step"), hooks=[extras]).build(g)


@asset("SM_Desert_Shop_A", "Desert", "Desert",
       notes="Rapan shop: wide arched shop front with a counter under a red cloth awning, upper floor with lattice "
             "windows and a wooden balcony screen, flat roof with merlons.")
def desert_shop_a(g, rng):
    def extras(h, g):
        _vigas(h, g, ("front",))
        L1 = h.levels[1]
        fr, Lf = L1["frames"]["front"]
        # closed wooden balcony (screen box) on corbels
        u0, u1 = Lf - 3.3, Lf - 0.9
        wplate(g, fr, rect(u0, 0.3, u1 - u0, 0.14), -0.1, 0.62, "MT_WoodPlanks")
        wplate(g, fr, rect(u0 + 0.04, 0.42, u1 - u0 - 0.08, 1.3), -0.05, 0.6, "MT_WoodPlanks")
        wplate(g, fr, rect(u0 - 0.04, 1.7, u1 - u0 + 0.08, 0.12), -0.1, 0.66, "MT_WoodPlanks")
        from arch_parts import bar3
        for k in range(6):
            u = u0 + 0.25 + (u1 - u0 - 0.5) * k / 5
            bar3(g, fr, u - 0.02, u + 0.02, 0.47, 1.68, 0.59, 0.62, "MT_Timber")
        for v in (0.8, 1.25):
            bar3(g, fr, u0 + 0.06, u1 - 0.06, v - 0.02, v + 0.02, 0.585, 0.612, "MT_Timber")
        for u in (u0 + 0.2, u1 - 0.2):
            bf = Frame(fr.p(u, 0, 0), fr.n, fr.v, None)
            g.plate(bf, [(-0.1, -0.3), (0.0, -0.3), (0.55, 0.31), (-0.1, 0.31)], -0.06, 0.06, "MT_Timber")

    W = 8.6
    shop = [Opening(arch_poly(1.0, 0.7, 3.4, 2.3, 10), depth=0.5, back="MT_WoodPlanks", kind="shop",
                    awning="MT_ClothRed")]
    desert_house(W, 7.4, [3.6, 3.1], door=dict(side="front", at=0.8, w=1.1, h=2.3, kind="arch"),
                 skip_windows={(0, "front")}, extra_openings={(0, "front"): shop},
                 window_positions={(1, "front"): [1.5, 3.6]}, hooks=[extras]).build(g)


# ---------------------------------------------------------------------------------------------- inn

@asset("SM_Desert_Inn_A", "Desert", "Desert", budget=(3000, 15000),
       notes="Rapan inn built around an open courtyard: two storeys of rooms, a deep arched gateway with double doors "
             "on the front, courtyard windows and awnings, parapets with merlons and a corner dome.")
def desert_inn_a(g, rng):
    W, D = 18.0, 15.0
    cw, cd = 9.0, 7.0
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    cx0, cy0, cx1, cy1 = -cw / 2, -cd / 2 + 0.5, cw / 2, cd / 2 + 0.5
    H = 6.6
    zb = g.buried(-0.4)
    zt = H + 0.2

    def wall(p0, p1, z0, z1):
        return [(p0[0], p0[1], z0), (p1[0], p1[1], z0), (p1[0], p1[1], z1), (p0[0], p0[1], z1)]

    outer = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    inner = [(cx0, cy0), (cx0, cy1), (cx1, cy1), (cx1, cy0)]   # clockwise from above: walls face the courtyard
    faces = []
    ops_outer = {0: [], 1: [], 2: [], 3: []}
    ops_inner = {0: [], 1: [], 2: [], 3: []}
    for i in range(4):
        a, b = outer[i], outer[(i + 1) % 4]
        L = math.dist(a, b)
        n = int((L - 2.0) / 2.6)
        for k in range(n):
            uc = 1.3 + (L - 2.6) * (k + 0.5) / n
            if i == 0 and abs(uc - L / 2) < 2.6:
                continue
            ops_outer[i].append(Opening(rect(uc - 0.4, 0.2 + 1.3 - zb, 0.8, 1.05), depth=0.3, kind="window"))
            ops_outer[i].append(Opening(rect(uc - 0.4, 0.2 + 4.2 - zb, 0.8, 1.05), depth=0.3, kind="window"))
    gate = Opening(arch_poly(W / 2 - 1.6, 0.2 - zb, 3.2, 4.2, 10), depth=1.6, back="MT_WoodPlanks", kind="door",
                   jamb_drop=0.05)
    ops_outer[0].append(gate)
    for i in range(4):
        a, b = inner[i], inner[(i + 1) % 4]
        L = math.dist(a, b)
        n = max(1, int((L - 1.5) / 2.4))
        for k in range(n):
            uc = 0.9 + (L - 1.8) * (k + 0.5) / n
            ops_inner[i].append(Opening(arch_poly(uc - 0.5, 0.22 - zb, 1.0, 2.3, 8), depth=0.3,
                                        back="MT_WoodPlanks", kind="door"))
            ops_inner[i].append(Opening(rect(uc - 0.4, 0.2 + 4.0 - zb, 0.8, 1.05), depth=0.3, kind="window"))
    for i in range(4):
        faces.append((wall(outer[i], outer[(i + 1) % 4], zb, zt), "MT_PlasterTan", ops_outer[i]))
        faces.append((wall(inner[i], inner[(i + 1) % 4], zb, zt), "MT_PlasterTan", ops_inner[i]))
    # top and bottom as four trapezoids each (ring with the courtyard hole)
    oc = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    ic = [(cx0, cy0), (cx1, cy0), (cx1, cy1), (cx0, cy1)]
    for i in range(4):
        j = (i + 1) % 4
        faces.append(([(oc[i][0], oc[i][1], zt), (oc[j][0], oc[j][1], zt), (ic[j][0], ic[j][1], zt),
                       (ic[i][0], ic[i][1], zt)], "MT_PlasterTan", None))
        faces.append(([(ic[i][0], ic[i][1], zb), (ic[j][0], ic[j][1], zb), (oc[j][0], oc[j][1], zb),
                       (oc[i][0], oc[i][1], zb)], "MT_PlasterTan", None))
    frames = solid(g, faces)
    for idx, (pts, mat, lst) in enumerate(faces):
        if not lst:
            continue
        fr = frames[idx]
        for op in lst:
            if op.kind == "door":
                door_trim(g, fr, op, D_DOOR if op is not gate else dict(D_DOOR, door_frame_w=0.3))
            else:
                window_trim(g, fr, op, D_WIN)
    g.box(x0 - 0.06, y0 - 0.06, g.buried(-0.45), x1 + 0.06, y1 + 0.06, 0.17, "MT_Sandstone")
    # parapets on the outer and courtyard edges
    parapet_ring(g, x0, y0, x1, y1, zt, 0.8, "MT_PlasterTan", thick=0.4, coping="MT_Sandstone", merlons="round")
    g.prism_holes([(cx0 - 0.35, cy0 - 0.35), (cx1 + 0.35, cy0 - 0.35), (cx1 + 0.35, cy1 + 0.35), (cx0 - 0.35, cy1 + 0.35)],
                  [[(cx0 - 0.05, cy0 - 0.05), (cx1 + 0.05, cy0 - 0.05), (cx1 + 0.05, cy1 + 0.05), (cx0 - 0.05, cy1 + 0.05)]],
                  zt - 0.1, zt + 0.65, "MT_PlasterTan")
    fro, Lo = side_frame(x0, y0, x1, y1, 0.0, "front")
    viga_row(g, fro, Lo, zt - 0.55, n_out=0.4, spacing=1.1)
    # courtyard awnings
    for i in range(4):
        a, b = inner[i], inner[(i + 1) % 4]
        fr = frames[2 * i + 1]
        L = math.dist(a, b)
        awning(g, fr, 1.4, L - 1.4, 3.4 - zb, 1.3, 0.5, "MT_ClothTan" if i % 2 == 0 else "MT_ClothRed", poles=False)
    # corner dome room on the roof
    _kiosk(g, x1 - 1.8, y1 - 1.8, zt, s=2.6, h=2.4)
    # steps up to the gate
    stair(g, fro, W / 2 - 2.0, W / 2 + 2.0, 0.185, "MT_Sandstone")
    return {"attach": {"gate_width": 3.2}}


# ---------------------------------------------------------------------------------------------- tower

@asset("SM_Desert_Tower_A", "Desert", "Desert", budget=(1500, 8000),
       notes="Tapering sandstone watch tower with slit windows, a corbelled balcony and an open domed lantern.")
def desert_tower_a(g, rng):
    b0, b1 = 2.7, 2.1
    zt = 16.0
    zb = g.buried(-0.5)

    def sq(h, z):
        return [(-h, -h, z), (h, -h, z), (h, h, z), (-h, h, z)]

    B = sq(b0 + (b0 - b1) * (-zb) / zt, zb)
    T = sq(b1, zt)
    tilt = math.atan((b0 - b1) / zt)
    ct = math.cos(tilt)
    faces = []
    ops = {}
    for i in range(4):
        pts = [B[i], B[(i + 1) % 4], T[(i + 1) % 4], T[i]]
        Lb = math.dist(B[i][:2], B[(i + 1) % 4][:2])
        lst = []
        for k, z in enumerate((3.5, 7.0, 10.5)):
            lst.append(Opening(arch_poly(Lb / 2 - 0.22, (z - zb) / ct, 0.44, 1.2, 6), depth=0.3, kind="arch"))
        if i == 0:
            lst = lst[1:] + [Opening(arch_poly(Lb / 2 - 0.6, (0.3 - zb) / ct, 1.2, 2.4, 8), depth=0.4,
                                     back="MT_WoodPlanks", kind="door")]
        ops[i] = lst
        faces.append((pts, "MT_Sandstone", lst))
    faces.append((list(T), "MT_Sandstone", None))
    faces.append((list(reversed(B)), "MT_Sandstone", None))
    frames = solid(g, faces)
    for i in range(4):
        for op in ops[i]:
            if op.kind == "door":
                door_trim(g, frames[i], op, D_DOOR)
            else:
                window_trim(g, frames[i], op, dict(D_WIN, sill=False, mullions=False))
    stair(g, Frame((-b0 - 0.04, -b0 - 0.04, 0.0), (1, 0, 0), (0, 0, 1)), b0 - 0.9, b0 + 0.9, 0.285, "MT_Sandstone",
          embed=0.2)
    # corbelled balcony slab and parapet
    zc = zt
    rings = []
    for (hh, z) in ((b1 - 0.1, zc - 1.0), (b1 + 0.55, zc - 0.1), (b1 + 0.62, zc + 0.12)):
        rings.append([(-hh, -hh, z), (hh, -hh, z), (hh, hh, z), (-hh, hh, z)])
    g.loft(rings, "MT_Sandstone", cap0=True, cap1=True)
    parapet_ring(g, -b1 - 0.55, -b1 - 0.55, b1 + 0.55, b1 + 0.55, zc + 0.12, 0.85, "MT_Sandstone", proud=0.03,
                 thick=0.3, merlons="step")
    # open lantern: four piers, arches, dome
    s = 1.35
    for (px, py) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        g.box(px * s - 0.25, py * s - 0.25, zc - 0.1, px * s + 0.25, py * s + 0.25, zc + 2.6, "MT_Sandstone")
    g.box(-s - 0.33, -s - 0.33, zc + 2.5, s + 0.33, s + 0.33, zc + 3.0, "MT_Sandstone")
    top = dome(g, 0.0, 0.0, s + 0.1, zc + 2.98, "MT_PlasterTan", segs=16, rings=7, shape="pointed")
    finial_spike(g, 0.0, 0.0, top - 0.1, dict(mat="MT_Gold", h=1.1, r=0.07))


# ---------------------------------------------------------------------------------------------- caravan yard

@asset("SM_Desert_CaravanYard_A", "Desert", "Desert", budget=(2000, 15000),
       notes="Walled caravan yard (~27 x 23 m): sandstone perimeter wall with rounded merlons, an open arched gate "
             "(5 m wide) in the -Y wall, corner towers with domes, lean-to stalls along the back wall, a water "
             "trough and hitching posts. The yard floor is left to the terrain.")
def caravan_yard(g, rng):
    W, D = 26.0, 22.0
    t = 0.8
    Hw = 4.2
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    gw, gh = 5.0, 5.2
    # perimeter walls as four slabs (front split by the gatehouse); side walls sit between front/back walls
    def wall_x(xa, xb, y, inward):
        zb = g.buried(-0.4)
        ya, yb = (y, y + t) if inward > 0 else (y - t, y)
        g.box(xa, ya, zb, xb, yb, Hw, "MT_Sandstone")

    wall_x(x0, -gw / 2 - 1.0, y0, 1)
    wall_x(gw / 2 + 1.0, x1, y0, 1)
    wall_x(x0, x1, y1, -1)
    for x in (x0 + 0.03, x1 - t - 0.03):
        g.box(x, y0 + t - 0.05, g.buried(-0.4), x + t, y1 - t + 0.05, Hw - 0.02, "MT_Sandstone")
    # rounded merlons on the wall tops
    for (a, b, fixed, axis) in ((x0 + 1.4, -gw / 2 - 1.2, y0, "x"), (gw / 2 + 1.2, x1 - 1.4, y0, "x"),
                                (x0 + 1.4, x1 - 1.4, y1 - t, "x"), (y0 + 1.4, y1 - 1.4, x0 + 0.03, "y"),
                                (y0 + 1.4, y1 - 1.4, x1 - t - 0.03, "y")):
        L = b - a
        n = max(1, int(L / 1.2))
        for k in range(n):
            c = a + L * (k + 0.5) / n
            w = 0.55
            prof = [(0, 0), (w, 0), (w, 0.3)] + [(w / 2 + w / 2 * math.cos(math.pi * i / 6),
                                                  0.3 + w / 2 * math.sin(math.pi * i / 6)) for i in range(1, 6)] + [(0, 0.3)]
            if axis == "x":
                g.plate(Frame((c - w / 2, fixed + 0.03, Hw - 0.04), (1, 0, 0), (0, 0, 1)), prof, -(t - 0.06), 0.0,
                        "MT_Sandstone")
            else:
                g.plate(Frame((fixed + 0.03, c - w / 2, Hw - 0.04), (0, 1, 0), (0, 0, 1)), prof, 0.0, t - 0.06,
                        "MT_Sandstone")
    # gatehouse: block with an open arched passage (notched outline extruded through the wall)
    gx0, gx1 = -gw / 2 - 1.4, gw / 2 + 1.4
    gd0, gd1 = y0 - 0.6, y0 + t + 0.6
    zb = g.buried(-0.45)
    outline = arcade_outline(gx1 - gx0, 7.0 - zb, [(1.4, gw, gh - zb)])
    g.plate(Frame((gx0, gd0, zb), (1, 0, 0), (0, 0, 1)), outline, -(gd1 - gd0), 0.0, "MT_Sandstone")
    parapet_ring(g, gx0, gd0, gx1, gd1, 7.0, 0.8, "MT_Sandstone", proud=0.04, thick=0.3, merlons="round")
    frg, Lg = side_frame(gx0, gd0, gx1, gd1, 0.0, "front")
    viga_row(g, frg, Lg, 6.2, n_out=0.35, spacing=0.9)
    # corner towers with small domes
    for (cx, cy) in ((x0 + 0.4, y0 + 0.4), (x1 - 0.4, y0 + 0.4), (x1 - 0.4, y1 - 0.4), (x0 + 0.4, y1 - 0.4)):
        s = 1.6
        g.box(cx - s, cy - s, g.buried(-0.45), cx + s, cy + s, 6.0, "MT_Sandstone")
        cornice(g, cx - s, cy - s, cx + s, cy + s, 5.75, 0.3, 0.1, "MT_Sandstone")
        top = dome(g, cx, cy, s - 0.1, 6.02, "MT_PlasterTan", segs=12, rings=5)
    # lean-to stalls along the back wall
    for k in range(3):
        sx = -8.0 + 8.0 * k
        yb = y1 - t
        roof_shed(g, sx - 3.2, sx + 3.2, yb - 3.2, yb + 0.05, 2.6, 3.3, "MT_WoodPlanks", ov=0.25, ovv=0.2, th=0.12,
                  course=0.3, step=0.03, ov_back=0.3)
        for px in (sx - 3.0, sx + 3.0):
            g.box(px - 0.09, yb - 3.1, g.buried(-0.3), px + 0.09, yb - 2.92, 2.62, "MT_Timber")
        g.box(sx - 3.18, yb - 3.13, 2.42, sx + 3.18, yb - 2.89, 2.6, "MT_Timber")
        awning(g, Frame((sx - 3.1, yb - 3.05, 0.0), (1, 0, 0), (0, 0, 1)), 0.0, 6.2, 2.45, 0.9, 0.5,
               "MT_ClothTan" if k != 1 else "MT_ClothRed", poles=False)
    # water trough and hitching posts
    g.prism_holes([(-2.0, 1.0), (2.0, 1.0), (2.0, 2.0), (-2.0, 2.0)], [[(-1.85, 1.15), (1.85, 1.15), (1.85, 1.85),
                  (-1.85, 1.85)]], g.buried(-0.2), 0.75, "MT_Sandstone")
    g.box(-1.87, 1.13, g.buried(-0.2), 1.87, 1.87, 0.62, "MT_Glass")
    for k in range(5):
        x = -6.0 + 3.0 * k
        g.box(x - 0.08, -3.5, g.buried(-0.3), x + 0.08, -3.34, 1.1, "MT_Timber")
    g.box(-6.1, -3.47, 0.92, 6.1, -3.37, 1.0, "MT_Timber")
    return {"attach": {"gate_width": gw, "gate_height": gh}}
