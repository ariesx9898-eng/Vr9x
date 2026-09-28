"""Demon set (Rikarisu inside its crater, Wenport): organic lumpy rock domes and chunky chamfered blocks of dark
red-brown volcanic stone (MT_DemonRock), hide curtains and awnings (MT_Hide) on bone poles, bone tusks and totems
(MT_Bone) and glowing magic crystals (MT_Crystal)."""
import math

from mathutils import Vector

from arch_geo import Frame
from arch_parts import (Opening, arch_poly, circle_poly, crystal_lantern, frame_ring, poly_body, rect, window_trim,
                        wplate)
from arch_registry import asset
from arch_vfx import bez, tube

ROCK = "MT_DemonRock"


def chamfer_rect(x0, y0, x1, y1, c):
    """Rectangle footprint with 45 degree corner chamfers of size c (CCW)."""
    return [(x0 + c, y0), (x1 - c, y0), (x1, y0 + c), (x1, y1 - c), (x1 - c, y1), (x0 + c, y1), (x0, y1 - c),
            (x0, y0 + c)]


def lumpy_dome(g, rng, cx, cy, r, h, mat=ROCK, segs=24, rings=9, amp=0.07, z0=0.0, sy=1.0):
    """Organic dome: smooth lumps from a few random harmonics; its bottom ring is buried 0.35 m."""
    harm = [(rng.randint(2, 5), rng.uniform(0, math.tau), rng.uniform(0.4, 1.0), rng.uniform(0, math.pi))
            for _ in range(3)]
    zb = g.buried(z0 - 0.35)
    rows = []
    for k in range(rings + 1):
        t = k / rings
        a = t * math.pi / 2
        base_r = r * math.cos(a) ** 0.9
        z = z0 + h * math.sin(a)
        if k == 0:
            z = zb
            base_r = r * 1.02
        ring = []
        for i in range(segs):
            th = math.tau * i / segs
            f = sum(c * math.sin(m * th + ph) * math.sin(math.pi * min(t, 0.9) + ps) for (m, ph, c, ps) in harm)
            rr = base_r * (1 + amp * f / 1.6) if k < rings else 0.0
            ring.append((cx + rr * math.cos(th), cy + rr * math.sin(th) * sy, z + (0.0 if k in (0, rings) else
                                                                                 amp * r * 0.3 * f / 1.6)))
        rows.append(ring)
    with g.part():
        for k in range(rings):
            A, B = rows[k], rows[k + 1]
            for i in range(segs):
                j = (i + 1) % segs
                g.face([A[i], A[j], B[j], B[i]], mat, "box", True)
        g.face(list(reversed(rows[0])), mat, "box", False)
    return z0 + h


def crystal(g, x, y, z, h, r, tilt=(0.0, 0.0), sides=6, mat="MT_Crystal"):
    """Pointed hexagonal crystal standing at (x, y, z) (base buried 0.1 m), optionally tilted (degrees x/y)."""
    with g.xf(loc=(x, y, z), rotx=tilt[0], roty=tilt[1]):
        g.lathe([(r * 0.8, -0.1), (r, h * 0.55), (r * 0.7, h * 0.8), (0.0, h)], sides, mat, smooth=False)


def tusk(g, p0, p1, p2, p3, r0, segs=7, mat="MT_Bone"):
    path = bez(p0, p1, p2, p3, 10)
    tube(g, path, [r0 * (1 - k / 10) ** 0.8 for k in range(11)], segs, mat, smooth=True)


def entrance(g, cx, y_face, z0, w=1.9, d=1.5, h=2.9, door_w=1.15, door_h=2.1, tusks=True, back=0.9):
    """Chamfered entrance block projecting toward -Y from a dome/wall at y_face, with a hide-curtained arched door."""
    zb = g.buried(z0 - 0.3)
    fp = chamfer_rect(cx - w / 2, y_face - d, cx + w / 2, y_face + back, 0.3)
    ops = {0: [Opening(arch_poly((w - 0.6) / 2 - door_w / 2, z0 + 0.05 - zb, door_w, door_h, 8), depth=0.35,
                       back="MT_Hide", kind="door")]}
    frames = poly_body(g, fp, zb, z0 + h, ROCK, ops=ops)
    fr, Lf = frames[0]
    frame_ring(g, fr, ops[0][0], width=0.18, mat=ROCK, open_bottom=True)
    # rounded cap on the entrance block
    rings = []
    for (off, dz) in ((0.06, -0.12), (0.1, 0.12), (-0.25, 0.4)):
        rings.append([(p[0] + (off if p[0] > cx else -off), p[1] + (-off if p[1] < y_face else off), z0 + h + dz)
                      for p in fp])
    g.loft(rings, ROCK, cap0=True, cap1=True)
    if tusks:
        for s in (-1, 1):
            bx = cx + s * (w / 2 + 0.05)
            tusk(g, (bx, y_face - d + 0.2, z0 + 0.3), (bx + s * 0.5, y_face - d - 0.5, z0 + 1.2),
                 (bx + s * 0.35, y_face - d - 0.7, z0 + 2.6), (bx - s * 0.1, y_face - d - 0.35, z0 + 3.3), 0.14)
    return fr


# ---------------------------------------------------------------------------------------------- huts

def rubble(g, rng, cx, cy, R, n, sz=0.35):
    """A few faceted rocks scattered around (cx, cy) at radius ~R, half buried."""
    from arch_vfx import hull_rock
    for k in range(n):
        a = rng.uniform(0, math.tau)
        rr = R * rng.uniform(0.95, 1.12)
        s = sz * rng.uniform(0.7, 1.3)
        hull_rock(g, rng, (cx + rr * math.cos(a), cy + rr * math.sin(a), s * 0.25 - 0.003 * k), (s, s * 0.8, s * 0.6),
                  14, ROCK)


@asset("SM_Demon_Hut_A", "Demon", "Demon", budget=(600, 6000),
       notes="Round rock hut: lumpy dome with a chamfered entrance block, hide door curtain, bone tusks and a crystal.")
def demon_hut_a(g, rng):
    top = lumpy_dome(g, rng, 0.0, 0.3, 2.7, 3.0)
    entrance(g, 0.0, -1.7, 0.1)
    rubble(g, rng, 0.0, 0.5, 2.85, 5)
    crystal(g, 0.25, 0.4, top - 0.35, 0.9, 0.14, tilt=(8, -6))
    crystal(g, -0.2, 0.2, top - 0.3, 0.6, 0.1, tilt=(-12, 10))


@asset("SM_Demon_Hut_B", "Demon", "Demon", budget=(600, 6000),
       notes="Twin-dome rock hut with an entrance block and a hide awning on bone poles.")
def demon_hut_b(g, rng):
    lumpy_dome(g, rng, -0.9, 0.4, 2.4, 2.8)
    lumpy_dome(g, rng, 1.7, 0.9, 1.7, 2.1)
    entrance(g, -0.9, -1.6, 0.1, tusks=False)
    # hide awning in front of the small dome, on two bone poles
    _hide_canopy(g, rng, 0.6, 3.2, -2.9, 0.7, 1.8, 2.05)
    crystal(g, -0.8, 0.5, 2.55, 0.8, 0.13, tilt=(6, 8))


@asset("SM_Demon_Hut_C", "Demon", "Demon", budget=(600, 6000),
       notes="Wide low rock hut with a stone smoke pipe, a hide-covered side opening and a bone drying rack.")
def demon_hut_c(g, rng):
    top = lumpy_dome(g, rng, 0.0, 0.5, 3.3, 2.5, sy=0.85)
    rubble(g, rng, 0.0, 0.9, 3.2, 4)
    entrance(g, -0.8, -1.9, 0.1, w=1.8, d=1.2, h=2.6, door_w=1.05, door_h=1.9, tusks=True)
    # smoke pipe
    g.lathe([(0.42, 1.2), (0.38, 3.2), (0.48, 3.3), (0.48, 3.55)], 8, ROCK, 1.2, 1.1, smooth=False)
    # drying rack: two bone posts and a crossbar with hides
    for x in (1.9, 3.5):
        g.cylinder(x, -2.2, 0.07, g.buried(-0.3), 2.2, 6, "MT_Bone")
    with g.xf(loc=(2.7, -2.2, 2.05), roty=90):
        g.cylinder(0, 0, 0.06, -0.95, 0.95, 6, "MT_Bone")
    for k in range(2):
        x = 2.25 + 0.9 * k
        g.box(x - 0.3, -2.23, 1.05, x + 0.3, -2.19, 2.02, "MT_Hide")


def _hide_canopy(g, rng, x0, x1, y0, y1, z_lo, z_hi, poles=True):
    """Hide sheet sloping from (y1, z_hi) down to (y0, z_lo), sagging in the middle, on bone poles at the low edge."""
    nx, ny = 6, 4
    th = 0.03
    top = []
    for i in range(nx + 1):
        row = []
        for j in range(ny + 1):
            u, v = i / nx, j / ny
            x = x0 + (x1 - x0) * u
            y = y0 + (y1 - y0) * v
            z = z_lo + (z_hi - z_lo) * v - 0.18 * math.sin(math.pi * u) * math.sin(math.pi * max(v, 0.15))
            row.append((x, y, z))
        top.append(row)
    with g.part():
        for i in range(nx):
            for j in range(ny):
                a, b, c, d = top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1]
                g.face([a, b, c, d], "MT_Hide", "box", True)
                g.face([(p[0], p[1], p[2] - th) for p in (d, c, b, a)], "MT_Hide", "box", True)
        # rim
        loop = [top[i][0] for i in range(nx + 1)] + [top[nx][j] for j in range(1, ny + 1)] + \
               [top[i][ny] for i in range(nx - 1, -1, -1)] + [top[0][j] for j in range(ny - 1, 0, -1)]
        n = len(loop)
        for k in range(n):
            p, q = loop[k], loop[(k + 1) % n]
            g.face([p, (p[0], p[1], p[2] - th), (q[0], q[1], q[2] - th), q], "MT_Hide", "box", False)
    if poles:
        for x in (x0 + 0.1, x1 - 0.1):
            g.cylinder(x, y0 + 0.1, 0.07, g.buried(-0.3), z_lo + 0.02, 6, "MT_Bone")
            g.sphere(x, y0 + 0.1, z_lo + 0.1, 0.12, 8, 5, "MT_Bone")


# ---------------------------------------------------------------------------------------------- houses

@asset("SM_Demon_House_A", "Demon", "Demon",
       notes="Carved-block demon house: chunky chamfered lower block, stepped upper block under a lumpy dome, "
             "recessed windows, hide door and awning, crystal lanterns.")
def demon_house_a(g, rng):
    W, D = 8.4, 7.4
    zb = g.buried(-0.45)
    fp = chamfer_rect(-W / 2, -D / 2, W / 2, D / 2, 0.9)
    L0 = math.dist(fp[0], fp[1])
    ops = {0: [Opening(arch_poly(L0 / 2 - 0.65, 0.2 - zb, 1.3, 2.4, 8), depth=0.4, back="MT_Hide", kind="door"),
               Opening(arch_poly(0.5, 1.2 - zb, 0.8, 1.2, 6), depth=0.35, kind="arch"),
               Opening(arch_poly(L0 - 1.3, 1.2 - zb, 0.8, 1.2, 6), depth=0.35, kind="arch")],
           2: [Opening(arch_poly(math.dist(fp[2], fp[3]) / 2 - 0.4, 1.2 - zb, 0.8, 1.2, 6), depth=0.35, kind="arch")],
           6: [Opening(arch_poly(math.dist(fp[6], fp[7]) / 2 - 0.4, 1.2 - zb, 0.8, 1.2, 6), depth=0.35, kind="arch")]}
    H0 = 3.8
    frames = poly_body(g, fp, zb, H0, ROCK, ops=ops)
    for e, lst in ops.items():
        fr, _ = frames[e]
        for op in lst:
            if op.kind == "door":
                frame_ring(g, fr, op, width=0.2, mat=ROCK, open_bottom=True)
            else:
                window_trim(g, fr, op, dict(trim=ROCK, frame_w=0.14, sill_mat=ROCK, mullions=False, shutters=False))
    # overhanging rounded slab between the blocks
    ring = lambda off, z: [(p[0] * (1 + off / (W / 2)), p[1] * (1 + off / (D / 2)), z) for p in fp]
    g.loft([ring(0.02, H0 - 0.15), ring(0.28, H0 + 0.05), ring(0.3, H0 + 0.3), ring(0.05, H0 + 0.45)], ROCK,
           cap0=True, cap1=True)
    # upper block and dome
    fp2 = chamfer_rect(-2.6, -1.9, 2.6, 2.9, 0.7)
    uops = {0: [Opening(arch_poly(math.dist(fp2[0], fp2[1]) / 2 - 0.45, 0.9 + 0.3, 0.9, 1.3, 6), depth=0.3,
                        kind="arch")]}
    ufr = poly_body(g, fp2, H0 + 0.1, H0 + 3.0, ROCK, ops=uops)
    for op in uops[0]:
        window_trim(g, ufr[0][0], op, dict(trim=ROCK, frame_w=0.12, sill_mat=ROCK, mullions=False))
    top = lumpy_dome(g, rng, 0.0, 0.5, 3.0, 2.3, z0=H0 + 2.9, segs=16, rings=6)
    crystal(g, 0.0, 0.5, top - 0.3, 1.1, 0.16)
    # hide awning over the door on bone brackets
    fr0, _ = frames[0]
    _hide_canopy(g, rng, -1.6, 1.6, -D / 2 - 1.5, -D / 2 + 0.1, 2.75, 3.25)
    for s in (-1, 1):
        crystal_lantern(g, fr0, L0 / 2 + s * 1.15, 2.6, out=0.45)


@asset("SM_Demon_House_B", "Demon", "Demon",
       notes="Tall demon tower-house: tapering chamfered rock block with slit windows, a hide-covered balcony box, "
             "a lumpy cap and bone tusks at the top corners.")
def demon_house_b(g, rng):
    zb = g.buried(-0.45)
    b0, b1, H = 3.1, 2.4, 10.0
    fb = chamfer_rect(-b0 - (b0 - b1) * (-zb) / H, -b0 - (b0 - b1) * (-zb) / H, b0 + (b0 - b1) * (-zb) / H,
                      b0 + (b0 - b1) * (-zb) / H, 0.8)
    ft = chamfer_rect(-b1, -b1, b1, b1, 0.62)
    faces = []
    n = len(fb)
    ops = {}
    for i in range(n):
        j = (i + 1) % n
        pts = [(fb[i][0], fb[i][1], zb), (fb[j][0], fb[j][1], zb), (ft[j][0], ft[j][1], H), (ft[i][0], ft[i][1], H)]
        Lb = math.dist(fb[i], fb[j])
        lst = []
        if i % 2 == 0:
            for z in (3.6, 6.8):
                lst.append(Opening(arch_poly(Lb / 2 - 0.25, z - zb, 0.5, 1.2, 6), depth=0.35, kind="arch"))
        if i == 0:
            lst = [Opening(arch_poly(Lb / 2 - 0.6, 0.2 - zb, 1.2, 2.3, 8), depth=0.4, back="MT_Hide", kind="door"),
                   Opening(arch_poly(Lb / 2 - 0.25, 6.8 - zb, 0.5, 1.2, 6), depth=0.35, kind="arch")]
        ops[i] = lst
        faces.append((pts, ROCK, lst))
    faces.append(([(p[0], p[1], H) for p in ft], ROCK, None))
    faces.append(([(p[0], p[1], zb) for p in reversed(fb)], ROCK, None))
    from arch_parts import solid
    frames = solid(g, faces)
    for i in range(n):
        for op in ops[i]:
            if op.kind == "door":
                frame_ring(g, frames[i], op, width=0.2, mat=ROCK, open_bottom=True)
            else:
                window_trim(g, frames[i], op, dict(trim=ROCK, frame_w=0.12, sill=False, mullions=False))
    # hide-covered balcony box on the front at mid height
    fr0 = frames[0]
    Lb0 = math.dist(fb[0], fb[1])
    wplate(g, fr0, rect(Lb0 / 2 - 1.1, (4.9 - zb) / 1.0, 2.2, 0.16), -0.2, 0.95, ROCK)
    g.plate(fr0, rect(Lb0 / 2 - 1.05, (5.0 - zb), 2.1, 0.9), 0.8, 0.9, "MT_Hide")
    for u in (Lb0 / 2 - 1.0, Lb0 / 2 + 1.0):
        bfr = Frame(fr0.p(u, 0, 0), fr0.n, fr0.v, None)
        g.plate(bfr, [(-0.2, 4.2 - zb), (0.0, 4.2 - zb), (0.85, 4.91 - zb), (-0.2, 4.91 - zb)], -0.07, 0.07, "MT_Bone")
    # cap
    rings = []
    for (sc, z) in ((1.0, H - 0.2), (1.12, H + 0.25), (0.95, H + 0.9), (0.5, H + 1.6), (0.0, H + 2.0)):
        rings.append([(p[0] * sc, p[1] * sc, z) for p in ft])
    g.loft(rings, ROCK, cap0=True, cap1=False)
    for (cx, cy) in ((-b1, -b1), (b1, -b1), (b1, b1), (-b1, b1)):
        tusk(g, (cx * 0.9, cy * 0.9, H - 0.1), (cx * 1.25, cy * 1.25, H + 0.4), (cx * 1.35, cy * 1.35, H + 1.3),
             (cx * 1.2, cy * 1.2, H + 2.1), 0.16)
    crystal(g, 0.0, 0.0, H + 1.85, 1.3, 0.18)


# ---------------------------------------------------------------------------------------------- stall / totem

@asset("SM_Demon_Stall_A", "Demon", "Demon", kind="prop", budget=(300, 4000),
       notes="Demon market stall: carved rock counter, sagging hide canopy on four bone poles, clay pots, crates "
             "and a glowing crystal lamp.")
def demon_stall_a(g, rng):
    fp = chamfer_rect(-1.6, -0.55, 1.6, 0.55, 0.2)
    poly_body(g, fp, g.buried(-0.3), 0.95, ROCK)
    g.loft([[(p[0] * 1.02, p[1] * 1.04, 0.9) for p in fp], [(p[0] * 1.06, p[1] * 1.16, 1.02) for p in fp],
            [(p[0] * 1.02, p[1] * 1.08, 1.1) for p in fp]], ROCK, cap0=True, cap1=True)
    _hide_canopy(g, rng, -2.0, 2.0, -1.4, 1.2, 2.1, 2.55, poles=False)
    for (x, y, z1) in ((-1.9, -1.3, 2.12), (1.9, -1.3, 2.12), (-1.9, 1.1, 2.52), (1.9, 1.1, 2.52)):
        g.cylinder(x, y, 0.07, g.buried(-0.3), z1, 6, "MT_Bone")
        g.sphere(x, y, z1 + 0.08, 0.11, 8, 5, "MT_Bone")
    # goods on the counter
    for k, x in enumerate((-1.1, -0.55, 0.35)):
        r = 0.18 + 0.04 * (k % 2)
        g.lathe([(0.1, 1.09), (r, 1.25), (r * 0.9, 1.45), (0.09, 1.55), (0.11, 1.62), (0.0, 1.63)], 10, ROCK, x, 0.1)
    g.box(0.8, -0.35, 1.07, 1.35, 0.25, 1.45, "MT_WoodPlanks")
    g.lathe([(0.1, 1.085), (0.12, 1.2), (0.0, 1.22)], 6, "MT_Iron", 1.35, -0.3)
    crystal(g, 1.35, -0.3, 1.215, 0.45, 0.07)


@asset("SM_Demon_Totem_A", "Demon", "Demon", kind="prop", budget=(300, 4000),
       notes="Totem of stacked carved demon-rock blocks with glowing crystal eyes, bone horns and a crystal crown "
             "(~6 m).")
def demon_totem_a(g, rng):
    zc = g.buried(-0.4)
    blocks = [(1.3, 1.6), (1.05, 1.5), (0.85, 1.4)]
    for k, (sz, h) in enumerate(blocks):
        fp = chamfer_rect(-sz / 2 - 0.1, -sz / 2, sz / 2 + 0.1, sz / 2, 0.22)
        z0 = zc
        z1 = h if k == 0 else z0 + h
        ops = {}
        if k == 1:
            Lf = math.dist(fp[0], fp[1])
            H = z1 - z0
            ops = {0: [Opening(circle_poly(Lf / 2 - 0.22, H - 0.55, 0.13, 8), depth=0.12, back="MT_Crystal",
                               kind="window"),
                       Opening(circle_poly(Lf / 2 + 0.22, H - 0.55, 0.13, 8), depth=0.12, back="MT_Crystal",
                               kind="window"),
                       Opening(rect(Lf / 2 - 0.3, 0.35, 0.6, 0.18), depth=0.1, back=ROCK, kind="window")]}
        poly_body(g, fp, z0, z1, ROCK, ops=ops)
        # rounded slab on top of each block
        g.loft([[(p[0] * 1.1, p[1] * 1.15, z1 - 0.1) for p in fp], [(p[0] * 1.2, p[1] * 1.28, z1 + 0.02) for p in fp],
                [(p[0] * 1.05, p[1] * 1.1, z1 + 0.14) for p in fp]], ROCK, cap0=True, cap1=True)
        zc = z1 + 0.07
    # horns and crown
    for s in (-1, 1):
        tusk(g, (s * 0.35, 0.0, zc - 0.3), (s * 1.0, -0.2, zc + 0.1), (s * 1.2, -0.1, zc + 0.9), (s * 0.9, 0.1, zc + 1.5),
             0.16)
    crystal(g, 0.0, 0.0, zc - 0.1, 1.4, 0.22)
    crystal(g, 0.25, 0.1, zc - 0.1, 0.8, 0.12, tilt=(-10, 18))
    crystal(g, -0.25, -0.05, zc - 0.1, 0.7, 0.11, tilt=(8, -20))


# ---------------------------------------------------------------------------------------------- tower

@asset("SM_Demon_Tower_A", "Demon", "Demon", budget=(1500, 8000),
       notes="Demon watch tower: tall tapering chamfered rock shaft with slit windows and bone buttresses, a flared "
             "lumpy crown and a large glowing crystal spike on top (~20 m).")
def demon_tower_a(g, rng):
    zb = g.buried(-0.5)
    H = 14.0
    b0, b1 = 2.8, 1.9
    fb = chamfer_rect(-b0, -b0, b0, b0, 1.0)
    ft = chamfer_rect(-b1, -b1, b1, b1, 0.7)
    n = len(fb)
    faces = []
    ops = {}
    for i in range(n):
        j = (i + 1) % n
        pts = [(fb[i][0], fb[i][1], zb), (fb[j][0], fb[j][1], zb), (ft[j][0], ft[j][1], H), (ft[i][0], ft[i][1], H)]
        Lb = math.dist(fb[i], fb[j])
        lst = []
        if i % 2 == 0:
            for z in (4.0, 8.0, 11.5):
                if i == 0 and z == 4.0:
                    continue
                lst.append(Opening(arch_poly(Lb / 2 - 0.2, z - zb, 0.4, 1.1, 6), depth=0.35, kind="arch"))
        if i == 0:
            lst.append(Opening(arch_poly(Lb / 2 - 0.6, 0.2 - zb, 1.2, 2.4, 8), depth=0.4, back="MT_Hide", kind="door"))
        ops[i] = lst
        faces.append((pts, ROCK, lst))
    faces.append(([(p[0], p[1], H) for p in ft], ROCK, None))
    faces.append(([(p[0], p[1], zb) for p in reversed(fb)], ROCK, None))
    from arch_parts import solid
    frames = solid(g, faces)
    for i in range(n):
        for op in ops[i]:
            if op.kind == "door":
                frame_ring(g, frames[i], op, width=0.2, mat=ROCK, open_bottom=True)
            else:
                window_trim(g, frames[i], op, dict(trim=ROCK, frame_w=0.1, sill=False, mullions=False))
    # bone buttress ribs on the chamfer faces
    for i in (1, 3, 5, 7):
        p0 = Vector(((fb[i][0] + fb[(i + 1) % n][0]) / 2, (fb[i][1] + fb[(i + 1) % n][1]) / 2, 0.0))
        p3 = Vector(((ft[i][0] + ft[(i + 1) % n][0]) / 2, (ft[i][1] + ft[(i + 1) % n][1]) / 2, H * 0.8))
        out = p0.normalized() * 0.9
        tusk(g, tuple(p0 * 1.02 + Vector((0, 0, -0.2))), tuple(p0 + out + Vector((0, 0, 3.0))),
             tuple(p3 + out * 0.8 + Vector((0, 0, 1.0))), tuple(p3 * 0.95 + Vector((0, 0, 2.2))), 0.28)
    # flared crown and crystal
    rings = []
    for (sc, z) in ((1.0, H - 0.3), (1.35, H + 0.4), (1.3, H + 1.0), (0.9, H + 1.3)):
        rings.append([(p[0] * sc, p[1] * sc, z) for p in ft])
    g.loft(rings, ROCK, cap0=True, cap1=True)
    crystal(g, 0.0, 0.0, H + 1.2, 4.2, 0.55, sides=6)
    for (a, t) in ((0, 22), (120, 22), (240, 22)):
        ax, ay = math.cos(math.radians(a)), math.sin(math.radians(a))
        crystal(g, ax * 0.6, ay * 0.6, H + 1.2, 1.6, 0.25, tilt=(-t * ay, t * ax))
