"""North set (Sharia, Northern villages): stone ground floors, dark-timbered plaster upper floors, steep dark slate
roofs carrying separate snow blankets (sunk into the roof, never coplanar), heavy chimneys, small shuttered windows;
a magic shop with glowing crystal lanterns, the adventurer guild, a smithy workshop and a round watch tower."""
import math

from arch_geo import Frame
from arch_house import House
from arch_parts import (Opening, arch_poly, balcony, circle_poly, cornice, crystal_lantern, door_trim, emblem_shield,
                        exterior_chimney, hanging_sign, poly_body, rect, roof_cone, roof_hip, roof_shed, side_frame,
                        stair, window_trim)
from arch_registry import asset

N_STONE_WIN = dict(trim="MT_Stone", frame_w=0.11, shutters=True, shutter_mat="MT_WoodPlanks", sill_mat="MT_Stone",
                   win_w=0.75, win_h=1.05, win_sill=1.0, lintel=True, lintel_mat="MT_Stone")
N_TIMBER_WIN = dict(trim="MT_Timber", frame=False, frame_w=0.045, shutters=False, sill=False, win_w=0.8, win_h=1.1,
                    win_sill=0.8)
N_DOOR = dict(door_frame_mat="MT_Stone", door_frame_w=0.14, straps=True, battens=True)
SLATE = dict(roof_mat="MT_RoofDark", roof_under="MT_WoodPlanks", fascia="MT_Timber", verge="MT_Timber",
             course=0.28, step=0.035, ov=0.5, ovv=0.34, snow="MT_Snow", snow_from=0.22)


def north_house(W, D, heights, **kw):
    base = dict(walls=["MT_Stone", "MT_Plaster"], timber=[False, True], pitch=58, floor0=0.5,
                jetty=[(0.11, 0.11, 0.11)], win_styles=[N_STONE_WIN, N_TIMBER_WIN], door_style=N_DOOR,
                timber_mat="MT_Timber", max_panel=1.7, win_spacing=2.3)
    base.update(SLATE)
    base.update(kw)
    return House(W, D, heights, **base)


@asset("SM_North_House_A", "North", "North",
       notes="Northern house: stone ground floor, timber upper floor, steep slate roof with a snow blanket.")
def north_house_a(g, rng):
    north_house(7.6, 6.6, [3.0, 2.7], ridge="x", door=dict(side="front", at=0.35, w=1.05, h=2.1, kind="arch"),
                chimneys=[dict(x=0.3, y=0.15, pots=2, w=0.8, d=0.7)],
                window_counts={(0, "front"): 2, (1, "front"): 3}).build(g)


@asset("SM_North_House_B", "North", "North",
       notes="Gable-fronted northern house with a steep 60 degree roof, exterior chimney and a covered entry.")
def north_house_b(g, rng):
    def extras(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        fr, Lf = side_frame(x0, y0, x1, y1, 0.0, "right")
        exterior_chimney(g, fr, Lf * 0.55, 0.85, 0.65, h.ridge_z() + 0.8, pots=2, pot_mat="MT_Stone")
        # entry canopy on brackets over the door
        frd, Lfd = side_frame(x0, y0, x1, y1, h.floor0, "front")
        u = Lfd * 0.3
        for du in (-0.85, 0.85):
            bfr = Frame(frd.p(u + du, 0, 0), frd.n, frd.v, None)
            g.plate(bfr, [(-0.12, 2.0), (0.02, 2.0), (1.0, 2.7), (-0.12, 3.1)], -0.06, 0.06, "MT_Timber")
        roof_shed(g, x0 + u - 1.1, x0 + u + 1.1, y0 - 1.1, y0 + 0.05, h.floor0 + 2.62, h.floor0 + 3.05, "MT_RoofDark",
                  ov=0.15, ovv=0.1, th=0.12, course=0.25, step=0.03, ov_back=0.05)

    north_house(7.2, 9.4, [3.0, 2.8], ridge="y", pitch=60, door=dict(side="front", at=0.3, w=1.05, h=2.1),
                window_counts={(0, "front"): 1, (1, "front"): 2, (0, "right"): 1, (1, "right"): 1},
                window_positions={(0, "front"): [5.1]}, hooks=[extras]).build(g)


@asset("SM_North_House_C", "North", "North",
       notes="Stone cottage, one and a half storeys, two snow-capped dormers and gable-end chimneys.")
def north_house_c(g, rng):
    north_house(9.4, 7.0, [2.9, 1.1], walls=["MT_Stone"], timber=[False], jetty=[(-0.04, -0.04, -0.04)],
                ridge="x", pitch=55, door=dict(side="front", at=0.5, w=1.1, h=2.15, kind="arch"),
                win_styles=[N_STONE_WIN], skip_windows={(1, "front"), (1, "back")},
                dormers=[dict(u=-0.25, w=1.4), dict(u=0.25, w=1.4)], dormer_wall="MT_Plaster",
                chimneys=[dict(x=-0.44, y=0.0, pots=2, w=0.9, d=0.8), dict(x=0.44, y=0.0, pots=1, w=0.8, d=0.8)],
                cornices="MT_Stone", window_counts={(0, "front"): 2}, gable_windows=False).build(g)


# ---------------------------------------------------------------------------------------------- shop

@asset("SM_North_Shop_A", "North", "North",
       notes="Magic shop: wide shop window, door flanked by two glowing crystal lanterns (MT_Crystal), hanging sign, "
             "snow-capped gable roof with a dormer.")
def north_shop_a(g, rng):
    def extras(h, g):
        L0 = h.levels[0]
        fr, Lf = L0["frames"]["front"]
        du = Lf * 0.72
        for s in (-1, 1):
            crystal_lantern(g, fr, du + s * 0.95, 2.45, out=0.5)
        hanging_sign(g, fr, 0.75, L0["H"] - 0.2, out=1.0, w=0.75, h=0.5)

    W = 8.6
    shopwin = [Opening(rect(1.0, 0.75, 2.6, 1.7), depth=0.3, kind="window")]
    north_house(W, 8.4, [3.4, 2.8], ridge="x", pitch=56, door=dict(side="front", at=0.72, w=1.1, h=2.2, kind="arch"),
                skip_windows={(0, "front")}, extra_openings={(0, "front"): shopwin},
                dormers=[dict(u=-0.1, w=1.6)], chimneys=[dict(x=0.36, y=0.2, pots=2)],
                window_counts={(1, "front"): 3}, hooks=[extras]).build(g)


# ---------------------------------------------------------------------------------------------- guild

@asset("SM_North_Guild_A", "North", "North", budget=(4000, 15000),
       notes="Adventurer guild: three storeys (stone, then timber), hipped snow roof with dormers, square corner "
             "tower with a spire, arched double doors, balcony and a shield-and-swords emblem over the entrance.")
def north_guild_a(g, rng):
    def extras(h, g):
        L0, L1 = h.levels[0], h.levels[1]
        fr0, Lf0 = L0["frames"]["front"]
        fr1, Lf1 = L1["frames"]["front"]
        balcony(g, fr1, Lf1 / 2 - 2.4, Lf1 / 2 + 2.4, 0.055, 1.1, bracket_us=[Lf1 / 2 - 2.25, Lf1 / 2 - 1.4,
                                                                            Lf1 / 2 + 1.4, Lf1 / 2 + 2.25])
        for s in (-1, 1):
            crystal_lantern(g, fr0, Lf0 / 2 + s * 1.75, 2.9, out=0.5)
        # corner tower (front-right)
        x0, y0, x1, y1 = h.levels[-1]["fp"]
        tx, ty = x1 - 1.45, y0 + 1.75
        S = 2.45
        zt = h.zw + 4.6
        ops = {}
        for side in ("front", "right"):
            ops[side] = [Opening(arch_poly(S - 0.35, vk, 0.7, 1.4, 6), depth=0.25, kind="arch")
                         for k, vk in enumerate((4.0, 7.2, 10.4, 12.9)) if not (side == "front" and k == 1)]
        from arch_parts import block
        zb = g.buried(-0.5)
        block(g, tx - S, ty - S, tx + S, ty + S, zb, zt, "MT_Stone",
              ops={k: [Opening([(u, v - zb) for (u, v) in o.poly], o.depth, o.back, o.reveal, o.kind) for o in lst]
                   for k, lst in ops.items()})
        for side, lst in ops.items():
            frt, Lft = side_frame(tx - S, ty - S, tx + S, ty + S, 0.0, side)
            for op in lst:
                window_trim(g, frt, op, dict(trim="MT_Stone", frame_w=0.1, shutters=False, sill_mat="MT_Stone",
                                             mullions=False))
        cornice(g, tx - S, ty - S, tx + S, ty + S, zt - 0.35, 0.3, 0.16, "MT_Stone")
        frt, Lft = side_frame(tx - S, ty - S, tx + S, ty + S, 0.0, "front")
        emblem_shield(g, frt, S, 8.75, w=1.25, h=1.55)
        roof_hip(g, tx - S, tx + S, ty - S, ty + S, zt, 66, "MT_RoofDark", ov=0.4, th=0.2, course=0.3, step=0.035,
                 finial=dict(mat="MT_Gold", h=1.6, r=0.09), snow="MT_Snow", snow_from=0.3)

    stone = dict(N_STONE_WIN, win_w=0.95, win_h=1.5, win_sill=1.0)
    W = 16.0
    bal_doors = [Opening(rect((W + 0.22) / 2 - 1.6 - 0.5, 0.0, 1.0, 2.15), depth=0.2, back="MT_WoodPlanks",
                         kind="door"),
                 Opening(rect((W + 0.22) / 2 + 1.6 - 0.5, 0.0, 1.0, 2.15), depth=0.2, back="MT_WoodPlanks",
                         kind="door")]
    h = north_house(W, 12.0, [3.9, 3.1, 2.9], roof="hip", pitch=55, walls=["MT_Stone", "MT_Plaster"],
                    timber=[False, True], jetty=[(0.11, 0.11, 0.11), (0.11, 0.11, 0.11)],
                    win_styles=[stone, N_TIMBER_WIN], door=dict(side="front", at=0.5, w=2.4, h=3.2, kind="arch"),
                    door_style=dict(N_DOOR, door_frame_w=0.2), cornices="MT_Stone",
                    window_positions={(0, "front"): [1.5, 3.8, 10.5], (1, "front"): [1.6, 3.9],
                                      (2, "front"): [1.6, 3.9, 6.2, 8.5, 10.8]},
                    extra_openings={(1, "front"): bal_doors}, win_spacing=2.4, under_braces=False, max_panel=2.4,
                    dormers=[dict(u=-0.25, w=1.6), dict(u=0.02, w=1.6)],
                    chimneys=[dict(x=-0.35, y=0.2, pots=3, w=1.0, d=0.8), dict(x=0.1, y=0.25, pots=2)], hooks=[extras])
    h.build(g)


# ---------------------------------------------------------------------------------------------- workshop

@asset("SM_North_Workshop_A", "North", "North",
       notes="Smithy / workshop: long stone building with a wide open work front, massive forge chimney and a snowy "
             "lean-to shelter with an anvil and a woodpile.")
def north_workshop_a(g, rng):
    def extras(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        fr, Lf = side_frame(x0, y0, x1, y1, 0.0, "left")
        exterior_chimney(g, fr, Lf * 0.55, 1.3, 0.95, h.ridge_z() + 1.4, pots=0, base_w=2.0, base_h=2.6)
        # lean-to on the right end
        ax0, ax1 = x1 - 0.1, x1 + 3.0
        with g.xf(loc=((ax0 + ax1) / 2, (y0 + y1) / 2, 0.0), rotz=90):
            hw = (y1 - y0) / 2 - 0.5
            hd = (ax1 - ax0) / 2
            roof_shed(g, -hw, hw, -hd, hd, 2.3, 3.0, "MT_RoofDark", ov=0.3, ovv=0.2, th=0.14, course=0.28, step=0.03,
                      ov_back=0.3)
            for sx in (-1, 1):
                g.box(sx * (hw - 0.15) - 0.1, -hd + 0.15, g.buried(-0.3), sx * (hw - 0.15) + 0.1, -hd + 0.35, 2.38,
                      "MT_Timber")
            g.box(-hw + 0.02, -hd + 0.13, 2.18, hw - 0.02, -hd + 0.37, 2.34, "MT_Timber")
            # anvil on a stump
            g.cylinder(0.3, 0.3, 0.3, g.buried(-0.1), 0.55, 8, "MT_Timber", smooth=False)
            g.box(0.05, 0.18, 0.54, 0.62, 0.42, 0.72, "MT_Iron")
            g.box(0.15, 0.22, 0.71, 0.9, 0.38, 0.86, "MT_Iron")
            for k in range(3):
                for m in range(4 - k):
                    with g.xf(loc=(-hw + 0.6 + m * 0.34 + k * 0.17, 0.9, 0.16 + k * 0.28), rotx=90):
                        g.cylinder(0, 0, 0.14, -0.35, 0.35, 7, "MT_Timber", smooth=False)

    W = 12.0
    shopfront = [Opening(rect(W * 0.58 - 1.9, 0.0 + 0.4, 3.8, 2.6), depth=0.6, back="MT_Timber", kind="shop",
                         awning="MT_Hide")]
    north_house(W, 8.0, [3.6], walls=["MT_Stone"], timber=[False], ridge="x", pitch=54, floor0=0.25,
                door=dict(side="front", at=0.18, w=1.1, h=2.2), skip_windows={(0, "front")}, win_styles=[N_STONE_WIN],
                extra_openings={(0, "front"): shopfront}, window_counts={(0, "back"): 3, (0, "right"): 0,
                                                                        (0, "left"): 0},
                hooks=[extras]).build(g)


# ---------------------------------------------------------------------------------------------- tower

@asset("SM_North_Tower_A", "North", "North", budget=(1500, 8000),
       notes="Round stone watch tower (12-sided, radius 3.2 m) with arched windows, a corbelled parapet band and a "
             "steep conical slate roof with a snow cap.")
def north_tower_a(g, rng):
    r = 3.2
    segs = 12
    zt = 15.0
    zb = g.buried(-0.5)
    poly = circle_poly(0, 0, r, segs, a0=math.pi / segs)
    Lf = 2 * r * math.sin(math.pi / segs)
    ops = {}
    front = None
    for e in range(segs):
        a = math.pi / segs + (e + 0.5) * math.tau / segs
        if abs(math.cos(a)) < 0.3 and math.sin(a) < 0:
            front = e
    for e in range(segs):
        lst = []
        if e == front:
            lst.append(Opening(arch_poly(Lf / 2 - 0.55, 0.45 - zb, 1.1, 2.3, 8), depth=0.4, back="MT_WoodPlanks",
                               kind="door"))
        for k, zw in enumerate((4.5, 8.0, 11.5)):
            if (e + k) % 3 == 0 and not (e == front and k == 0):
                lst.append(Opening(arch_poly(Lf / 2 - 0.3, zw - zb, 0.6, 1.3, 6), depth=0.35, kind="arch"))
        ops[e] = lst
    frames = poly_body(g, poly, zb, zt, "MT_Stone", ops=ops)
    for e, lst in ops.items():
        fr, _ = frames[e]
        for op in lst:
            if op.back == "MT_WoodPlanks":
                door_trim(g, fr, op, N_DOOR)
                stair(g, Frame((fr.o.x, fr.o.y, 0.0), fr.u, (0, 0, 1)), Lf / 2 - 0.9, Lf / 2 + 0.9, 0.435, "MT_Stone",
                      embed=0.3)
            else:
                window_trim(g, fr, op, dict(trim="MT_Stone", frame_w=0.1, shutters=False, sill_mat="MT_Stone",
                                            mullions=False))
    # base batter and parapet band
    g.lathe([(r + 0.45, g.buried(-0.55)), (r + 0.3, 0.2), (r + 0.06, 0.4)], segs, "MT_Stone", a0=math.pi / segs,
            smooth=False)
    g.lathe([(r - 0.3, zt - 1.45), (r + 0.04, zt - 1.45), (r + 0.4, zt - 1.0), (r + 0.4, zt + 0.25),
             (r - 0.3, zt + 0.25)], segs, "MT_Stone", a0=math.pi / segs, smooth=False)
    roof_cone(g, 0, 0, r + 0.4, zt + 0.25, 8.2, "MT_RoofDark", ov=0.45, th=0.22, course=0.32, step=0.04, segs=segs,
              finial=dict(mat="MT_Iron", h=1.5), snow="MT_Snow", a0=math.pi / segs)
