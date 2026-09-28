"""Road network: least-cost paths (A*) on the 18 m grid, smoothing, grade-limited profiles and bridges.

Move cost (per metre) = 1 + grade penalty (quadratic to 8 %, steep above 12 %, prohibitive above 20 %)
                         + side-slope penalty (cut/fill on steep hillsides) + altitude penalty (snow line)
                         + wetness penalty (marsh near water); sea and lakes are forbidden; entering a river
                         costs a fixed bridge penalty. Cells already on a road cost 45 % (roads merge and share).
16 move directions (incl. knight moves) so paths can follow contours and switch back smoothly.
The cell path is smoothed (corner cutting + Laplacian), resampled at 6 m, and given a Z profile that follows the
terrain but is grade-limited to 12 % (bridges are raised above the water, towns set the end levels).
"""
import heapq
import math

import numpy as np

import wg_config as C
import wg_geo as GEO
import wg_grid as G
from wg_grid import smoothstep

MOVES = [(0, 1), (1, 0), (0, -1), (-1, 0), (1, 1), (1, -1), (-1, 1), (-1, -1),
         (1, 2), (2, 1), (-1, 2), (-2, 1), (1, -2), (2, -1), (-1, -2), (-2, -1)]


class CostGrid:
    def __init__(self, h1, water1, sea1, lake1, river1, region1, sites, lv1=G.L1, lv=G.L2):
        f = lv.step // lv1.step
        self.lv = lv
        self.h = G.downsample(h1, f).astype(np.float64)
        H, W = self.h.shape
        self.H, self.W = H, W
        self.sea = G.downsample(sea1.astype(np.float32), f) > 0.35
        self.lake = G.downsample(lake1.astype(np.float32), f) > 0.35
        self.river = G.max_filter(river1.astype(np.float32), 1)[::f, ::f] > 0.5
        # side slope seen at 9 m (block maximum), so narrow steep banks are visible to the planner
        s1 = G.max_filter(G.slope(h1, lv1.cell), 1)[::f, ::f]
        slope = np.maximum(G.slope(G.blur(self.h, 1.0), lv.cell), 0.8 * s1)
        self.slope = slope
        wet = G.dilate_mask(self.sea | self.lake | self.river, 2)
        # static per-cell cost (per metre)
        c = 1.0 + 30.0 * np.maximum(0.0, slope - 0.18) ** 2 + 8.0 * np.maximum(0.0, slope - 0.35)
        c += np.maximum(0.0, self.h - 420.0) / 180.0
        c += 0.35 * wet
        self.static = c
        self.blocked = self.sea | self.lake
        self.road = np.zeros((H, W), bool)
        self.town = np.zeros((H, W), bool)
        for s in sites:
            sl, d = _disk(lv, s.x, s.y, s.r)
            self.town[sl] |= d <= s.r
        self.blocked &= ~self.town | self.sea

    def astar(self, a, b, max_expand=3_000_000):
        """a, b: (x, y) metres. Returns list of (x, y) metres along the path (grid centres)."""
        lv = self.lv
        H, W = self.H, self.W
        cell = lv.cell
        sx, sy = int(round(a[0] / cell)), int(round(a[1] / cell))
        tx, ty = int(round(b[0] / cell)), int(round(b[1] / cell))
        hl = self.h.ravel().tolist()
        st = self.static.ravel().tolist()
        blk = self.blocked.ravel().tolist()
        riv = self.river.ravel().tolist()
        road = self.road.ravel().tolist()
        start, goal = sy * W + sx, ty * W + tx
        blk[start] = False
        blk[goal] = False
        g = {start: 0.0}
        came = {}
        heap = [(0.0, start)]
        mv = [(dy * W + dx, dy, dx, cell * math.hypot(dy, dx)) for dy, dx in MOVES]
        hscale = 0.55
        closed = set()
        n = 0
        push, pop = heapq.heappush, heapq.heappop
        while heap:
            _, i = pop(heap)
            if i == goal:
                break
            if i in closed:
                continue
            closed.add(i)
            n += 1
            if n > max_expand:
                break
            iy, ix = divmod(i, W)
            gi = g[i]
            hi = hl[i]
            for o, dy, dx, dist in mv:
                jy, jx = iy + dy, ix + dx
                if jy < 1 or jy >= H - 1 or jx < 1 or jx >= W - 1:
                    continue
                j = i + o
                if blk[j]:
                    continue
                if abs(dy) + abs(dx) == 3:
                    # knight move: check the two cells it passes between
                    m1 = i + (dy // 2 if abs(dy) == 2 else 0) * W + (dx // 2 if abs(dx) == 2 else 0)
                    m2 = j - (dy // 2 if abs(dy) == 2 else 0) * W - (dx // 2 if abs(dx) == 2 else 0)
                    if blk[m1] or blk[m2]:
                        continue
                grade = abs(hl[j] - hi) / dist
                if grade > 0.20:
                    continue
                gc = 1.0 + (grade / 0.06) ** 2 + (80.0 * (grade - 0.10) if grade > 0.10 else 0.0)
                c = dist * 0.5 * (st[i] + st[j]) * gc
                if riv[j] and not riv[i]:
                    c += 260.0
                if road[j]:
                    c *= 0.45
                ng = gi + c
                if ng < g.get(j, 1e30):
                    g[j] = ng
                    came[j] = i
                    jy2, jx2 = divmod(j, W)
                    hh = hscale * cell * math.hypot(jy2 - ty, jx2 - tx)
                    push(heap, (ng + hh, j))
        if goal not in came and goal != start:
            return None
        path = [goal]
        while path[-1] != start:
            path.append(came[path[-1]])
        path.reverse()
        return [((k % W) * cell, (k // W) * cell) for k in path]

    def mark(self, pts):
        cell = self.lv.cell
        for x, y in pts:
            j, i = int(round(x / cell)), int(round(y / cell))
            if 0 <= i < self.H and 0 <= j < self.W:
                self.road[max(0, i - 1):i + 2, max(0, j - 1):j + 2] = True


def _disk(lv, cx, cy, r):
    cell = lv.cell
    x0 = max(0, int((cx - r) / cell) - 1)
    x1 = min(lv.W - 1, int((cx + r) / cell) + 1)
    y0 = max(0, int((cy - r) / cell) - 1)
    y1 = min(lv.H - 1, int((cy + r) / cell) + 1)
    xs = np.arange(x0, x1 + 1) * cell
    ys = np.arange(y0, y1 + 1) * cell
    d = np.hypot(xs[None, :] - cx, ys[:, None] - cy)
    return (slice(y0, y1 + 1), slice(x0, x1 + 1)), d


def smooth_path(pts, blocked_fn=None, passes=40):
    p = np.array(pts, np.float64)
    if len(p) < 4:
        return p
    # drop duplicate points
    keep = np.concatenate([[True], np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1])) > 1e-6])
    p = p[keep]
    # resample densely then relax (endpoints fixed)
    p = resample(p, 6.0)
    orig = p.copy()
    for it in range(passes):
        q = p.copy()
        q[1:-1] = 0.25 * p[:-2] + 0.5 * p[1:-1] + 0.25 * p[2:]
        # tether to the A* path so smoothing never drifts more than ~18 m
        dx = q - orig
        dl = np.hypot(dx[:, 0], dx[:, 1])
        lim = 18.0
        over = dl > lim
        q[over] = orig[over] + dx[over] * (lim / dl[over])[:, None]
        if blocked_fn is not None:
            bad = blocked_fn(q)
            q[bad] = p[bad]
        p = q
    p = resample(p, 6.0)
    return limit_curvature(p, blocked_fn)


def limit_curvature(p, blocked_fn=None, r_min=14.0, iters=120):
    """Relax only the points whose turning radius is below r_min (hairpins become proper switchback curves)."""
    p = p.copy()
    for _ in range(iters):
        a, b, c = p[:-2], p[1:-1], p[2:]
        ab = b - a
        bc = c - b
        la = np.hypot(ab[:, 0], ab[:, 1])
        lb = np.hypot(bc[:, 0], bc[:, 1])
        cosang = np.clip((ab[:, 0] * bc[:, 0] + ab[:, 1] * bc[:, 1]) / np.maximum(la * lb, 1e-9), -1.0, 1.0)
        turn = np.arccos(cosang)
        radius = 0.5 * (la + lb) / np.maximum(turn, 1e-6)
        tight = radius < r_min
        if not tight.any():
            break
        # spread the relaxation to the neighbours of tight points as well
        tt = np.zeros(len(p), bool)
        tt[1:-1] = tight
        tt[:-2] |= tight
        tt[2:] |= tight
        tt[0] = tt[-1] = False
        q = p.copy()
        q[1:-1] = np.where(tt[1:-1, None], 0.25 * p[:-2] + 0.5 * p[1:-1] + 0.25 * p[2:], p[1:-1])
        if blocked_fn is not None:
            bad = blocked_fn(q)
            q[bad] = p[bad]
        p = resample(q, 6.0)
    return p


def resample(p, step):
    seg = np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1]))
    s = np.concatenate([[0.0], np.cumsum(seg)])
    if s[-1] <= step:
        return p
    n = int(math.ceil(s[-1] / step)) + 1
    ss = np.linspace(0.0, s[-1], n)
    return np.stack([np.interp(ss, s, p[:, 0]), np.interp(ss, s, p[:, 1])], axis=1)


def seg_intersections(p, q):
    """All intersections between polylines p and q: list of (i_p, t_p, j_q, t_q)."""
    out = []
    a0, a1 = p[:-1], p[1:]
    for j in range(len(q) - 1):
        b0, b1 = q[j], q[j + 1]
        # bbox prefilter
        minx, maxx = min(b0[0], b1[0]), max(b0[0], b1[0])
        miny, maxy = min(b0[1], b1[1]), max(b0[1], b1[1])
        cand = np.nonzero((np.maximum(a0[:, 0], a1[:, 0]) >= minx) & (np.minimum(a0[:, 0], a1[:, 0]) <= maxx) &
                          (np.maximum(a0[:, 1], a1[:, 1]) >= miny) & (np.minimum(a0[:, 1], a1[:, 1]) <= maxy))[0]
        for i in cand:
            r = a1[i] - a0[i]
            s = b1 - b0
            den = r[0] * s[1] - r[1] * s[0]
            if abs(den) < 1e-12:
                continue
            w = b0 - a0[i]
            t = (w[0] * s[1] - w[1] * s[0]) / den
            u = (w[0] * r[1] - w[1] * r[0]) / den
            if 0 <= t <= 1 and 0 <= u <= 1:
                out.append((int(i), float(t), int(j), float(u)))
    return out


def build(D, h1, Hy, sites, log=print):
    lv1 = G.L1
    site_by_id = {s.id: s for s in sites}
    sea1 = ~D.land
    lake1 = Hy.lake_mask
    from wg_hydro import river_mask
    river1 = river_mask(lv1, Hy.rivers, extra=3.0)
    cg = CostGrid(h1, Hy.water, sea1, lake1, river1, D.region, sites)
    roads = []

    def blocked_fn(q):
        ii = np.clip(np.rint(q[:, 1] / lv1.cell).astype(int), 0, lv1.H - 1)
        jj = np.clip(np.rint(q[:, 0] / lv1.cell).astype(int), 0, lv1.W - 1)
        return sea1[ii, jj] | lake1[ii, jj]

    for Rd in GEO.ROADS:
        stops = []
        for sid in Rd["stops"]:
            if sid in site_by_id:
                s = site_by_id[sid]
                stops.append((s.x, s.y))
            else:
                u, v = GEO.WAYPOINTS[sid]
                stops.append((u * C.WORLD_W_M, v * C.WORLD_H_M))
        full = []
        ok = True
        for a, b in zip(stops[:-1], stops[1:]):
            seg = cg.astar(a, b)
            if seg is None:
                log("  road %s: no path between %s and %s" % (Rd["key"], a, b))
                ok = False
                break
            if full:
                seg = seg[1:]
            full += seg
        if not ok or len(full) < 3:
            continue
        cg.mark(full)
        p = smooth_path(full, blocked_fn)
        roads.append(dict(key=Rd["key"], name=Rd["name"], width=Rd["width"], klass=Rd["klass"], pts=p,
                          stops=Rd["stops"]))
        L = float(np.sum(np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1]))))
        log("  road %-16s %5.2f km" % (Rd["key"], L / 1000.0))
    return roads


def nearest_on_polyline(pts, line, zline, chunk=256):
    """Distance from each point to a polyline and the polyline's z at the projection."""
    a = line[:-1]
    b = line[1:]
    ab = b - a
    L2 = np.maximum((ab ** 2).sum(1), 1e-9)
    dist = np.empty(len(pts))
    zout = np.empty(len(pts))
    for c in range(0, len(pts), chunk):
        P = pts[c:c + chunk]
        t = np.clip(((P[:, None, :] - a[None]) * ab[None]).sum(2) / L2[None], 0.0, 1.0)
        proj = a[None] + t[..., None] * ab[None]
        d2 = ((P[:, None, :] - proj) ** 2).sum(2)
        k = np.argmin(d2, axis=1)
        tk = t[np.arange(len(P)), k]
        dist[c:c + chunk] = np.sqrt(d2[np.arange(len(P)), k])
        zout[c:c + chunk] = zline[k] * (1 - tk) + zline[k + 1] * tk
    return dist, zout


def grade_limit(z, s, lo, hi, g, iters=80):
    """Smallest-change profile with |dz/ds| <= g, respecting per-point bounds inside every sweep (so a bound that
    lifts a stretch, e.g. a bridge deck, also lifts its approach ramps)."""
    zz = [float(v) for v in z]
    lo_l, hi_l = lo.tolist(), hi.tolist()
    ds = np.diff(s).tolist()
    n = len(zz)
    for _ in range(iters):
        ch = 0.0
        for i in range(1, n):
            a = g * ds[i - 1]
            v = min(max(zz[i], zz[i - 1] - a), zz[i - 1] + a)
            v = min(max(v, lo_l[i]), hi_l[i])
            ch = max(ch, abs(v - zz[i]))
            zz[i] = v
        for i in range(n - 2, -1, -1):
            a = g * ds[i]
            v = min(max(zz[i], zz[i + 1] - a), zz[i + 1] + a)
            v = min(max(v, lo_l[i]), hi_l[i])
            ch = max(ch, abs(v - zz[i]))
            zz[i] = v
        if ch < 1e-4:
            break
    return np.array(zz)


def profiles(roads, h1, Hy, sites, max_grade=0.12, lv=None, log=print):
    """Z profile per road (terrain-following, smoothed, grade-limited) + bridges over rivers."""
    lv = lv or G.L1
    from wg_fullres import flatten_roads
    bridges = []
    site_list = list(sites)
    h_work = h1.copy()           # earlier roads are flattened into this, so shared stretches get one level
    dummy = np.zeros(h1.shape, np.uint8)
    done = []
    for rd in roads:
        p = rd["pts"]
        n = len(p)
        s = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1])))])
        z = G.sample_bilinear(G.blur(h_work, 1.0), p[:, 0] / lv.cell, p[:, 1] / lv.cell).astype(np.float64)
        # stretches shared with an earlier road keep that road's exact level
        on_road = np.zeros(n, bool)
        z_exact = np.zeros(n)
        for prev in done:
            dprev, zprev = nearest_on_polyline(p, prev["pts"], prev["z"])
            close = dprev < 2.5
            on_road |= close
            z_exact[close] = zprev[close]
        # moving average (~48 m)
        k = 9
        zs = np.convolve(np.pad(z, k // 2, mode="edge"), np.ones(k) / k, mode="valid")
        lo = np.full(n, -1e9)
        hi = np.full(n, 1e9)
        # towns: road level = site plane inside the footprint, blend to 1.45 R
        for st in site_list:
            dd = np.hypot(p[:, 0] - st.x, p[:, 1] - st.y)
            plane = st.ground + st.tilt[0] * (p[:, 0] - st.x) + st.tilt[1] * (p[:, 1] - st.y)
            w = 1.0 - smoothstep(0.62 * st.r, 1.45 * st.r + 40.0, dd)
            zs = zs + (plane - zs) * w
            inside = dd < 0.62 * st.r
            lo[inside] = plane[inside] - 0.05
            hi[inside] = plane[inside] + 0.05
        # bridges
        rd_bridges = []
        for rv in Hy.rivers:
            for (i, t, j, u) in seg_intersections(p, rv["pts"]):
                wz = rv["zw"][j] * (1 - u) + rv["zw"][min(j + 1, len(rv["zw"]) - 1)] * u
                wd = rv["width"][j] * (1 - u) + rv["width"][min(j + 1, len(rv["width"]) - 1)] * u
                sc = s[i] * (1 - t) + s[min(i + 1, n - 1)] * t
                # span along the road: river half-width / sin(angle) + abutments
                rdir = p[min(i + 1, n - 1)] - p[i]
                vdir = rv["pts"][min(j + 1, len(rv["pts"]) - 1)] - rv["pts"][j]
                ca = abs(rdir[0] * vdir[1] - rdir[1] * vdir[0]) / max(1e-9, np.hypot(*rdir) * np.hypot(*vdir))
                half = 0.5 * wd / max(ca, 0.35) + 5.0
                clear = 2.2 + 0.06 * wd
                deck = wz + clear
                sel = (s > sc - half) & (s < sc + half)
                lo[sel] = np.maximum(lo[sel], deck)
                zs[sel] = np.maximum(zs[sel], deck)
                rd_bridges.append(dict(road=rd["key"], river=rv["key"], s0=sc - half, s1=sc + half, deck=deck,
                                       water=wz, width_river=wd, x=float(p[i, 0] + t * (p[min(i + 1, n - 1), 0] - p[i, 0])),
                                       y=float(p[i, 1] + t * (p[min(i + 1, n - 1), 1] - p[i, 1]))))
        # grade limiting (alternating passes), respecting bounds
        # where an earlier road already runs, keep its level exactly
        lo = np.where(on_road, np.maximum(lo, z_exact - 0.05), lo)
        hi = np.where(on_road, np.minimum(hi, z_exact + 0.05), hi)
        zs = np.where(on_road, z_exact, zs)
        zz = np.clip(zs, lo, hi)
        zz = grade_limit(zz, s, lo, hi, max_grade)
        ds = np.diff(s)
        # final light smoothing keeps grades continuous (does not exceed the limit meaningfully)
        z2 = zz.copy()
        z2[1:-1] = 0.25 * zz[:-2] + 0.5 * zz[1:-1] + 0.25 * zz[2:]
        z2 = np.clip(z2, lo, hi)
        rd["z"] = z2
        rd["s"] = s
        gr = np.abs(np.diff(z2)) / np.maximum(ds, 1e-6)
        rd["max_grade"] = float(gr.max()) if len(gr) else 0.0
        rd["cut_fill"] = float(np.max(np.abs(z2 - z)))
        for b in rd_bridges:
            i0 = int(np.searchsorted(s, b["s0"]))
            i1 = int(np.searchsorted(s, b["s1"]))
            i0, i1 = max(0, min(i0, n - 1)), max(0, min(i1, n - 1))
            b["start"] = (float(p[i0, 0]), float(p[i0, 1]), float(z2[i0]))
            b["end"] = (float(p[i1, 0]), float(p[i1, 1]), float(z2[i1]))
            b["deck"] = float(max(z2[i0:i1 + 1].max(), b["deck"])) if i1 >= i0 else b["deck"]
            b["length"] = float(b["s1"] - b["s0"])
            b["road_width"] = rd["width"]
            bridges.append(b)
        rd["bridges"] = rd_bridges
        flatten_roads(h_work, [rd], dummy, lv)
        done.append(rd)
        log("  road %-16s max grade %4.1f%%  max cut/fill %4.1f m  bridges %d" %
            (rd["key"], 100 * rd["max_grade"], rd["cut_fill"], len(rd_bridges)))
    return roads, bridges
