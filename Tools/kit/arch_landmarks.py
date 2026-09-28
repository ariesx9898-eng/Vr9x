"""Landmarks: the Silver Palace (Ars), Ranoa University (Sharia), Millis Cathedral, Roa Keep, Rikarisu Hall,
Rapan Guild and a Labyrinth Gate. Budgets up to 40k triangles; fronts face -Y; origin at the base centre."""
import math

from mathutils import Vector

from arch_geo import Frame
from arch_house import House
from arch_parts import (Opening, arcade_outline, arch_poly, balustrade, block, buttress, circle_poly, cornice,
                        corner_posts, door_trim, dome, drum_dome, finial_spike, frame_ring, parapet_ring, poly_body,
                        portico, rect, roof_cone, roof_gable, roof_hip, rose_tracery, side_frame, stair, window_trim,
                        wplate)
from arch_registry import asset

SILVER = "MT_RoofSilver"
WHITE = "MT_StoneWhite"


def _spire_turret(g, cx, cy, z0, r, h_body, h_spire, mat=WHITE, roof=SILVER, segs=8, gold=True):
    """Slender round turret with a tall conical spire."""
    g.cylinder(cx, cy, r, z0, z0 + h_body, segs, mat, smooth=False)
    g.lathe([(r - 0.2, z0 + h_body - 0.3), (r + 0.05, z0 + h_body - 0.3), (r + 0.25, z0 + h_body - 0.05),
             (r + 0.25, z0 + h_body + 0.15), (r - 0.2, z0 + h_body + 0.15)], segs, mat, cx, cy, smooth=False)
    roof_cone(g, cx, cy, r + 0.25, z0 + h_body + 0.15, h_spire, roof, ov=0.2, th=0.15, course=max(0.8, h_spire / 5),
              step=0.04, segs=segs, under=mat, finial=dict(mat="MT_Gold" if gold else "MT_Iron", h=1.2 + r * 0.3,
                                                          r=0.06 + r * 0.02), a0=math.pi / segs)


def _tower(g, cx, cy, r, z0, h, roof_h, mat=WHITE, roof=SILVER, segs=16, windows=3, crown=True, gold=True):
    """Round tower with windows on the outward half, a corbelled cornice and a tall conical roof."""
    zb = z0
    poly = circle_poly(cx, cy, r, segs, a0=math.pi / segs)
    Lf = 2 * r * math.sin(math.pi / segs)
    ops = {}
    for e in range(0, segs, 2):
        lst = []
        for k in range(windows):
            zw = 6.0 + k * (h - 10.0) / max(windows - 1, 1)
            lst.append(Opening(arch_poly(Lf / 2 - 0.55, zw, 1.1, 2.4, 6), depth=0.4, kind="arch"))
        ops[e] = lst
    frames = poly_body(g, poly, zb, z0 + h, mat, ops=ops)
    for e, lst in ops.items():
        fr, _ = frames[e]
        for op in lst:
            window_trim(g, fr, op, dict(trim=mat, frame_w=0.14, sill=False, mullions=False, shutters=False))
    if crown:
        g.lathe([(r - 0.8, z0 + h - 1.4), (r + 0.02, z0 + h - 1.4), (r + 0.6, z0 + h - 0.3), (r + 0.6, z0 + h + 0.2),
                 (r - 0.8, z0 + h + 0.2)], segs, mat, cx, cy, a0=math.pi / segs, smooth=False)
    return roof_cone(g, cx, cy, r + 0.6, z0 + h + 0.2, roof_h, roof, ov=0.5, th=0.25, course=roof_h / 8, step=0.08,
                     segs=segs, under=mat, finial=dict(mat="MT_Gold" if gold else "MT_Iron", h=2.4, r=0.14),
                     a0=math.pi / segs)


# ---------------------------------------------------------------------------------------------- Silver Palace

@asset("SM_Landmark_SilverPalace", "Landmarks", "AsuraNoble", kind="landmark", foundation=1.6, budget=(5000, 40000),
       notes="The Silver Palace of Ars: white-stone palace on a terrace with a grand stair (front, -Y), 82 m main "
             "block and two forward wings around a court of honour, four corner towers and a central tower with "
             "silver spires (~62 m), roofline turrets and a giant columned portico.")
def silver_palace(g, rng):
    P = 4.0                      # terrace height
    # terrace podium and its cornice
    zb = g.buried(-1.5)
    g.box(-58.0, -44.0, zb, 58.0, 36.0, P, WHITE)
    cornice(g, -58.0, -44.0, 58.0, 36.0, P - 0.45, 0.5, 0.3, WHITE)
    g.box(-58.12, -44.12, g.buried(-1.5), 58.12, 36.12, 0.6, WHITE)
    # grand stair in front (36 m wide), with side cheek blocks
    frs = Frame((-18.0, -44.0, 0.0), (1, 0, 0), (0, 0, 1))
    stair(g, frs, 0.0, 36.0, P - 0.012, WHITE, run_per=0.42, embed=0.3)
    for sx in (-1, 1):
        g.box(sx * 18.0 - 1.2 if sx > 0 else -19.2, -54.6, g.buried(-1.5), sx * 18.0 + 1.2 if sx > 0 else -16.8,
              -43.8, 1.4, WHITE)
        g.box(sx * 18.0 - 1.15 if sx > 0 else -19.15, -48.41, 1.2, sx * 18.0 + 1.15 if sx > 0 else -16.85, -43.75,
              P + 1.0, WHITE)
        finial_spike(g, sx * 18.0, -46.0, P + 0.95, dict(mat="MT_Gold", h=1.8, r=0.12))
    # front terrace balustrade either side of the stair
    frt = Frame((-58.0, -44.0, 0.0), (1, 0, 0), (0, 0, 1))
    balustrade(g, frt, 0.8, 38.6, P - 0.03, 1.1, -0.6, 0.05, WHITE, spacing=0.9, posts_every=6, segs=4)
    balustrade(g, frt, 77.4, 115.2, P - 0.03, 1.1, -0.6, 0.05, WHITE, spacing=0.9, posts_every=6, segs=4)
    # court of honour paving
    g.box(-22.7, -43.0, P - 0.05, 22.7, 0.5, P + 0.05, "MT_Cobble")
    # palace blocks (House: storeys on the terrace)
    win = dict(trim=WHITE, frame_w=0.18, shutters=False, sill_mat=WHITE, win_w=1.5, win_h=3.2, win_sill=1.2,
               mullions=False, lintel=True, lintel_mat=WHITE)
    win_up = dict(win, win_h=2.8, win_sill=1.0, pediment="tri", lintel=False)
    common = dict(walls=[WHITE], timber=[False], jetty=[(-0.04, -0.04, -0.04)] * 3, cornices=WHITE, pilasters=WHITE,
                  plinth=WHITE, floor0=P + 0.3, foundation=-(P - 0.5), door_steps=False, roof_mat=SILVER,
                  roof_under=WHITE, fascia=WHITE, verge=WHITE, course=0.9, step=0.06, crest="MT_Gold", crest_spacing=2.4,
                  hip_finial=dict(mat="MT_Gold", h=2.0, r=0.12), side_win_styles=[win], step_mat=WHITE)
    with g.xf(loc=(0.0, 12.0, 0.0)):
        House(82.0, 24.0, [7.0, 6.2, 6.0], roof="mansard", pitch=70, pitch2=24, mansard_h=4.0, ov=0.5,
              win_styles=[win, win_up, dict(win_up, pediment="seg")], win_spacing=4.6, door=None,
              window_positions={(0, "front"): [2.5 + 4.6 * k for k in range(17) if abs(2.5 + 4.6 * k - 41) > 8]},
              extra_openings={(0, "front"): [Opening(arch_poly(41.0 - 1.8, 0.0, 3.6, 6.0, 10), depth=0.6,
                                                     back="MT_WoodPlanks", kind="door")]},
              chimneys=[dict(x=sx * 0.3, y=0.0, pots=3, w=1.4, d=1.0, mat=WHITE, cap=WHITE, pot_mat=WHITE)
                        for sx in (-1, 1)], **common).build(g)
    for sx in (-1, 1):
        with g.xf(loc=(sx * 31.85, -16.0, 0.0)):
            House(18.0, 44.0, [6.5, 5.8, 5.6], roof="hip", pitch=50, ov=0.5, floor0=P + 0.36,
                  win_styles=[win, dict(win_up, win_h=2.6), dict(win_up, pediment="seg", win_h=2.4)],
                  win_spacing=5.2, door=None, **{k: v for k, v in common.items() if k != "floor0"}).build(g)
    # giant portico on the main block
    frm, _ = side_frame(-41.0, 0.0, 41.0, 24.0, 0.0, "front")
    top_pediment = portico(g, frm, 41.0, 17.0, 4.2, 13.0, 6, col_r=0.75, floor_h=P + 0.25, gold="MT_Gold")
    # central tower rising through the main block
    tx, ty = 0.0, 14.0
    S = 8.0
    tz0 = g.buried(P - 0.3)
    tz1 = 44.0
    tops = {}
    for side in ("front", "right", "back", "left"):
        tops[side] = [Opening(arch_poly(S - 1.1, z - tz0, 2.2, 4.6, 10), depth=0.6, kind="arch")
                      for z in (33.0, 38.5)]
    tfr = block(g, tx - S, ty - S, tx + S, ty + S, tz0, tz1, WHITE, ops=tops)
    for side, lst in tops.items():
        for op in lst:
            window_trim(g, tfr[side], op, dict(trim=WHITE, frame_w=0.25, sill_mat=WHITE, mullions=False,
                                                 keystone=True, keystone_mat="MT_Gold"))
    corner_posts(g, tx - S, ty - S, tx + S, ty + S, 24.0, tz1 - 0.6, WHITE, s=1.1, p=0.12)
    cornice(g, tx - S, ty - S, tx + S, ty + S, tz1 - 0.8, 0.9, 0.45, WHITE)
    g.lathe([(S + 0.6, tz1 - 0.1), (S + 0.6, tz1 + 0.6), (0.0, tz1 + 20.0)], 8, SILVER, tx, ty, a0=math.pi / 8,
            smooth=False)
    finial_spike(g, tx, ty, tz1 + 19.6, dict(mat="MT_Gold", h=3.2, r=0.2))
    for (px, py) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        _spire_turret(g, tx + px * (S - 0.2), ty + py * (S - 0.2), tz1 - 0.5, 1.3, 3.2, 7.5)
    # four corner towers
    for (cx, cy) in ((-41.0, -38.0), (41.0, -38.0), (-41.0, 24.0), (41.0, 24.0)):
        _tower(g, cx, cy, 6.5, g.buried(P - 0.4), 32.0, 22.0, windows=2)
    # roofline turrets on the wings' front ends and the main block
    for sx in (-1, 1):
        for (x, y) in ((sx * 23.6, -37.4), (sx * 22.0, -1.33)):
            _spire_turret(g, x, y, 21.0, 1.25, 6.0, 8.5)
        for x in (sx * 14.0, sx * 27.0):
            _spire_turret(g, x, 0.3, 22.35, 1.1, 5.6, 7.0)
    return {"attach": {"terrace_height": P, "grand_stair_width": 36.0}}


# ---------------------------------------------------------------------------------------------- Ranoa University

GREEN = "MT_RoofGreen"
STONE = "MT_Stone"


def _crystal_cluster(g, x, y, z, s=1.0):
    from arch_demon import crystal
    crystal(g, x, y, z, 2.6 * s, 0.34 * s)
    for (a, t, h) in ((20, 18, 1.4), (140, 22, 1.1), (260, 20, 1.6)):
        ax, ay = math.cos(math.radians(a)), math.sin(math.radians(a))
        crystal(g, x + ax * 0.45 * s, y + ay * 0.45 * s, z, h * s, 0.2 * s, tilt=(-t * ay, t * ax))


@asset("SM_Landmark_RanoaUniversity", "Landmarks", "North", kind="landmark", foundation=1.1, budget=(5000, 40000),
       notes="Ranoa University of Magic (Sharia): a 3-storey stone quadrangle (~146 x 116 m) around a courtyard, a "
             "gate tower with an open arched passage on the front (-Y), four corner towers with green copper "
             "domes, and the great domed hall (green dome, ~52 m) with a glowing crystal lantern; crystal clusters on "
             "the towers.")
def ranoa_university(g, rng):
    uwin = dict(trim=STONE, frame=False, frame_w=0.16, shutters=False, sill_mat=STONE, win_w=1.3, win_h=2.3,
                win_sill=1.0, mullions=False, lintel=True, lintel_mat=STONE)
    common = dict(walls=[STONE], timber=[False], jetty=[(-0.04, -0.04, -0.04)] * 3, cornices=STONE, plinth=STONE,
                  floor0=0.4, foundation=0.6, door=None, roof_mat=GREEN, roof_under="MT_WoodPlanks", fascia=STONE,
                  verge=STONE, ridge_mat=GREEN, course=0.9, step=0.06, pitch=45, ov=0.5, ovv=0.3,
                  win_styles=[uwin], side_win_styles=[uwin], gable_windows=False, door_steps=False)
    # front range split by the gate tower, back range, side ranges (inset so outer faces never coincide)
    c2 = {k: v for k, v in common.items() if k not in ("floor0", "ov")}
    no_ends = lambda sides: {(i, sd): 0 for i in range(3) for sd in sides}
    for sx in (-1, 1):
        with g.xf(loc=(sx * 40.0, -47.0, 0.0)):
            House(63.0, 18.0, [6.0, 5.2, 5.0], ridge="x", win_spacing=7.6, floor0=0.4, ov=0.5,
                  window_counts=no_ends(("left", "right")), **c2).build(g)
    with g.xf(loc=(0.0, 47.0, 0.0)):
        House(142.0, 18.0, [6.02, 5.2, 5.0], ridge="x", win_spacing=7.6, floor0=0.41, ov=0.5,
              window_counts=no_ends(("left", "right")), **c2).build(g)
    for sx in (-1, 1):
        with g.xf(loc=(sx * 61.8, 0.0, 0.0)):
            House(18.0, 76.4, [6.04, 5.2, 5.0], ridge="y", win_spacing=5.8, floor0=0.42, ov=0.45,
                  window_counts=no_ends(("front", "back")), **c2).build(g)
    # courtyard paving
    g.box(-52.5, -37.8, -0.05, 52.5, 37.8, 0.08, "MT_Cobble")
    # gate tower with an open arched passage through the front range
    gx, gy0, gy1 = 9.0, -57.4, -36.4
    zb = g.buried(-0.6)
    gt = 30.0
    outline = arcade_outline(2 * gx, gt - zb, [(gx - 3.5, 7.0, 8.5 - zb)], segs=12)
    g.plate(Frame((-gx, gy0, zb), (1, 0, 0), (0, 0, 1)), outline, -(gy1 - gy0), 0.0, STONE)
    for z in (14.0, 22.0):
        cornice(g, -gx, gy0, gx, gy1, z, 0.35, 0.2, STONE)
    cornice(g, -gx, gy0, gx, gy1, gt - 0.6, 0.65, 0.35, STONE)
    frg = Frame((-gx, gy0, 0.0), (1, 0, 0), (0, 0, 1))
    top = roof_hip(g, -gx, gx, gy0, gy1, gt, 58, GREEN, ov=0.6, th=0.3, course=1.1, step=0.08,
                   under="MT_WoodPlanks", fascia=STONE, hips=True, finial=None)
    _crystal_cluster(g, 0.0, (gy0 + gy1) / 2, top.z_ridge + 0.1, 1.4)
    from arch_parts import crystal_lantern
    for s in (-1, 1):
        crystal_lantern(g, frg, gx + s * 4.6, 5.2, out=0.6, h=0.9)
    # corner towers with green pointed domes and crystals
    for (cx, cy) in ((-66.5, -51.0), (66.5, -51.0), (-66.5, 51.0), (66.5, 51.0)):
        S = 7.0
        tz = 34.0
        tzb = g.buried(-0.6)
        tops = {s: [Opening(arch_poly(S - 0.9, z - tzb, 1.8, 3.6, 8), depth=0.5, kind="arch") for z in (20.0, 27.0)]
                for s in ("front", "right", "back", "left")}
        tfr = block(g, cx - S, cy - S, cx + S, cy + S, tzb, tz, STONE, ops=tops)
        for s, lst in tops.items():
            for op in lst:
                window_trim(g, tfr[s], op, dict(trim=STONE, frame_w=0.2, sill_mat=STONE, mullions=False))
        corner_posts(g, cx - S, cy - S, cx + S, cy + S, 0.2, tz - 0.6, STONE, s=0.9, p=0.12)
        cornice(g, cx - S, cy - S, cx + S, cy + S, tz - 0.7, 0.8, 0.4, STONE)
        g.cylinder(cx, cy, S - 1.3, tz - 0.2, tz + 3.0, 16, STONE, smooth=False)
        dt = dome(g, cx, cy, S - 1.1, tz + 3.0, GREEN, segs=20, rings=8, shape="pointed")
        _crystal_cluster(g, cx, cy, dt - 0.3, 0.9)
    # great domed hall projecting into the courtyard from the back range
    hx0, hx1, hy0, hy1 = -24.0, 24.0, 12.0, 44.0
    hzb = g.buried(-0.6)
    hH = 20.0
    ops = {"front": [Opening(arch_poly(24.0 - 2.5, 0.45 - hzb, 5.0, 8.0, 12), depth=0.8, back="MT_WoodPlanks",
                             kind="door")]}
    for s, L in (("front", 48.0), ("left", 32.0), ("right", 32.0)):
        lst = ops.setdefault(s, [])
        n = int(L / 6.0)
        for k in range(n):
            uc = 3.0 + (L - 6.0) * (k + 0.5) / n
            if s == "front" and abs(uc - 24.0) < 5.0:
                continue
            lst.append(Opening(arch_poly(uc - 1.1, 6.0 - hzb, 2.2, 9.0, 10), depth=0.6, kind="arch"))
    hfr = block(g, hx0, hy0, hx1, hy1, hzb, hH, STONE, ops=ops)
    for s, lst in ops.items():
        for op in lst:
            if op.kind == "door":
                door_trim(g, hfr[s], op, dict(door_frame_mat=STONE, door_frame_w=0.4, straps=True, battens=True))
            else:
                window_trim(g, hfr[s], op, dict(trim=STONE, frame_w=0.25, sill_mat=STONE, mullion_mat="MT_Iron",
                                                keystone=True, keystone_mat="MT_Crystal"))
    corner_posts(g, hx0, hy0, hx1, hy1, 0.3, hH - 0.8, STONE, s=1.2, p=0.15)
    cornice(g, hx0, hy0, hx1, hy1, hH - 0.9, 1.0, 0.5, STONE)
    frh, _ = side_frame(hx0, hy0, hx1, hy1, 0.0, "front")
    stair(g, frh, 24.0 - 5.0, 24.0 + 5.0, 0.435, STONE)
    g.box(hx0 + 1.0, hy0 + 1.0, hH - 0.3, hx1 - 1.0, hy1 - 1.0, hH + 1.2, STONE)
    top = drum_dome(g, 0.0, 28.0, hH + 1.0, 13.0, 8.0, GREEN, wall_mat=STONE, segs=20, shape="pointed",
                    lantern_mat="MT_Crystal")
    _crystal_cluster(g, 0.0, 28.0, top - 0.2, 1.8)
    return {"attach": {"gate_passage_width": 7.0, "gate_passage_height": 8.5}}


# ---------------------------------------------------------------------------------------------- Millis Cathedral

BLUE = "MT_RoofBlue"


@asset("SM_Landmark_MillisCathedral", "Landmarks", "Millis", kind="landmark", foundation=0.7, budget=(5000, 40000),
       notes="Millis cathedral (~84 x 52 m): twin west towers with spires (~62 m) framing a rose window and a triple "
             "portal on the -Y front, buttressed nave with aisles, transept, crossing lantern, apse and flying "
             "buttresses; white stone, blue roofs.")
def millis_cathedral(g, rng):
    nx = 11.0            # nave half width
    ny0, ny1 = -30.0, 34.0
    NH = 26.0
    ax = 19.0            # aisle outer line
    AH = 13.0
    zb = g.buried(-0.6)
    # nave block with gable ends front/back; west front openings: rose window + portal
    rose_r = 5.2
    from arch_house import gable_heights
    ze, zr, ta, ca = gable_heights(NH, 2 * nx, 55, 0.35)
    fops = [Opening(arch_poly(nx - 2.6, 0.4 - zb, 5.2, 10.5, 12, pointed=True), depth=1.2, back="MT_WoodPlanks",
                    kind="door"),
            Opening(circle_poly(nx, 19.0 - zb, rose_r, 32), depth=0.6, kind="rose")]
    side_ops = []
    for k in range(6):
        uc = 6.0 + (ny1 - ny0 - 12.0) * (k + 0.5) / 6
        if 36.0 < uc < 52.0:
            continue
        side_ops.append(Opening(arch_poly(uc - 1.4, AH + 2.0 - zb, 2.8, 8.0, 10, pointed=True), depth=0.5,
                                kind="arch"))
    nfr = block(g, -nx, ny0, nx, ny1, zb, NH, "MT_StoneWhite", ops={"front": fops, "left": list(side_ops),
                                                                  "right": list(side_ops)},
                gable=("y", ze, zr))
    for op in fops:
        if op.kind == "door":
            door_trim(g, nfr["front"], op, dict(door_frame_mat=WHITE, door_frame_w=0.6, straps=True, battens=True))
        else:
            frame_ring(g, nfr["front"], op, width=0.6, mat=WHITE)
            rose_tracery(g, nfr["front"], nx, 19.0 - zb, rose_r, 0.6)
    for s in ("left", "right"):
        for op in side_ops:
            window_trim(g, nfr[s], op, dict(trim=WHITE, frame_w=0.25, sill_mat=WHITE, mullion_mat=WHITE))
    roof_gable(g, -nx, nx, ny0, ny1, NH, 55, BLUE, ov=0.6, ovv=0.5, th=0.35, course=0.8, step=0.06,
               under="MT_WoodPlanks", fascia=WHITE, verge=WHITE, axis="y", ridge_mat=BLUE, ridge_w=0.3)
    # aisles with lean-to roofs and tall pointed windows between buttresses
    from arch_parts import roof_shed, shed_body
    for sx in (-1, 1):
        a0, a1 = (nx - 0.3, ax) if sx > 0 else (-ax, -nx + 0.3)
        aops = []
        L = ny1 - ny0 - 1.0
        for k in range(7):
            uc = 4.0 + (L - 8.0) * (k + 0.5) / 7
            aops.append(Opening(arch_poly(uc - 1.1, 2.0 - zb, 2.2, 7.5, 8, pointed=True), depth=0.45, kind="arch"))
        side = "right" if sx > 0 else "left"
        with g.xf(loc=((a0 + a1) / 2, (ny0 + 0.5 + ny1) / 2, 0.0), rotz=(90 if sx > 0 else -90)):
            hw = (ny1 - ny0 - 1.0) / 2
            hd = (a1 - a0) / 2
            zu, tv = roof_shed(g, -hw, hw, -hd, hd, AH, AH + 5.5, BLUE, ov=0.5, ovv=0.3, th=0.25, course=0.7,
                               step=0.05, under="MT_WoodPlanks", fascia=WHITE, ov_back=0.6)
            frs = shed_body(g, -hw, -hd, hw, hd, g.buried(-0.6), AH + tv / 2, AH + 5.5 + tv / 2, WHITE,
                            ops={"front": aops})
            for op in aops:
                window_trim(g, frs["front"], op, dict(trim=WHITE, frame_w=0.2, sill_mat=WHITE, mullion_mat=WHITE))
            # buttresses between the windows with pinnacles and flying buttresses to the nave
            fr, _ = frs["front"], None
            for k in range(8):
                u = 4.0 + (2 * hw - 8.0) * k / 7
                buttress(g, frs["front"], u - 0.0, g.buried(-0.3) - frs["front"].o.z, AH + 1.0 - frs["front"].o.z,
                         2.2, width=1.1, mat=WHITE)
                p = frs["front"].p(u, 0, 0.9)
                g.cylinder(p.x, p.y, 0.45, AH + 0.5, AH + 3.2, 6, WHITE, smooth=False)
                roof_cone(g, p.x, p.y, 0.5, AH + 3.2, 2.6, WHITE, ov=0.05, th=0.08, course=1.3, step=0.03, segs=6,
                          under=WHITE, finial=dict(mat="MT_Iron", h=0.6, r=0.04))
                q0 = frs["front"].p(u, AH + 1.4 - frs["front"].o.z, 0.6)
                q1 = frs["front"].p(u, NH - 3.0 - frs["front"].o.z, -(a1 - a0) - 0.3)
                g.beam(q0, q1, 0.7, 0.9, WHITE, up=(0, 0, 1))
    # transept
    tx0, tx1, ty0, ty1 = -25.0, 25.0, 8.0, 22.0
    ze2, zr2, _, _ = gable_heights(NH - 1.0, ty1 - ty0, 55, 0.3)
    tops = {"left": [Opening(circle_poly(7.0, 17.0 - zb, 3.0, 24), depth=0.5, kind="rose")],
            "right": [Opening(circle_poly(7.0, 17.0 - zb, 3.0, 24), depth=0.5, kind="rose")]}
    tfr = block(g, tx0, ty0, tx1, ty1, g.buried(-0.6), NH - 1.0, WHITE, ops=tops, gable=("x", ze2, zr2))
    for s, lst in tops.items():
        for op in lst:
            frame_ring(g, tfr[s], op, width=0.4, mat=WHITE)
            rose_tracery(g, tfr[s], 7.0, 17.0 - zb, 3.0, 0.5)
    roof_gable(g, tx0, tx1, ty0, ty1, NH - 1.0, 55, BLUE, ov=0.5, ovv=0.45, th=0.3, course=0.8, step=0.06,
               under="MT_WoodPlanks", fascia=WHITE, verge=WHITE, axis="x", ridge_mat=BLUE, ridge_w=0.28)
    # apse (half 12-gon) with a conical roof
    ar = 11.0
    angs = [-15 + 30 * k for k in range(8)]
    apoly = [(ar * math.cos(math.radians(a)), ny1 - 2.0 + ar * math.sin(math.radians(a))) for a in angs]
    azb = g.buried(-0.6)
    ael = 2 * ar * math.sin(math.radians(15))
    aops = {e: [Opening(arch_poly(ael / 2 - 0.9, 5.0 - azb, 1.8, 9.0, 8, pointed=True), depth=0.45, kind="arch")]
            for e in range(1, 6)}
    afr = poly_body(g, apoly, azb, NH - 4.0, WHITE, ops=aops)
    for e, lst in aops.items():
        for op in lst:
            window_trim(g, afr[e][0], op, dict(trim=WHITE, frame_w=0.2, sill_mat=WHITE, mullion_mat=WHITE))
    roof_cone(g, 0.0, ny1 - 2.0, ar + 0.1, NH - 4.0, 9.5, BLUE, ov=0.5, th=0.3, course=0.9, step=0.06, segs=12,
              under="MT_WoodPlanks", a0=math.radians(-15))
    # crossing lantern tower
    cs = 6.2
    cz = zr + 1.2
    g.box(-cs, 15.0 - cs, NH - 2.0, cs, 15.0 + cs, cz + 3.5, WHITE)
    cornice(g, -cs, 15.0 - cs, cs, 15.0 + cs, cz + 2.9, 0.7, 0.35, WHITE)
    g.lathe([(cs * 1.1, cz + 3.4), (cs * 1.1, cz + 4.0), (0.0, cz + 12.0)], 8, BLUE, 0.0, 15.0, a0=math.pi / 8,
            smooth=False)
    finial_spike(g, 0.0, 15.0, cz + 11.7, dict(mat="MT_Gold", h=2.0, r=0.12))
    # twin west towers with spires
    for sx in (-1, 1):
        cx, cy = sx * 15.5, ny0 - 5.0
        S = 6.5
        tzb = g.buried(-0.6)
        TH = 44.0
        tops = {}
        for s in ("front", "right", "back", "left"):
            lst = [Opening(arch_poly(S - 1.2, z - tzb, 2.4, 7.5, 10, pointed=True), depth=0.6, kind="arch")
                   for z in (31.0,)]
            if s == "front":
                lst.append(Opening(arch_poly(S - 1.5, 0.4 - tzb, 3.0, 7.0, 10, pointed=True), depth=1.0,
                                   back="MT_WoodPlanks", kind="door"))
                lst.append(Opening(arch_poly(S - 1.0, 12.0 - tzb, 2.0, 6.0, 8, pointed=True), depth=0.5, kind="arch"))
            tops[s] = lst
        tfr = block(g, cx - S, cy - S, cx + S, cy + S, tzb, TH, WHITE, ops=tops)
        for s, lst in tops.items():
            for op in lst:
                if op.kind == "door":
                    door_trim(g, tfr[s], op, dict(door_frame_mat=WHITE, door_frame_w=0.4, straps=True, battens=True))
                else:
                    window_trim(g, tfr[s], op, dict(trim=WHITE, frame_w=0.25, sill_mat=WHITE, mullion_mat=WHITE))
        corner_posts(g, cx - S, cy - S, cx + S, cy + S, 0.3, TH - 0.8, WHITE, s=1.4, p=0.2)
        for z in (10.0, 23.0):
            cornice(g, cx - S, cy - S, cx + S, cy + S, z, 0.5, 0.25, WHITE)
        cornice(g, cx - S, cy - S, cx + S, cy + S, TH - 1.0, 0.95, 0.45, WHITE)
        g.lathe([(S + 0.6, TH - 0.1), (S + 0.6, TH + 0.5), (0.0, TH + 18.0)], 8, BLUE, cx, cy, a0=math.pi / 8,
                smooth=False)
        finial_spike(g, cx, cy, TH + 17.7, dict(mat="MT_Gold", h=2.4, r=0.13))
        for (px, py) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            _spire_turret(g, cx + px * (S - 0.4), cy + py * (S - 0.4), TH - 0.6, 0.8, 2.5, 5.0, roof=BLUE,
                          gold=False)
        frt, _ = side_frame(cx - S, cy - S, cx + S, cy + S, 0.0, "front")
        stair(g, frt, S - 2.2, S + 2.2, 0.385, WHITE)
    frn, _ = side_frame(-nx, ny0, nx, ny1, 0.0, "front")
    stair(g, frn, nx - 4.0, nx + 4.0, 0.385, WHITE)
    return {"attach": {"main_door_width": 5.2}}


# ---------------------------------------------------------------------------------------------- Roa Keep

@asset("SM_Landmark_RoaKeep", "Landmarks", "Asura", kind="landmark", foundation=1.3, budget=(5000, 40000),
       notes="Castle of Roa: crenellated curtain wall (~66 x 54 m) with four round corner towers (red conical roofs), "
             "a twin-towered gatehouse with an open 5 m passage on the -Y side, the great keep at the back and a "
             "red-roofed great hall inside the bailey.")
def roa_keep(g, rng):
    from arch_walls import STYLES, build_gate, build_wall, tower_body, tower_crown
    import arch_asura
    S = dict(STYLES["Asura"])
    W2, D2 = 32.0, 26.0             # half extents of the wall axis rectangle
    # curtain walls: front split by the gate (the gate spans x = -6..6 with towers centred on +/-6)
    with g.xf(loc=(0.0, -D2, 0.0)):
        build_wall(g, S, -W2, -6.0)
        build_wall(g, S, 6.0, W2)
        build_gate(g, S)
    with g.xf(loc=(0.0, D2, 0.0), rotz=180):
        build_wall(g, S, -W2, W2)
    for sx in (-1, 1):
        with g.xf(loc=(sx * W2, 0.0, 0.0), rotz=(-90 if sx < 0 else 90)):
            # ends stop inside the corner towers so the walls never overlap each other
            build_wall(g, S, -D2 + 2.9, D2 - 2.9)
    for (cx, cy) in ((-W2, -D2), (W2, -D2), (W2, D2), (-W2, D2)):
        with g.xf(loc=(cx, cy, 0.0)):
            poly, Ht = tower_body(g, S, doors=False, height_extra=6.5)
            tower_crown(g, S, 0.0, 0.0, Ht, poly)
    # the great keep (the Asura keep, a little larger) at the back of the bailey
    with g.xf(loc=(0.0, 9.5, 0.0), scale=1.12):
        arch_asura.keep_a(g, rng)
    # great hall along the west (left) side of the bailey, door facing +X
    hall_win = dict(trim="MT_Stone", frame_w=0.14, shutters=False, sill_mat="MT_Stone", win_w=1.2, win_h=2.6,
                    win_sill=1.2, kind="arch", mullions=True)
    with g.xf(loc=(-20.5, -6.0, 0.0), rotz=90):
        House(22.0, 9.0, [6.5], walls=["MT_Stone"], timber=[False], ridge="x", pitch=52, roof_mat="MT_RoofRed",
              ov=0.5, ovv=0.35, floor0=0.5, win_styles=[hall_win], door=dict(side="front", at=0.5, w=1.8, h=3.2,
                                                                              kind="arch"),
              door_style=dict(door_frame_mat="MT_Stone", door_frame_w=0.2, straps=True, battens=True),
              chimneys=[dict(x=0.35, y=0.25, pots=2, w=1.0, d=0.9)], win_spacing=3.4, back_windows=False,
              window_counts={(0, "left"): 0, (0, "right"): 0}).build(g)
    # bailey paving
    g.box(-W2 + 1.8, -D2 + 1.8, -0.06, W2 - 1.8, D2 - 1.8, 0.06, "MT_Cobble")
    return {"attach": {"gate_opening_width": 5.0, "bailey": [2 * W2 - 3.6, 2 * D2 - 3.6]}}


# ---------------------------------------------------------------------------------------------- Rikarisu Hall

@asset("SM_Landmark_RikarisuHall", "Landmarks", "Demon", kind="landmark", foundation=0.9, budget=(5000, 40000),
       notes="Rikarisu great hall / arena carved into a demon-rock outcrop (~64 x 52 m, ~24 m): a lumpy oval rock "
             "ring with stepped seating tiers around a sunken arena floor, a monumental carved portal on the -Y side "
             "with pillars, giant bone tusks and crystal braziers, and crystal clusters around the rim.")
def rikarisu_hall(g, rng):
    from arch_demon import ROCK, chamfer_rect, crystal, tusk
    segs = 56
    R0 = 29.0
    sy = 0.8
    zb = g.buried(-0.8)
    prof = [(R0 + 2.5, zb), (R0 + 1.6, 3.0), (R0 + 0.6, 9.0), (R0 - 0.8, 14.5), (R0 - 2.6, 17.8), (R0 - 4.5, 18.6),
            (R0 - 6.0, 18.0)]
    r, z = R0 - 6.0, 18.0
    for k in range(8):
        prof.append((r, z - 1.1))
        r -= 1.3
        prof.append((r, z - 1.1))
        z -= 1.1
    prof.append((r, 1.1))
    prof.append((0.0, 1.1))
    harm = [(rng.randint(3, 7), rng.uniform(0, math.tau), rng.uniform(0.4, 1.0)) for _ in range(4)]
    rings = []
    for k, (rr, zz) in enumerate(prof):
        ring = []
        for i in range(segs):
            th = math.tau * i / segs
            f = sum(c * math.sin(m * th + ph) for (m, ph, c) in harm) / 2.4
            amp = 0.06 if k <= 5 else 0.0
            rv = rr * (1 + amp * f)
            zv = zz + (0.9 * f if 1 <= k <= 5 else 0.0)
            ring.append((rv * math.cos(th), rv * math.sin(th) * sy, zv))
        rings.append(ring)
    with g.part():
        for k in range(len(rings) - 1):
            A, B = rings[k], rings[k + 1]
            for i in range(segs):
                j = (i + 1) % segs
                g.face([A[i], A[j], B[j], B[i]], ROCK, "box", k <= 5)
        g.face(list(reversed(rings[0])), ROCK, "box", False)
    # monumental portal block on the front (-Y)
    pw, pd, ph = 20.0, 12.0, 20.0
    py0 = -R0 * sy - 5.5
    fp = chamfer_rect(-pw / 2, py0, pw / 2, py0 + pd, 1.6)
    pzb = g.buried(-0.8)
    L0 = math.dist(fp[0], fp[1])
    ops = {0: [Opening(arch_poly(L0 / 2 - 4.0, 0.3 - pzb, 8.0, 12.0, 12, pointed=True), depth=5.0, back="MT_Hide",
                       kind="door")]}
    frames = poly_body(g, fp, pzb, ph, ROCK, ops=ops)
    fr, _ = frames[0]
    frame_ring(g, fr, ops[0][0], width=0.9, mat=ROCK, open_bottom=True)
    g.loft([[(p[0] * 1.02, p[1] + (-0.3 if p[1] < py0 + pd / 2 else 0.3), ph - 0.3) for p in fp],
            [(p[0] * 1.08, p[1] + (-0.9 if p[1] < py0 + pd / 2 else 0.5), ph + 0.8) for p in fp],
            [(p[0] * 0.9, p[1] + (-0.4 if p[1] < py0 + pd / 2 else 0.2), ph + 1.8) for p in fp]], ROCK,
           cap0=True, cap1=True)
    # carved pillars flanking the portal with crystal braziers
    for sx in (-1, 1):
        x = sx * 7.2
        pf = chamfer_rect(x - 1.3, py0 - 2.2, x + 1.3, py0 + 0.4, 0.45)
        poly_body(g, pf, g.buried(-0.6), 15.0, ROCK)
        g.loft([[(p[0] + (0.35 if p[0] > x else -0.35), p[1] + (0.35 if p[1] > py0 - 0.9 else -0.35), 14.8) for p in pf],
                [(p[0] + (0.55 if p[0] > x else -0.55), p[1] + (0.55 if p[1] > py0 - 0.9 else -0.55), 15.6) for p in pf],
                [(p[0] + (0.2 if p[0] > x else -0.2), p[1] + (0.2 if p[1] > py0 - 0.9 else -0.2), 16.2) for p in pf]],
               ROCK, cap0=True, cap1=True)
        g.lathe([(0.5, 16.0), (1.3, 16.9), (1.4, 17.4), (1.2, 17.4)], 10, "MT_Iron", x, py0 - 0.9, smooth=False)
        crystal(g, x, py0 - 0.9, 17.2, 2.6, 0.45)
        crystal(g, x + 0.5, py0 - 0.7, 17.2, 1.5, 0.25, tilt=(10, 18))
        # giant tusks arching over the portal
        tusk(g, (sx * 5.2, py0 - 1.2, 0.5), (sx * 7.5, py0 - 5.0, 8.0), (sx * 4.5, py0 - 5.5, 17.0),
             (sx * 0.8, py0 - 3.0, 21.5), 0.9, segs=10)
    # crystal clusters around the rim
    for k in range(6):
        th = math.tau * (k + 0.25) / 6
        x, y = (R0 - 3.5) * math.cos(th), (R0 - 3.5) * math.sin(th) * sy
        if y < -15:
            continue
        crystal(g, x, y, 17.4, 3.4, 0.55)
        crystal(g, x + 0.7, y + 0.3, 17.4, 2.0, 0.32, tilt=(12, -15))
    # the great hall: carved block with a lumpy dome on the back (+Y) side of the ring, facing the arena
    from arch_demon import lumpy_dome as _ld
    hx0, hx1, hy0, hy1 = -12.0, 12.0, R0 * sy - 6.0, R0 * sy + 9.0
    hfp = chamfer_rect(hx0, hy0, hx1, hy1, 3.0)
    hzb = g.buried(-0.8)
    back_ops = {4: [Opening(arch_poly(math.dist(hfp[4], hfp[5]) / 2 - 3.0, 9.5 - hzb, 6.0, 8.0, 10, pointed=True),
                            depth=2.5, back="MT_Hide", kind="door")]}
    for e in (0, 2, 6):
        L = math.dist(hfp[e], hfp[(e + 1) % 8])
        back_ops.setdefault(e, [])
        if e != 0:
            for k in range(2):
                uc = L * (k + 1) / 3
                back_ops[e].append(Opening(arch_poly(uc - 0.8, 12.0 - hzb, 1.6, 3.4, 8, pointed=True), depth=0.8,
                                           kind="arch"))
    hfr = poly_body(g, hfp, hzb, 22.0, ROCK, ops=back_ops)
    for e, lst in back_ops.items():
        for op in lst:
            if op.kind == "door":
                frame_ring(g, hfr[e][0], op, width=0.6, mat=ROCK, open_bottom=True)
            else:
                window_trim(g, hfr[e][0], op, dict(trim=ROCK, frame_w=0.3, sill_mat=ROCK, mullions=False,
                                                   shutters=False))
    g.loft([[(p[0] * 1.02, p[1] + (0.3 if p[1] > (hy0 + hy1) / 2 else -0.3), 21.7) for p in hfp],
            [(p[0] * 1.1, p[1] + (0.9 if p[1] > (hy0 + hy1) / 2 else -0.9), 22.6) for p in hfp],
            [(p[0] * 0.95, p[1] + (0.2 if p[1] > (hy0 + hy1) / 2 else -0.2), 23.2) for p in hfp]], ROCK,
           cap0=True, cap1=True)
    top = _ld(g, rng, 0.0, (hy0 + hy1) / 2, 11.0, 8.5, z0=23.0, segs=28, rings=9)
    crystal(g, 0.0, (hy0 + hy1) / 2, top - 0.6, 5.0, 0.8)
    for (a, t) in ((30, 20), (150, 20), (270, 20)):
        ax_, ay_ = math.cos(math.radians(a)), math.sin(math.radians(a))
        crystal(g, ax_ * 1.1, (hy0 + hy1) / 2 + ay_ * 1.1, top - 0.6, 2.6, 0.4, tilt=(-t * ay_, t * ax_))
    # bone totems on the rim
    for k in range(8):
        th = math.tau * (k + 0.5) / 8
        x, y = (R0 - 4.8) * math.cos(th), (R0 - 4.8) * math.sin(th) * sy
        if y > R0 * sy - 9.0 or y < -16.0:
            continue
        g.cylinder(x, y, 0.35, 17.6, 23.0, 8, "MT_Bone")
        tusk(g, (x, y, 22.4), (x + 0.9, y, 23.4), (x + 1.2, y, 24.8), (x + 0.6, y, 25.8), 0.28)
        tusk(g, (x, y, 22.4), (x - 0.9, y, 23.4), (x - 1.2, y, 24.8), (x - 0.6, y, 25.8), 0.28)
        g.sphere(x, y, 23.2, 0.6, 10, 6, "MT_Bone")
    return {"attach": {"arena_floor_z": 1.1, "portal_width": 8.0}}


# ---------------------------------------------------------------------------------------------- Rapan Guild

@asset("SM_Landmark_RapanGuild", "Landmarks", "Desert", kind="landmark", foundation=0.7, budget=(5000, 40000),
       notes="Rapan adventurer guild (~52 x 40 m): two-storey sandstone block with rounded merlons, a monumental "
             "pointed-arch portal (iwan) with the guild shield on the -Y front, two slender towers with domes, a great "
             "pointed dome on a windowed drum (~36 m), arcaded ground floor, awnings and banners.")
def rapan_guild(g, rng):
    from arch_desert import D_WIN, D_DOOR
    from arch_parts import awning, emblem_shield, viga_row
    W, D = 44.0, 34.0
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    H = 12.5
    zb = g.buried(-0.6)
    ops = {}
    for side, L in (("front", W), ("back", W), ("left", D), ("right", D)):
        lst = []
        n = int(L / 4.2)
        for k in range(n):
            uc = 2.1 + (L - 4.2) * (k + 0.5) / n
            if side == "front" and abs(uc - W / 2) < 14.6:
                continue
            lst.append(Opening(arch_poly(uc - 0.7, 1.2 - zb, 1.4, 2.8, 8, pointed=True), depth=0.45, kind="arch"))
            lst.append(Opening(arch_poly(uc - 0.55, 7.2 - zb, 1.1, 2.2, 8, pointed=True), depth=0.45, kind="arch"))
        ops[side] = lst
    frs = block(g, x0, y0, x1, y1, zb, H, "MT_Sandstone", ops=ops)
    for side, lst in ops.items():
        for op in lst:
            window_trim(g, frs[side], op, dict(D_WIN, sill=False, mullions=False, trim="MT_Sandstone"))
    cornice(g, x0, y0, x1, y1, 6.0, 0.35, 0.12, "MT_Sandstone")
    parapet_ring(g, x0, y0, x1, y1, H, 1.0, "MT_Sandstone", proud=0.06, thick=0.45, coping="MT_PlasterTan",
                 merlons="round")
    g.box(x0 - 0.1, y0 - 0.1, g.buried(-0.65), x1 + 0.1, y1 + 0.1, 0.35, "MT_Sandstone")
    for side in ("front", "back", "left", "right"):
        fr, L = side_frame(x0, y0, x1, y1, 0.0, side)
        viga_row(g, fr, L, H - 1.25, n_out=0.45, spacing=1.6, margin=1.2)
    # the iwan: tall rectangular portal block with a deep pointed arch and the guild shield
    iw, idp, ih = 18.0, 4.0, 22.0
    iy0 = y0 - 2.4
    izb = g.buried(-0.6)
    iop = Opening(arch_poly(iw / 2 - 5.5, 0.4 - izb, 11.0, 16.5, 14, pointed=True), depth=3.2, back="MT_PlasterTan",
                  kind="rose")
    ifr = block(g, -iw / 2, iy0, iw / 2, iy0 + idp + 2.4, izb, ih, "MT_Sandstone", ops={"front": [iop]})
    frame_ring(g, ifr["front"], iop, width=0.7, mat="MT_PlasterTan", open_bottom=True, bottom_drop=0.05)
    # inner door and windows on the back wall of the iwan recess
    back_fr = ifr["front"].shifted(dn=-3.2)
    dop = Opening(arch_poly(iw / 2 - 1.8, 0.4 - izb + 0.0, 3.6, 6.0, 10, pointed=True), depth=0.4,
                  back="MT_WoodPlanks", kind="door")
    wplate(g, back_fr, arch_poly(iw / 2 - 2.3, 0.43 - izb, 4.6, 6.6, 10, pointed=True), -0.05, 0.12, "MT_Sandstone")
    wplate(g, back_fr, arch_poly(iw / 2 - 1.8, 0.46 - izb, 3.6, 6.0, 10, pointed=True), -0.05, 0.16, "MT_WoodPlanks")
    for s in (-1, 1):
        wplate(g, back_fr, arch_poly(iw / 2 + s * 3.6 - 0.8, 7.5 - izb, 1.6, 3.2, 8, pointed=True), -0.04, 0.1,
               "MT_Glass")
    parapet_ring(g, -iw / 2, iy0, iw / 2, iy0 + idp + 2.4, ih, 1.1, "MT_Sandstone", proud=0.05, thick=0.4,
                 coping="MT_PlasterTan", merlons="round")
    emblem_shield(g, ifr["front"], iw / 2, ih - izb - 1.2, w=2.6, h=3.0, out=0.1)
    stair(g, Frame((-iw / 2, iy0, 0.0), (1, 0, 0), (0, 0, 1)), iw / 2 - 4.0, iw / 2 + 4.0, 0.385, "MT_Sandstone")
    # two slender towers flanking the iwan
    for sx in (-1, 1):
        cx, cy = sx * (iw / 2 + 2.6), iy0 + 1.4
        s2 = 2.2
        tzb = g.buried(-0.6)
        tz = 28.0
        tops = {s: [Opening(arch_poly(s2 - 0.3, z - tzb, 0.6, 1.6, 6, pointed=True), depth=0.35, kind="arch")
                    for z in (14.0, 19.0, 24.0)] for s in ("front", "left" if sx < 0 else "right")}
        tfr = block(g, cx - s2, cy - s2, cx + s2, cy + s2, tzb, tz, "MT_Sandstone", ops=tops)
        for s, lst in tops.items():
            for op in lst:
                window_trim(g, tfr[s], op, dict(D_WIN, sill=False, mullions=False, trim="MT_Sandstone"))
        rings = [[(cx - h, cy - h, z), (cx + h, cy - h, z), (cx + h, cy + h, z), (cx - h, cy + h, z)]
                 for (h, z) in ((s2 - 0.1, tz - 0.9), (s2 + 0.7, tz - 0.05), (s2 + 0.72, tz + 0.2))]
        g.loft(rings, "MT_Sandstone", cap0=True, cap1=True)
        parapet_ring(g, cx - s2 - 0.7, cy - s2 - 0.7, cx + s2 + 0.7, cy + s2 + 0.7, tz + 0.2, 0.9, "MT_Sandstone",
                     proud=0.0, thick=0.3, merlons="step")
        g.cylinder(cx, cy, 1.5, tz + 0.1, tz + 3.2, 12, "MT_PlasterTan", smooth=False)
        top = dome(g, cx, cy, 1.65, tz + 3.2, "MT_PlasterTan", segs=16, rings=6, shape="pointed")
        finial_spike(g, cx, cy, top - 0.1, dict(mat="MT_Gold", h=1.6, r=0.08))
        banner_z = 11.5
        from arch_walls import banner
        banner(g, cx, cy - s2, banner_z, 1.4, 5.0, "MT_ClothRed")
    # great dome on a windowed drum
    top = drum_dome(g, 0.0, 3.0, H - 0.4, 11.0, 5.0, "MT_PlasterTan", wall_mat="MT_Sandstone", segs=20,
                    shape="pointed", lantern_mat="MT_Gold")
    # awnings on the front ground floor either side of the iwan
    frf, Lf = side_frame(x0, y0, x1, y1, 0.0, "front")
    awning(g, frf, 1.0, W / 2 - 9.6, 4.6, 2.2, 0.8, "MT_ClothRed", poles=True, pole_mat="MT_Timber")
    awning(g, frf, W / 2 + 9.6, W - 1.0, 4.6, 2.2, 0.8, "MT_ClothTan", poles=True, pole_mat="MT_Timber")
    return {"attach": {"portal_width": 11.0, "door_width": 3.6}}


# ---------------------------------------------------------------------------------------------- Labyrinth Gate

@asset("SM_Landmark_LabyrinthGate", "Landmarks", "Desert", kind="landmark", foundation=1.2, budget=(3000, 40000),
       notes="Colossal labyrinth gate (~46 m wide, ~33 m tall) meant to be set into a cliff: the facade faces -Y and "
             "the block runs 14 m back (+Y) into the rock. A 13 x 22 m pointed opening is a dark 9 m deep recess with "
             "stepped archivolts; two 20 m guardian statues, a carved face with glowing crystal eyes above the arch, "
             "rune bands and a toothed frieze.")
def labyrinth_gate(g, rng):
    from arch_demon import crystal
    from arch_vfx import hull_rock
    Wg, Dg, Hg = 46.0, 14.0, 33.0
    x0, y0, x1, y1 = -Wg / 2, -Dg / 2, Wg / 2, Dg / 2
    zb = g.buried(-1.2)
    ow, oh = 13.0, 22.0
    gop = Opening(arch_poly(Wg / 2 - ow / 2, 1.5 - zb, ow, oh, 16, pointed=True), depth=9.0, back="MT_DemonRock",
                  reveal="MT_Stone", kind="gate")
    frs = block(g, x0, y0, x1, y1, zb, Hg, "MT_Stone", ops={"front": [gop]})
    fr = frs["front"]
    # stepped archivolts (three concentric pointed arch rings, each proud more)
    for k, (wd, n1) in enumerate(((0.9, 0.35), (1.8, 0.7), (2.8, 1.05))):
        frame_ring(g, fr, gop, width=wd, inset=0.05 + 0.02 * k, n0=-0.3 - 0.07 * k, n1=n1, mat="MT_Sandstone"
                   if k % 2 == 0 else "MT_Stone", open_bottom=True, bottom_drop=0.1 + 0.03 * k)
    # threshold stair
    stair(g, Frame((x0, y0, 0.0), (1, 0, 0), (0, 0, 1)), Wg / 2 - 9.0, Wg / 2 + 9.0, 1.485, "MT_Stone",
          run_per=0.45)
    # the carved face over the arch: brow ridge, glowing eyes in deep sockets, nose ridge, fangs
    fz = 1.5 - zb + oh + 1.2
    face_c = Wg / 2
    wplate(g, fr, [(face_c - 6.0, fz + 3.6), (face_c + 6.0, fz + 3.6), (face_c + 5.0, fz + 4.6), (face_c - 5.0, fz + 4.6)],
           -0.2, 1.6, "MT_Stone")
    for s in (-1, 1):
        ex = face_c + s * 3.2
        eye = circle_poly(ex, fz + 2.6, 0.9, 10)
        ring = circle_poly(ex, fz + 2.6, 1.5, 10)
        g.ring_plate(fr, ring, eye, -0.2, 1.1, "MT_Stone", back=False)
        c = fr.p(ex, fz + 2.6, 0.2)
        with g.xf(loc=(c.x, c.y, c.z), rotx=90):
            g.lathe([(0.0, -0.3), (0.7, 0.0), (0.45, 0.45), (0.0, 0.7)], 8, "MT_Crystal", smooth=False)
    wplate(g, fr, [(face_c - 0.7, fz + 0.6), (face_c + 0.7, fz + 0.6), (face_c + 0.35, fz + 3.7),
                   (face_c - 0.35, fz + 3.7)], -0.2, 1.3, "MT_Stone")
    for k in range(6):
        u = face_c - 4.2 + k * 1.68
        wplate(g, fr, [(u - 0.35, fz + 0.25), (u + 0.35, fz + 0.25), (u, fz - 1.0 - 0.4 * (k in (1, 4)))],
               -0.2, 0.9 + 0.02 * k, "MT_Bone")
    # toothed frieze across the top and rune bands down the jambs
    n = 23
    for k in range(n):
        u = 1.0 + (Wg - 2.0) * (k + 0.5) / n
        wplate(g, fr, [(u - 0.6, Hg - zb - 2.5), (u + 0.6, Hg - zb - 2.5), (u, Hg - zb - 3.9)], -0.1, 0.55,
               "MT_Sandstone")
    wplate(g, fr, rect(0.3, Hg - zb - 2.6, Wg - 0.6, 1.0), -0.12, 0.62, "MT_Stone")
    for s in (-1, 1):
        ub = face_c + s * (ow / 2 + 4.0)
        for k in range(9):
            v = 2.5 - zb + k * 2.3
            wd = 0.5 + 0.4 * ((k * 7 + (s > 0) * 3) % 3)
            wplate(g, fr, rect(ub - wd / 2, v, wd, 1.1), -0.1, 0.28 + 0.05 * (k % 2), "MT_Sandstone")
    # stepped crown along the top and heavy pilaster bands framing the gate
    for k, (inset, hgt) in enumerate(((1.5, 1.4), (4.5, 1.2), (8.0, 1.1))):
        g.box(x0 + inset, y0 + 0.4 + k * 0.6, Hg - 0.3 + 1.25 * k, x1 - inset, y1 - 0.4 - 0.13 * k,
              Hg + hgt + 1.25 * k, "MT_Stone")
    for sx in (-1, 1):
        for u in (Wg / 2 + sx * (ow / 2 + 7.2), Wg / 2 + sx * (ow / 2 + 1.75)):
            wplate(g, fr, rect(u - 0.9, 1.3 - zb, 1.8, Hg - zb - 4.3), -0.2, 0.8, "MT_Stone")
            wplate(g, fr, rect(u - 1.2, Hg - zb - 3.2, 2.4, 0.9), -0.2, 1.05, "MT_Sandstone")
    # skull bosses along the frieze
    for k in range(7):
        u = Wg / 2 + (k - 3) * 5.4
        if abs(u - Wg / 2) < 1.0:
            continue
        c = fr.p(u, Hg - zb - 5.3, 0.6)
        g.sphere(c.x, c.y, c.z, 0.9, 10, 7, "MT_Bone")
        for e in (-1, 1):
            g.box(c.x + e * 0.36 - 0.2, c.y - 0.95, c.z - 0.05, c.x + e * 0.36 + 0.2, c.y - 0.6, c.z + 0.28,
                  "MT_DemonRock")
        g.box(c.x - 0.5, c.y - 0.72, c.z - 1.05, c.x + 0.5, c.y + 0.2, c.z - 0.45, "MT_Bone")
    # hooded guardian statues holding greatswords point-down
    for s in (-1, 1):
        gx = s * 16.3
        gy = y0 - 3.4
        g.box(gx - 3.2, gy - 3.0, g.buried(-1.2), gx + 3.2, gy + 3.2, 2.3, "MT_Stone")
        g.box(gx - 3.4, gy - 3.2, 2.1, gx + 3.4, gy + 3.3, 2.6, "MT_Sandstone")
        robe = [(2.6, 2.5), (2.35, 6.0), (1.95, 11.0), (1.8, 14.5), (2.15, 15.8), (1.5, 16.6), (0.9, 17.0)]
        g.lathe(robe, 10, "MT_Stone", gx, gy, smooth=False, a0=math.pi / 10)
        g.lathe([(1.25, 16.8), (1.4, 18.0), (1.1, 19.6), (0.55, 20.8), (0.0, 21.6)], 10, "MT_Stone", gx, gy + 0.15,
                smooth=False, a0=math.pi / 10)
        g.box(gx - 0.75, gy - 1.35, 17.2, gx + 0.75, gy - 0.6, 18.9, "MT_DemonRock")
        for e in (-1, 1):
            c = Vector((gx + e * 0.33, gy - 1.3, 18.2))
            g.box(c.x - 0.16, c.y - 0.12, c.z - 0.09, c.x + 0.16, c.y + 0.16, c.z + 0.09, "MT_Crystal")
        # arms folded onto the pommel
        g.beam((gx + 1.7, gy - 0.3, 15.0), (gx + 0.35, gy - 2.05, 13.3), 0.95, 0.95, "MT_Stone")
        g.beam((gx - 1.7, gy - 0.3, 15.0), (gx - 0.35, gy - 2.05, 13.3), 0.95, 0.95, "MT_Stone")
        # greatsword: blade down to the plinth, guard and pommel under the hands
        g.beam((gx, gy - 2.45, 2.45), (gx, gy - 2.45, 12.4), 1.1, 0.26, "MT_Iron", up=(0, 1, 0))
        g.beam((gx - 1.6, gy - 2.45, 12.1), (gx + 1.6, gy - 2.45, 12.1), 0.42, 0.42, "MT_Iron")
        g.beam((gx, gy - 2.45, 12.3), (gx, gy - 2.45, 13.3), 0.3, 0.3, "MT_Iron")
        g.sphere(gx, gy - 2.45, 13.45, 0.34, 8, 6, "MT_Iron")
    # rubble at the foot
    for k in range(7):
        a = rng.uniform(0, math.pi)
        rx = rng.uniform(-20.0, 20.0)
        if abs(rx) < 9.0:
            rx += 11.0 * (1 if rx >= 0 else -1)
        hull_rock(g, rng, (rx, y0 - rng.uniform(1.0, 6.0), rng.uniform(0.1, 0.4) - 0.003 * k),
                  (rng.uniform(0.6, 1.2), rng.uniform(0.5, 1.0), rng.uniform(0.4, 0.8)), 16, "MT_Stone")
    return {"attach": {"opening_width": ow, "opening_height": oh, "recess_depth": 9.0, "back_into_cliff": Dg}}
