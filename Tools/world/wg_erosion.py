"""Erosion and flow routing (numpy + plain Python loops; no scipy / numba).

flood_route():   Priority-Flood (Barnes 2014) from the sea: depression-filled heights, a receiver for every land cell
                 (its flood parent = lowest neighbour / spill path) and a topological order (outlets first).
accumulate():    drainage area by walking the order backwards.
stream_power():  implicit stream-power incision (Braun & Willett 2013, n = 1) + hillslope diffusion, many iterations
                 at the 18 m grid; routing heights get a small random jitter each pass so D8 channels do not align to
                 the grid axes.
thermal():       vectorised talus relaxation with a per-cell repose angle.
"""
import heapq
import math

import numpy as np

import wg_grid as G


def flood_route(h, land, eps=1e-3):
    """h: 2-D float array; land: bool mask (land never touches the grid border).
    Returns (hf, rcv, order): filled heights (numpy), receivers (flat int64 numpy, self for sea/outlets),
    order (flat int64 numpy of land cells + seed cells, parents always before children)."""
    H, W = h.shape
    N = H * W
    hl = h.ravel().astype(np.float64).tolist()
    landf = land.ravel()
    closed = bytearray((~landf).astype(np.uint8).tobytes())
    # seeds: sea cells with a land neighbour
    sea = ~land
    nb = np.zeros_like(land)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dy or dx:
                nb |= G.shift(land, dy, dx, fill=False)
    seeds = np.nonzero((sea & nb).ravel())[0].tolist()
    rcv = list(range(N))
    heap = [(hl[i], i) for i in seeds]
    heapq.heapify(heap)
    order = []
    push, pop = heapq.heappush, heapq.heappop
    offs = (-W - 1, -W, -W + 1, -1, 1, W - 1, W, W + 1)
    app = order.append
    while heap:
        z, i = pop(heap)
        app(i)
        for o in offs:
            j = i + o
            if closed[j]:
                continue
            closed[j] = 1
            rcv[j] = i
            zj = hl[j]
            if zj <= z:
                zj = z + eps
                hl[j] = zj
            push(heap, (zj, j))
    hf = np.array(hl, dtype=np.float64).reshape(H, W)
    return hf, np.array(rcv, dtype=np.int64), np.array(order, dtype=np.int64)


_DIRS8 = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]


def route_active(h, active, eps=1e-3, steepest=True, cell=1.0):
    """Priority-Flood restricted to `active` cells; every non-active cell adjacent to the domain is an outlet.
    Returns (hf, rcv, order) like flood_route, with steepest-descent receivers on the filled surface."""
    H, W = h.shape
    N = H * W
    hl = h.ravel().astype(np.float64).tolist()
    af = active.ravel()
    closed = bytearray((~af).astype(np.uint8).tobytes())
    nb = np.zeros_like(active)
    for dy, dx in _DIRS8:
        nb |= G.shift(active, dy, dx, fill=False)
    seeds = np.nonzero((~active & nb).ravel())[0].tolist()
    rcv = list(range(N))
    heap = [(hl[i], i) for i in seeds]
    heapq.heapify(heap)
    order = []
    push, pop = heapq.heappush, heapq.heappop
    offs = (-W - 1, -W, -W + 1, -1, 1, W - 1, W, W + 1)
    app = order.append
    while heap:
        z, i = pop(heap)
        app(i)
        for o in offs:
            j = i + o
            if closed[j]:
                continue
            closed[j] = 1
            rcv[j] = i
            zj = hl[j]
            if zj <= z:
                zj = z + eps
                hl[j] = zj
            push(heap, (zj, j))
    hf = np.array(hl, dtype=np.float64).reshape(H, W)
    rcv = np.array(rcv, dtype=np.int64)
    if steepest:
        best = np.zeros((H, W), np.float64)
        brc = rcv.reshape(H, W).copy()
        idx = np.arange(N, dtype=np.int64).reshape(H, W)
        for dy, dx in _DIRS8:
            nbh = G.shift(hf, -dy, -dx)          # value of neighbour at (y+dy, x+dx)
            s = (hf - nbh) / (cell * math.hypot(dy, dx))
            better = active & (s > best)
            best = np.where(better, s, best)
            brc = np.where(better, idx + dy * W + dx, brc)
        rcv = brc.ravel()
    return hf, rcv, np.array(order, dtype=np.int64)


def slope_cap(h, active, cell, max_slope):
    """Largest surface <= h whose slope to every 8-neighbour is <= max_slope (per-cell array or scalar), changing
    only `active` cells. Dijkstra-style propagation from the lowest cells (threshold hillslopes: ridges are
    lowered until the flanks stand at the limiting angle, channels and fixed cells are untouched)."""
    H, W = h.shape
    hl = h.astype(np.float64).ravel().tolist()
    T = np.broadcast_to(np.asarray(max_slope, np.float64), (H, W)).ravel().tolist()
    act = active.ravel().tolist()
    # seeds: every active cell and its neighbours
    nb = active.copy()
    for dy, dx in _DIRS8:
        nb |= G.shift(active, dy, dx, fill=False)
    idx = np.nonzero(nb.ravel())[0].tolist()
    heap = [(hl[i], i) for i in idx]
    heapq.heapify(heap)
    offs = [(-W - 1, math.sqrt(2)), (-W, 1.0), (-W + 1, math.sqrt(2)), (-1, 1.0), (1, 1.0), (W - 1, math.sqrt(2)),
            (W, 1.0), (W + 1, math.sqrt(2))]
    pop, push = heapq.heappop, heapq.heappush
    N = H * W
    while heap:
        z, i = pop(heap)
        if z > hl[i]:
            continue
        x = i % W
        for o, dl in offs:
            j = i + o
            if j < 0 or j >= N or not act[j]:
                continue
            jx = j % W
            if jx - x > 1 or x - jx > 1:
                continue
            lim = z + T[j] * dl * cell
            if hl[j] > lim:
                hl[j] = lim
                push(heap, (lim, j))
    return np.array(hl, dtype=np.float64).reshape(H, W)


def uplift_erosion(base, uplift_total, active, cell, iters=80, K=0.035, m=0.5, talus_deg=38.0, thermal_iters=4,
                   relax_iters=14, log=None):
    """Grow relief by uplift while an implicit stream-power law and talus relaxation erode it (landscape
    evolution). base: starting surface; uplift_total: total uplift over the run (m). Only `active` cells move.
    K and talus_deg may be scalars or per-cell arrays."""
    H, W = base.shape
    s = base.astype(np.float64).copy()
    U = (uplift_total / iters).astype(np.float64)
    area = cell * cell
    Kf = np.where(active, K, 0.0).astype(np.float64).ravel()
    tal = np.broadcast_to(np.asarray(talus_deg, np.float32), (H, W))
    info = None
    for it in range(iters):
        s = np.where(active, s + U, s)
        hf, rcv, order = route_active(s, active, cell=cell)
        A = accumulate(order, rcv, np.full(H * W, area))
        L = _receiver_dist(rcv, W, cell)
        F = (Kf * np.power(A, m) / L).tolist()
        hl = s.ravel().tolist()
        rl = rcv.tolist()
        for i in order.tolist():
            r = rl[i]
            if r == i:
                continue
            f = F[i]
            if f <= 0.0:
                continue
            hi = hl[i]
            hr = hl[r]
            if hi > hr:
                hl[i] = (hi + f * hr) / (1.0 + f)
        s = np.array(hl, dtype=np.float64).reshape(H, W)
        if thermal_iters:
            s = np.where(active, thermal(s, tal, cell, iters=thermal_iters, rate=0.5).astype(np.float64), s)
        if log and (it % 20 == 0 or it == iters - 1):
            log("    uplift/erosion %d/%d  relief %.0f m  max A %.2f km2" % (
                it + 1, iters, float((s - base)[active].max()), A.max() / 1e6))
        info = (rcv, order, A)
    # final hillslope relaxation: slopes settle to the repose angle, ridges stay sharp
    if relax_iters:
        s = np.where(active, thermal(s, tal, cell, iters=relax_iters, rate=0.5).astype(np.float64), s)
    return s.astype(np.float32), info


CAP_DEG = 50.0          # steepest sustained flank of the simulated ranges at 18 m (rock faces steepen at 3 m)


UPLIFT_FACTOR = 3.0


def erode_design(D, iters=80, log=None):
    """Landscape-evolution pass over the designed terrain (returns the eroded 9 m heightfield).

    Mountain ranges (Central / Millis) are regrown by uplift on top of the designed lowland; the Demon Continent and
    Begaritt relief (crags, badlands, crater rims, mesas, ring, buttes) is regrown above a smooth floor. Stream power
    carves dendritic valley networks, talus relaxation shapes the slopes (steep repose in Begaritt keeps mesa
    cliffs), then relief is rescaled to the designed height envelope so peaks keep their spec heights."""
    import wg_geo as GEO
    lv1, lv2 = G.L1, G.L2
    f = lv2.step // lv1.step
    cid = GEO.CONTINENT_IDS
    h_nm = D.h
    land = D.land
    base1 = h_nm
    uplift1 = D.mountain
    fixed = D.lake_zone | D.crater_floor | D.heaven
    active1 = land & (D.mountain > 12.0) & ~fixed
    K1 = np.full(h_nm.shape, 0.2, np.float32)
    T1 = np.full(h_nm.shape, 39.0, np.float32)
    base2 = G.downsample(base1, f)
    up2 = G.downsample(uplift1, f)
    act2 = G.downsample(active1.astype(np.float32), f) > 0.5
    land2 = G.downsample(land.astype(np.float32), f) > 0.5
    act2 &= land2
    K2 = G.downsample(K1, f)
    T2 = G.downsample(T1, f)
    if log:
        log("  erosion domain: %d cells at %.0f m" % (int(act2.sum()), lv2.cell))
    s2, info = uplift_erosion(base2, up2 * UPLIFT_FACTOR, act2, lv2.cell, iters=iters, K=K2, talus_deg=T2, log=log)
    r2 = np.where(act2, np.maximum(s2 - base2, 0.0), 0.0)
    k3 = lambda a, ax: 0.25 * np.roll(a, 1, ax) + 0.5 * a + 0.25 * np.roll(a, -1, ax)
    r2 = np.where(act2, k3(k3(r2, 0), 1), 0.0)
    env_s = G.blur(G.max_filter(r2, 12), 6)
    env_d = G.blur(G.max_filter(up2, 12), 6)
    gain = np.clip(env_d / np.maximum(env_s, 2.0), 0.6, 1.25)
    sim2 = base2 + np.where(act2, r2 * gain, up2)
    # threshold hillslopes: no flank steeper than CAP_DEG at 18 m (after the height rescale)
    sim2 = slope_cap(sim2, act2, lv2.cell, math.tan(math.radians(CAP_DEG))).astype(np.float32)
    if log:
        g = gain[act2 & (up2 > 150.0)]
        log("  relief gain in ranges: median %.2f, p90 %.2f" % (float(np.median(g)), float(np.percentile(g, 90))))
    sim1 = G.upsample(sim2, f, kind="catmull_clamped")
    # break the grid alignment of D8 channels / talus facets: resample through a gentle domain warp
    X1, Y1 = lv1.xy_full()
    wx, wy = D.noise.warp(X1, Y1, 120.0, 10.0, octaves=3, k0=700)
    sim1 = G.sample_bilinear(sim1, wx / lv1.cell, wy / lv1.cell)
    design1 = base1 + uplift1
    resid = design1 - G.upsample(G.downsample(design1, f), f)
    wact = G.blur(G.upsample(act2.astype(np.float32), f), 4.0)
    h1 = design1 * (1.0 - wact) + (sim1 + 0.15 * resid) * wact
    h1 = np.where(land, np.maximum(h1, 0.35), h_nm)
    # final cap at 9 m: ranges <= 55 deg, other land <= 64 deg (mesas, crags, crater walls), Heaven cliffs <= 80 deg
    tcap = np.where(D.heaven, math.tan(math.radians(80.0)),
                    np.where(wact > 0.5, math.tan(math.radians(55.0)), math.tan(math.radians(64.0))))
    h1 = slope_cap(h1, land & ~D.lake_zone, lv1.cell, tcap).astype(np.float32)
    return h1.astype(np.float32), dict(gain=gain, act2=act2, info=info)


def accumulate(order, rcv, weight):
    """Sum weight (flat numpy) downstream along receivers; returns flat numpy array."""
    A = weight.astype(np.float64).tolist()
    rl = rcv.tolist()
    for i in reversed(order.tolist()):
        r = rl[i]
        if r != i:
            A[r] += A[i]
    return np.array(A, dtype=np.float64)


def _receiver_dist(rcv, W, cell):
    idx = np.arange(rcv.size, dtype=np.int64)
    d = np.abs(rcv - idx)
    diag = (d == W - 1) | (d == W + 1)
    return np.where(diag, cell * math.sqrt(2.0), cell)


def stream_power(h, land, K, cell, iters=30, dt=1.0, m=0.5, kd=0.0, jitter=0.15, protect=None, seed=1,
                 log=None, uplift=None):
    """Implicit stream-power incision. h float array (m), K erodibility map (per cell, already x dt scale).
    protect: bool mask of cells that must not change. Returns eroded copy (float32) and last routing info."""
    rng = np.random.default_rng(seed)
    H, W = h.shape
    hh = h.astype(np.float64).copy()
    Kf = K.ravel().astype(np.float64)
    prot = protect.ravel() if protect is not None else np.zeros(H * W, bool)
    area = cell * cell
    info = None
    for it in range(iters):
        route_h = hh + rng.uniform(-jitter, jitter, hh.shape) * land
        hf, rcv, order = flood_route(route_h, land)
        A = accumulate(order, rcv, np.full(H * W, area))
        L = _receiver_dist(rcv, W, cell)
        F = (Kf * dt * np.power(A, m) / L)
        F[prot] = 0.0
        Fl = F.tolist()
        hl = hh.ravel().tolist()
        rl = rcv.tolist()
        for i in order.tolist():
            r = rl[i]
            if r == i:
                continue
            f = Fl[i]
            if f <= 0.0:
                continue
            hr = hl[r]
            hi = hl[i]
            if hi > hr:
                hn = (hi + f * hr) / (1.0 + f)
                hl[i] = hn
        hh = np.array(hl, dtype=np.float64).reshape(H, W)
        if uplift is not None:
            hh += uplift * dt
        if kd > 0.0:
            lap = (G.shift(hh, 1, 0) + G.shift(hh, -1, 0) + G.shift(hh, 0, 1) + G.shift(hh, 0, -1) - 4.0 * hh)
            hh = np.where(land & ~protect if protect is not None else land, hh + kd * lap, hh)
        if log and (it % 10 == 0 or it == iters - 1):
            log("    stream power %d/%d  max A %.2f km2" % (it + 1, iters, A.max() / 1e6))
        info = (rcv, order, A)
    return hh.astype(np.float32), info


def droplets(h, land, cell, n_drops, seed=7, batch=150000, steps=48, inertia=0.12, cap_factor=3.0, min_cap=0.004,
             erode_rate=0.35, deposit_rate=0.25, evaporate=0.03, gravity=9.0, radius=2, zscale=900.0,
             strength=None, spawn_weight=None, log=None):
    """Vectorised particle hydraulic erosion (Beyer 2015 style) on a working grid.
    Heights are normalised by zscale internally; `strength` (per cell, 0..1+) scales erosion; droplets spawn with
    probability ~ spawn_weight (defaults to land). Returns the eroded height (float32)."""
    rng = np.random.default_rng(seed)
    H, W = h.shape
    N = H * W
    z = (h.astype(np.float64) / zscale).ravel().copy()
    strength_f = np.ones(N) if strength is None else np.asarray(strength, np.float64).ravel()
    landf = land.ravel()
    # brush offsets and weights
    offs = []
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            d = math.hypot(dy, dx)
            if d <= radius + 0.01:
                offs.append((dy, dx, max(0.0, radius + 0.5 - d)))
    bw = np.array([o[2] for o in offs])
    bw /= bw.sum()
    bdy = np.array([o[0] for o in offs], np.int64)
    bdx = np.array([o[1] for o in offs], np.int64)
    sw = (land.astype(np.float64) if spawn_weight is None else np.asarray(spawn_weight, np.float64)).ravel()
    cdf = np.cumsum(sw)
    cdf /= cdf[-1]
    done = 0
    while done < n_drops:
        n = min(batch, n_drops - done)
        done += n
        start = np.searchsorted(cdf, rng.random(n))
        px = (start % W) + rng.random(n)
        py = (start // W) + rng.random(n)
        dxv = np.zeros(n)
        dyv = np.zeros(n)
        vel = np.ones(n)
        water = np.ones(n)
        sed = np.zeros(n)
        alive = np.ones(n, bool)
        for _ in range(steps):
            idx = np.nonzero(alive)[0]
            if idx.size == 0:
                break
            x = px[idx]
            y = py[idx]
            xi = np.clip(np.floor(x).astype(np.int64), 1, W - 3)
            yi = np.clip(np.floor(y).astype(np.int64), 1, H - 3)
            fx = x - xi
            fy = y - yi
            c = yi * W + xi
            h00, h10, h01, h11 = z[c], z[c + 1], z[c + W], z[c + W + 1]
            gx = (h10 - h00) * (1 - fy) + (h11 - h01) * fy
            gy = (h01 - h00) * (1 - fx) + (h11 - h10) * fx
            hcur = h00 * (1 - fx) * (1 - fy) + h10 * fx * (1 - fy) + h01 * (1 - fx) * fy + h11 * fx * fy
            ndx = dxv[idx] * inertia - gx * (1 - inertia)
            ndy = dyv[idx] * inertia - gy * (1 - inertia)
            ln = np.sqrt(ndx * ndx + ndy * ndy)
            zero = ln < 1e-12
            if zero.any():
                a = rng.random(int(zero.sum())) * 2 * np.pi
                ndx[zero] = np.cos(a)
                ndy[zero] = np.sin(a)
                ln[zero] = 1.0
            ndx /= ln
            ndy /= ln
            nx = x + ndx
            ny = y + ndy
            out = (nx < 2) | (ny < 2) | (nx > W - 3) | (ny > H - 3)
            nxi = np.clip(np.floor(nx).astype(np.int64), 1, W - 3)
            nyi = np.clip(np.floor(ny).astype(np.int64), 1, H - 3)
            nfx = nx - nxi
            nfy = ny - nyi
            nc = nyi * W + nxi
            hnew = (z[nc] * (1 - nfx) * (1 - nfy) + z[nc + 1] * nfx * (1 - nfy) + z[nc + W] * (1 - nfx) * nfy
                    + z[nc + W + 1] * nfx * nfy)
            dh = hnew - hcur
            v = vel[idx]
            wv = water[idx]
            s = sed[idx]
            cap = np.maximum(-dh * v * wv * cap_factor, min_cap)
            dep_mask = (s > cap) | (dh > 0)
            amt_dep = np.where(dh > 0, np.minimum(dh, s), (s - cap) * deposit_rate)
            amt_dep = np.where(dep_mask, amt_dep, 0.0)
            st = strength_f[c]
            amt_ero = np.where(dep_mask, 0.0, np.minimum((cap - s) * erode_rate * st, -dh))
            amt_ero = np.maximum(amt_ero, 0.0)
            s = s - amt_dep + amt_ero
            delta = np.zeros(N)
            # deposition: bilinear to the 4 corners of the old position
            if amt_dep.any():
                w00 = (1 - fx) * (1 - fy)
                w10 = fx * (1 - fy)
                w01 = (1 - fx) * fy
                w11 = fx * fy
                ii = np.concatenate([c, c + 1, c + W, c + W + 1])
                ww = np.concatenate([amt_dep * w00, amt_dep * w10, amt_dep * w01, amt_dep * w11])
                delta += np.bincount(ii, ww, minlength=N)
            if amt_ero.any():
                sel = amt_ero > 0
                cc = c[sel]
                ae = amt_ero[sel]
                ii = (cc[:, None] + (bdy * W + bdx)[None, :]).ravel()
                ww = (ae[:, None] * bw[None, :]).ravel()
                delta -= np.bincount(ii, ww, minlength=N)
            z += delta
            vel[idx] = np.sqrt(np.maximum(v * v - dh * gravity * zscale / cell * 0.01, 0.0) + 1e-6)
            water[idx] = wv * (1 - evaporate)
            sed[idx] = s
            dxv[idx] = ndx
            dyv[idx] = ndy
            px[idx] = nx
            py[idx] = ny
            # die when leaving the grid or entering the sea
            dead = out | ~landf[nc]
            alive[idx[dead]] = False
        if log:
            log("    droplets %d/%d" % (done, n_drops))
    return (z.reshape(H, W) * zscale).astype(np.float32)


def thermal(h, talus_deg, cell, iters=20, rate=0.45, mask=None):
    """Talus relaxation: material above the repose angle slides to lower neighbours (8-neighbourhood)."""
    hh = h.astype(np.float32).copy()
    T = np.tan(np.radians(talus_deg)).astype(np.float32)
    dirs = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
    dists = [cell * math.hypot(dy, dx) for dy, dx in dirs]
    for _ in range(iters):
        exc = []
        tot = np.zeros_like(hh)
        mx = np.zeros_like(hh)
        for (dy, dx), dd in zip(dirs, dists):
            nbh = G.shift(hh, dy, dx)
            e = hh - nbh - T * dd
            np.maximum(e, 0.0, out=e)
            exc.append(e)
            tot += e
            np.maximum(mx, e, out=mx)
        move = rate * 0.5 * mx
        if mask is not None:
            move *= mask
        frac = np.where(tot > 0, move / np.maximum(tot, 1e-12), 0.0).astype(np.float32)
        delta = -move
        for (dy, dx), e in zip(dirs, exc):
            f = e * frac
            delta += G.shift(f, -dy, -dx, fill=0.0)
        hh += delta
    return hh
