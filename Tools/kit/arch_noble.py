"""AsuraNoble set (Ars noble district and government buildings): white limestone, blue slate mansard and hip roofs
with gold crests and finials, cornices, corner pilasters, pedimented windows with iron balconettes, porticoes,
a domed government hall and a clock tower with a spire."""
import math


from arch_geo import Frame
from arch_house import House
from arch_parts import (Opening, arch_poly, block, circle_poly, column, cornice, corner_posts, door_trim, drum_dome,
                        finial_spike, gable_block, poly_body, portico, rect, roof_cone, roof_hip, side_frame, stair,
                        window_trim, wplate)
from arch_registry import asset

W_GROUND = dict(trim="MT_StoneWhite", frame_w=0.13, shutters=False, sill_mat="MT_StoneWhite", win_w=1.05, win_h=2.0,
                win_sill=0.75, lintel=True, lintel_mat="MT_StoneWhite", mullion_mat="MT_Timber")
W_NOBILE = dict(trim="MT_StoneWhite", frame_w=0.13, shutters=False, sill=False, win_w=1.05, win_h=2.25, win_sill=0.35,
                pediment="tri", balconette=True, mullion_mat="MT_Timber")
W_UPPER = dict(trim="MT_StoneWhite", frame_w=0.12, shutters=False, sill_mat="MT_StoneWhite", win_w=0.95, win_h=1.7,
               win_sill=0.6, pediment="seg", mullion_mat="MT_Timber")
N_DOOR = dict(door_frame_mat="MT_StoneWhite", door_frame_w=0.2, straps=True, battens=True)
W_SIDE = dict(trim="MT_StoneWhite", frame_w=0.12, shutters=False, sill_mat="MT_StoneWhite", mullion_mat="MT_Timber")
BLUE = dict(roof_mat="MT_RoofBlue", roof_under="MT_StoneWhite", fascia="MT_StoneWhite", verge="MT_StoneWhite",
            course=0.42, step=0.035, crest="MT_Gold", hip_finial=dict(mat="MT_Gold", h=1.3, r=0.08))


def noble_house(W, D, heights, **kw):
    base = dict(walls=["MT_StoneWhite"], timber=[False], jetty=[(-0.04, -0.04, -0.04)] * 3, cornices="MT_StoneWhite",
                pilasters="MT_StoneWhite", plinth="MT_StoneWhite", floor0=0.9, win_styles=[W_GROUND, W_NOBILE, W_UPPER],
                door_style=N_DOOR, win_spacing=2.5, side_windows=True, dormer_wall="MT_StoneWhite",
                timber_mat="MT_StoneWhite", side_win_styles=[W_SIDE], step_mat="MT_StoneWhite")
    base.update(BLUE)
    base.update(kw)
    return House(W, D, heights, **base)


def _front_steps(g, fr, u0, u1, z_top, depth_out=0.0):
    stair(g, fr.shifted(dn=depth_out), u0, u1, z_top, "MT_StoneWhite", run_per=0.34)


@asset("SM_Noble_House_A", "AsuraNoble", "AsuraNoble",
       notes="Noble town mansion: raised ground floor, pedimented piano nobile with balconettes, blue mansard roof "
             "with dormers and a gold crest, columned door portico with steps.")
def noble_house_a(g, rng):
    def extras(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        fr, Lf = side_frame(x0, y0, x1, y1, 0.0, "front")
        portico(g, fr, Lf / 2, 3.4, 1.35, 3.3, 2, floor_h=h.floor0 - 0.03, pediment=True, gold="MT_Gold")
        _front_steps(g, fr, Lf / 2 - 1.2, Lf / 2 + 1.2, h.floor0 - 0.045, depth_out=1.45)

    noble_house(12.6, 10.4, [4.2, 3.8, 3.3], roof="mansard", pitch=72, pitch2=26, mansard_h=2.5, ov=0.35,
                door=dict(side="front", at=0.5, w=1.5, h=2.9, kind="arch"), door_steps=False,
                dormers=[dict(u=-0.3, w=1.4), dict(u=0.0, w=1.4), dict(u=0.3, w=1.4)],
                chimneys=[dict(x=-0.28, y=0.2, pots=2, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_StoneWhite"),
                          dict(x=0.28, y=0.2, pots=2, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_StoneWhite")],
                window_positions={(0, "front"): [1.6, 3.9, 8.7, 11.0], (1, "front"): [1.6, 3.9, 8.62, 10.92],
                                  (2, "front"): [1.6, 3.9, 6.26, 8.62, 10.92]},
                hooks=[extras]).build(g)


@asset("SM_Noble_House_B", "AsuraNoble", "AsuraNoble",
       notes="Noble corner house: arched ground floor, balconettes, steep blue hip roof with a gold crest and a round "
             "corner turret with a conical spire.")
def noble_house_b(g, rng):
    def turret(h, g):
        top = h.levels[-1]
        x0, y0, x1, y1 = h.levels[1]["fp"]
        cx, cy = x1 + 0.2, y0 - 0.2
        R = 1.7
        z0 = h.levels[1]["z0"] - 0.5
        z1 = h.zw + 2.2
        poly = circle_poly(cx, cy, R, 12, a0=math.pi / 12)
        ops = {}
        for e in (8, 10):
            ops[e] = [Opening(arch_poly(0.16, L["z0"] - z0 + 0.6, 0.55, 1.7, 6), depth=0.2, kind="arch")
                      for L in h.levels[1:]]
        frames = poly_body(g, poly, z0, z1, "MT_StoneWhite", ops=ops)
        for e, lst in ops.items():
            fr, _ = frames[e]
            for op in lst:
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.07, sill=False, mullions=False))
        g.lathe([(0.3, z0 - 1.8), (0.8, z0 - 1.2), (R * 0.97, z0 - 0.3), (R * 0.98, z0 + 0.05)], 12, "MT_StoneWhite",
                cx, cy, a0=math.pi / 12, smooth=False)
        g.lathe([(R - 0.3, z1 - 0.35), (R + 0.03, z1 - 0.35), (R + 0.2, z1 - 0.1), (R + 0.2, z1 + 0.08),
                 (R - 0.3, z1 + 0.08)], 12, "MT_StoneWhite", cx, cy, a0=math.pi / 12, smooth=False)
        roof_cone(g, cx, cy, R + 0.2, z1 + 0.08, 5.6, "MT_RoofBlue", ov=0.25, th=0.15, course=0.6, step=0.04,
                  segs=12, under="MT_StoneWhite", finial=dict(mat="MT_Gold", h=1.4, r=0.08))

    arch_ground = dict(W_GROUND, kind="arch", lintel=False, keystone=True, keystone_mat="MT_StoneWhite", win_h=2.2)
    noble_house(10.4, 12.4, [4.3, 3.8, 3.4], roof="hip", pitch=56, ov=0.45, win_styles=[arch_ground, W_NOBILE, W_UPPER],
                door=dict(side="front", at=0.36, w=1.4, h=2.9, kind="arch"),
                window_positions={(0, "front"): [1.4, 6.3, 8.4], (1, "front"): [1.3, 3.5, 5.7, 7.9],
                                  (2, "front"): [1.3, 3.5, 5.7, 7.9]},
                window_counts={(i, sd): n for i in range(3) for sd, n in (("left", 2), ("right", 2), ("back", 3))},
                chimneys=[dict(x=-0.25, y=0.25, pots=2, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_StoneWhite")],
                hooks=[turret]).build(g)


@asset("SM_Noble_House_C", "AsuraNoble", "AsuraNoble", budget=(1000, 8000),
       notes="Symmetrical two-storey noble residence with a projecting central frontispiece crowned by a pediment, "
             "a balcony on columns over the door, hip roof with gold crest.")
def noble_house_c(g, rng):
    def front(h, g):
        L0, L1 = h.levels[0], h.levels[1]
        x0, y0, x1, y1 = L0["fp"]
        fw, pj = 5.2, 0.7
        zt = h.zw + 0.35
        fx0, fx1 = -fw / 2, fw / 2
        fy0, fy1 = y0 - pj, y0 + 0.6
        zb = g.buried(-0.5)
        dop = Opening(arch_poly(fw / 2 - 0.75, h.floor0 - zb, 1.5, 2.9, 8), depth=0.3, back="MT_WoodPlanks", kind="door")
        wop = Opening(arch_poly(fw / 2 - 0.6, L1["z0"] + 0.35 - zb, 1.2, 2.5, 8), depth=0.25, kind="arch")
        frames = block(g, fx0, fy0, fx1, fy1, zb, zt, "MT_StoneWhite", ops={"front": [dop, wop]})
        fr = frames["front"]
        door_trim(g, fr, dop, N_DOOR)
        window_trim(g, fr, wop, dict(trim="MT_StoneWhite", frame_w=0.13, sill=False, keystone=True,
                                     keystone_mat="MT_Gold", mullion_mat="MT_Timber"))
        corner_posts(g, fx0, fy0, fx1, fy1, h.floor0 - 0.08, zt - 0.16, "MT_StoneWhite", s=0.5, p=0.07)
        # pediment on top of the frontispiece
        frs, _ = side_frame(fx0, fy0, fx1, fy1, 0.0, "front")
        cornice(g, fx0, fy0, fx1, fy1, zt - 0.4, 0.36, 0.22, "MT_StoneWhite")
        gable_block(g, frs, -0.25, fw + 0.25, zt - 0.02, 1.7, -3.0, 0.2, "MT_RoofBlue", "MT_StoneWhite")
        o = frs.shifted(dn=0.2)
        g.ring_plate(o, [(-0.32, zt - 0.06), (fw + 0.32, zt - 0.06), (fw / 2, zt + 1.8)],
                     [(0.35, zt + 0.16), (fw - 0.35, zt + 0.16), (fw / 2, zt + 1.4)], -0.06, 0.09, "MT_StoneWhite")
        top = frs.p(fw / 2, zt + 1.8, 0.1)
        finial_spike(g, top.x, top.y, top.z - 0.1, dict(mat="MT_Gold", h=1.0, r=0.07))
        # balcony on two columns over the door
        from arch_parts import balcony
        bz = L1["z0"] + 0.055
        balcony(g, frs, fw / 2 - 1.5, fw / 2 + 1.5, bz, 1.5, floor_mat="MT_StoneWhite", rail_mat="MT_Iron",
                bracket_mat="MT_StoneWhite", bracket_us=[])
        for s in (-1, 1):
            c = frs.p(fw / 2 + s * 1.25, 0, 1.2)
            zc0 = g.buried(-0.3)
            column(g, c.x, c.y, zc0, bz - 0.2 - zc0 + 0.02, 0.16, "MT_StoneWhite", segs=10)
        stair(g, frs.shifted(dn=0.0), fw / 2 - 1.6, fw / 2 + 1.6, h.floor0 - 0.015, "MT_StoneWhite", run_per=0.34)

    noble_house(17.0, 10.0, [4.3, 3.9], roof="hip", pitch=46, ov=0.5, floor0=0.75,
                win_styles=[dict(W_GROUND, win_h=2.1), dict(W_NOBILE, balconette=False, sill=True,
                                                            sill_mat="MT_StoneWhite", win_sill=0.55)],
                door=None, window_positions={(0, "front"): [1.6, 3.9, 13.1, 15.4], (1, "front"): [1.6, 3.9, 13.1,
                                                                                                    15.4]},
                chimneys=[dict(x=-0.33, y=0.2, pots=2, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_StoneWhite"),
                          dict(x=0.33, y=0.2, pots=2, mat="MT_StoneWhite", cap="MT_StoneWhite", pot_mat="MT_StoneWhite")],
                dormers=[dict(u=-0.22, w=1.4), dict(u=0.22, w=1.4)], hooks=[front]).build(g)


# ---------------------------------------------------------------------------------------------- government hall

@asset("SM_Noble_Hall_A", "AsuraNoble", "AsuraNoble", budget=(4000, 15000), foundation=0.7,
       notes="Government hall: stone podium with a grand stair, hexastyle portico with a pediment, two tall storeys "
             "of arched windows, low blue hip roof behind a balustrade and a central dome on a windowed drum with a "
             "gold lantern.")
def noble_hall_a(g, rng):
    W, D = 26.0, 16.0
    podium = 1.5
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    zb = g.buried(-0.6)
    # podium platform (wider than the hall) and grand stair in front
    g.box(x0 - 1.5, y0 - 3.4, zb, x1 + 1.5, y1 + 1.5, podium, "MT_StoneWhite")
    g.box(x0 - 1.62, y0 - 3.52, g.buried(-0.6), x1 + 1.62, y1 + 1.62, 0.45, "MT_StoneWhite")
    fr_st = Frame((x0 - 1.5, y0 - 3.4, 0.0), (1, 0, 0), (0, 0, 1))
    stair(g, fr_st, W / 2 + 1.5 - 5.5, W / 2 + 1.5 + 5.5, podium - 0.012, "MT_StoneWhite", run_per=0.36)
    # hall body with two storeys of arched windows
    H1, H2 = 5.4, 4.8
    zt = podium + H1 + H2
    zb2 = g.buried(-0.6)
    ops = {}
    for side, L in (("front", W), ("back", W), ("left", D), ("right", D)):
        lst = []
        n = int((L - 2.0) / 3.2)
        for k in range(n):
            uc = 1.6 + (L - 3.2) * (k + 0.5) / n
            if side == "front" and abs(uc - W / 2) < 6.5:
                continue
            if side != "front" and k % 2 == 1:
                continue
            lst.append(Opening(arch_poly(uc - 0.75, podium + 1.0 - zb2, 1.5, 3.4, 6), depth=0.35, kind="arch"))
            lst.append(Opening(arch_poly(uc - 0.7, podium + H1 + 0.8 - zb2, 1.4, 3.0, 6), depth=0.35, kind="arch"))
        ops[side] = lst
    dop = Opening(arch_poly(W / 2 - 1.3, podium + 0.02 - zb2, 2.6, 4.2, 10), depth=0.45, back="MT_WoodPlanks",
                  kind="door", jamb_drop=0.05)
    ops["front"].append(dop)
    frames = block(g, x0, y0, x1, y1, zb2, zt, "MT_StoneWhite", ops=ops)
    for side, lst in ops.items():
        fr = frames[side]
        for op in lst:
            if op.kind == "door":
                door_trim(g, fr, op, dict(N_DOOR, door_frame_w=0.3))
            elif side == "front":
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.16, sill_mat="MT_StoneWhite",
                                            keystone=True, keystone_mat="MT_Gold", mullion_mat="MT_Iron"))
            else:
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.16, sill_mat="MT_StoneWhite",
                                            mullion_mat="MT_Iron"))
    corner_posts(g, x0, y0, x1, y1, podium - 0.05, zt - 0.3, "MT_StoneWhite", s=0.8, p=0.1)
    cornice(g, x0, y0, x1, y1, podium + H1 - 0.12, 0.3, 0.14, "MT_StoneWhite")
    cornice(g, x0, y0, x1, y1, zt - 0.5, 0.55, 0.32, "MT_StoneWhite")
    # low hip roof behind a balustrade parapet
    roof_hip(g, x0 + 0.9, x1 - 0.9, y0 + 0.9, y1 - 0.9, zt - 0.1, 24, "MT_RoofBlue", ov=0.0, th=0.2, course=0.4,
             step=0.03, under="MT_StoneWhite", fascia="MT_StoneWhite", hips=True)
    for side in ("front", "back", "left", "right"):
        fr, L = side_frame(x0, y0, x1, y1, 0.0, side)
        from arch_parts import balustrade as bal
        bal(g, fr, 0.7, L - 0.7, zt - 0.03, 1.0, -0.62, 0.02, "MT_StoneWhite", spacing=0.42, posts_every=6, segs=4,
            solid=(side != "front"))
    for (cx, cy) in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        sx = 1 if cx == x0 else -1
        sy = 1 if cy == y0 else -1
        g.box(min(cx - sx * 0.1, cx + sx * 0.9), min(cy - sy * 0.1, cy + sy * 0.9), zt - 0.1,
              max(cx - sx * 0.1, cx + sx * 0.9), max(cy - sy * 0.1, cy + sy * 0.9), zt + 1.12, "MT_StoneWhite")
    # portico
    frf, _ = side_frame(x0, y0, x1, y1, 0.0, "front")
    portico(g, frf, W / 2, 12.4, 3.2, H1 + H2 - 1.4, 6, col_r=0.48, floor_h=podium - 0.03, gold="MT_Gold")
    # dome on a drum over the centre
    drum_dome(g, 0.0, 1.0, zt - 0.3, 4.6, 4.2, "MT_RoofBlue", segs=16, shape="hemi")
    return {"attach": {"podium_height": podium, "main_door_width": 2.6}}


# ---------------------------------------------------------------------------------------------- tower

@asset("SM_Noble_Tower_A", "AsuraNoble", "AsuraNoble", budget=(1500, 8000), foundation=0.66,
       notes="Slender white clock tower: pilastered shaft with string courses, four gilded clock faces, an open "
             "arcaded belfry and an octagonal blue spire with a gold finial.")
def noble_tower_a(g, rng):
    S = 2.7
    zb = g.buried(-0.6)
    shaft_top = 21.0
    ops = {}
    for side in ("front", "right", "back", "left"):
        lst = []
        for z in (4.5, 9.0, 13.5):
            lst.append(Opening(arch_poly(S - 0.35, z - zb, 0.7, 1.7, 6), depth=0.3, kind="arch"))
        if side == "front":
            lst.append(Opening(arch_poly(S - 0.8, 0.3 - zb, 1.6, 3.0, 8), depth=0.4, back="MT_WoodPlanks", kind="door"))
        ops[side] = lst
    frames = block(g, -S, -S, S, S, zb, shaft_top, "MT_StoneWhite", ops=ops)
    for side, lst in ops.items():
        fr = frames[side]
        for op in lst:
            if op.kind == "door":
                door_trim(g, fr, op, N_DOOR)
            else:
                window_trim(g, fr, op, dict(trim="MT_StoneWhite", frame_w=0.1, sill_mat="MT_StoneWhite",
                                            mullions=False))
    fr0, _ = side_frame(-S, -S, S, S, 0.0, "front")
    stair(g, fr0, S - 1.2, S + 1.2, 0.285, "MT_StoneWhite")
    g.box(-S - 0.1, -S - 0.1, g.buried(-0.62), S + 0.1, S + 0.1, 0.24, "MT_StoneWhite")
    corner_posts(g, -S, -S, S, S, 0.2, shaft_top - 0.2, "MT_StoneWhite", s=0.5, p=0.07)
    for z in (7.2, 11.7, 16.2):
        cornice(g, -S, -S, S, S, z, 0.22, 0.1, "MT_StoneWhite")
    cornice(g, -S, -S, S, S, shaft_top - 0.45, 0.5, 0.22, "MT_StoneWhite")
    # clock faces
    for side in ("front", "right", "back", "left"):
        fr, _ = side_frame(-S, -S, S, S, 0.0, side)
        cz = 18.6
        ring_o = [(S + 1.05 * math.cos(math.tau * i / 24), cz + 1.05 * math.sin(math.tau * i / 24)) for i in range(24)]
        ring_i = [(S + 0.9 * math.cos(math.tau * i / 24), cz + 0.9 * math.sin(math.tau * i / 24)) for i in range(24)]
        g.ring_plate(fr, ring_o, ring_i, -0.05, 0.12, "MT_Gold", back=False)
        wplate(g, fr, [(S + 0.95 * math.cos(math.tau * i / 24), cz + 0.95 * math.sin(math.tau * i / 24))
                       for i in range(24)], -0.04, 0.06, "MT_StoneWhite")
        wplate(g, fr, rect(S - 0.04, cz - 0.05, 0.08, 0.75), 0.05, 0.085, "MT_Iron")
        hand = [(S - 0.035, cz - 0.03), (S + 0.03, cz + 0.035), (S + 0.52, cz - 0.28), (S + 0.46, cz - 0.34)]
        wplate(g, fr, hand, 0.045, 0.1, "MT_Iron")
    # belfry: open arcade on four piers
    bz0 = shaft_top
    bh = 4.2
    Sb = S - 0.25
    for (px, py) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        xa, xb = (Sb - 0.9, Sb) if px > 0 else (-Sb, -Sb + 0.9)
        ya, yb = (Sb - 0.9, Sb) if py > 0 else (-Sb, -Sb + 0.9)
        g.box(xa, ya, bz0 - 0.1, xb, yb, bz0 + bh, "MT_StoneWhite")
    for side in ("front", "right", "back", "left"):
        fr, L = side_frame(-Sb, -Sb, Sb, Sb, 0.0, side)
        g.plate(fr, arch_band(0.85, L - 0.85, bz0 + bh - 1.5, bz0 + bh + 0.05), -0.45, 0.02, "MT_StoneWhite")
    g.box(-Sb - 0.3, -Sb - 0.3, bz0 + bh - 0.05, Sb + 0.3, Sb + 0.3, bz0 + bh + 0.45, "MT_StoneWhite")
    # bell
    g.lathe([(0.0, bz0 + 1.3), (0.62, bz0 + 1.32), (0.5, bz0 + 1.8), (0.42, bz0 + 2.6), (0.0, bz0 + 2.75)], 12,
            "MT_Gold", 0.0, 0.0)
    g.box(-0.08, -0.08, bz0 + 2.7, 0.08, 0.08, bz0 + bh, "MT_Timber")
    # octagonal spire
    oz = bz0 + bh + 0.45
    g.lathe([(Sb + 0.25, oz - 0.1), (Sb + 0.25, oz + 0.35), (0.0, oz + 10.5)], 8, "MT_RoofBlue", a0=math.pi / 8,
            smooth=False)
    finial_spike(g, 0.0, 0.0, oz + 10.3, dict(mat="MT_Gold", h=2.2, r=0.1))
    for (px, py) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        finial_spike(g, px * (Sb + 0.1), py * (Sb + 0.1), oz - 0.05, dict(mat="MT_Gold", h=1.2, r=0.07))


def arch_band(u0, u1, v_spring_top, v_top, segs=10):
    """Solid spandrel strip above an arched opening spanning u0..u1: the region between the arch and v_top."""
    w = u1 - u0
    r = w / 2
    vs = v_spring_top - r * 0.0
    pts = [(u0, vs), (u0, v_top), (u1, v_top), (u1, vs)]
    arc = [(u0 + r + r * math.cos(math.pi * i / segs), vs - r * 0.0 + r * math.sin(math.pi * i / segs) * 0.9)
           for i in range(1, segs)]
    pts = [(u1, vs)] + arc + [(u0, vs), (u0, v_top), (u1, v_top)]
    return pts
