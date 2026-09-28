"""Full-resolution (3 m, 6097 x 4573) heightfield assembly.

1. Catmull-Rom upsample of the 9 m eroded terrain (vertex aligned: every 3rd vertex is exact).
2. Fine detail: fBm / ridged rock breakup scaled by slope and biome, faded out near sea level, water, roads, towns.
3. Begaritt dunes: transverse, wind-aligned, asymmetric (gentle windward, steep lee), with a secondary oblique set.
4. Rivers carved at full resolution (bed below the water surface, banks >= water + 0.25 m), lake beds kept below
   their water level and shores clean.
5. Site footprints (plane + landmark mounds) and roads (level cross-section, adaptive embankments; bridge spans keep
   the river channel) are applied last.
All steps run in row bands so peak memory stays low.
"""
import math

import numpy as np

import wg_config as C
import wg_geo as GEO
import wg_grid as G
import wg_sites
from wg_grid import smoothstep

BAND = 384


def upsample_rows(a1, r0, r1, f=3):
    """Bilinear vertex-aligned upsample of an L1 field for full-res rows [r0, r1)."""
    H1, W1 = a1.shape
    ys = np.arange(r0, r1) / f
    y0 = np.clip(np.floor(ys).astype(int), 0, H1 - 2)
    fy = (ys - y0).astype(np.float32)[:, None]
    xs = np.arange(0, (W1 - 1) * f + 1) / f
    x0 = np.clip(np.floor(xs).astype(int), 0, W1 - 2)
    fx = (xs - x0).astype(np.float32)[None, :]
    a = a1.astype(np.float32)
    top = a[y0][:, x0] * (1 - fx) + a[y0][:, x0 + 1] * fx
    bot = a[y0 + 1][:, x0] * (1 - fx) + a[y0 + 1][:, x0 + 1] * fx
    return top * (1 - fy) + bot * fy


def build(D, h1, Hy, sites, roads, bridges, log=print):
    nz = D.noise
    lv0, lv1 = G.L0, G.L1
    H0, W0 = lv0.shape
    log("  upsampling 9 m -> 3 m")
    h0 = G.upsample(h1, 3, kind="catmull_clamped")
    # ---------------------------------------------------------------- L1 helper fields
    slope1 = G.slope(h1, lv1.cell)
    reg = D.region
    cid = GEO.CONTINENT_IDS
    rocky = np.isin(reg, (3, 12, 7, 8, 9, 14)).astype(np.float32)
    demon = np.isin(reg, (8, 9)).astype(np.float32)
    desert = (reg == 13).astype(np.float32)
    # quiet zones: water, towns, roads (their own shaping happens later)
    quiet = Hy.water.astype(np.float32)
    quiet = np.maximum(quiet, G.dilate_mask(Hy.lake_mask, 3).astype(np.float32))
    for s in sites:
        sl, d = wg_sites._disk(lv1, s.x, s.y, s.r * 1.2)
        quiet[sl] = np.maximum(quiet[sl], (d < s.r * 1.2).astype(np.float32))
    quiet = G.blur(quiet, 1.5)
    amp1 = (0.35 + 0.45 * smoothstep(0.08, 0.5, slope1) + 1.2 * rocky * smoothstep(0.35, 1.0, slope1)
            * (1.0 - smoothstep(1.0, 1.8, slope1)) + 0.8 * demon)
    amp1 = amp1 * (1.0 - np.clip(quiet, 0, 1)) * (1.0 - 0.8 * desert) * (1.0 - 0.75 * D.heaven)
    ridge1 = (rocky * smoothstep(0.3, 0.9, slope1) + demon * 0.7).astype(np.float32)
    # dunes: desert flats away from towns, oases, mesas and the coast
    bad = (reg == 14).astype(np.float32)
    dune1 = (desert + 0.45 * bad) * smoothstep(90.0, 320.0, D.coast_d) * (1.0 - smoothstep(0.35, 0.7, slope1))
    dune1 *= 1.0 - np.clip(D.mesa * 3.0, 0, 1)
    dune1 *= (1.0 - np.clip(quiet * 2.0, 0, 1))
    dune1 = G.blur(dune1.astype(np.float32), 5.0)
    dune1 *= smoothstep(-0.35, 0.1, nz.fbm(*lv1.xy_full(), 2600.0, 3, k0=230))
    D.dune1 = dune1
    log("  detail noise + dunes")
    wind = (-math.cos(math.radians(18.0)), math.sin(math.radians(18.0)))   # blowing toward the WSW
    for r0 in range(0, H0, BAND):
        r1 = min(H0, r0 + BAND)
        ys = (np.arange(r0, r1, dtype=np.float64) * lv0.cell)[:, None]
        xs = (np.arange(W0, dtype=np.float64) * lv0.cell)[None, :]
        X = np.broadcast_to(xs, (r1 - r0, W0))
        Y = np.broadcast_to(ys, (r1 - r0, W0))
        a = upsample_rows(amp1, r0, r1)
        rg = upsample_rows(ridge1, r0, r1)
        fine = nz.fbm(X, Y, 70.0, 4, gain=0.5, k0=240)
        rock = nz.ridged(X, Y, 95.0, 4, gain=2.2, k0=250) - 0.45
        det = a * (0.8 * fine * (1.0 - rg) + 1.6 * rock * rg)
        band = h0[r0:r1]
        det *= smoothstep(0.4, 3.0, np.abs(band))
        dn = upsample_rows(dune1, r0, r1)
        if dn.max() > 1e-3:
            ph_w = nz.fbm(X, Y, 900.0, 3, k0=260) * 1.6 + nz.fbm(X, Y, 330.0, 2, k0=261) * 0.55
            proj = (X * wind[0] + Y * wind[1]) / 185.0 + ph_w
            fr = proj - np.floor(proj)
            prof = np.where(fr < 0.8, smoothstep(0.0, 0.8, fr), 1.0 - smoothstep(0.8, 1.0, fr))
            a2 = math.radians(18.0 + 38.0)
            proj2 = (X * -math.cos(a2) + Y * math.sin(a2)) / 90.0 + 0.6 * ph_w
            fr2 = proj2 - np.floor(proj2)
            prof2 = np.where(fr2 < 0.75, smoothstep(0.0, 0.75, fr2), 1.0 - smoothstep(0.75, 1.0, fr2))
            projd = (X * wind[0] + Y * wind[1]) / 720.0 + 0.5 * ph_w
            frd = projd - np.floor(projd)
            draa = np.where(frd < 0.7, smoothstep(0.0, 0.7, frd), 1.0 - smoothstep(0.7, 1.0, frd))
            damp = 6.0 + 9.0 * np.clip(0.5 + nz.fbm(X, Y, 1400.0, 3, k0=262), 0.0, 1.0)
            ripples = 0.12 * np.sin((X * wind[0] + Y * wind[1]) / 2.3 + 3.0 * ph_w)
            along = (-X * wind[1] + Y * wind[0])
            brk = smoothstep(-0.25, 0.25, nz.fbm(along, proj * 185.0, 420.0, 2, k0=263))
            dunes = damp * (0.8 * prof * (0.35 + 0.65 * brk) + 0.28 * prof2 - 0.45) + 11.0 * (draa - 0.5) + ripples
            det = det + dn * dunes
        h0[r0:r1] = band + det.astype(np.float32)
    # ---------------------------------------------------------------- rivers, lakes
    log("  carving rivers and lakes at 3 m")
    from wg_hydro import carve_rivers
    h0 = carve_rivers(h0, lv0, Hy.rivers, Hy.drybeds)
    lake_level0 = np.full((H0, W0), np.nan, np.float32)
    for lk in Hy.lakes:
        sl1 = lk["mask_sl"]
        m1 = lk["mask"].astype(np.float32)
        r0, r1 = sl1[0].start * 3, (sl1[0].stop - 1) * 3 + 1
        c0, c1 = sl1[1].start * 3, (sl1[1].stop - 1) * 3 + 1
        m0 = G.upsample(m1, 3, kind="catmull")
        sub = h0[r0:r1, c0:c1]
        inside = m0 > 0.5
        lvl = lk["level"]
        # bed below the water inside, gentle shore just above it outside
        dist_in = G.blur(m0, 2.0)
        bed = lvl - 0.6 - 3.0 * smoothstep(0.5, 0.95, dist_in)
        new = np.where(inside, np.minimum(sub, bed), sub)
        shore = (m0 > 0.05) & ~inside
        new = np.where(shore, np.maximum(new, lvl + 0.15), new)
        h0[r0:r1, c0:c1] = new
        ll = lake_level0[r0:r1, c0:c1]
        ll[inside] = lvl
    # ---------------------------------------------------------------- sites
    log("  site footprints")
    h0 = wg_sites.shape_footprints(h0, sites, lv0)
    # ---------------------------------------------------------------- roads
    log("  roads")
    road_w = np.zeros((H0, W0), np.uint8)
    h0 = flatten_roads(h0, roads, road_w, lv0)
    np.clip(h0, C.Z_MIN_M + 0.5, C.Z_MAX_M - 0.5, out=h0)
    return h0, road_w, lake_level0


def flatten_roads(h, roads, road_w, lv):
    """Level cross-section along each road; embankment width adapts to the height difference (1:1.6 slopes).
    Bridge spans do not touch the terrain (the river channel stays). road_w receives the paint mask (0..255)."""
    cell = lv.cell
    H, W = h.shape
    for rd in roads:
        p = rd["pts"]
        z = rd["z"]
        s = rd["s"]
        hw = rd["width"] * 0.5
        n = len(p)
        span = np.zeros(n, bool)
        for b in rd.get("bridges", []):
            span |= (s > b["s0"] + 3.0) & (s < b["s1"] - 3.0)
        CH = 20
        for c in range(0, n - 1, CH):
            # segments [c, c+CH) are "own"; one extra segment each side decides pixel ownership
            a0 = max(0, c - 2)
            e = min(n, c + CH + 3)
            seg = p[a0:e]
            zs = z[a0:e]
            dz_est = np.abs(zs - G.sample_bilinear(h, seg[:, 0] / cell, seg[:, 1] / cell))
            reach = hw + 6.0 + float(dz_est.max()) * 1.6 + 2 * cell
            x0 = max(0, int((seg[:, 0].min() - reach) / cell))
            x1 = min(W - 1, int((seg[:, 0].max() + reach) / cell) + 1)
            y0 = max(0, int((seg[:, 1].min() - reach) / cell))
            y1 = min(H - 1, int((seg[:, 1].max() + reach) / cell) + 1)
            xs = (np.arange(x0, x1 + 1) * cell)[None, :, None]
            ys = (np.arange(y0, y1 + 1) * cell)[:, None, None]
            ax, ay = seg[:-1, 0], seg[:-1, 1]
            dx, dy = seg[1:, 0] - ax, seg[1:, 1] - ay
            L2 = np.maximum(dx * dx + dy * dy, 1e-9)
            t = np.clip(((xs - ax) * dx + (ys - ay) * dy) / L2, 0.0, 1.0)
            d = np.sqrt((xs - (ax + t * dx)) ** 2 + (ys - (ay + t * dy)) ** 2)
            k = np.argmin(d, axis=2)
            dmin = np.take_along_axis(d, k[..., None], axis=2)[..., 0]
            tk = np.take_along_axis(t, k[..., None], axis=2)[..., 0]
            gk = a0 + k                                   # global segment index
            own = (gk >= c) & (gk < min(c + CH, n - 1))
            zr = zs[k] * (1 - tk) + zs[np.minimum(k + 1, len(zs) - 1)] * tk
            zr = zr + 0.12 * (1.0 - smoothstep(0.0, hw + 0.5, dmin))       # slight crown for drainage
            on_span = span[gk] & span[np.minimum(gk + 1, n - 1)]
            sub = h[y0:y1 + 1, x0:x1 + 1]
            diff = np.abs(sub - zr)
            shoulder = 4.0 + diff * 1.6
            w = 1.0 - smoothstep(hw + 1.0, hw + 1.0 + shoulder, dmin)
            w = np.where(on_span | ~own, 0.0, w)
            h[y0:y1 + 1, x0:x1 + 1] = sub + (zr - sub) * w
            pw = (1.0 - smoothstep(hw - 0.8, hw + 1.6, dmin)) * 255.0
            pw = np.where(on_span | ~own, 0.0, pw)
            rw = road_w[y0:y1 + 1, x0:x1 + 1]
            road_w[y0:y1 + 1, x0:x1 + 1] = np.maximum(rw, pw.astype(np.uint8))
    return h


def encode(h0):
    hv = (h0.astype(np.float64) * 100.0 - C.LANDSCAPE_Z_CM) * 128.0 / C.LANDSCAPE_SCALE_Z + 32768.0
    return np.clip(np.rint(hv), 0, 65535).astype("<u2")
