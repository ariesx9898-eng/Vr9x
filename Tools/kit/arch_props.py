"""Street props, bridges, pier and boat for the LA PLACE towns."""
import math


from arch_geo import Frame
from arch_parts import arcade_outline, awning, circle_poly, finial_spike, lantern_cage, roof_gable
from arch_registry import asset


# ---------------------------------------------------------------------------------------------- helpers

def crate(g, cx, cy, z0, s, rot=0.0):
    """Wooden crate of size s with corner posts and a middle band (all proud by distinct amounts)."""
    with g.xf(loc=(cx, cy, z0), rotz=rot):
        h = s
        g.box(-s / 2, -s / 2, 0.0, s / 2, s / 2, h, "MT_WoodPlanks")
        for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            xa, xb = (s / 2 - 0.07, s / 2 + 0.015) if sx > 0 else (-s / 2 - 0.015, -s / 2 + 0.07)
            ya, yb = (s / 2 - 0.07, s / 2 + 0.015) if sy > 0 else (-s / 2 - 0.015, -s / 2 + 0.07)
            g.box(xa, ya, 0.01, xb, yb, h - 0.01, "MT_Timber")
        ring_o = [(-s / 2 - 0.01, -s / 2 - 0.01), (s / 2 + 0.01, -s / 2 - 0.01), (s / 2 + 0.01, s / 2 + 0.01),
                  (-s / 2 - 0.01, s / 2 + 0.01)]
        ring_i = [(-s / 2 + 0.05, -s / 2 + 0.05), (s / 2 - 0.05, -s / 2 + 0.05), (s / 2 - 0.05, s / 2 - 0.05),
                  (-s / 2 + 0.05, s / 2 - 0.05)]
        g.prism_holes(ring_o, [ring_i], h * 0.45, h * 0.55, "MT_Timber")


def barrel(g, cx, cy, z0, r=0.32, h=0.9, lying=False, rot=0.0):
    prof = [(r * 0.86, 0.0), (r * 0.97, h * 0.2), (r, h * 0.5), (r * 0.97, h * 0.8), (r * 0.86, h)]
    if lying:
        with g.xf(loc=(cx, cy, z0 + r * 0.99), rotz=rot, roty=90):
            with g.xf(loc=(0, 0, -h / 2)):
                g.lathe(prof, 14, "MT_WoodPlanks", smooth=True)
                for t in (0.12, 0.88):
                    rr = r * (0.9 + 0.1 * math.sin(math.pi * t)) + 0.012
                    g.lathe([(rr, h * t - 0.03), (rr, h * t + 0.03)], 14, "MT_Iron", smooth=True)
        return
    with g.xf(loc=(cx, cy, z0), rotz=rot):
        g.lathe(prof, 14, "MT_WoodPlanks", smooth=True)
        for t in (0.12, 0.88):
            rr = r * (0.9 + 0.1 * math.sin(math.pi * t)) + 0.012
            g.lathe([(rr, h * t - 0.03), (rr, h * t + 0.03)], 14, "MT_Iron", smooth=True)


def sack(g, cx, cy, z0, r=0.28):
    g.lathe([(r * 0.8, z0), (r, z0 + r * 0.6), (r * 0.9, z0 + r * 1.3), (r * 0.35, z0 + r * 1.75),
             (r * 0.25, z0 + r * 2.0), (0.0, z0 + r * 2.05)], 10, "MT_ClothTan", cx, cy, smooth=True)


def stall_frame(g, w, d, h_front, h_back, post_mat="MT_Timber"):
    """Counter (front, -Y) with a back shelf and four posts; returns nothing."""
    # counter
    g.box(-w / 2, -d / 2, g.buried(0.0), w / 2, -d / 2 + 0.6, 0.86, "MT_WoodPlanks")
    g.box(-w / 2 - 0.05, -d / 2 - 0.05, 0.84, w / 2 + 0.05, -d / 2 + 0.65, 0.92, "MT_WoodPlanks")
    # back shelf
    g.box(-w / 2 + 0.15, d / 2 - 0.45, g.buried(0.0), w / 2 - 0.15, d / 2 - 0.1, 1.2, "MT_WoodPlanks")
    for (x, y, zt) in ((-w / 2 + 0.02, -d / 2 - 0.02, h_front), (w / 2 - 0.02, -d / 2 - 0.02, h_front),
                       (-w / 2 + 0.06, d / 2 - 0.06, h_back), (w / 2 - 0.06, d / 2 - 0.06, h_back)):
        g.box(x - 0.06, y - 0.06, g.buried(-0.2), x + 0.06, y + 0.06, zt, post_mat)


# ---------------------------------------------------------------------------------------------- stalls

@asset("SM_Prop_MarketStall_A", "Props", "Props", kind="prop", foundation=0.25,
       notes="Market stall with a sloped red cloth roof and scalloped valance, crates and sacks; counter faces -Y.")
def stall_a(g, rng):
    w, d = 2.8, 1.8
    stall_frame(g, w, d, 2.25, 2.65)
    fr = Frame((-w / 2 - 0.1, d / 2 + 0.05, 0.0), (1, 0, 0), (0, 0, 1))
    # awning attached to the back posts' line, sloping toward -Y
    fr = Frame((-w / 2 - 0.15, d / 2 - 0.02, 0.0), (1, 0, 0), (0, 0, 1))
    awning(g, fr, 0.0, w + 0.3, 2.62, d + 0.4, 0.5, "MT_ClothRed", poles=False)
    crate(g, -0.8, 0.12, g.buried(0.0), 0.5, rot=8)
    crate(g, -0.75, 0.15, 0.48, 0.42, rot=-5)
    sack(g, 0.45, 0.2, g.buried(0.0))
    sack(g, 0.95, 0.15, g.buried(0.0), 0.24)
    for k, x in enumerate((-0.9, -0.3, 0.3, 0.9)):
        g.box(x - 0.2, -0.75, 0.905, x + 0.2, -0.45, 1.05, "MT_WoodPlanks")


@asset("SM_Prop_MarketStall_B", "Props", "Props", kind="prop", foundation=0.25,
       notes="Market stall with a gabled blue cloth roof on a ridge pole, barrels and baskets; counter faces -Y.")
def stall_b(g, rng):
    w, d = 3.0, 2.0
    stall_frame(g, w, d, 2.3, 2.3)
    roof_gable(g, -w / 2 + 0.06, w / 2 - 0.06, -d / 2 + 0.06, d / 2 - 0.06, 2.3, 30, "MT_ClothBlue", ov=0.35, ovv=0.2,
               th=0.04, courses=False, under="MT_ClothBlue", fascia="MT_ClothBlue", verge="MT_ClothBlue",
               ridge=True, ridge_mat="MT_Timber", ridge_w=0.06)
    barrel(g, 0.9, 0.2, g.buried(0.0))
    barrel(g, 0.25, 0.25, g.buried(0.0), r=0.28, h=0.8)
    for x in (-1.0, -0.45):
        g.lathe([(0.12, 0.905), (0.2, 1.0), (0.22, 1.12), (0.2, 1.13)], 10, "MT_WoodPlanks", x, -0.6)


@asset("SM_Prop_MarketStall_C", "Props", "Props", kind="prop", foundation=0.25,
       notes="Market stall with a flat green-and-tan cloth canopy, side curtain, hanging goods and crates; counter "
             "faces -Y.")
def stall_c(g, rng):
    w, d = 2.6, 1.8
    stall_frame(g, w, d, 2.4, 2.4)
    # flat canopy with a slight sag: two layers of cloth slabs
    g.box(-w / 2 - 0.2, -d / 2 - 0.3, 2.38, w / 2 + 0.2, d / 2 + 0.1, 2.44, "MT_ClothGreen")
    fr = Frame((-w / 2 - 0.2, -d / 2 - 0.3, 0.0), (1, 0, 0), (0, 0, 1))
    for k in range(6):
        u0 = (w + 0.4) * k / 6 + 0.03
        u1 = (w + 0.4) * (k + 1) / 6 - 0.03
        g.plate(fr, [(u0, 2.4), (u1, 2.4), ((u0 + u1) / 2, 2.12)], -0.02, 0.02, "MT_ClothTan" if k % 2 else
                "MT_ClothGreen")
    # side curtain
    g.box(w / 2 - 0.02, -d / 2 + 0.1, 1.2, w / 2 + 0.02, d / 2 - 0.1, 2.37, "MT_ClothTan")
    crate(g, -0.7, 0.1, g.buried(0.0), 0.55, rot=-10)
    crate(g, 0.2, 0.15, g.buried(0.0), 0.45, rot=12)
    for x in (-0.7, 0.0, 0.7):
        g.cylinder(x, -d / 2 + 0.08, 0.012, 1.8, 2.39, 4, "MT_ClothTan")
        g.sphere(x, -d / 2 + 0.08, 1.72, 0.1, 8, 5, "MT_ClothRed")


# ---------------------------------------------------------------------------------------------- small props

@asset("SM_Prop_Crates_A", "Props", "Props", kind="prop", foundation=0.02,
       notes="Stack of four wooden crates with timber corners and bands.")
def crates_a(g, rng):
    crate(g, -0.35, 0.0, g.buried(0.0), 0.65, rot=4)
    crate(g, 0.38, 0.05, g.buried(0.0), 0.6, rot=-7)
    crate(g, -0.3, 0.02, 0.63, 0.55, rot=-12)
    crate(g, 0.55, -0.62, g.buried(0.0), 0.5, rot=20)


@asset("SM_Prop_Barrels_A", "Props", "Props", kind="prop", foundation=0.02,
       notes="Group of three iron-hooped barrels (two standing, one lying).")
def barrels_a(g, rng):
    barrel(g, -0.38, 0.0, g.buried(0.0))
    barrel(g, 0.36, 0.12, g.buried(0.0), r=0.3, h=0.86)
    barrel(g, 0.05, -0.8, g.buried(0.0), r=0.3, h=0.85, lying=True, rot=15)


@asset("SM_Prop_LampPost_A", "Props", "Props", kind="prop", foundation=0.3,
       notes="Iron street lamp post (~3.9 m) with a caged glowing magic crystal lantern.")
def lamppost_a(g, rng):
    g.lathe([(0.2, g.buried(-0.3)), (0.2, 0.12), (0.14, 0.2), (0.09, 0.42), (0.06, 0.6), (0.05, 3.2), (0.07, 3.26),
             (0.07, 3.32)], 8, "MT_Iron", smooth=True)
    lantern_cage(g, 0.0, 0.0, 3.3, 3.85, 0.13)
    finial_spike(g, 0.0, 0.0, 3.8, dict(mat="MT_Iron", h=0.35, r=0.03))
    # decorative arms
    for s in (-1, 1):
        g.beam((s * 0.05, 0.0, 2.9), (s * 0.1, 0.0, 3.32), 0.03, 0.03, "MT_Iron")


@asset("SM_Prop_Signpost_A", "Props", "Props", kind="prop", foundation=0.35,
       notes="Wooden crossroads signpost with three pointing boards and a small cap.")
def signpost_a(g, rng):
    g.box(-0.08, -0.08, g.buried(-0.35), 0.08, 0.08, 2.75, "MT_Timber")
    g.lathe([(0.16, 2.72), (0.16, 2.78), (0.0, 2.98)], 4, "MT_WoodPlanks", a0=math.pi / 4, smooth=False)
    for k, (ang, z) in enumerate(((20, 2.35), (135, 2.0), (-70, 1.65))):
        with g.xf(loc=(0, 0, z), rotz=ang):
            L = 0.95
            pts = [(0.03, -0.12), (L - 0.15, -0.12), (L, 0.0), (L - 0.15, 0.12), (0.03, 0.12)]
            fr = Frame((0, 0.0 + 0.03 + 0.01 * k, 0.0), (1, 0, 0), (0, 0, 1))
            g.plate(fr, pts, -0.04, 0.0, "MT_WoodPlanks")


@asset("SM_Prop_Fountain_A", "Props", "Props", kind="prop", foundation=0.4, budget=(300, 4000),
       notes="Town fountain: octagonal stone basin (radius 3 m, rim 0.75 m) with a flat water surface (MT_Glass "
             "placeholder) 12 cm below the rim, central column, upper bowl with its own water and a finial.")
def fountain_a(g, rng):
    segs = 8
    a0 = math.pi / 8
    R = 3.0
    zb = g.buried(-0.4)
    g.prism_holes(circle_poly(0, 0, R, segs, a0=a0), [circle_poly(0, 0, R - 0.35, segs, a0=a0)], zb, 0.7, "MT_Stone")
    g.prism_holes(circle_poly(0, 0, R + 0.12, segs, a0=a0), [circle_poly(0, 0, R - 0.4, segs, a0=a0)], 0.62, 0.78,
                  "MT_StoneWhite")
    # basin floor and water: water disc slightly larger than the hole (edges buried in the basin wall)
    g.prism(circle_poly(0, 0, R - 0.33, segs, a0=a0), g.buried(-0.3), 0.08, "MT_Stone")
    g.prism(circle_poly(0, 0, R - 0.3, segs, a0=a0), 0.2, 0.66, "MT_Glass")
    # column and upper bowl
    g.lathe([(0.55, 0.5), (0.55, 0.9), (0.4, 1.05), (0.3, 1.4), (0.26, 2.1), (0.34, 2.2)], 12, "MT_StoneWhite",
            smooth=True)
    g.lathe([(0.3, 2.15), (1.1, 2.3), (1.3, 2.55), (1.28, 2.64), (1.15, 2.64), (1.1, 2.45), (0.0, 2.45)], 16,
            "MT_StoneWhite", smooth=True, caps=True)
    g.cylinder(0, 0, 1.14, 2.4, 2.57, 16, "MT_Glass", smooth=False)
    g.lathe([(0.18, 2.55), (0.2, 3.1), (0.12, 3.2), (0.16, 3.35), (0.0, 3.6)], 10, "MT_StoneWhite", smooth=True)
    finial_spike(g, 0, 0, 3.5, dict(mat="MT_Gold", h=0.5, r=0.04))
    return {"attach": {"water_height": 0.66, "rim_height": 0.78}}


@asset("SM_Prop_Bench_A", "Props", "Props", kind="prop", foundation=0.02,
       notes="Park bench, 1.8 m, wooden slats on stone legs; the seat faces -Y.")
def bench_a(g, rng):
    for x in (-0.72, 0.72):
        g.box(x - 0.08, -0.26, g.buried(0.0), x + 0.08, 0.18, 0.45, "MT_Stone")
        g.box(x - 0.06, 0.1, 0.38, x + 0.06, 0.19, 0.97, "MT_Stone")
    for k in range(4):
        y = -0.25 + k * 0.105
        g.box(-0.92, y, 0.44, 0.92, y + 0.09, 0.49, "MT_WoodPlanks")
    for k in range(3):
        z = 0.58 + k * 0.13
        g.box(-0.92, 0.17, z, 0.92, 0.22, z + 0.1, "MT_WoodPlanks")


@asset("SM_Prop_Statue_A", "Props", "Props", kind="prop", foundation=0.3, budget=(300, 4000),
       notes="Statue of a cloaked hero with a raised sword on a stepped stone pedestal (~6 m); faces -Y.")
def statue_a(g, rng):
    zb = g.buried(-0.3)
    g.box(-1.3, -1.3, zb, 1.3, 1.3, 0.4, "MT_Stone")
    g.box(-1.0, -1.0, 0.35, 1.0, 1.0, 2.3, "MT_Stone")
    g.box(-1.12, -1.12, 2.2, 1.12, 1.12, 2.45, "MT_Stone")
    g.box(-1.14, -1.14, 0.3, 1.14, 1.14, 0.62, "MT_Stone")
    z0 = 2.44
    m = "MT_StoneWhite"
    # robe / cloak (flared lathe) and torso
    g.lathe([(0.55, z0 - 0.02), (0.5, z0 + 0.6), (0.36, z0 + 1.3), (0.3, z0 + 1.75), (0.34, z0 + 2.05),
             (0.18, z0 + 2.2), (0.0, z0 + 2.24)], 12, m, 0.0, 0.05, smooth=True)
    g.sphere(0.0, 0.0, z0 + 2.42, 0.2, 12, 8, m)
    # arms: left along the body holding the cloak, right raising a sword
    g.beam((0.28, 0.0, z0 + 1.95), (0.42, -0.05, z0 + 1.4), 0.14, 0.14, m)
    g.beam((-0.3, 0.0, z0 + 2.0), (-0.52, -0.08, z0 + 2.55), 0.13, 0.13, m)
    g.beam((-0.52, -0.08, z0 + 2.5), (-0.62, -0.1, z0 + 3.05), 0.11, 0.11, m)
    g.beam((-0.64, -0.1, z0 + 3.0), (-0.72, -0.12, z0 + 4.2), 0.05, 0.02, m)
    g.beam((-0.78, -0.12, z0 + 3.0), (-0.5, -0.1, z0 + 3.0), 0.06, 0.06, m)
    # cloak falling behind
    g.box(-0.42, 0.18, z0 + 0.05, 0.42, 0.3, z0 + 1.95, m)
    return {"attach": {"pedestal_top": 2.45}}


# ---------------------------------------------------------------------------------------------- bridges

def stone_bridge(g, L, arches, width=5.0, hump=0.6, bed=-3.6, mat="MT_Stone"):
    """Arched stone bridge along X, deck ends at z = 0 (banks) at x = +/-L/2, humped deck, parapets.
    arches: list of (x_centre, span, crown_z)."""
    hl = L / 2
    zb = g.buried(bed)
    n = 16
    deck = [(-hl + L * i / n, hump * math.sin(math.pi * i / n)) for i in range(n + 1)]
    # side elevation outline in (x, z): deck line on top, arches cut from below
    outline = arcade_outline(L, 10.0, [(xc - sp / 2 + hl, sp, cz - zb) for (xc, sp, cz) in arches], segs=12)
    # replace the flat top of the arcade outline by the humped deck line
    body = [p for p in outline if p[1] < 9.9]
    top = [(x + hl, z - zb) for (x, z) in reversed(deck)]
    poly = body + top
    fr = Frame((-hl, width / 2, zb), (1, 0, 0), (0, 0, 1))
    g.plate(fr, poly, 0.0, width, mat)
    # parapets (thin, following the deck) and coping
    pdeck = [(-hl + 0.25 + (L - 0.5) * i / n, hump * math.sin(math.pi * (0.25 + (L - 0.5) * i / n) / L))
             for i in range(n + 1)]
    for side in (-1, 1):
        y_out = side * (width / 2 + 0.02)
        y_in = side * (width / 2 - 0.38)
        top_pts = [(x + hl, z + 1.0) for (x, z) in pdeck]
        bot_pts = [(x + hl, z - 0.25) for (x, z) in reversed(pdeck)]
        pfr = Frame((-hl, max(y_out, y_in), 0.0), (1, 0, 0), (0, 0, 1))
        g.plate(pfr, bot_pts + top_pts, 0.0, abs(y_out - y_in), mat)
        cdeck = [(-hl + 0.2 + (L - 0.4) * i / n, hump * math.sin(math.pi * (0.2 + (L - 0.4) * i / n) / L))
                 for i in range(n + 1)]
        cop = [(x + hl, z + 1.12) for (x, z) in cdeck] + [(x + hl, z + 0.96) for (x, z) in reversed(cdeck)]
        cfr = Frame((-hl, max(y_out, y_in) + 0.05, 0.0), (1, 0, 0), (0, 0, 1))
        g.plate(cfr, cop, 0.0, abs(y_out - y_in) + 0.1, mat)
    # cobbled deck surface: thin slab riding on the deck line between the parapets
    ddeck = [(-hl - 0.05 + (L + 0.1) * i / n, hump * math.sin(math.pi * min(max((-0.05 + (L + 0.1) * i / n) / L, 0.0),
                                                                          1.0))) for i in range(n + 1)]
    dk = [(x + hl, z + 0.06) for (x, z) in ddeck] + [(x + hl, z - 0.2) for (x, z) in reversed(ddeck)]
    dfr = Frame((-hl, width / 2 - 0.35, 0.0), (1, 0, 0), (0, 0, 1))
    g.plate(dfr, dk, 0.0, width - 0.7, "MT_Cobble")


@asset("SM_Bridge_Stone_12m", "Props", "Props", kind="bridge", pivot="base_point", recentre=False, foundation=3.7,
       ground=False, view=(-35, 12),
       notes="Single-arch stone bridge 12 m long along X (x = -6..+6) with 5 m deck width; the deck ends at z = 0 on "
             "the banks, rises 0.6 m at mid-span; arch span 8 m, abutments reach z = -3.6 (river bed).")
def bridge_stone_12(g, rng):
    stone_bridge(g, 12.0, [(0.0, 8.0, -0.6)], hump=0.6)
    return {"attach": {"length": 12.0, "deck_width": 4.3, "deck_ends_z": 0.0, "arch_span": 8.0}}


@asset("SM_Bridge_Stone_24m", "Props", "Props", kind="bridge", pivot="base_point", recentre=False, foundation=3.7,
       ground=False, view=(-35, 12),
       notes="Three-arch stone bridge 24 m long along X (x = -12..+12), 5 m deck; deck ends at z = 0, rises 1.0 m "
             "at mid-span; spans 6.5 m on two piers.")
def bridge_stone_24(g, rng):
    stone_bridge(g, 24.0, [(-7.4, 6.2, -0.9), (0.0, 6.8, -0.2), (7.4, 6.2, -0.9)], hump=1.0)
    return {"attach": {"length": 24.0, "deck_width": 4.3, "deck_ends_z": 0.0}}


@asset("SM_Bridge_Wood_10m", "Props", "Props", kind="bridge", pivot="base_point", recentre=False, foundation=3.2,
       ground=False, view=(-35, 15),
       notes="Timber footbridge 10 m along X (x = -5..+5), 2.6 m wide plank deck at z = 0..0.25 (slight camber) on "
             "stringers and two trestle bents reaching z = -3.0, with rails.")
def bridge_wood_10(g, rng):
    L, W = 10.0, 2.6
    hl = L / 2
    camber = 0.25
    n = 10

    def zdeck(x):
        return camber * math.cos(math.pi * x / L)

    # stringers under the deck (continuous timbers following the camber)
    for y in (-0.9, 0.0, 0.9):
        pts = [(-hl + L * i / n, zdeck(-hl + L * i / n)) for i in range(n + 1)]
        outline = [(x + hl, z - 0.05) for (x, z) in pts] + [(x + hl, z - 0.35) for (x, z) in reversed(pts)]
        g.plate(Frame((-hl - 0.2, y + 0.11, 0.0), (1, 0, 0), (0, 0, 1)), [(u + 0.2, v) for (u, v) in outline], 0.0,
                0.22, "MT_Timber")
    # deck planks (bands of 5 planks as slabs across the stringers)
    for i in range(n * 3):
        x = -hl + (i + 0.5) * L / (n * 3)
        z = zdeck(x)
        g.box(x - L / (n * 3) / 2 + 0.01, -W / 2, z - 0.06, x + L / (n * 3) / 2 - 0.01, W / 2, z + 0.02, "MT_WoodPlanks")
    # trestle bents
    for xb in (-hl / 2, hl / 2):
        for y in (-1.05, 1.05):
            g.box(xb - 0.12, y - 0.12, g.buried(-3.0), xb + 0.12, y + 0.12, zdeck(xb) - 0.32, "MT_Timber")
        g.box(xb - 0.13, -1.25, zdeck(xb) - 0.55, xb + 0.13, 1.25, zdeck(xb) - 0.33, "MT_Timber")
        g.beam((xb, -1.05, -2.2), (xb, 1.05, zdeck(xb) - 0.6), 0.1, 0.12, "MT_Timber", up=(1, 0, 0))
    # rails and posts
    for y in (-W / 2 + 0.08, W / 2 - 0.08):
        for k in range(6):
            i = 1 + k * 28 // 5 if k < 5 else n * 3 - 2
            x = -hl + (i + 0.5) * L / (n * 3)
            g.box(x - 0.07, y - 0.07, zdeck(x) - 0.04, x + 0.07, y + 0.07, zdeck(x) + 1.05, "MT_Timber")
        pts = [(-hl + 0.2 + (L - 0.4) * i / n, 0.0) for i in range(n + 1)]
        outline = [(x + hl, zdeck(x) + 0.98) for (x, _) in pts] + [(x + hl, zdeck(x) + 0.88) for (x, _) in reversed(pts)]
        g.plate(Frame((-hl, y + 0.055, 0.0), (1, 0, 0), (0, 0, 1)), outline, 0.0, 0.11, "MT_Timber")
    return {"attach": {"length": L, "deck_width": W - 0.3, "deck_ends_z": 0.0}}


@asset("SM_Prop_Pier_10m", "Props", "Props", kind="bridge", pivot="base_point", recentre=False, foundation=3.2,
       ground=False, view=(-40, 15),
       notes="Wooden pier: 10 m plank deck (2.8 m wide) running from the shore end at y = +5 out to y = -5 over the "
             "water; deck top at z = 0 (bank level), piles down to z = -3.0, mooring bollards and a ladder at the end.")
def pier_10(g, rng):
    L, W = 10.0, 2.8
    for i in range(20):
        y = 5.0 - (i + 0.5) * L / 20
        g.box(-W / 2, y - L / 40 + 0.012, -0.08, W / 2, y + L / 40 - 0.012, 0.0, "MT_WoodPlanks")
    for x in (-1.0, 0.0, 1.0):
        g.box(x - 0.1, -5.05, -0.3, x + 0.1, 5.05, -0.07, "MT_Timber")
    for k in range(5):
        y = 4.5 - k * 2.25
        for x in (-1.25, 1.25):
            g.cylinder(x, y, 0.14, g.buried(-3.0), 0.35 if k == 4 else -0.05, 8, "MT_Timber")
        g.box(-1.45, y - 0.1, -0.5, 1.45, y + 0.1, -0.29, "MT_Timber")
    for x in (-1.0, 1.0):
        g.lathe([(0.12, -0.05), (0.12, 0.35), (0.16, 0.42), (0.16, 0.5), (0.0, 0.52)], 8, "MT_Iron", x, -4.4)
    # ladder at the end
    for x in (-0.3, 0.3):
        g.box(x - 0.04, -5.18, -1.6, x + 0.04, -5.1, 0.1, "MT_Timber")
    for k in range(4):
        z = -1.35 + 0.35 * k
        g.box(-0.3, -5.17, z, 0.3, -5.11, z + 0.05, "MT_Timber")
    return {"attach": {"deck_z": 0.0, "shore_end_y": 5.0, "water_end_y": -5.0}}


@asset("SM_Prop_Boat_A", "Props", "Props", kind="prop", pivot="base_point", recentre=False, foundation=0.45,
       ground=False, view=(-35, 22),
       notes="Wooden rowing / fishing boat 5.2 m long along Y (bow toward -Y) with two thwarts and oars; origin at "
             "the waterline centre, the hull reaches 0.42 m below it.")
def boat_a(g, rng):
    Lb = 5.2
    ns = 12
    sections = []
    for i in range(ns + 1):
        t = i / ns
        y = -Lb / 2 + Lb * t
        s = math.sin(math.pi * t) ** 0.55
        hw = 0.72 * s + 0.14
        keel = -0.15 - 0.27 * math.sin(math.pi * t) ** 0.35
        sheer = 0.55 + 0.25 * (abs(t - 0.5) * 2) ** 2.2
        sections.append([(-hw, y, sheer), (-hw * 0.92, y, sheer * 0.35 + keel * 0.3),
                         (-hw * 0.5, y, keel * 0.9), (0.0, y, keel), (hw * 0.5, y, keel * 0.9),
                         (hw * 0.92, y, sheer * 0.35 + keel * 0.3), (hw, y, sheer)])
    # outer hull (open top): quads between sections, winding outward
    with g.part(closed=True):
        m = len(sections[0])
        for i in range(ns):
            A, B = sections[i], sections[i + 1]
            for j in range(m - 1):
                g.face([A[j], B[j], B[j + 1], A[j + 1]], "MT_WoodPlanks", "box", True)
        # inner hull (offset inward by scaling) and the gunwale strip closing the top
        inner = []
        for sec in sections:
            y = sec[0][1]
            inner.append([(p[0] * 0.9, y, p[2] * 0.92 + 0.02 if p[2] < 0 else p[2] - 0.0) for p in sec])
        for i in range(ns):
            A, B = inner[i], inner[i + 1]
            for j in range(m - 1):
                g.face([A[j + 1], B[j + 1], B[j], A[j]], "MT_WoodPlanks", "box", True)
        for i in range(ns):
            for (j, s) in ((0, 1), (m - 1, -1)):
                p0, p1 = sections[i][j], sections[i + 1][j]
                q0, q1 = inner[i][j], inner[i + 1][j]
                if s > 0:
                    g.face([p0, q0, q1, p1], "MT_Timber", "box", False)
                else:
                    g.face([p1, q1, q0, p0], "MT_Timber", "box", False)
        # bow and stern caps
        for (sec, isec, flip) in ((sections[0], inner[0], False), (sections[-1], inner[-1], True)):
            ring = list(sec) + list(reversed(isec))
            g.face(ring if not flip else list(reversed(ring)), "MT_WoodPlanks", "box", False)
    # thwarts
    for y in (-0.8, 0.9):
        g.box(-0.78, y - 0.12, 0.3, 0.78, y + 0.12, 0.36, "MT_WoodPlanks")
    # oars resting across the thwarts
    for s in (-1, 1):
        g.beam((s * 0.35, -1.6, 0.42), (s * 0.55, 1.9, 0.44), 0.05, 0.04, "MT_WoodPlanks")
        g.beam((s * 0.55, 1.6, 0.44), (s * 0.58, 2.2, 0.44), 0.16, 0.025, "MT_WoodPlanks")
    return {"attach": {"waterline_z": 0.0, "draft": 0.42}}
