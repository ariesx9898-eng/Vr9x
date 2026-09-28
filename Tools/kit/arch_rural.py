"""Rural set (Buena village): plank-walled cottages and barns on low stone plinths with thatch or red tile roofs,
porches and stone chimneys; a tower windmill (body + separately pivoted blades), well, fences, haystack, cart, shed
and the Greyrat family house."""
import math

from mathutils import Vector

from arch_geo import Frame, ccw
from arch_house import House, gable_heights
from arch_parts import (Opening, arch_poly, block, circle_poly, corner_posts, door_trim, exterior_chimney, hoist_beam,
                        porch, rect, roof_cone, roof_gable, roof_shed, shed_body, side_frame, solid, stair,
                        window_trim, wplate)
from arch_registry import asset

WOOD_WIN = dict(trim="MT_Timber", frame_w=0.08, shutters=True, shutter_mat="MT_WoodPlanks", sill_mat="MT_Timber",
                win_w=0.8, win_h=1.0, win_sill=0.9)
BOX_WIN = dict(WOOD_WIN, flowerbox=True)
WOOD_DOOR = dict(door_frame_mat="MT_Timber", door_frame_w=0.1, straps=True, battens=True)
THATCH = dict(roof_mat="MT_RoofThatch", roof_under="MT_RoofThatch", fascia="MT_RoofThatch", verge="MT_RoofThatch",
              ridge_mat="MT_RoofThatch", th=0.45, course=1.45, step=0.13, ov=0.6, ovv=0.42)


def rural_house(W, D, heights, **kw):
    base = dict(walls=["MT_WoodPlanks"], timber=[False], corner_boards="MT_Timber", plinth="MT_Stone",
                floor0=0.3, win_styles=[WOOD_WIN], door_style=WOOD_DOOR, win_spacing=2.2, pitch=50,
                roof_mat="MT_RoofRed")
    base.update(kw)
    return House(W, D, heights, **base)


# ---------------------------------------------------------------------------------------------- houses

@asset("SM_Rural_House_A", "Rural", "Rural", notes="Thatched plank cottage with an exterior stone chimney.")
def rural_house_a(g, rng):
    def chim(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        fr, Lf = side_frame(x0, y0, x1, y1, 0.0, "right")
        exterior_chimney(g, fr, Lf * 0.5, 0.75, 0.6, h.ridge_z() + 0.9, pots=1)

    h = rural_house(7.4, 5.6, [2.8], ridge="x", pitch=52, win_styles=[BOX_WIN], door=dict(side="front", at=0.42,
                    w=1.0, h=2.0), window_counts={(0, "front"): 2, (0, "right"): 0}, hooks=[chim], **THATCH)
    h.build(g)


@asset("SM_Rural_House_B", "Rural", "Rural",
       notes="One-and-a-half storey farmhouse: plank walls, red tile roof with two dormers, front porch.")
def rural_house_b(g, rng):
    def por(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        dx = (x0 + x1) / 2 + (x1 - x0) * (0.4 - 0.5)
        porch(g, dx - 1.6, dx + 1.6, y0, 1.7, h.floor0, h.floor0 + 2.35, h.floor0 + 2.75, roof_mat="MT_RoofRed")

    h = rural_house(8.6, 6.6, [2.8, 1.2], ridge="x", pitch=48, door=dict(side="front", at=0.4, w=1.0, h=2.05),
                    skip_windows={(1, "front"), (1, "back")}, window_counts={(0, "front"): 3},
                    dormers=[dict(u=-0.22, w=1.4), dict(u=0.25, w=1.4)], dormer_wall="MT_WoodPlanks", door_steps=False,
                    chimneys=[dict(x=0.3, y=0.0, pots=2)], jetty=[(-0.04, -0.04, -0.04)], hooks=[por])
    h.build(g)


@asset("SM_Rural_House_C", "Rural", "Rural",
       notes="Long thatched farmhouse with a hipped roof, a lean-to woodshed on its right end and a chimney.")
def rural_house_c(g, rng):
    def annex(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        ax0, ax1 = x1 - 0.12, x1 + 2.6
        ay0, ay1 = y0 + 0.6, y1 - 0.6
        # lean-to: slope rises toward the house (-X side) -> build with the roof along Y by rotating
        fr, Lf = side_frame(ax0, ay0, ax1, ay1, 0.0, "right")
        with g.xf(loc=((ax0 + ax1) / 2, (ay0 + ay1) / 2, 0.0), rotz=90):
            hw = (ay1 - ay0) / 2
            hd = (ax1 - ax0) / 2
            # local: x along the wall (world y), y from the outer edge (-hd) to the house (+hd)
            zu, tv = roof_shed(g, -hw, hw, -hd, hd, 1.9, 2.5, "MT_RoofThatch", ov=0.35, ovv=0.25, th=0.3,
                               under="MT_RoofThatch", fascia="MT_RoofThatch", course=0.6, step=0.08, ov_back=0.4)
            zb = g.buried(-0.3)
            shed_body(g, -hw, -hd, hw, hd, zb, 1.9 + tv / 2, 2.5 + tv / 2, "MT_WoodPlanks",
                      ops={"front": [Opening(rect(0.6, 0.25 - zb, 1.6, 1.3), depth=0.4, back="MT_Timber",
                                             kind="shop")]})
            # stacked logs in the open front
            for k in range(3):
                for m in range(4 - k):
                    xx = -hw + 0.85 + m * 0.36 + k * 0.18
                    zz = 0.4 + k * 0.27
                    with g.xf(loc=(xx, -hd + 0.25, zz), rotx=90):
                        g.cylinder(0, 0, 0.14, -0.2, 0.55, 7, "MT_Timber", smooth=False)

    h = rural_house(11.0, 5.8, [2.8], roof="hip", pitch=50, door=dict(side="front", at=0.36, w=1.0, h=2.0),
                    win_styles=[BOX_WIN], window_counts={(0, "front"): 3, (0, "back"): 3, (0, "right"): 0},
                    window_positions={(0, "front"): [1.3, 6.4, 9.2]}, chimneys=[dict(x=-0.3, y=0.1, pots=1)],
                    hooks=[annex], **THATCH)
    h.build(g)


# ---------------------------------------------------------------------------------------------- barns

@asset("SM_Rural_Barn_A", "Rural", "Rural",
       notes="Thatched gable-fronted barn with big double doors (3.6 m wide x 3.8 m) facing -Y and a hayloft hatch.")
def barn_a(g, rng):
    def loft(h, g):
        L0 = h.levels[0]
        fr, Lf = L0["frames"]["front"]
        hoist_beam(g, fr, Lf / 2, L0["H"] + 2.6, out=0.9)

    W = 8.4
    doors = [Opening(rect(W / 2 - 1.8, 0.0, 3.6, 3.8), depth=0.25, back="MT_WoodPlanks", kind="door"),
             Opening(rect(W / 2 - 0.6, 4.55, 1.2, 1.3), depth=0.2, back="MT_WoodPlanks", kind="loft")]
    h = rural_house(W, 12.0, [4.4], ridge="y", pitch=52, floor0=0.08, door=dict(side="back", at=0.5, w=1.1, h=2.2),
                    skip_windows={(0, "front")}, gable_windows=False, extra_openings={(0, "front"): doors},
                    win_styles=[dict(WOOD_WIN, win_w=0.6, win_h=0.6, win_sill=2.4, shutters=False, mullions=False)],
                    window_counts={(0, "left"): 3, (0, "right"): 3}, hooks=[loft], **THATCH)
    h.build(g)


@asset("SM_Rural_Barn_B", "Rural", "Rural",
       notes="Red-roofed barn: stone lower storey, plank loft storey, two cart doors on the long -Y side, "
             "hoist beam in the gable.")
def barn_b(g, rng):
    def hoist(h, g):
        top = h.levels[-1]
        fr, Lf = top["frames"]["right"]
        hoist_beam(g, fr, Lf / 2, top["H"] + 2.35, out=0.95)

    W = 13.0
    doors = [Opening(rect(1.6, 0.0, 3.0, 3.1), depth=0.3, back="MT_WoodPlanks", kind="door"),
             Opening(rect(W - 4.6, 0.0, 3.0, 3.1), depth=0.3, back="MT_WoodPlanks", kind="door")]
    loft_door = [Opening(rect(8.08 / 2 - 0.6, 1.95, 1.2, 1.3), depth=0.2, back="MT_WoodPlanks", kind="loft")]
    h = rural_house(W, 8.0, [3.6, 1.6], walls=["MT_Stone", "MT_WoodPlanks"], corner_boards=None, ridge="x", pitch=45,
                    floor0=0.1, jetty=[(0.04, 0.04, 0.04)], door=None, skip_windows={(0, "front"), (1, "front"),
                                                                                    (1, "back")},
                    extra_openings={(0, "front"): doors, (1, "right"): loft_door}, gable_windows=False,
                    win_styles=[dict(WOOD_WIN, win_w=0.7, win_h=0.8, win_sill=1.6, trim="MT_Stone", sill_mat="MT_Stone"),
                                dict(WOOD_WIN, win_w=0.7, win_h=0.7, win_sill=0.5, shutters=False)],
                    window_counts={(0, "back"): 3, (0, "left"): 1, (0, "right"): 1, (1, "left"): 0, (1, "right"): 0},
                    hooks=[hoist], cornices="MT_Timber")
    h.build(g)


# ---------------------------------------------------------------------------------------------- windmill

MILL_H = 10.0
MILL_R0 = 3.0
MILL_R1 = 2.1
HUB = (0.0, -3.15, MILL_H + 1.25)


@asset("SM_Rural_Windmill_Body", "Rural", "Rural", recentre=False, pivot="base_point", budget=(800, 6000),
       notes="Tower windmill body: tapered whitewashed octagonal tower on a stone base with a thatched conical "
             "cap and the windshaft; origin on the tower axis at ground level. Attach SM_Rural_Windmill_Blades at the 'hub' anchor (0, -3.15, 11.25): the "
             "blades rotate about the Y axis (their forward axis, facing -Y).")
def windmill_body(g, rng):
    n = 8
    a0 = math.pi / n
    zb = g.buried(-0.5)

    def ring(z):
        R = MILL_R0 + (MILL_R1 - MILL_R0) * (z / MILL_H)
        return [(R * math.cos(a0 + math.tau * i / n), R * math.sin(a0 + math.tau * i / n), z) for i in range(n)]

    B = ring(zb)
    T = ring(MILL_H)
    faces = []
    front_edge = None
    for i in range(n):
        j = (i + 1) % n
        pts = [B[i], B[j], T[j], T[i]]
        mid = Vector(((B[i][0] + B[j][0]) / 2, (B[i][1] + B[j][1]) / 2, 0))
        faces.append(pts)
        if mid.y < -1.0 and abs(mid.x) < 0.5:
            front_edge = i
    # openings: door on the front face, small windows on alternate faces
    tilt = math.atan((MILL_R0 - MILL_R1) * math.cos(math.pi / n) / MILL_H)
    ct = math.cos(tilt)
    ops = {}
    for i in range(n):
        Lb = math.dist(B[i][:2], B[(i + 1) % n][:2])
        lst = []
        if i == front_edge:
            lst.append(Opening(arch_poly(Lb / 2 - 0.6, (0.32 - zb) / ct, 1.2, 2.2, 8), depth=0.3,
                               back="MT_WoodPlanks", kind="arch"))
            lst.append(Opening(rect(Lb / 2 - 0.3, (6.6 - zb) / ct, 0.6, 0.8), depth=0.22, kind="window"))
        elif i % 2 == 0:
            lst.append(Opening(rect(Lb / 2 - 0.28, (3.4 - zb) / ct, 0.56, 0.8), depth=0.22, kind="window"))
        else:
            lst.append(Opening(rect(Lb / 2 - 0.28, (5.2 - zb) / ct, 0.56, 0.8), depth=0.22, kind="window"))
        ops[i] = lst
    allf = [(faces[i], "MT_Plaster", ops.get(i)) for i in range(n)]
    allf.append((list(T), "MT_Plaster", None))
    allf.append((list(reversed(B)), "MT_Plaster", None))
    frames = solid(g, allf)
    for i in range(n):
        fr = frames[i]
        for op in ops[i]:
            if op.back == "MT_WoodPlanks":
                door_trim(g, fr, op, dict(door_frame_mat="MT_Stone", door_frame_w=0.14, straps=True, battens=True))
            else:
                window_trim(g, fr, op, dict(trim="MT_Timber", frame_w=0.07, shutters=False, sill=False, mullions=True))
    # stone base band
    g.lathe([(MILL_R0 + 0.22, g.buried(-0.55)), (MILL_R0 + 0.16, 0.12), (MILL_R0 + 0.06, 0.27)], n, "MT_Stone",
            a0=a0, smooth=False)
    # steps to the door
    fr = frames[front_edge]
    Lb = math.dist(B[front_edge][:2], B[(front_edge + 1) % n][:2])
    stair(g, Frame((fr.o.x, fr.o.y, 0.0), fr.u, (0, 0, 1)), Lb / 2 - 0.9, Lb / 2 + 0.9, 0.305, "MT_Stone",
          run_per=0.3, embed=0.3)
    # cap: thatched cone on a timber curb ring
    curb_o = circle_poly(0, 0, MILL_R1 + 0.16, 16)
    curb_i = circle_poly(0, 0, MILL_R1 - 0.45, 16)
    g.prism_holes(curb_o, [curb_i], MILL_H - 0.33, MILL_H + 0.28, "MT_Timber")
    roof_cone(g, 0, 0, MILL_R1 + 0.16, MILL_H + 0.28, 3.4, "MT_RoofThatch", ov=0.35, th=0.3, course=0.6, step=0.08,
              segs=16, under="MT_RoofThatch", fascia="MT_RoofThatch", finial=dict(mat="MT_Timber", h=0.8, r=0.09))
    # windshaft from inside the cap out to the hub (tilt-free, along -Y)
    with g.xf(loc=(HUB[0], 0.0, HUB[2]), rotx=90):
        g.cylinder(0, 0, 0.2, 0.0, -HUB[1] - 0.2, 10, "MT_Timber")
    g.anchor("hub", HUB)
    return {"attach": {"blades": "SM_Rural_Windmill_Blades", "hub": list(HUB), "rotation_axis": "Y",
                       "hub_height": HUB[2]}}


@asset("SM_Rural_Windmill_Blades", "Rural", "Rural", kind="prop", pivot="hub", recentre=False, ground=False,
       budget=(200, 3000), view=(-20, 10),
       notes="Four windmill sails (7.4 m radius) with the hub centre at the origin; the sails lie in the XZ plane "
             "and rotate about the Y axis. Place at SM_Rural_Windmill_Body's 'hub' anchor (0, -3.15, 11.25).")
def windmill_blades(g, rng):
    # hub
    with g.xf(rotx=90):
        g.cylinder(0, 0, 0.36, -0.3, 0.3, 12, "MT_Timber")      # local z -> world -y
        g.cylinder(0, 0, 0.24, 0.26, 0.42, 10, "MT_Iron")
    Rlen = 7.4
    for k in range(4):
        phi = math.radians(45 + 90 * k)
        d = Vector((math.cos(phi), 0, math.sin(phi)))
        p = Vector((-math.sin(phi), 0, math.cos(phi)))
        # stock
        g.beam(d * 0.28, d * Rlen, 0.22, 0.2, "MT_Timber", up=(0, 1, 0))
        # sail frame offset to the trailing side (+p)
        for off, w in ((0.17, 0.08), (1.28, 0.1)):
            g.beam(d * 1.5 + p * off, d * (Rlen - 0.15) + p * off, w, 0.12, "MT_Timber", up=(0, 1, 0))
        nb = 10
        for b in range(nb + 1):
            r = 1.6 + (Rlen - 1.85) * b / nb
            g.beam(d * r + p * 0.06, d * r + p * 1.3, 0.06, 0.09, "MT_Timber", up=(0, 1, 0))
        # sail cloth behind the bars (+Y side)
        g.obox(d * ((1.75 + Rlen - 0.4) / 2) + p * 0.74 + Vector((0, 0.058, 0)), d, p, Vector((0, 1, 0)),
               (Rlen - 0.4 - 1.75) / 2, 0.5, 0.008, "MT_ClothTan")
    return {"attach": {"rotation_axis": "Y", "radius": Rlen}}


# ---------------------------------------------------------------------------------------------- small props

@asset("SM_Rural_Well", "Rural", "Rural", kind="prop", foundation=0.35, budget=(200, 3000),
       notes="Village well: stone ring with coping, water surface (MT_Glass) inside, timber posts, winch with "
             "crank, rope and bucket under a small shingled roof.")
def well(g, rng):
    g.prism_holes(circle_poly(0, 0, 0.95, 18), [circle_poly(0, 0, 0.68, 18)], g.buried(-0.3), 0.8, "MT_Stone")
    g.prism_holes(circle_poly(0, 0, 1.03, 18), [circle_poly(0, 0, 0.62, 18)], 0.74, 0.9, "MT_Stone")
    g.cylinder(0, 0, 0.7, 0.2, 0.235, 18, "MT_Glass", smooth=False)
    for sx in (-1, 1):
        g.box(sx * 0.82 - 0.08, -0.08, 0.5, sx * 0.82 + 0.08, 0.08, 2.72, "MT_Timber")
    with g.xf(loc=(0, 0, 1.55), roty=90):
        g.cylinder(0, 0, 0.09, -0.8, 0.8, 10, "MT_Timber")
    # crank
    g.box(0.88, -0.03, 1.52, 0.95, 0.03, 1.9, "MT_Iron")
    g.box(0.93, -0.02, 1.84, 1.18, 0.02, 1.88, "MT_Iron")
    # rope and bucket
    g.cylinder(0.0, -0.09, 0.012, 1.18, 1.52, 5, "MT_ClothTan")
    g.lathe([(0.12, 0.93), (0.15, 1.19), (0.13, 1.2)], 10, "MT_WoodPlanks", 0.0, -0.09)
    g.box(-0.16, -0.1, 1.19, 0.16, -0.08, 1.215, "MT_Iron")
    roof_gable(g, -0.9, 0.9, -0.62, 0.62, 2.25, 40, "MT_WoodPlanks", ov=0.18, ovv=0.2, th=0.1, course=0.25,
               step=0.03, under="MT_WoodPlanks", fascia="MT_Timber", verge="MT_Timber", ridge_w=0.1)


def _fence(g, L):
    hl = L / 2
    zb = g.buried(-0.35)
    g.box(-hl - 0.07, -0.07, zb, -hl + 0.07, 0.07, 1.15, "MT_Timber")
    g.box(hl - 0.06, -0.06, g.buried(-0.33), hl + 0.06, 0.06, 1.1, "MT_Timber")
    if L > 3:
        g.box(-0.065, -0.065, g.buried(-0.34), 0.065, 0.065, 1.12, "MT_Timber")
    for z0, z1 in ((0.42, 0.54), (0.82, 0.94)):
        g.box(-hl + 0.05, -0.105, z0, hl + 0.04, -0.055, z1, "MT_WoodPlanks")


@asset("SM_Rural_Fence_2m", "Rural", "Rural", kind="prop", recentre=False, foundation=0.4, budget=(20, 400),
       notes="Post-and-rail fence segment spanning x = -1..+1 m. Chain segments end to end every 2 m: the smaller "
             "+X post nests inside the next segment's -X post, so joints never z-fight.")
def fence_2m(g, rng):
    _fence(g, 2.0)


@asset("SM_Rural_Fence_4m", "Rural", "Rural", kind="prop", recentre=False, foundation=0.4, budget=(20, 400),
       notes="Post-and-rail fence segment spanning x = -2..+2 m with a middle post; chains like the 2 m segment.")
def fence_4m(g, rng):
    _fence(g, 4.0)


@asset("SM_Rural_Haystack", "Rural", "Rural", kind="prop", foundation=0.2, budget=(100, 1500),
       notes="Beehive haystack (MT_RoofThatch straw), ~2.9 m across, with a drying pole.")
def haystack(g, rng):
    segs, nr = 16, 9
    prof = [(1.35, g.buried(-0.15)), (1.42, 0.35), (1.46, 0.8), (1.38, 1.3), (1.18, 1.75), (0.9, 2.15), (0.55, 2.45),
            (0.22, 2.65), (0.0, 2.72)]
    rings = []
    for k, (r, z) in enumerate(prof):
        ring = []
        for i in range(segs):
            a = math.tau * i / segs
            jr = r * (1 + rng.uniform(-0.05, 0.05)) if 0 < k < len(prof) - 1 else r
            jz = z + (rng.uniform(-0.05, 0.05) if 0 < k < len(prof) - 1 else 0.0)
            ring.append((jr * math.cos(a), jr * math.sin(a), jz))
        rings.append(ring)
    with g.part():
        for k in range(len(rings) - 1):
            A, B = rings[k], rings[k + 1]
            for i in range(segs):
                j = (i + 1) % segs
                g.face([A[i], A[j], B[j], B[i]], "MT_RoofThatch", "box", True)
        g.face(list(reversed(rings[0])), "MT_RoofThatch", "box", False)
    g.cylinder(0.05, 0.02, 0.05, 1.8, 3.5, 6, "MT_Timber")


@asset("SM_Rural_Cart", "Rural", "Rural", kind="prop", foundation=0.02, budget=(200, 3000),
       notes="Two-wheeled farm cart, shafts pointing -Y, resting on a prop leg.")
def cart(g, rng):
    # bed and boards
    g.box(-0.7, -1.1, 0.72, 0.7, 1.1, 0.8, "MT_WoodPlanks")
    for sx in (-1, 1):
        g.box(sx * 0.65, -1.12, 0.78, sx * 0.72, 1.12, 1.17, "MT_WoodPlanks")
    for sy in (-1, 1):
        g.box(-0.66, sy * 1.04, 0.79, 0.66, sy * 1.09, 1.12, "MT_WoodPlanks")
    # frame beams under the bed
    for sx in (-1, 1):
        g.box(sx * 0.48 - 0.05, -1.3, 0.62, sx * 0.48 + 0.05, 1.15, 0.73, "MT_Timber")
    # axle
    R = 0.62
    zc = R * math.cos(math.pi / 20) - 0.01
    with g.xf(loc=(0, 0.1, zc), roty=90):
        g.cylinder(0, 0, 0.05, -0.9, 0.9, 8, "MT_Timber")
    # wheels
    for sx in (-1, 1):
        with g.xf(loc=(sx * 0.8, 0.1, zc), roty=90):
            g.cylinder(0, 0, 0.1, -0.09, 0.09, 10, "MT_Timber")
            g.prism_holes(circle_poly(0, 0, R - 0.035, 20), [circle_poly(0, 0, R - 0.11, 20)], -0.04, 0.04,
                          "MT_WoodPlanks")
            g.prism_holes(circle_poly(0, 0, R, 20), [circle_poly(0, 0, R - 0.045, 20)], -0.05, 0.05, "MT_Iron")
            for k in range(8):
                a = math.tau * k / 8 + 0.2
                p0 = Vector((0.07 * math.cos(a), 0.07 * math.sin(a), 0))
                p1 = Vector(((R - 0.08) * math.cos(a), (R - 0.08) * math.sin(a), 0))
                g.beam(p0, p1, 0.05, 0.04, "MT_Timber", up=(0, 0, 1))
    # shafts and prop leg
    for sx in (-1, 1):
        g.beam((sx * 0.46, -0.9, 0.68), (sx * 0.4, -3.0, 0.6), 0.08, 0.09, "MT_Timber")
    g.beam((0.4, -2.7, 0.6), (0.42, -2.74, 0.0), 0.06, 0.06, "MT_Timber")
    g.beam((-0.44, -2.95, 0.62), (0.44, -2.95, 0.62), 0.07, 0.07, "MT_Timber")


@asset("SM_Rural_Shed_A", "Rural", "Rural", foundation=0.35, budget=(300, 3000),
       notes="Small lean-to plank shed with a boarded mono-pitch roof and a woodpile on its right side.")
def shed_a(g, rng):
    W, D = 3.4, 2.6
    x0, y0, x1, y1 = -W / 2, -D / 2, W / 2, D / 2
    zf, zk = 2.15, 2.75
    zu, tv = roof_shed(g, x0, x1, y0, y1, zf, zk, "MT_WoodPlanks", ov=0.3, ovv=0.25, th=0.12, course=0.3, step=0.03)
    zb = g.buried(-0.3)
    op = Opening(rect(W / 2 - 0.5, 0.08 - zb, 1.0, 1.85), depth=0.12, back="MT_WoodPlanks", kind="door")
    frames = shed_body(g, x0, y0, x1, y1, zb, zf + tv / 2, zk + tv / 2, "MT_WoodPlanks", ops={"front": [op]})
    door_trim(g, frames["front"], op, dict(door_frame_mat="MT_Timber", door_frame_w=0.08, straps=True, battens=True))
    corner_posts(g, x0, y0, x1, y1, -0.05, zf - 0.02, "MT_Timber", s=0.16, p=0.04)
    # woodpile along the right wall
    for k in range(3):
        for m in range(5 - k):
            yy = y0 + 0.35 + m * 0.34 + k * 0.17
            zz = 0.14 + k * 0.27
            with g.xf(loc=(x1 + 0.3, yy, zz), roty=90):
                g.cylinder(0, 0, 0.13, -0.28, 0.28, 7, "MT_Timber", smooth=False)


# ---------------------------------------------------------------------------------------------- Greyrat house

@asset("SM_Rural_GreyratHouse", "Rural", "Rural",
       notes="The Greyrat family house (Buena): two storeys, stone ground floor, plastered upper floor with a timber "
             "band, red roof with dormers, front porch, exterior stone chimney and a lean-to kitchen annex behind.")
def greyrat_house(g, rng):
    def extras(h, g):
        L0 = h.levels[0]
        x0, y0, x1, y1 = L0["fp"]
        porch(g, -1.9, 1.9, y0, 1.9, h.floor0, h.floor0 + 2.5, h.floor0 + 2.95, roof_mat="MT_RoofRed")
        fr, Lf = side_frame(x0, y0, x1, y1, 0.0, "left")
        exterior_chimney(g, fr, Lf * 0.72, 0.8, 0.62, h.ridge_z() + 1.0, pots=2)
        # kitchen annex behind (lean-to, rising toward the house)
        ax0, ax1 = -3.2, 1.4
        ay_out, ay_in = y1 + 2.8, y1 + 0.1
        with g.xf(rotz=180):
            # local y increases toward the house: rotate so the annex's low side is the outer (+Y world) side
            zu, tv = roof_shed(g, -ax1, -ax0, -ay_out, -ay_in, 2.65, 3.35, "MT_RoofRed", ov=0.35, ovv=0.25, th=0.16,
                               ov_back=0.2)
            zb = g.buried(-0.3)
            wop = Opening(rect(1.4, 1.2 - zb, 0.8, 0.9), depth=0.16, kind="window")
            dop = Opening(rect(3.0, 0.45 - zb, 0.95, 2.0), depth=0.16, back="MT_WoodPlanks", kind="door")
            frs = shed_body(g, -ax1, -ay_out, -ax0, -ay_in, zb, 2.65 + tv / 2, 3.35 + tv / 2, "MT_Plaster",
                            ops={"front": [wop, dop]})
            window_trim(g, frs["front"], wop, dict(WOOD_WIN))
            door_trim(g, frs["front"], dop, WOOD_DOOR)
            stair(g, frs["front"], 3.0 - 0.3, 3.95 + 0.3, 0.435, "MT_Stone")

    stone_win = dict(trim="MT_Stone", frame_w=0.1, shutters=True, shutter_mat="MT_WoodPlanks", sill_mat="MT_Stone",
                     win_w=0.9, win_h=1.25, win_sill=0.9)
    up_win = dict(trim="MT_Timber", frame_w=0.08, shutters=True, sill_mat="MT_Timber", flowerbox=True, win_w=0.9,
                  win_h=1.2, win_sill=0.8)
    h = House(10.4, 8.0, [3.1, 2.8], floor0=0.5, walls=["MT_Stone", "MT_Plaster"], timber=[False],
              jetty=[(-0.04, -0.04, -0.04)], cornices="MT_Timber", ridge="x", pitch=46, roof_mat="MT_RoofRed",
              ov=0.5, ovv=0.35, win_styles=[stone_win, up_win], door=dict(side="front", at=0.5, w=1.15, h=2.25),
              door_style=dict(door_frame_mat="MT_Timber", door_frame_w=0.12, straps=True, battens=True),
              window_counts={(0, "front"): 4, (1, "front"): 4, (0, "left"): 1, (1, "left"): 1},
              window_positions={(0, "front"): [1.2, 3.35, 7.05, 9.2]}, door_steps=False,
              dormers=[dict(u=-0.24, w=1.5), dict(u=0.24, w=1.5)], hooks=[extras])
    h.build(g)
