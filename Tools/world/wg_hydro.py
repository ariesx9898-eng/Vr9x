"""Rivers, lakes and dry riverbeds on the eroded 9 m terrain.

* Routing: Priority-Flood from the sea gives depression-filled heights, steepest-descent receivers and drainage area.
* Lakes (designed basins): water level = spill level of the basin (filled height at the lake centre), so every
  lake is exactly full and overflows into its outflow river.
* Rivers: traced downstream from each guide's source along the receivers until they reach the sea or a lake
  (falls back to the main stem walked upstream from the mouth when the downstream trace strays). Centrelines are
  smoothed, water surface Z is made monotone (never rises downstream, 0 at a sea mouth, lake level at a lake),
  widths grow with sqrt(drainage area); channels are carved below the water surface with sloping banks.
* Begaritt dry riverbeds: main stems of the largest desert basins, carved as shallow flat sandy beds.
"""
import math

import numpy as np

import wg_config as C
import wg_geo as GEO
import wg_grid as G
import wg_erosion as E
from wg_grid import smoothstep


class Hydro:
    pass


def route(h, land):
    hf, rcv, order = E.route_active(h, land, cell=G.L1.cell)
    A = E.accumulate(order, rcv, np.full(h.size, G.L1.cell ** 2))
    return hf, rcv, order, A


def _trace_down(start, rcv, stop_mask, max_n=20000):
    path = [start]
    i = start
    for _ in range(max_n):
        r = rcv[i]
        if r == i:
            break
        path.append(r)
        i = r
        if stop_mask[r]:
            break
    return path


def _main_stem_up(mouth, rcv, A, W, H, a_min):
    """Walk upstream from mouth choosing the donor with the largest drainage area."""
    # donors lookup via neighbours (receivers point to us)
    path = [mouth]
    i = mouth
    seen = {mouth}
    for _ in range(20000):
        y, x = divmod(i, W)
        best, ba = -1, 0.0
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy == 0 and dx == 0:
                    continue
                yy, xx = y + dy, x + dx
                if 0 <= yy < H and 0 <= xx < W:
                    j = yy * W + xx
                    if rcv[j] == i and A[j] > ba and j not in seen:
                        best, ba = j, A[j]
        if best < 0 or ba < a_min:
            break
        path.append(best)
        seen.add(best)
        i = best
    return path[::-1]


def _smooth_path(xy, passes=6):
    p = np.array(xy, np.float64)
    for _ in range(passes):
        if len(p) < 5:
            break
        q = p.copy()
        q[1:-1] = 0.25 * p[:-2] + 0.5 * p[1:-1] + 0.25 * p[2:]
        p = q
    return p


def _resample(p, step):
    seg = np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1]))
    s = np.concatenate([[0.0], np.cumsum(seg)])
    if s[-1] < step:
        return p, s
    n = int(s[-1] / step) + 1
    ss = np.linspace(0.0, s[-1], n)
    return np.stack([np.interp(ss, s, p[:, 0]), np.interp(ss, s, p[:, 1])], axis=1), ss


def lake_mask_from_level(h, sl, cy, cx, level):
    """Connected cells (8-conn) below `level` containing the local cell (cy, cx), within window sl."""
    sub = h[sl] < level
    H, W = sub.shape
    oy, ox = sl[0].start, sl[1].start
    y0, x0 = cy - oy, cx - ox
    if not (0 <= y0 < H and 0 <= x0 < W) or not sub[y0, x0]:
        return None
    lab = G.label_components(sub, 8)
    m = lab == lab[y0, x0]
    return m


def carve_guide_valleys(h, land, guides, lake_levels, lv, lake_zone=None):
    """Meandering river valleys along the guide lines on the eroded terrain: monotone floor (sampled ground minus
    a few metres, never rising downstream, reaching the sea / lake level), flat floodplain widening downstream,
    concave valley walls blending into the surrounding relief."""
    cell = lv.cell
    h = h.copy()
    # lowest ground within ~170 m of each point: the valley floor must stay below both banks
    ok = land if lake_zone is None else (land & ~lake_zone)
    hmin = -G.max_filter(-np.where(ok, h, 1e4).astype(np.float32), 19)
    for Rv, mpts, total in guides:
        d, along, _, total = G.polyline_field(lv, mpts, 560.0)
        n = len(mpts)
        mx = np.array([p[0] for p in mpts])
        my = np.array([p[1] for p in mpts])
        ss = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(mx), np.diff(my)))])
        prof = G.sample_bilinear(hmin, mx / cell, my / cell)
        depth = np.linspace(4.0, 2.5, n)
        floor = prof - depth
        if "lake_in" in Rv and Rv["lake_in"] in lake_levels:
            # the outflow starts exactly at the lake's level (this notch is the lake's spill point)
            lvl = lake_levels[Rv["lake_in"]] - 0.4
            floor = np.minimum(floor, lvl)
            floor = np.maximum(floor, lvl - 0.035 * ss)
        floor = np.minimum.accumulate(floor)
        if "lake_out" in Rv and Rv["lake_out"] in lake_levels:
            end_z = lake_levels[Rv["lake_out"]] - 1.0
        else:
            end_z = 0.2
        floor = np.minimum(floor, np.linspace(max(floor[0], end_z + 1.0), end_z, n))
        floor = np.minimum.accumulate(floor)
        fz = np.interp(along, ss, floor)
        tt = along / max(total, 1.0)
        fw = 16.0 + 50.0 * tt
        vw = 140.0 + 180.0 * tt
        wv = smoothstep(fw, vw, d) ** 0.85
        near = np.isfinite(d) & land
        h = np.where(near, np.minimum(h, fz + (h - fz) * wv), h)
    return h


def grade_corridors(h, land, lv, log=print):
    """Broad graded cols through the Red Wyrm passes: a smooth, grade-limited floor ~110 m wide, blending into the
    mountain flanks over ~260 m, so roads can cross the range at the Upper and Lower Jaw."""
    import wg_roads
    cell = lv.cell
    h = h.copy()
    for key, P in GEO.PASS_CORRIDORS.items():
        pts = np.array([(u * C.WORLD_W_M, v * C.WORLD_H_M) for u, v in P["pts"]], np.float64)
        dense = _resample(_smooth_path(_resample(pts, 20.0)[0], passes=12), 9.0)[0]
        s = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(dense[:, 0]), np.diff(dense[:, 1])))])
        ground = G.sample_bilinear(G.blur(h, 2.0), dense[:, 0] / cell, dense[:, 1] / cell).astype(np.float64)
        kp = int(np.argmin(np.hypot(dense[:, 0] - pts[1, 0], dense[:, 1] - pts[1, 1])))
        desired = np.minimum(ground, P["saddle"])
        lo = np.full(len(s), -1e9)
        hi = np.full(len(s), 1e9)
        lo[kp] = hi[kp] = P["saddle"]
        zc = wg_roads.grade_limit(desired, s, lo, hi, P["grade"])
        d, along, _, total = G.polyline_field(lv, [tuple(p) for p in dense], 420.0)
        zf = np.interp(along, s, zc)
        w = 1.0 - smoothstep(P.get("core", 55.0), P.get("core", 55.0) + P.get("blend", 245.0), d)
        w *= smoothstep(0.0, 260.0, np.minimum(along, total - along))
        w = np.where(np.isfinite(d) & land, w, 0.0)
        h = h + (zf - h) * w
        log("  pass corridor %-8s saddle %.0f m, %.2f km, max grade %.1f%%" % (
            key, zc[kp], s[-1] / 1000.0, 100 * float(np.max(np.abs(np.diff(zc)) / np.diff(s)))))
    return h


def build(D, h1, log=print):
    lv = G.L1
    H, W = h1.shape
    cell = lv.cell
    X = None
    Hy = Hydro()
    land = D.land
    h = grade_corridors(h1.astype(np.float32), land, lv, log)
    h = carve_guide_valleys(h, land, D.river_guides, D.lake_level_design, lv, D.lake_zone)

    # ---------------------------------------------------------------- lakes: enforce designed levels
    Hy.lakes = []
    lake_any = np.zeros((H, W), bool)
    hf, rcv, order, A = route(h, land)
    for L in GEO.LAKES:
        lx, ly = L["uv"][0] * C.WORLD_W_M, L["uv"][1] * C.WORLD_H_M
        a, b = L["ab"]
        sl, re = D.lake_re[L["key"]]
        # lowest cell of the basin near the centre
        sub = h[sl]
        cand = np.where(re < 0.6, sub, np.inf)
        k = int(np.argmin(cand))
        by, bx = divmod(k, sub.shape[1])
        by += sl[0].start
        bx += sl[1].start
        level = float(hf[by, bx])
        depth_here = level - float(h[by, bx])
        if depth_here < 0.8:
            # basin was breached: rebuild a bowl at the designed position
            ground = float(np.percentile(sub[(re > 1.3) & (re < 2.0)], 25))
            level = ground - 0.6
            bowl = level - (5.0 if L["kind"] == "Oasis" else 12.0) * (1.0 - np.clip(re, 0, 1) ** 2) - 0.8
            h[sl] = np.where(re < 1.0, np.minimum(sub, bowl), sub)
            hf, rcv, order, A = route(h, land)
            level = float(hf[by, bx])
        m = lake_mask_from_level(h, sl, by, bx, level - 0.05)
        if m is None or m.sum() < 4:
            log("  lake %s: basin too small, skipped" % L["key"])
            continue
        # cap runaway lakes (spill level reaching far beyond the design): lower the level to the design size
        area = m.sum() * cell * cell
        design_area = math.pi * a * b
        tries = 0
        while area > 3.0 * design_area and tries < 30:
            level -= 0.5
            m2 = lake_mask_from_level(h, sl, by, bx, level - 0.05)
            if m2 is None:
                level += 0.5
                break
            m = m2
            area = m.sum() * cell * cell
            tries += 1
        full = np.zeros((H, W), bool)
        full[sl] = m
        lake_any |= full
        Hy.lakes.append(dict(key=L["key"], name=L["name"], level=level, mask_sl=sl, mask=m, frozen=L["frozen"],
                             kind=L["kind"], centre=(bx, by), area_m2=float(area)))
        log("  lake %-15s level %6.1f m  area %.3f km2" % (L["key"], level, area / 1e6))
    Hy.lake_mask = lake_any

    # ---------------------------------------------------------------- rivers
    hf, rcv, order, A = route(h, land)
    Hy.hf, Hy.rcv, Hy.A = hf, rcv, A
    sea = ~land
    stop = (sea | lake_any).ravel()
    Hy.rivers = []
    lake_by_key = {lk["key"]: lk for lk in Hy.lakes}
    Af = A
    for gi, (Rv, mpts, total) in enumerate(D.river_guides):
        src = mpts[0]
        mouth = mpts[-1]
        sx, sy = int(round(src[0] / cell)), int(round(src[1] / cell))
        if "lake_in" in Rv and Rv["lake_in"] in lake_by_key:
            lk = lake_by_key[Rv["lake_in"]]
            # outflow: start at the lake's spill cell = lake cell whose receiver leaves the lake with max area
            full = np.zeros((H, W), bool)
            full[lk["mask_sl"]] = lk["mask"]
            idx = np.nonzero(full.ravel())[0]
            leave = idx[~full.ravel()[rcv[idx]]]
            if len(leave) == 0:
                log("  river %s: lake has no outlet" % Rv["key"])
                continue
            start = int(rcv[leave[np.argmax(A[leave])]])
            stop_here = sea.ravel() | (lake_any.ravel() & ~full.ravel())
            path = [int(leave[np.argmax(A[leave])])] + _trace_down(start, rcv, stop_here)
        else:
            start = sy * W + sx
            path = _trace_down(start, rcv, stop)
            # headwaters: continue upstream along the largest tributary into the hills / mountains
            head = _main_stem_up(start, rcv, A, W, H, a_min=0.06e6)
            if len(head) > 1:
                path = head[:-1] + path
        end = path[-1]
        ey, ex = divmod(end, W)
        dm = math.hypot(ex * cell - mouth[0], ey * cell - mouth[1])
        if dm > 900.0 and "lake_in" not in Rv:
            # the flow went elsewhere: use the main stem that reaches the intended mouth
            mx, my = int(round(mouth[0] / cell)), int(round(mouth[1] / cell))
            r = 25
            y0, y1 = max(0, my - r), min(H, my + r + 1)
            x0, x1 = max(0, mx - r), min(W, mx + r + 1)
            Ab = A.reshape(H, W)[y0:y1, x0:x1]
            wet_nb = G.dilate_mask(sea | lake_any, 1)[y0:y1, x0:x1]
            cand = np.where(land[y0:y1, x0:x1] & wet_nb, Ab, 0.0)
            k = int(np.argmax(cand))
            mcell = (y0 + k // cand.shape[1]) * W + (x0 + k % cand.shape[1])
            path = _main_stem_up(mcell, rcv, A, W, H, a_min=0.08e6)
            log("  river %s: downstream trace ended %.0f m from the mouth, using the main stem (%d cells)" %
                (Rv["key"], dm, len(path)))
            end = path[-1]
            ey, ex = divmod(end, W)
        ys, xs = np.divmod(np.array(path), W)
        pts = np.stack([xs * cell, ys * cell], axis=1).astype(np.float64)
        if len(pts) < 6:
            log("  river %s: too short, skipped" % Rv["key"])
            continue
        areas = A[np.array(path)]
        # extend the last point one cell into the water so the channel meets the sea / lake
        p = _smooth_path(pts, passes=5)
        p[0] = pts[0]
        p[-1] = pts[-1]
        p, ss = _resample(p, 12.0)
        a_s = np.interp(ss, np.linspace(0, ss[-1], len(areas)), areas)
        zbed = G.sample_bilinear(h, p[:, 0] / cell, p[:, 1] / cell)
        ends_in = "sea"
        end_level = 0.0
        ey2, ex2 = int(round(p[-1, 1] / cell)), int(round(p[-1, 0] / cell))
        for lk in Hy.lakes:
            sl = lk["mask_sl"]
            yy, xx = ey2 - sl[0].start, ex2 - sl[1].start
            m = lk["mask"]
            near = False
            for dy in range(-2, 3):
                for dx in range(-2, 3):
                    if 0 <= yy + dy < m.shape[0] and 0 <= xx + dx < m.shape[1] and m[yy + dy, xx + dx]:
                        near = True
            if near and lk["key"] != Rv.get("lake_in"):
                ends_in = "lake:" + lk["key"]
                end_level = lk["level"]
        wmin, wmax = Rv["width"]
        amax = max(a_s.max(), 1.0)
        width = wmin + (wmax - wmin) * np.sqrt(np.clip(a_s / amax, 0.0, 1.0))
        depth = 0.8 + 1.6 * (width - wmin) / max(1.0, wmax - wmin)
        # water surface: follow the smoothed bed a little above it, monotone downstream
        zs = np.convolve(np.pad(zbed, 6, mode="edge"), np.ones(13) / 13.0, mode="valid")
        # the river runs incised in its floodplain: water ~0.8 m below the banks
        zw = np.minimum(zs, zbed) - 0.8
        if "lake_in" in Rv and Rv["lake_in"] in lake_by_key:
            zw[0] = min(zw[0], lake_by_key[Rv["lake_in"]]["level"] - 0.15)
        zw = np.minimum.accumulate(zw)
        zw = np.maximum(zw, end_level + np.linspace(0.3, 0.0, len(zw)) * 0.0)
        zw[-1] = end_level
        # gently force the last stretch to reach the mouth level
        n_tail = min(len(zw) - 1, 25)
        if n_tail > 1:
            t = np.linspace(0.0, 1.0, n_tail + 1)
            zw[-n_tail - 1:] = np.minimum(zw[-n_tail - 1:], zw[-n_tail - 1] * (1 - t) + end_level * t)
        zw = np.minimum.accumulate(zw)
        zw = np.maximum(zw, end_level)
        Hy.rivers.append(dict(key=Rv["key"], name=Rv["name"], continent=Rv["continent"], pts=p, zw=zw,
                              width=width, depth=depth, area=a_s, ends_in=ends_in,
                              lake_in=Rv.get("lake_in")))
        log("  river %-14s %5.1f km  %s  source %.0f m" % (Rv["key"], ss[-1] / 1000.0, ends_in, zw[0]))

    # ---------------------------------------------------------------- dry riverbeds (Begaritt)
    Hy.drybeds = []
    beg = D.cont == GEO.CONTINENT_IDS["Begaritt"]
    Ag = A.reshape(H, W)
    coastal_out = land & G.dilate_mask(sea, 1) & beg
    cand = np.where(coastal_out, Ag, 0.0)
    flat_idx = np.argsort(cand.ravel())[::-1]
    chosen = []
    for k in flat_idx[:4000]:
        if cand.ravel()[k] < 0.35e6:
            break
        ky, kx = divmod(int(k), W)
        if any(math.hypot(ky - cy, kx - cx) < 90 for cy, cx in chosen):
            continue
        chosen.append((ky, kx))
        if len(chosen) >= 6:
            break
    for (ky, kx) in chosen:
        path = _main_stem_up(ky * W + kx, rcv, A, W, H, a_min=0.03e6)
        if len(path) < 60:
            continue
        ys, xs = np.divmod(np.array(path), W)
        p = _smooth_path(np.stack([xs * cell, ys * cell], axis=1).astype(np.float64), 5)
        p, ss = _resample(p, 12.0)
        a_s = np.interp(ss, np.linspace(0, ss[-1], len(path)), A[np.array(path)])
        width = 10.0 + 30.0 * np.sqrt(np.clip(a_s / a_s.max(), 0, 1))
        Hy.drybeds.append(dict(pts=p, width=width))
    log("  dry riverbeds: %d" % len(Hy.drybeds))

    # ---------------------------------------------------------------- carve channels at 9 m
    h = carve_rivers(h, lv, Hy.rivers, Hy.drybeds)
    Hy.h = h
    # water mask + distance field for painting / densities / roads
    water = ~land | lake_any | river_mask(lv, Hy.rivers, extra=0.0)
    Hy.water = water
    return Hy


def river_mask(lv, rivers, extra=0.0):
    H, W = lv.shape
    m = np.zeros((H, W), bool)
    for rv in rivers:
        p = rv["pts"]
        hw = rv["width"] * 0.5 + extra
        for i in range(0, len(p) - 1):
            x, y = p[i]
            r = hw[i]
            c0, c1 = int((x - r) / lv.cell), int((x + r) / lv.cell) + 1
            r0, r1 = int((y - r) / lv.cell), int((y + r) / lv.cell) + 1
            c0, r0 = max(c0, 0), max(r0, 0)
            ys = np.arange(r0, min(r1 + 1, H)) * lv.cell
            xs = np.arange(c0, min(c1 + 1, W)) * lv.cell
            if len(ys) == 0 or len(xs) == 0:
                continue
            dd = (xs[None, :] - x) ** 2 + (ys[:, None] - y) ** 2
            m[r0:r0 + len(ys), c0:c0 + len(xs)] |= dd <= r * r
    return m


def carve_rivers(h, lv, rivers, drybeds):
    """Carve channels: bed = water - depth at the centre, banks rise to the terrain over ~2.5 half-widths;
    banks are kept at least 0.25 m above the water surface so the water ribbon never floats."""
    h = h.copy()
    cell = lv.cell
    H, W = h.shape
    for rv in list(rivers) + [dict(pts=d["pts"], zw=None, width=d["width"], depth=None) for d in drybeds]:
        p = rv["pts"]
        n = len(p)
        # process in chunks of polyline points
        CH = 24
        for c in range(0, n - 1, CH):
            a0 = max(0, c - 2)
            seg = p[a0:min(n, c + CH + 3)]
            hw = rv["width"][a0:min(n, c + CH + 3)] * 0.5
            reach = float(hw.max()) * 3.2 + 2 * cell
            x0 = max(0, int((seg[:, 0].min() - reach) / cell))
            x1 = min(W - 1, int((seg[:, 0].max() + reach) / cell) + 1)
            y0 = max(0, int((seg[:, 1].min() - reach) / cell))
            y1 = min(H - 1, int((seg[:, 1].max() + reach) / cell) + 1)
            xs = (np.arange(x0, x1 + 1) * cell)[None, :, None]
            ys = (np.arange(y0, y1 + 1) * cell)[:, None, None]
            ax, ay = seg[:-1, 0], seg[:-1, 1]
            bx, by = seg[1:, 0], seg[1:, 1]
            dx, dy = bx - ax, by - ay
            L2 = np.maximum(dx * dx + dy * dy, 1e-9)
            t = np.clip(((xs - ax) * dx + (ys - ay) * dy) / L2, 0.0, 1.0)
            px = ax + t * dx
            py = ay + t * dy
            d = np.sqrt((xs - px) ** 2 + (ys - py) ** 2)
            k = np.argmin(d, axis=2)
            dmin = np.take_along_axis(d, k[..., None], axis=2)[..., 0]
            tk = np.take_along_axis(t, k[..., None], axis=2)[..., 0]
            ii = a0 + k
            own = (ii >= c) & (ii < min(c + CH, n - 1))
            hwk = rv["width"][np.minimum(ii, n - 1)] * 0.5 * (1 - tk) + rv["width"][np.minimum(ii + 1, n - 1)] * 0.5 * tk
            sub = h[y0:y1 + 1, x0:x1 + 1]
            if rv["zw"] is not None:
                zw = rv["zw"][np.minimum(ii, n - 1)] * (1 - tk) + rv["zw"][np.minimum(ii + 1, n - 1)] * tk
                dep = rv["depth"][np.minimum(ii, n - 1)]
                u = dmin / np.maximum(hwk, 1.0)
                bed = zw - dep * (1.0 - smoothstep(0.55, 1.05, u)) + 0.35 * smoothstep(0.9, 1.3, u)
                bank_z = np.maximum(sub, zw + 0.25)
                target = np.where(u < 1.0, np.minimum(sub, bed),
                                  np.where(u < 1.4, np.maximum(np.minimum(sub, bank_z + 0.8), zw + 0.25),
                                           sub))
                blend = 1.0 - smoothstep(1.4, 3.2, u)
                new = sub + (target - sub) * np.where(u < 1.4, 1.0, blend)
                # outside the channel never lower the terrain below the bank level
                new = np.where(u >= 1.0, np.maximum(new, np.minimum(sub, zw + 0.25)), new)
            else:
                u = dmin / np.maximum(hwk, 1.0)
                zb = sub - 2.5 * (1.0 - smoothstep(0.6, 1.4, u))
                new = np.minimum(sub, zb)
            sel = (dmin < reach) & own
            h[y0:y1 + 1, x0:x1 + 1] = np.where(sel, new, sub)
    return h
