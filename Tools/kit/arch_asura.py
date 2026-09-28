"""Asura set (Asura towns incl. Roa and Ars commoner districts): half-timbered plaster houses on stone plinths,
red clay-tile roofs with stepped tile courses, jettied upper floors, dormers and stone chimneys; a Boreas-style
manor and a castle keep for Roa."""
import math

from arch_geo import Frame
from arch_house import House, shift
from arch_parts import (Opening, arch_poly, balcony, block, circle_poly, cornice, frame_ring, hanging_sign,
                        hoist_beam, poly_body, rect, roof_cone, roof_hip, stair, window_trim, door_trim, finial_spike)
from arch_registry import asset

STONE_WIN = dict(trim="MT_Stone", frame_w=0.1, shutters=True, sill_mat="MT_Stone", win_w=0.85, win_h=1.2,
                 win_sill=0.95)
TIMBER_WIN = dict(trim="MT_Timber", frame=False, frame_w=0.045, shutters=False, sill=False, win_w=0.9, win_h=1.2,
                  win_sill=0.85)
PLASTER_WIN = dict(trim="MT_Timber", frame_w=0.08, shutters=True, sill_mat="MT_Timber", win_w=0.85, win_h=1.2,
                   win_sill=0.95)
DOOR = dict(door_frame_mat="MT_Timber", straps=True)
STONE_DOOR = dict(door_frame_mat="MT_Stone", door_frame_w=0.14, straps=True)


def asura_house(**kw):
    base = dict(walls=["MT_Plaster"], timber=[True], roof_mat="MT_RoofRed", pitch=50, ov=0.45, ovv=0.32,
                win_styles=[PLASTER_WIN, TIMBER_WIN], door_style=DOOR)
    base.update(kw)
    W = base.pop("W")
    D = base.pop("D")
    Hs = base.pop("heights")
    return House(W, D, Hs, **base)


# ---------------------------------------------------------------------------------------------- houses

@asset("SM_Asura_House_S_A", "Asura", "Asura", notes="Small two-storey half-timbered house, eaves to the street.")
def house_s_a(g, rng):
    asura_house(W=6.2, D=5.6, heights=[2.9, 2.7], jetty=[(0.35, 0.11, 0.11)], ridge="x",
                walls=["MT_Plaster", "MT_Plaster"], timber=[True, True],
                door=dict(side="front", at=0.3, w=1.05, h=2.1),
                chimneys=[dict(x=0.32, y=0.12, pots=1)],
                window_counts={(0, "front"): 2, (1, "front"): 2}).build(g)


@asset("SM_Asura_House_S_B", "Asura", "Asura",
       notes="Small gable-fronted house: stone ground floor, jettied timber upper floor, loft door with hoist beam.")
def house_s_b(g, rng):
    def loft(h, g):
        top = h.levels[-1]
        fr, Lf = top["frames"]["front"]
        hoist_beam(g, fr, Lf / 2, top["H"] + 2.35, out=0.85)

    h = asura_house(W=5.6, D=6.8, heights=[3.0, 2.7], jetty=[(0.35, 0.11, 0.11)], ridge="y", pitch=52,
                    walls=["MT_Stone", "MT_Plaster"], timber=[False, True], win_styles=[STONE_WIN, TIMBER_WIN],
                    door=dict(side="front", at=0.5, w=1.1, h=2.2, kind="arch"), door_style=STONE_DOOR,
                    gable_windows=False, window_counts={(0, "front"): 2, (1, "front"): 2},
                    extra_openings={(1, "front"): [Opening(rect(5.82 / 2 - 0.5, 2.9, 1.0, 1.3), depth=0.2,
                                                           back="MT_WoodPlanks", kind="loft")]},
                    chimneys=[dict(x=-0.28, y=0.3, pots=2)], hooks=[loft])
    h.build(g)


@asset("SM_Asura_House_M_A", "Asura", "Asura",
       notes="Medium house, eaves to the street: stone ground floor, jettied timber upper floor, two dormers.")
def house_m_a(g, rng):
    asura_house(W=8.6, D=7.6, heights=[3.1, 2.8], jetty=[(0.32, 0.32, 0.11)], ridge="x", pitch=50,
                walls=["MT_Stone", "MT_Plaster"], timber=[False, True], win_styles=[STONE_WIN, TIMBER_WIN],
                door=dict(side="front", at=0.5, w=1.2, h=2.3, kind="arch"), door_style=STONE_DOOR,
                dormers=[dict(u=-0.25, w=1.5), dict(u=0.25, w=1.5)],
                chimneys=[dict(x=-0.36, y=0.15, pots=2)], window_counts={(0, "front"): 3, (1, "front"): 4}).build(g)


@asset("SM_Asura_House_M_B", "Asura", "Asura",
       notes="Medium gable-fronted town house, three storeys with two stacked jetties.")
def house_m_b(g, rng):
    asura_house(W=7.2, D=9.4, heights=[3.0, 2.7, 2.6], jetty=[(0.3, 0.11, 0.11), (0.28, 0.11, 0.11)], ridge="y",
                pitch=54, walls=["MT_Plaster"], timber=[True], door=dict(side="front", at=0.32, w=1.1, h=2.15),
                chimneys=[dict(x=0.3, y=0.2, pots=1)], window_counts={(0, "front"): 2, (1, "front"): 3,
                                                                     (2, "front"): 3}).build(g)


@asset("SM_Asura_House_L_A", "Asura", "Asura",
       notes="Large three-storey town house, eaves to the street, three dormers, two chimneys.")
def house_l_a(g, rng):
    asura_house(W=10.8, D=9.2, heights=[3.2, 2.8, 2.7], jetty=[(0.3, 0.3, 0.11), (0.25, 0.11, 0.11)], ridge="x",
                pitch=50, walls=["MT_Stone", "MT_Plaster"], timber=[False, True], win_styles=[STONE_WIN, TIMBER_WIN],
                door=dict(side="front", at=0.5, w=1.3, h=2.4, kind="arch"), door_style=STONE_DOOR,
                dormers=[dict(u=-0.3, w=1.4), dict(u=0.0, w=1.6), dict(u=0.3, w=1.4)],
                chimneys=[dict(x=-0.38, y=0.18, pots=2), dict(x=0.38, y=-0.1, pots=1)],
                window_counts={(0, "front"): 4, (1, "front"): 5, (2, "front"): 5},
                win_spacing=2.1, under_braces=False, max_panel=2.4).build(g)


@asset("SM_Asura_House_L_B", "Asura", "Asura",
       notes="Large hip-roofed corner house with an octagonal oriel turret on the front-left corner.")
def house_l_b(g, rng):
    def turret(h, g):
        L1 = h.levels[1]
        top = h.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        cx, cy = x0 + 0.35, y0 + 0.35
        R = 1.45
        z0 = L1["z0"] - 0.25
        z1 = h.zw + 1.6
        poly = circle_poly(cx, cy, R, 8, a0=math.pi / 8)
        # windows on the four outward facets, per storey
        ops = {}
        for e in (3, 5):
            lst = []
            for L in h.levels[1:]:
                lst.append(Opening(rect(0.28, L["z0"] - z0 + 0.8, 0.55, 1.05), depth=0.15, kind="window"))
            ops[e] = lst
        frames = poly_body(g, poly, z0, z1, "MT_Plaster", ops=ops)
        for e, lst in ops.items():
            fr, Lf = frames[e]
            for op in lst:
                window_trim(g, fr, op, dict(trim="MT_Timber", frame_w=0.07, shutters=False, sill=False,
                                            mullions=False))
        # corbel under the turret
        g.lathe([(0.25, z0 - 1.6), (0.7, z0 - 1.1), (R * 0.96, z0 - 0.35), (R * 0.97, z0 + 0.05)], 8, "MT_Stone",
                cx, cy, a0=math.pi / 8, smooth=False)
        # timber bands
        for L in h.levels[1:]:
            band = circle_poly(cx, cy, R + 0.05, 8, a0=math.pi / 8)
            inner = circle_poly(cx, cy, R - 0.3, 8, a0=math.pi / 8)
            g.prism_holes(band, [inner], L["z0"] - 0.155, L["z0"] + 0.05, "MT_Timber")
        roof_cone(g, cx, cy, R, z1, 4.2, "MT_RoofRed", ov=0.25, th=0.15, course=0.45, step=0.04, segs=8,
                  finial=dict(mat="MT_Iron", h=1.1))

    asura_house(W=10.4, D=12.6, heights=[3.3, 2.9, 2.8], jetty=[(0.3, 0.3, 0.3), (0.11, 0.11, 0.11)], roof="hip",
                pitch=48, walls=["MT_Stone", "MT_Plaster"], timber=[False, True], win_styles=[STONE_WIN, TIMBER_WIN],
                door=dict(side="front", at=0.58, w=1.3, h=2.4, kind="arch"), door_style=STONE_DOOR,
                chimneys=[dict(x=0.3, y=0.25, pots=2)], window_counts={(0, "front"): 3, (1, "front"): 3,
                                                                      (2, "front"): 3},
                window_positions={(1, "front"): [4.0, 6.5, 9.0], (2, "front"): [4.0, 6.5, 9.0]},
                hooks=[turret], under_braces=False, max_panel=2.4, course=0.4).build(g)


# ---------------------------------------------------------------------------------------------- shop / inn

@asset("SM_Asura_Shop_A", "Asura", "Asura",
       notes="Shop: two ground-floor counters under red and blue cloth awnings, hanging sign, timber upper floor.")
def shop_a(g, rng):
    def sign(h, g):
        L0 = h.levels[0]
        fr, Lf = L0["frames"]["front"]
        hanging_sign(g, fr, 5.35, L0["H"] - 0.25, out=1.1)

    W = 8.6
    shops = [Opening(rect(0.55, 0.85, 2.3, 1.55), depth=0.45, back="MT_WoodPlanks", kind="shop", awning="MT_ClothRed"),
             Opening(rect(W - 0.55 - 2.3, 0.85, 2.3, 1.55), depth=0.45, back="MT_WoodPlanks", kind="shop",
                     awning="MT_ClothBlue")]
    asura_house(W=W, D=8.0, heights=[3.2, 2.8], jetty=[(0.35, 0.11, 0.11)], ridge="x", pitch=50,
                walls=["MT_Plaster"], timber=[True], door=dict(side="front", at=0.5, w=1.15, h=2.2),
                skip_windows={(0, "front")}, extra_openings={(0, "front"): shops},
                dormers=[dict(u=0.0, w=1.6)], chimneys=[dict(x=0.35, y=0.2, pots=1)],
                window_counts={(1, "front"): 4}, hooks=[sign]).build(g)


@asset("SM_Asura_Inn_A", "Asura", "Asura",
       notes="Three-storey inn: stone ground floor with a carriage arch, balcony, hanging sign, dormers.")
def inn_a(g, rng):
    def extras(h, g):
        L1 = h.levels[1]
        fr, Lf = L1["frames"]["front"]
        balcony(g, fr, 3.95, 9.27, 0.055, 1.05, bracket_us=[4.1, 5.3, 7.9, 9.1])
        L0 = h.levels[0]
        fr0, Lf0 = L0["frames"]["front"]
        hanging_sign(g, fr0, 1.2, L0["H"] - 0.3, out=1.2, w=0.85, h=0.6)

    W = 13.0
    bal_doors = [Opening(rect(5.2 - 0.5, 0.0, 1.0, 2.1), depth=0.2, back="MT_WoodPlanks", kind="door"),
                 Opening(rect(8.02 - 0.5, 0.0, 1.0, 2.1), depth=0.2, back="MT_WoodPlanks", kind="door")]
    asura_house(W=W, D=10.4, heights=[3.6, 2.9, 2.8], jetty=[(0.11, 0.11, 0.11), (0.3, 0.11, 0.11)], ridge="x",
                pitch=50, walls=["MT_Stone", "MT_Plaster"], timber=[False, True],
                win_styles=[STONE_WIN, TIMBER_WIN], door=dict(side="front", at=0.5, w=2.4, h=3.0, kind="arch"),
                door_style=dict(STONE_DOOR, battens=True), window_counts={(0, "front"): 4, (1, "front"): 4,
                                                                          (2, "front"): 5},
                window_positions={(1, "front"): [1.35, 2.85, 6.61, 10.37, 11.87]},
                extra_openings={(1, "front"): bal_doors},
                dormers=[dict(u=-0.3, w=1.5), dict(u=0.0, w=1.5), dict(u=0.3, w=1.5)],
                chimneys=[dict(x=-0.4, y=0.2, pots=2), dict(x=0.4, y=0.2, pots=2)], hooks=[extras],
                win_spacing=2.4, under_braces=False, max_panel=2.4).build(g)


# ---------------------------------------------------------------------------------------------- tower

@asset("SM_Asura_Tower_A", "Asura", "Asura",
       notes="Stone watch / bell tower with a jettied half-timbered belfry and a steep pyramid roof.")
def tower_a(g, rng):
    slit = dict(trim="MT_Stone", frame_w=0.1, shutters=False, sill_mat="MT_Stone", win_w=0.45, win_h=1.0,
                win_sill=1.1, kind="arch", mullions=False)
    belfry = dict(trim="MT_Timber", frame_w=0.07, shutters=False, sill=False, win_w=1.2, win_h=1.6, win_sill=0.55,
                  kind="arch", mullions=False)
    h = House(5.4, 5.4, [3.4, 3.2, 3.2, 3.2, 3.0], walls=["MT_Stone"] * 4 + ["MT_Plaster"],
              timber=[False] * 4 + [True], jetty=[(-0.04, -0.04, -0.04)] * 3 + [(0.34, 0.34, 0.34)], roof="hip",
              pitch=64, roof_mat="MT_RoofRed", ov=0.4, win_styles=[slit] * 4 + [belfry],
              door=dict(side="front", at=0.5, w=1.2, h=2.4, kind="arch"), door_style=STONE_DOOR,
              window_counts={(i, s): 1 for i in range(5) for s in ("front", "back", "left", "right")},
              skip_windows={(0, "front"), (0, "left"), (0, "right")},
              cornices="MT_Stone", hip_finial=dict(mat="MT_Iron", h=1.6))
    h.build(g)


# ---------------------------------------------------------------------------------------------- manor

@asset("SM_Asura_Manor_A", "Asura", "Asura", kind="building", budget=(4000, 15000),
       notes="Boreas-style noble manor (Roa): three-storey main block, two forward wings and a courtyard wall "
             "with a gate; the courtyard opens toward -Y.")
def manor_a(g, rng):
    main_win = dict(trim="MT_Stone", frame_w=0.12, shutters=False, sill_mat="MT_Stone", win_w=1.0, win_h=1.7,
                    win_sill=0.8, lintel=True, lintel_mat="MT_Stone")
    up_win = dict(main_win, win_h=1.6, win_sill=0.7)
    with g.xf(loc=(0, 4.0, 0)):
        House(22.0, 10.0, [3.8, 3.4, 3.1], walls=["MT_Stone", "MT_Plaster", "MT_Plaster"], timber=[False],
              jetty=[(-0.04, -0.04, -0.04)] * 2,
              roof="hip", pitch=44, roof_mat="MT_RoofRed", ov=0.55, win_styles=[main_win, up_win, up_win],
              door=dict(side="front", at=0.5, w=1.8, h=3.0, kind="arch"),
              door_style=dict(door_frame_mat="MT_Stone", door_frame_w=0.18, straps=True, battens=True),
              win_spacing=2.55, cornices="MT_Stone", side_windows=False,
              dormers=[dict(u=-0.28, w=1.6), dict(u=0.0, w=1.8), dict(u=0.28, w=1.6)], dormer_wall="MT_Plaster",
              chimneys=[dict(x=-0.3, y=0.0, pots=3, w=0.9), dict(x=0.3, y=0.0, pots=3, w=0.9)]).build(g)
    for sx in (-1, 1):
        with g.xf(loc=(sx * 11.8, -3.6, 0)):
            House(7.4, 13.0, [3.8, 3.4], floor0=0.55, walls=["MT_Stone", "MT_Plaster"], timber=[False], roof="hip",
                  jetty=[(-0.04, -0.04, -0.04)],
                  pitch=44, roof_mat="MT_RoofRed", ov=0.5, win_styles=[main_win, up_win],
                  door=dict(side=("right" if sx < 0 else "left"), at=0.3, w=1.3, h=2.5, kind="arch"),
                  door_style=dict(door_frame_mat="MT_Stone", door_frame_w=0.14, straps=True),
                  win_spacing=2.6, cornices="MT_Stone", back_windows=False,
                  chimneys=[dict(x=0.0, y=0.3, pots=2)]).build(g)
    # courtyard wall with gate piers and an iron gate
    zc = 2.5
    for sx in (-1, 1):
        xa, xb = sx * 2.6, sx * 8.3
        g.box(min(xa, xb), -9.9, g.buried(-0.4), max(xa, xb), -9.4, zc, "MT_Stone")
        g.box(min(xa, xb) + 0.02, -9.99, zc - 0.05, max(xa, xb) - 0.02, -9.31, zc + 0.16, "MT_Stone")
        # gate pier
        px = sx * 2.4
        g.box(px - 0.45, -10.1, g.buried(-0.4), px + 0.45, -9.2, 3.4, "MT_Stone")
        g.box(px - 0.55, -10.2, 3.3, px + 0.55, -9.1, 3.55, "MT_Stone")
        g.sphere(px, -9.65, 3.9, 0.36, 10, 6, "MT_Stone")
    # iron gate: two leaves of bars
    for sx in (-1, 1):
        for k in range(9):
            x = sx * (0.12 + k * 0.215)
            g.box(x - 0.02, -9.67, 0.0, x + 0.02, -9.63, 2.3 + 0.25 * math.sin(math.pi * k / 9), "MT_Iron")
        g.box(sx * 0.05, -9.68, 0.35, sx * 2.0, -9.62, 0.42, "MT_Iron")
        g.box(sx * 0.05, -9.685, 1.9, sx * 2.0, -9.615, 1.97, "MT_Iron")
    return {"attach": {"courtyard_gate_width": 4.0}}


# ---------------------------------------------------------------------------------------------- keep

def crenel_ring(g, x0, y0, x1, y1, z0, zt, thick, mat, mh=0.8, mw=0.8, gap=0.6):
    """Rectangular parapet ring (closed) with merlon blocks on top (merlons 3 cm proud on both faces)."""
    outer = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    inner = [(x0 + thick, y0 + thick), (x1 - thick, y0 + thick), (x1 - thick, y1 - thick), (x0 + thick, y1 - thick)]
    g.prism_holes(outer, [inner], z0, zt, mat)
    t = thick + 0.06
    # corner merlons
    for cx, cy in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        sx = 1 if cx == x0 else -1
        sy = 1 if cy == y0 else -1
        ax, bx = cx - sx * 0.03, cx + sx * (mw + 0.1)
        ay, by = cy - sy * 0.03, cy + sy * (mw + 0.1)
        g.box(min(ax, bx), min(ay, by), zt - 0.05, max(ax, bx), max(ay, by), zt + mh, mat)
    for (a, b, fixed, axis, inward) in ((x0, x1, y0, "x", 1), (x0, x1, y1, "x", -1), (y0, y1, x0, "y", 1),
                                       (y0, y1, x1, "y", -1)):
        L = (b - a) - 2 * (mw + 0.1)
        cell = mw + gap
        n = max(1, int(round((L - gap) / cell)))
        step = (L - gap) / n
        for k in range(n):
            c = a + (mw + 0.1) + gap + step * k + (step - gap) / 2
            wdt = min(mw, step - gap)
            f0 = fixed - inward * 0.03
            f1 = fixed + inward * (thick + 0.03)
            if axis == "x":
                g.box(c - wdt / 2, min(f0, f1), zt - 0.05, c + wdt / 2, max(f0, f1), zt + mh, mat)
            else:
                g.box(min(f0, f1), c - wdt / 2, zt - 0.05, max(f0, f1), c + wdt / 2, zt + mh, mat)


@asset("SM_Asura_Keep_A", "Asura", "Asura", kind="building", budget=(3000, 15000), foundation=1.05,
       notes="Castle keep for Roa: square stone keep with a battered base, crenellated walk, four corner turrets "
             "with red conical roofs and a hipped central roof.")
def keep_a(g, rng):
    S = 6.5
    top = 17.0
    slit_win = dict(trim="MT_Stone", frame_w=0.12, shutters=False, sill_mat="MT_Stone", mullions=False)
    big_win = dict(trim="MT_Stone", frame_w=0.14, shutters=False, sill_mat="MT_Stone", mullions=True,
                   keystone=True, keystone_mat="MT_Stone")
    ops = {}
    for side in ("front", "right", "back", "left"):
        lst = []
        for u in (3.2, 6.5, 9.8):
            lst.append(Opening(arch_poly(u - 0.2, 5.6, 0.4, 1.3, 6), depth=0.3, kind="arch"))
            lst.append(Opening(arch_poly(u - 0.2, 9.2, 0.4, 1.3, 6), depth=0.3, kind="arch"))
        for u in (4.0, 9.0):
            lst.append(Opening(arch_poly(u - 0.55, 12.6, 1.1, 2.2, 8), depth=0.3, kind="arch"))
        if side == "front":
            lst.append(Opening(arch_poly(S - 0.9, 1.0, 1.8, 3.2, 10), depth=0.4, back="MT_WoodPlanks", kind="door"))
        ops[side] = lst
    zb = g.buried(-0.8)
    frames = block(g, -S, -S, S, S, zb, top, "MT_Stone", ops={k: [shift_op(o, -zb) for o in v] for k, v in
                                                                ops.items()})
    from arch_parts import side_frame
    for side, lst in ops.items():
        fr, Lf = side_frame(-S, -S, S, S, 0.0, side)
        for op in lst:
            if op.back == "MT_WoodPlanks":
                door_trim(g, fr, op, dict(door_frame_mat="MT_Stone", door_frame_w=0.2, straps=True, battens=True))
            elif op.box[3] - op.box[1] < 1.5:
                window_trim(g, fr, op, slit_win)
            else:
                window_trim(g, fr, op, big_win)
    # battered base (a sloped skirt, closed)
    rings = []
    for (h, z) in ((S + 0.8, g.buried(-0.9)), (S + 0.15, 0.85)):
        rings.append([(-h, -h, z), (h, -h, z), (h, h, z), (-h, h, z)])
    g.loft(rings, "MT_Stone", cap0=True, cap1=True)
    # entrance stair up to the raised door
    fr, Lf = side_frame(-S, -S, S, S, 0.0, "front")
    stair(g, fr, S - 1.2, S + 1.2, 0.985, "MT_Stone", run_per=0.32)
    cornice(g, -S, -S, S, S, 11.4, 0.3, 0.14, "MT_Stone")
    # corbelled parapet walk
    cornice(g, -S, -S, S, S, top - 0.55, 0.45, 0.25, "MT_Stone")
    crenel_ring(g, -S - 0.3, -S - 0.3, S + 0.3, S + 0.3, top - 0.12, top + 1.05, 0.55, "MT_Stone")
    # central hipped roof on a low drum
    g.box(-4.6, -4.6, top - 0.1, 4.6, 4.6, top + 0.9, "MT_Stone")
    roof_hip(g, -4.6, 4.6, -4.6, 4.6, top + 0.9, 45, "MT_RoofRed", ov=0.35, th=0.2, finial=dict(mat="MT_Iron", h=1.4))
    # corner turrets
    for cx, cy in ((-S, -S), (S, -S), (S, S), (-S, S)):
        r = 2.1
        segs = 12
        tz = top + 4.2
        tops = []
        tops_ops = {}
        poly = circle_poly(cx, cy, r, segs, a0=math.pi / segs)
        # slits on the outward facets
        outward = []
        for e in range(segs):
            a = math.pi / segs + (e + 0.5) * math.tau / segs
            if math.cos(a) * cx + math.sin(a) * cy > 0.3 * S:
                outward.append(e)
        for e in outward[1:-1]:
            Lf_e = 2 * r * math.sin(math.pi / segs)
            tops_ops[e] = [Opening(arch_poly(Lf_e / 2 - 0.16, 8.0 - zb, 0.32, 1.1, 6), depth=0.25, kind="arch"),
                           Opening(arch_poly(Lf_e / 2 - 0.16, 14.5 - zb, 0.32, 1.1, 6), depth=0.25, kind="arch")]
        fr_list = poly_body(g, poly, g.buried(zb), tz, "MT_Stone", ops=tops_ops)
        g.lathe([(r + 0.35, g.buried(zb - 0.1)), (r + 0.3, 0.4), (r + 0.03, 2.2), (r + 0.03, 2.3)], segs, "MT_Stone", cx, cy,
                a0=math.pi / segs, smooth=False, caps=True)
        ring_o = circle_poly(cx, cy, r + 0.28, segs, a0=math.pi / segs)
        ring_i = circle_poly(cx, cy, r - 0.4, segs, a0=math.pi / segs)
        g.prism_holes(ring_o, [ring_i], tz - 0.85, tz + 0.1, "MT_Stone")
        roof_cone(g, cx, cy, r + 0.28, tz + 0.1, 5.6, "MT_RoofRed", ov=0.3, th=0.18, course=0.4, step=0.05,
                  segs=segs, finial=dict(mat="MT_Iron", h=1.3))
    return {"attach": {"door_sill_height": 1.0}}


def shift_op(op, dv):
    return shift(op, dv)
