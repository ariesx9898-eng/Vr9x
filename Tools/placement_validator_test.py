#!/usr/bin/env python3
"""
Python mirror of the pure 2D maths in Source/MushokuRPG/Private/World/MTPlacementValidator.cpp
(SAT oriented-rectangle overlap, circle tests, polyline distance, min spacing, registry check order)
plus unit tests. Keep both files in sync: same formulas, same epsilon, same corner order.

Run:  python3 Tools/placement_validator_test.py      (exit code 0 == all PASS)
"""
import math
import random
import sys

OVERLAP_EPS = 1e-3
HUGE = 1e12


class Footprint:
    def __init__(self, cx, cy, hx, hy, yaw=0.0, pad=0.0):
        self.c = (float(cx), float(cy))
        self.h = (float(hx), float(hy))
        self.yaw = float(yaw)
        self.pad = float(pad)

    def axis_x(self):
        return rot((1.0, 0.0), self.yaw)

    def axis_y(self):
        return rot((0.0, 1.0), self.yaw)

    def padded(self):
        return (self.h[0] + self.pad, self.h[1] + self.pad)


def rot(v, yaw_deg):
    r = math.radians(yaw_deg)
    c, s = math.cos(r), math.sin(r)
    return (v[0] * c - v[1] * s, v[0] * s + v[1] * c)


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1])


def mul(a, k):
    return (a[0] * k, a[1] * k)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1]


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def to_local(f, p):
    return rot(sub(p, f.c), -f.yaw)


def corners(f, include_padding):
    h = f.padded() if include_padding else f.h
    ax, ay = mul(f.axis_x(), h[0]), mul(f.axis_y(), h[1])
    return [add(add(f.c, ax), ay), add(sub(f.c, ax), ay), sub(sub(f.c, ax), ay), sub(add(f.c, ax), ay)]


def footprints_overlap(a, b):
    ha, hb = a.padded(), b.padded()
    ax, ay, bx, by = a.axis_x(), a.axis_y(), b.axis_x(), b.axis_y()
    d = sub(b.c, a.c)
    for n in (ax, ay, bx, by):
        ra = abs(dot(ax, n)) * ha[0] + abs(dot(ay, n)) * ha[1]
        rb = abs(dot(bx, n)) * hb[0] + abs(dot(by, n)) * hb[1]
        if abs(dot(d, n)) >= ra + rb - OVERLAP_EPS:
            return False
    return True


def dist_point_aabb(p, h):
    dx = max(abs(p[0]) - h[0], 0.0)
    dy = max(abs(p[1]) - h[1], 0.0)
    return math.hypot(dx, dy)


def circle_overlaps_footprint(c, r, f):
    return dist_point_aabb(to_local(f, c), f.padded()) < r - OVERLAP_EPS


def circles_overlap(ca, ra, cb, rb):
    return dist(ca, cb) < ra + rb - OVERLAP_EPS


def dist_point_segment(p, a, b):
    ab = sub(b, a)
    l2 = dot(ab, ab)
    if l2 < 1e-8:
        return dist(p, a)
    t = min(max(dot(sub(p, a), ab) / l2, 0.0), 1.0)
    return dist(p, add(a, mul(ab, t)))


def dist_point_polyline(p, pts):
    if len(pts) < 2:
        return dist(p, pts[0]) if len(pts) == 1 else HUGE
    return min(dist_point_segment(p, pts[i], pts[i + 1]) for i in range(len(pts) - 1))


def segment_intersects_aabb(a, b, h):
    if max(a[0], b[0]) < -h[0] or min(a[0], b[0]) > h[0]:
        return False
    if max(a[1], b[1]) < -h[1] or min(a[1], b[1]) > h[1]:
        return False
    d = sub(b, a)
    ln = math.hypot(*d)
    if ln < 1e-8:
        return True
    n = (-d[1] / ln, d[0] / ln)
    return abs(dot(n, a)) <= abs(n[0]) * h[0] + abs(n[1]) * h[1]


def dist_footprint_polyline(f, pts):
    h = f.padded()
    cs = corners(f, True)
    best = HUGE
    for i in range(len(pts) - 1):
        la, lb = to_local(f, pts[i]), to_local(f, pts[i + 1])
        if segment_intersects_aabb(la, lb, h):
            return 0.0
        best = min(best, dist_point_aabb(la, h), dist_point_aabb(lb, h))
        for c in cs:
            best = min(best, dist_point_segment(c, pts[i], pts[i + 1]))
    return best


def meets_min_spacing(c, existing, min_spacing):
    return all((c[0] - e[0]) ** 2 + (c[1] - e[1]) ** 2 >= min_spacing * min_spacing for e in existing)


# ------------------------------------------------------------------------------------------------ tests
RESULTS = []


def check(name, cond):
    RESULTS.append((name, bool(cond)))
    print(("PASS " if cond else "FAIL ") + name)


def brute_overlap(a, b, steps=90):
    """Independent reference: sample points of A (interior grid) and test containment in B (and vice versa)."""
    def inside(f, p):
        l = to_local(f, p)
        h = f.padded()
        return abs(l[0]) < h[0] - 1e-6 and abs(l[1]) < h[1] - 1e-6

    for f, g in ((a, b), (b, a)):
        h = f.padded()
        for i in range(steps + 1):
            for j in range(steps + 1):
                lx = -h[0] + 2 * h[0] * i / steps
                ly = -h[1] + 2 * h[1] * j / steps
                p = add(f.c, rot((lx, ly), f.yaw))
                if inside(g, p):
                    return True
    return False


def run():
    # SAT basics
    check("sat_identical_boxes_overlap", footprints_overlap(Footprint(0, 0, 100, 50), Footprint(0, 0, 100, 50)))
    check("sat_separated_on_x", not footprints_overlap(Footprint(0, 0, 100, 50), Footprint(250, 0, 100, 50)))
    check("sat_touching_edges_do_not_overlap", not footprints_overlap(Footprint(0, 0, 100, 50), Footprint(200, 0, 100, 50)))
    check("sat_padding_creates_overlap", footprints_overlap(Footprint(0, 0, 100, 50, 0, 10), Footprint(210, 0, 100, 50, 0, 10)))
    # Rotated case where the axis-aligned bounding boxes overlap but the rectangles do not (needs a B axis).
    a = Footprint(0, 0, 100, 100, 45)
    b = Footprint(150, 150, 100, 100, 45)
    aabb_overlap = True  # AABB half extent of a 45deg square = 141 -> |150| < 282
    check("sat_rotated_diamonds_separated_although_aabbs_overlap", aabb_overlap and not footprints_overlap(a, b))
    check("sat_rotated_diamonds_overlap_when_close", footprints_overlap(a, Footprint(100, 100, 100, 100, 45)))
    check("sat_symmetric", footprints_overlap(a, b) == footprints_overlap(b, a))
    # Long thin house vs rotated barn corner poke
    check("sat_corner_poke_detected", footprints_overlap(Footprint(0, 0, 500, 100, 0), Footprint(0, 230, 100, 100, 45)))
    check("sat_corner_miss_detected", not footprints_overlap(Footprint(0, 0, 500, 100, 0), Footprint(0, 250, 100, 100, 45)))

    # Randomised agreement with the brute-force reference (deterministic seed)
    rng = random.Random(1717)
    agree = 0
    total = 250
    for _ in range(total):
        fa = Footprint(rng.uniform(-300, 300), rng.uniform(-300, 300), rng.uniform(20, 200), rng.uniform(20, 200), rng.uniform(0, 180))
        fb = Footprint(rng.uniform(-300, 300), rng.uniform(-300, 300), rng.uniform(20, 200), rng.uniform(20, 200), rng.uniform(0, 180))
        if footprints_overlap(fa, fb) == brute_overlap(fa, fb, 40):
            agree += 1
    check(f"sat_matches_bruteforce_{agree}_of_{total}", agree >= total - 2)  # sampling grid may miss sliver overlaps

    # Circles
    f = Footprint(0, 0, 100, 50, 30)
    check("circle_inside_rect_overlaps", circle_overlaps_footprint((0, 0), 10, f))
    check("circle_far_no_overlap", not circle_overlaps_footprint((400, 0), 50, f))
    check("circle_near_rotated_corner", circle_overlaps_footprint(add(corners(f, False)[0], rot((20, 0), 30)), 25, f))
    check("circle_tangent_no_overlap", not circle_overlaps_footprint((150, 0), 50, Footprint(0, 0, 100, 50)))
    check("circles_overlap", circles_overlap((0, 0), 100, (150, 0), 60))
    check("circles_touching_no_overlap", not circles_overlap((0, 0), 100, (160, 0), 60))

    # Polyline distances
    road = [(0, 0), (1000, 0), (1000, 1000)]
    check("point_polyline_distance", abs(dist_point_polyline((500, 300), road) - 300) < 1e-6)
    check("point_polyline_second_segment", abs(dist_point_polyline((1300, 500), road) - 300) < 1e-6)
    check("footprint_crossing_road_is_zero", dist_footprint_polyline(Footprint(500, 0, 100, 100, 20), road) == 0.0)
    d = dist_footprint_polyline(Footprint(500, 600, 100, 100, 0), road)
    check(f"footprint_road_distance_{d:.1f}", abs(d - 400.0) < 1e-6)
    d45 = dist_footprint_polyline(Footprint(500, 600, 100, 100, 45), road)
    # nearest feature is the vertical segment x=1000: right diamond vertex at x = 500 + 100*sqrt(2)
    check(f"rotated_footprint_road_distance_{d45:.2f}", abs(d45 - (1000 - (500 + 100 * math.sqrt(2)))) < 1e-6)
    check("footprint_padding_reduces_distance",
          abs(dist_footprint_polyline(Footprint(500, 600, 100, 100, 0, 50), road) - 350.0) < 1e-6)

    # Min spacing (houses >= 25 m apart)
    houses = [(0, 0), (3000, 0)]
    check("spacing_ok_at_25m", meets_min_spacing((0, 2500), houses, 2500))
    check("spacing_rejects_24m", not meets_min_spacing((0, 2400), houses, 2500))
    check("spacing_empty_ok", meets_min_spacing((0, 0), [], 2500))

    # Determinism: same inputs -> same outputs across runs
    seq1 = [footprints_overlap(Footprint(i * 37 % 400, i * 91 % 400, 120, 80, i * 13 % 180),
                               Footprint(200, 200, 150, 60, 33)) for i in range(200)]
    seq2 = [footprints_overlap(Footprint(i * 37 % 400, i * 91 % 400, 120, 80, i * 13 % 180),
                               Footprint(200, 200, 150, 60, 33)) for i in range(200)]
    check("deterministic_results", seq1 == seq2)

    failed = [n for n, ok in RESULTS if not ok]
    print(f"\n{len(RESULTS) - len(failed)}/{len(RESULTS)} tests passed")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(run())
