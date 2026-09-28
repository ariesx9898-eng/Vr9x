"""Designed (pre-erosion) heightfield of LA PLACE at the 9 m working grid.

Layers, all in metres above sea level:
  * land mask from the hand-traced outlines, domain-warped + fractal coast noise, cleaned, exact signed distance;
  * regions (Spec section 3) from partition polygons evaluated at warped coordinates + range masks;
  * regional base level + coast ramp (beaches) + cliff coasts + rolling hills (fBm / billow);
  * mountain ranges: warped crest-distance profile x ridged multifractal, passes cut as saddles;
  * region features: Heaven cliff plateau with terraced edge, Demon crags / ravines / craters / caldera,
    Begaritt mesas + canyons + rocky ring basin + buttes, lake basins, river valleys along the guide lines;
  * sea floor: shelf to -30 m near coasts, -200 m in open sea.
Dunes and fine detail are added later (after erosion).
"""
import math

import numpy as np

import wg_config as C
import wg_geo as GEO
import wg_grid as G
from wg_grid import smoothstep, lerp
from wg_noise import Noise2D

# per-region design parameters: base level (m), hill amplitude (m), coast ramp length (m), erodibility, talus (deg)
REGION_PARAMS = {
    0: (0.0, 0.0, 300.0, 0.0, 35.0),
    1: (30.0, 26.0, 520.0, 1.0, 33.0),
    2: (42.0, 32.0, 480.0, 1.0, 33.0),
    3: (70.0, 44.0, 400.0, 1.15, 41.0),
    4: (92.0, 46.0, 450.0, 0.9, 37.0),
    5: (86.0, 38.0, 500.0, 1.0, 33.0),
    6: (52.0, 42.0, 450.0, 1.0, 34.0),
    7: (850.0, 45.0, 150.0, 0.06, 80.0),
    8: (112.0, 50.0, 420.0, 0.75, 43.0),
    9: (150.0, 20.0, 420.0, 0.25, 43.0),
    10: (60.0, 36.0, 420.0, 1.0, 34.0),
    11: (36.0, 34.0, 450.0, 1.0, 33.0),
    12: (60.0, 36.0, 400.0, 1.15, 41.0),
    13: (50.0, 11.0, 420.0, 0.55, 33.0),
    14: (92.0, 36.0, 380.0, 0.6, 46.0),
    15: (14.0, 16.0, 140.0, 1.0, 34.0),
}


class Design:
    """Holds every working-grid field produced by the design stage."""
    pass


def _uv_poly_to_px(poly, lv):
    return [(u * (lv.W - 1), v * (lv.H - 1)) for u, v in poly]


def uv_to_m(u, v):
    return u * C.WORLD_W_M, v * C.WORLD_H_M


def meander_line(pts, nz, k, width, step=15.0):
    """Densify a guide polyline and add a tapered meander (sine + noise) perpendicular to it."""
    xs = np.array([p[0] for p in pts])
    ys = np.array([p[1] for p in pts])
    cum = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(xs), np.diff(ys)))])
    total = cum[-1]
    n = max(16, int(total / step))
    ss = np.linspace(0.0, total, n)
    px = np.interp(ss, cum, xs)
    py = np.interp(ss, cum, ys)
    # smooth the corners of the guide first
    for _ in range(6):
        px[1:-1] = 0.25 * px[:-2] + 0.5 * px[1:-1] + 0.25 * px[2:]
        py[1:-1] = 0.25 * py[:-2] + 0.5 * py[1:-1] + 0.25 * py[2:]
    tx = np.gradient(px)
    ty = np.gradient(py)
    tl = np.maximum(np.hypot(tx, ty), 1e-9)
    nx_, ny_ = -ty / tl, tx / tl
    lam = 620.0 + 22.0 * width
    amp = 45.0 + 2.2 * width
    ph = (k * 2.399) % (2 * math.pi)
    wander = nz.fbm(ss.astype(np.float64), np.full(n, 1000.0 * k + 77.0), lam * 1.7, 3, k0=220)
    off = amp * (np.sin(2 * math.pi * ss / lam + ph) + 1.6 * wander)
    taper = smoothstep(0.0, 350.0, ss) * smoothstep(0.0, 300.0, total - ss)
    off *= taper
    mx = px + nx_ * off
    my = py + ny_ * off
    out = list(zip(mx.tolist(), my.tolist()))
    tot = float(np.sum(np.hypot(np.diff(mx), np.diff(my))))
    return out, tot


def window(lv, cx, cy, R):
    c0 = max(0, int((cx - R) / lv.cell))
    c1 = min(lv.W, int((cx + R) / lv.cell) + 2)
    r0 = max(0, int((cy - R) / lv.cell))
    r1 = min(lv.H, int((cy + R) / lv.cell) + 2)
    return (slice(r0, r1), slice(c0, c1))


def build(seed=C.SEED, lv=G.L1, log=print):
    D = Design()
    D.lv = lv
    nz = Noise2D(seed)
    D.noise = nz
    H, W = lv.shape
    X, Y = lv.xy_full()
    cell = lv.cell
    cid = GEO.CONTINENT_IDS

    # ------------------------------------------------------------------ land mask
    l2 = G.L2
    polys = []
    for name, poly in GEO.CONTINENTS.items():
        polys.append((_uv_poly_to_px(poly, l2), cid[name]))
    for grp, poly in GEO.ISLANDS:
        polys.append((_uv_poly_to_px(poly, l2), cid["Islands"]))
    cont2 = G.rasterize_polygons(polys, l2.shape, supersample=2)
    sd2 = G.signed_distance(cont2 > 0, l2.cell)
    _, sy, sx = G.jfa_nearest(cont2 > 0)
    cont_near2 = cont2[np.clip(sy, 0, l2.H - 1), np.clip(sx, 0, l2.W - 1)].astype(np.float32)
    sd1 = G.upsample(sd2, C.L2_STEP // C.L1_STEP)
    cont_near = np.rint(G.sample_nearest(cont_near2, X / l2.cell, Y / l2.cell)).astype(np.uint8)
    is_island = cont_near == cid["Islands"]

    wx, wy = nz.warp(X, Y, 1600.0, 150.0, octaves=4, k0=0)
    sd_w = G.sample_bilinear(sd1, wx / cell, wy / cell)
    rough = 0.55 + 0.45 * smoothstep(-0.25, 0.35, nz.fbm(X, Y, 5000.0, 3, k0=11))
    amp = np.where(is_island, 22.0, 75.0 * rough).astype(np.float32)
    sd_f = sd_w + amp * nz.fbm(X, Y, 700.0, 7, gain=0.56, k0=20)
    land = sd_f > 0.0
    for s in GEO.SITES:
        if s["mode"] in ("inland", "fixed"):
            sx_m, sy_m = uv_to_m(*s["uv"])
            rr = s["r"] + 90.0
            sl = window(lv, sx_m, sy_m, rr)
            land[sl] |= ((X[sl] - sx_m) ** 2 + (Y[sl] - sy_m) ** 2) < rr * rr
    lab = G.label_components(land, 8)
    ids, counts = G.component_sizes(lab)
    small = ids[counts < 90]
    if len(small):
        land &= ~np.isin(lab, small)
    lab = G.label_components(~land, 4)
    ids, counts = G.component_sizes(lab)
    small = ids[counts < 400]
    if len(small):
        land |= np.isin(lab, small)
    log("  land: %.1f%% of the map" % (100.0 * land.mean()))
    coast_d = G.signed_distance(land, cell)
    # continuous shoreline coordinate: the smooth fractal field where it agrees with the cleaned mask
    agree = land == (sd_f > 0.0)
    D.shore_s = np.where(agree, sd_f, np.where(land, np.maximum(coast_d, 0.5), np.minimum(coast_d, -0.5))).astype(np.float32)
    # near the coast use the continuous field for every coast-relative profile (no 9 m staircase)
    near_c = np.abs(D.shore_s) < 220.0
    coast_d = np.where(near_c, D.shore_s, coast_d).astype(np.float32)
    e = np.maximum(coast_d, 0.0)
    lab = G.label_components(land, 8)
    flat_lab = lab.ravel()
    ok_ = flat_lab >= 0
    comp_ids, inv = np.unique(flat_lab[ok_], return_inverse=True)
    votes = np.zeros((len(comp_ids), 8), np.int64)
    np.add.at(votes, (inv, cont_near.ravel()[ok_].astype(np.int64)), 1)
    maj = votes.argmax(axis=1).astype(np.uint8)
    cn = cont_near.ravel().copy()
    cn[np.nonzero(ok_)[0]] = maj[inv]
    cont_near = cn.reshape(H, W)
    D.cont = np.where(land, cont_near, 0).astype(np.uint8)
    D.cont_near = cont_near

    # ------------------------------------------------------------------ range fields
    rwx, rwy = nz.warp(X, Y, 900.0, 230.0, octaves=4, k0=40)
    ranges = []
    for R in GEO.RANGES:
        best_d = np.full((H, W), 1e6, np.float32)
        best_h = np.zeros((H, W), np.float32)
        best_w = np.ones((H, W), np.float32)
        for line in R["lines"]:
            pts = [uv_to_m(p[0], p[1]) for p in line]
            att = [[p[2], p[3] if len(p) > 3 else 1.0] for p in line]
            d, along, at, total = G.polyline_field(lv, pts, R["foot"] * 1.6 + 400.0, attrs=att)
            d = np.where(np.isfinite(d), d, 1e6).astype(np.float32)
            dw = G.sample_bilinear(d, rwx / cell, rwy / cell)
            hw = G.sample_bilinear(at[0], rwx / cell, rwy / cell)
            ww = G.sample_bilinear(np.where(at[1] > 0, at[1], 1.0).astype(np.float32), rwx / cell, rwy / cell)
            # compare in width-normalised distance so narrow arms do not steal cells from wide ones
            better = dw / np.maximum(ww, 0.2) < best_d / np.maximum(best_w, 0.2)
            best_d = np.where(better, dw, best_d)
            best_h = np.where(better, hw, best_h)
            best_w = np.where(better, ww, best_w)
        ranges.append((R, best_d, best_h, best_w))
    D.ranges = ranges

    # ------------------------------------------------------------------ regions
    gwx, gwy = nz.warp(X, Y, 2200.0, 260.0, octaves=3, k0=60)
    gu, gv = gwx / C.WORLD_W_M, gwy / C.WORLD_H_M
    region = np.zeros((H, W), np.uint8)
    central = D.cont == cid["Central"]
    rem = central.copy()
    for poly, rid in ((GEO.NORTH_POLY, 4), (GEO.FITTOA_POLY, 2), (GEO.ASURA_POLY, 1), (GEO.STRIFE_POLY, 5)):
        inside = rem & G.point_in_polygon(gu, gv, poly)
        region[inside] = rid
        rem &= ~inside
    region[rem] = 6
    region[D.cont == cid["Heaven"]] = 7
    demon = D.cont == cid["Demon"]
    region[demon] = 8
    millis = D.cont == cid["Millis"]
    region[millis] = 11
    region[millis & G.point_in_polygon(gu, gv, GEO.GREAT_FOREST_POLY)] = 10
    beg = D.cont == cid["Begaritt"]
    region[beg] = 13
    region[beg & G.point_in_polygon(gu, gv, GEO.BEGARITT_BADLANDS_POLY)] = 14
    region[D.cont == cid["Islands"]] = 15
    for R, d, hc, wf in ranges:
        if R["region"] in (3, 12):
            core = R["core"] * wf
            prof = (1.0 - smoothstep(0.0, core, d)) ** 1.25
            fp = (1.0 - smoothstep(core * 0.25, R["foot"] * wf, d)) ** 1.6
            est = np.maximum(hc * prof * 0.8, R["foot_h"] * fp * 0.8)
            body = land & (hc > 260.0) & ((d < core) | (est > 110.0))
            body &= central if R["region"] == 3 else millis
            region[body] = R["region"]
    rk = GEO.CRATERS[0]
    cx, cy = uv_to_m(rk[1], rk[2])
    sl = window(lv, cx, cy, rk[3] * 1.1)
    rr = np.hypot(X[sl] - cx, Y[sl] - cy)
    region[sl][demon[sl] & (rr < rk[3] * 1.02)] = 9
    D.region = region

    # smoothed per-region parameters (extended into the sea from the nearest land cell)
    _, ly, lx = G.jfa_nearest(land)
    ly = np.clip(ly, 0, H - 1)
    lx = np.clip(lx, 0, W - 1)
    par = np.zeros((5, H, W), np.float32)
    table = np.array([REGION_PARAMS[i] for i in range(C.N_REGIONS)], np.float32)
    reg_near = region[ly, lx]
    for k in range(5):
        par[k] = G.blur_m(table[reg_near, k], lv, 300.0)
    base_lvl, hill_amp, ramp_len, erod, talus = par
    D.erod = erod
    D.talus = talus
    D.near_land_y, D.near_land_x = ly, lx

    # ------------------------------------------------------------------ base land surface
    ramp = smoothstep(0.0, ramp_len, e) ** 0.85
    cliffy = smoothstep(0.15, 0.45, nz.fbm(X, Y, 2600.0, 4, k0=80))
    cliffy = np.maximum(cliffy, 0.8 * (region == 4) * smoothstep(0.0, 0.3, nz.fbm(X, Y, 1800.0, 3, k0=81)))
    cliffy = np.where(np.isin(region, (13, 11, 10, 1, 15)), cliffy * 0.3, cliffy)
    cliff_w = 45.0 + 35.0 * np.abs(nz.fbm(X, Y, 700.0, 2, k0=82))
    cliff_frac = 0.8 * smoothstep(0.0, cliff_w, e) + 0.2 * ramp
    coastal = lerp(ramp, np.maximum(ramp, cliff_frac), cliffy)
    D.cliffy = cliffy

    macro = nz.fbm(X, Y, 4200.0, 3, k0=90)
    hwx, hwy = nz.warp(X, Y, 1600.0, 260.0, octaves=3, k0=95)
    bill = nz.billow(hwx, hwy, 1150.0, 5, k0=110)          # rounded hills with creased hollows
    broad = nz.fbm(X, Y, 2600.0, 3, k0=100)                 # uplands and broad lows
    mid = nz.fbm(hwx, hwy, 520.0, 4, gain=0.5, k0=105)
    vwx, vwy = nz.warp(X, Y, 1200.0, 300.0, octaves=3, k0=96)
    vn = np.abs(nz.fbm(vwx, vwy, 1900.0, 4, k0=97))
    vcrease = (1.0 - smoothstep(0.0, 0.10, vn)) ** 1.5     # dry side valleys between the hills
    rolling = 0.62 * bill + 0.55 * broad + 0.22 * mid - 0.45 * vcrease
    D.floor = (1.2 + (base_lvl * (1.0 + 0.35 * macro) - 1.2) * coastal).astype(np.float32)
    h = D.floor + hill_amp * rolling * smoothstep(0.0, 350.0, e)
    h = np.maximum(h, 0.6 + 2.2 * smoothstep(0.0, 60.0, e))

    # ------------------------------------------------------------------ mountains
    rgx, rgy = nz.warp(X, Y, 700.0, 120.0, octaves=3, k0=120)
    ridged_main = nz.ridged(rgx, rgy, 1150.0, octaves=7, gain=2.1, k0=130)
    ridged_jag = nz.ridged(rgx * 1.3, rgy * 1.3, 520.0, octaves=6, gain=2.6, k0=140)
    mountain = np.zeros((H, W), np.float32)       # simulated ranges (uplift target)
    mountain_fixed = np.zeros((H, W), np.float32)  # designed crags (Demon Continent)
    D.snowline = np.full((H, W), 9999.0, np.float32)
    for R, d, hc, wf in ranges:
        core, foot = R["core"] * wf, R["foot"] * wf
        prof = (1.0 - smoothstep(0.0, core, d)) ** 1.25
        if R["jag"] <= 1.0:
            rid = ridged_main
        else:
            rid = 0.45 * ridged_main + 0.55 * ridged_jag ** 1.3
        mt = hc * prof * (0.52 + 0.68 * rid)
        fp = (1.0 - smoothstep(core * 0.25, foot, d)) ** 1.6
        fh = R["foot_h"] * fp * (0.55 + 0.6 * np.clip(bill + 0.5, 0.0, 1.2))
        mtn = np.maximum(mt, fh) + 0.25 * np.minimum(mt, fh)
        mtn *= smoothstep(-20.0, 120.0, coast_d)
        own = {3: cid["Central"], 6: cid["Central"], 4: cid["Central"], 12: cid["Millis"], 8: cid["Demon"]}[R["region"]]
        mtn *= G.blur_m((cont_near == own).astype(np.float32), lv, 40.0)
        if R["region"] == 8:
            mountain_fixed = np.maximum(mountain_fixed, mtn)
        else:
            mountain = np.maximum(mountain, mtn)
        D.snowline = np.where(d < foot, np.minimum(D.snowline, R["snow"]), D.snowline)
    for key, P in GEO.PASSES.items():
        px, py = uv_to_m(*P["uv"])
        sl = window(lv, px, py, P["span"] + 3.0 * P["radius"])
        Xs, Ys = X[sl], Y[sl]
        a = math.radians(P["cross_deg"])
        ax = (Xs - px) * math.cos(a) + (Ys - py) * math.sin(a)       # along the road axis (across the range)
        cr = -(Xs - px) * math.sin(a) + (Ys - py) * math.cos(a)      # along the crest
        base_here = h[sl]
        cap = np.maximum(P["saddle"] - base_here, 0.0) + (cr / P["radius"]) ** 2 * 520.0 \
            - 70.0 * smoothstep(0.0, 1.0, np.abs(ax) / P["span"])
        cap = np.maximum(cap, 0.0)
        w = np.exp(-(np.abs(ax) / P["span"]) ** 4) * np.exp(-(cr / (2.2 * P["radius"])) ** 2)
        m = mountain[sl]
        mountain[sl] = lerp(m, np.minimum(m, cap), np.clip(1.3 * w, 0.0, 1.0))
    D.mountain = (mountain * (land | (coast_d > -60.0))).astype(np.float32)
    D.mountain_fixed = mountain_fixed
    h = h + mountain_fixed * land

    # ------------------------------------------------------------------ Heaven Continent plateau
    heaven = (cont_near == cid["Heaven"]) & land
    # continuous edge coordinate (the fractal shoreline field, not the pixel distance transform)
    e_c = np.where(np.abs(D.shore_s) < 250.0, np.maximum(D.shore_s, 0.0), e)
    hx, hy = nz.warp(X, Y, 520.0, 70.0, octaves=3, k0=156)
    e_w = e_c + 45.0 * nz.fbm(hx, hy, 420.0, 3, k0=157) * smoothstep(0.0, 40.0, e_c) \
        + 12.0 * nz.fbm(X, Y, 110.0, 2, k0=158) * smoothstep(0.0, 25.0, e_c)
    gully = np.abs(nz.fbm(hx, hy, 300.0, 3, k0=159))
    e_w = e_w - 50.0 * (1.0 - smoothstep(0.0, 0.08, gully)) * smoothstep(30.0, 150.0, e_c)
    e_w = np.maximum(e_w, 0.0)
    top = (835.0 + 55.0 * nz.fbm(X, Y, 1700.0, 4, k0=150) + 20.0 * nz.billow(hx, hy, 520.0, 4, k0=151)
           + 6.0 * nz.fbm(X, Y, 140.0, 3, k0=161))
    top = top + 26.0 * smoothstep(0.15, 0.45, nz.fbm(hx, hy, 330.0, 3, k0=162))   # rounded rocky knolls
    wc = 75.0 + 85.0 * np.clip(0.5 + nz.fbm(X, Y, 900.0, 3, k0=152), 0.0, 1.0)
    t = np.clip(e_w / wc, 0.0, 1.0)
    m1 = np.clip(0.5 + 1.2 * nz.fbm(X, Y, 1100.0, 2, k0=153), 0.0, 1.0)
    m2 = np.clip(0.5 + 1.2 * nz.fbm(X, Y, 1300.0, 2, k0=154), 0.0, 1.0)
    jt = 0.05 * nz.fbm(X, Y, 500.0, 2, k0=155)
    w0 = 0.34 + 0.3 * m1
    w1 = (1.0 - w0) * 0.55 * m2
    w2 = 1.0 - w0 - w1
    step = (w0 * smoothstep(0.02 + jt, 0.30 + jt, t) + w1 * smoothstep(0.38 + jt, 0.62 + jt, t)
            + w2 * smoothstep(0.70 + jt, 0.98, t))
    scree = 20.0 * (1.0 - smoothstep(0.0, 45.0, e_w)) * np.clip(0.5 + nz.fbm(X, Y, 250.0, 2, k0=163), 0.0, 1.0)
    hh = 1.5 + (top - 1.5) * step
    hh = np.maximum(hh, 1.5 + scree * smoothstep(0.0, 10.0, e_c))
    hh = hh + 4.0 * nz.fbm(X, Y, 70.0, 3, k0=164) * smoothstep(0.0, 15.0, e_c)
    h = np.where(heaven, hh, h)
    D.heaven = heaven
    D.heaven_step = np.where(heaven, step, 0.0).astype(np.float32)

    # ------------------------------------------------------------------ Demon Continent badlands
    demon_all = (cont_near == cid["Demon"]) & land
    crag_small = nz.ridged(rgx, rgy, 380.0, octaves=5, gain=2.6, k0=160)
    bad = 22.0 * crag_small ** 2.2 + 10.0 * nz.fbm(X, Y, 220.0, 4, k0=161)
    vx, vy = nz.warp(X, Y, 1300.0, 350.0, octaves=3, k0=162)
    rv = np.abs(nz.fbm(vx, vy, 2400.0, 4, k0=163))
    rav = (1.0 - smoothstep(0.0, 0.035, rv)) ** 1.4
    rav *= smoothstep(0.1, 0.35, nz.fbm(X, Y, 3000.0, 2, k0=164) + 0.3)
    rav *= smoothstep(80.0, 300.0, e)
    h = np.where(demon_all, h + (bad - 48.0 * rav) * smoothstep(0.0, 200.0, e), h)
    D.ravine = (rav * demon_all).astype(np.float32)

    # craters and caldera (local windows)
    hs = G.blur_m(h, lv, 420.0)
    D.crater_floor = np.zeros((H, W), bool)
    D.crater_rim = np.zeros((H, W), np.float32)
    for name, u, v, Rc, rim_h, depth in GEO.CRATERS:
        cx, cy = uv_to_m(u, v)
        sl = window(lv, cx, cy, 3.0 * Rc)
        Xs, Ys, hsl = X[sl], Y[sl], h[sl]
        wxx, wyy = nz.warp(Xs, Ys, 0.9 * Rc, 0.13 * Rc, octaves=3, k0=170)
        r = np.hypot(wxx - cx, wyy - cy) / Rc
        ground = float(G.sample_bilinear(hs, np.array([cx / cell]), np.array([cy / cell]))[0])
        floor_r = 0.62 if name == "Rikarisu" else 0.45
        rim_var = rim_h * (0.35 + 0.85 * np.clip(0.5 + 1.3 * nz.fbm(Xs, Ys, 0.7 * Rc, 3, k0=171), 0.0, 1.0))
        angc = np.arctan2(Ys - cy, Xs - cx)
        radial = np.abs(np.sin(angc * (7 + int(Rc / 40)) + 2.5 * nz.fbm(Xs, Ys, 0.5 * Rc, 2, k0=172))) ** 2
        rim_var = rim_var * (1.0 - 0.35 * radial * smoothstep(0.7, 1.1, r))
        inner = -depth + (depth + rim_var) * smoothstep(floor_r, 1.0, r) ** 1.7
        outer = rim_var * np.exp(-3.2 * (r - 1.0))
        prof = np.where(r < 1.0, inner, outer)
        if name == "Rikarisu":
            ang = np.arctan2(Ys - cy, Xs - cx)
            dn = np.abs(np.angle(np.exp(1j * (ang - math.radians(95.0)))))
            notch = np.exp(-(dn / 0.22) ** 2) * smoothstep(0.55, 0.95, r) * smoothstep(1.9, 1.0, r)
            prof = prof - notch * (rim_h * 0.8)
            D.rikarisu_floor_z = ground - depth
        target = ground + prof
        wgt = smoothstep(3.0, 1.6, r)
        inside = r < 1.0
        newh = np.where(inside, target + 0.15 * (hsl - ground) * smoothstep(floor_r, 1.0, r),
                        lerp(hsl, np.maximum(hsl, target), wgt))
        h[sl] = newh
        D.crater_rim[sl] = np.maximum(D.crater_rim[sl], np.exp(-((r - 1.0) / 0.18) ** 2))
        if name == "Rikarisu":
            D.crater_floor[sl] |= r < floor_r
    cal = GEO.CALDERA
    cx, cy = uv_to_m(*cal["uv"])
    sl = window(lv, cx, cy, 3.0 * cal["rim_r"])
    Xs, Ys, hsl = X[sl], Y[sl], h[sl]
    r = np.hypot(Xs - cx, Ys - cy) / cal["rim_r"]
    ground = float(G.sample_bilinear(hs, np.array([cx / cell]), np.array([cy / cell]))[0])
    ang = np.arctan2(Ys - cy, Xs - cx)
    dn = np.abs(np.angle(np.exp(1j * (ang - math.pi))))
    breach = np.exp(-(dn / 0.16) ** 2)
    rim = cal["rim_h"] * (0.8 + 0.35 * nz.ridged(Xs, Ys, 300.0, 3, k0=175)) * (1.0 - 0.93 * breach)
    inner = -70.0 + (70.0 + rim) * smoothstep(0.35, 1.0, r) ** 1.5
    outer = rim * np.exp(-2.6 * (r - 1.0))
    target = ground + np.where(r < 1.0, inner, outer)
    h[sl] = np.where(r < 1.0, target, np.maximum(hsl, lerp(hsl, target, smoothstep(3.0, 1.4, r))))
    D.crater_rim[sl] = np.maximum(D.crater_rim[sl], np.exp(-((r - 1.0) / 0.2) ** 2))
    D.caldera_ground = ground

    # ------------------------------------------------------------------ Begaritt: mesas, canyons, ring, buttes
    beg_all = (cont_near == cid["Begaritt"]) & land
    mx, my = nz.warp(X, Y, 1500.0, 300.0, octaves=3, k0=180)
    Pm = nz.fbm(mx, my, 2600.0, 5, gain=0.5, k0=181)
    badness = smoothstep(-0.1, 0.4, (region == 14).astype(np.float32) + 0.3 * nz.fbm(X, Y, 3000.0, 2, k0=182))
    t1 = smoothstep(0.02, 0.06, Pm) * badness
    t2 = smoothstep(0.17, 0.21, Pm) * badness
    t3 = smoothstep(0.30, 0.33, Pm) * badness
    mesa_h = 70.0 * t1 + 60.0 * t2 + 50.0 * t3
    cx_, cy_ = nz.warp(X, Y, 900.0, 260.0, octaves=3, k0=183)
    cv = np.abs(nz.fbm(cx_, cy_, 2100.0, 4, k0=184))
    canyon = 1.0 - smoothstep(0.012, 0.045, cv)
    mesa_h = mesa_h * (1.0 - 0.92 * canyon) * smoothstep(60.0, 260.0, e)
    h = np.where(beg_all, h + mesa_h, h)
    D.mesa = np.where(beg_all, mesa_h / 180.0, 0.0).astype(np.float32)
    D.canyon = np.where(beg_all, canyon * badness, 0.0).astype(np.float32)
    ring = GEO.BEGARITT_RING
    rcx, rcy = uv_to_m(*ring["uv"])
    sl = window(lv, rcx, rcy, ring["r_out"] * 2.0)
    Xs, Ys, hsl = X[sl], Y[sl], h[sl]
    qx, qy = nz.warp(Xs, Ys, 700.0, 130.0, octaves=3, k0=185)
    rr = np.hypot(qx - rcx, qy - rcy)
    ang_r = np.arctan2(Ys - rcy, Xs - rcx)
    rr = rr + 30.0 * np.abs(np.sin(ang_r * 13.0 + 3.0 * nz.fbm(Xs, Ys, 200.0, 2, k0=189))) ** 3
    tt = (rr - ring["r_in"]) / (ring["r_out"] - ring["r_in"])
    bump = np.where((tt > 0) & (tt < 1), np.sin(np.pi * np.clip(tt, 0, 1) ** 0.75) ** 0.8, 0.0)
    jag = nz.ridged(Xs * 1.1, Ys * 1.1, 260.0, 5, gain=2.5, k0=186)
    ring_h = ring["h_lo"] + (ring["h_hi"] - ring["h_lo"]) * np.clip(0.2 + 0.9 * jag, 0.0, 1.0)
    ang = np.arctan2(Ys - rcy, Xs - rcx)
    dg = np.abs(np.angle(np.exp(1j * (ang - math.radians(ring["gap_deg"])))))
    gap = np.exp(-(dg / 0.13) ** 2)
    ringz = ring_h * bump * (1.0 - 0.82 * gap)
    basin = ring["floor"] + 6.0 * nz.fbm(Xs, Ys, 250.0, 3, k0=187)
    inner_w = smoothstep(ring["r_in"] + 60.0, ring["r_in"] - 80.0, rr)
    apron = 0.28 * ring["h_lo"] * smoothstep(ring["r_out"] * 1.8, ring["r_out"], rr) * (1.0 - 0.8 * gap)
    hz = np.maximum(lerp(hsl, basin, inner_w), ringz)
    hz = np.where(tt >= 1.0, hsl + apron, hz)
    h[sl] = np.where(beg_all[sl], hz, hsl)
    D.ring_sl = sl
    D.ring_rr = rr
    D.buttes = []
    brng = np.random.default_rng(seed + 5)
    for bi, (u, v, Rb, Hb) in enumerate(GEO.OUTCROPS):
        bx, by = uv_to_m(u, v)
        sl = window(lv, bx, by, 2.0 * Rb)
        Xs, Ys = X[sl], Y[sl]
        qx, qy = nz.warp(Xs, Ys, 260.0, 0.22 * Rb, octaves=3, k0=190 + bi)
        r_eff = np.full(Xs.shape, 9.0)
        for lb in range(3):
            ox, oy = brng.uniform(-0.38, 0.38, 2) * Rb
            ra = Rb * brng.uniform(0.5, 0.78)
            rb_ = ra * brng.uniform(0.55, 0.95)
            an = brng.uniform(0, np.pi)
            dx, dy = qx - (bx + ox), qy - (by + oy)
            ex = (dx * math.cos(an) + dy * math.sin(an)) / ra
            ey = (-dx * math.sin(an) + dy * math.cos(an)) / rb_
            r_eff = np.minimum(r_eff, np.sqrt(ex * ex + ey * ey))
        ang = np.arctan2(Ys - by, Xs - bx)
        gul = np.abs(np.sin(ang * (9 + bi * 2) + 2.0 * nz.fbm(Xs, Ys, 150.0, 2, k0=192)))
        r_eff = r_eff + 0.06 * (1.0 - gul) ** 3
        cap = Hb * (1.0 - smoothstep(0.8, 1.0, r_eff)) * (0.95 + 0.05 * nz.fbm(Xs, Ys, 90.0, 3, k0=193))
        ledge = 0.5 * Hb * (1.0 - smoothstep(1.05, 1.22, r_eff)) * smoothstep(-0.2, 0.1, nz.fbm(Xs, Ys, 250.0, 2, k0=194))
        apron = 0.2 * Hb * (1.0 - smoothstep(1.0, 1.75, r_eff)) ** 1.4
        butte = np.maximum(np.maximum(cap, ledge), apron)
        h[sl] = h[sl] + butte
        D.mesa[sl] = np.maximum(D.mesa[sl], 1.0 - smoothstep(1.1, 1.25, r_eff))
        D.buttes.append((bx, by, Rb, Hb))

    # ------------------------------------------------------------------ lake basins
    D.lake_re = {}
    D.lake_level_design = {}
    D.lake_zone = np.zeros((H, W), bool)
    for L in GEO.LAKES:
        lx_, ly_ = uv_to_m(*L["uv"])
        a, b = L["ab"]
        sl = window(lv, lx_, ly_, 2.5 * a)
        Xs, Ys, hsl = X[sl], Y[sl], h[sl]
        ang = math.radians(L["ang"])
        qx, qy = nz.warp(Xs, Ys, max(120.0, a), 0.22 * a, octaves=2, k0=200)
        dx, dy = qx - lx_, qy - ly_
        ex = (dx * math.cos(ang) + dy * math.sin(ang)) / a
        ey = (-dx * math.sin(ang) + dy * math.cos(ang)) / b
        re = np.sqrt(ex * ex + ey * ey)
        D.lake_re[L["key"]] = (sl, re)
        D.lake_zone[sl] |= re < 1.7
        D.mountain[sl] *= smoothstep(1.3, 2.4, re)
        if L["key"] == "CalderaLake":
            continue
        ring_idx = (re > 1.4) & (re < 2.2)
        ground = float(np.percentile(hsl[ring_idx], 30)) if ring_idx.any() else float(hsl.mean())
        depth = 5.0 if L["kind"] == "Oasis" else 14.0
        D.lake_level_design[L["key"]] = ground - 1.5
        level = ground - 1.5
        bowl = level - depth * (1.0 - np.clip(re, 0, 1) ** 2)
        shore = lerp(level, hsl, smoothstep(1.0, 2.2, re))
        hn = np.where(re < 1.0, np.minimum(hsl, bowl), np.minimum(hsl, np.maximum(shore, ground - 1.0)))
        # natural rim: the basin holds water on every side (an outflow valley later cuts the only notch)
        rim = level + 1.2 * (1.0 - smoothstep(1.35, 2.0, re))
        hn = np.where(re >= 1.0, np.maximum(hn, np.where(re < 2.0, rim, hn)), hn)
        h[sl] = hn

    # ------------------------------------------------------------------ river valleys (guides)
    D.river_guides = []
    for ri, Rv in enumerate(GEO.RIVERS):
        mpts, total = meander_line([uv_to_m(u, v) for u, v in Rv["pts"]], nz, ri, Rv["width"][1])
        D.river_guides.append((Rv, mpts, total))

    # ------------------------------------------------------------------ sea floor
    sea = ~land
    dsea = np.maximum(-coast_d, 0.0)
    deep_near = G.blur_m((cont_near == cid["Heaven"]).astype(np.float32), lv, 450.0)
    shelf = -(1.0 + 29.0 * smoothstep(0.0, 260.0 - 180.0 * deep_near, dsea))
    ocean = shelf - 170.0 * smoothstep(250.0, 2600.0, dsea) - deep_near * 40.0 * smoothstep(0.0, 80.0, dsea)
    ocean = G.blur_m(ocean, lv, 140.0)
    ocean += 6.0 * nz.fbm(X, Y, 900.0, 4, k0=210) * smoothstep(50.0, 400.0, dsea)
    h = np.where(sea, np.minimum(ocean, -0.8), h)
    h = np.where(land, np.maximum(h, 0.35), h)
    # continuous beach / shelf profile through the waterline (smooth coastline at any resolution)
    s_ = D.shore_s
    hvn = G.blur_m((cont_near == cid["Heaven"]).astype(np.float32), lv, 60.0)
    wide = 45.0 - 30.0 * hvn
    shore = 0.055 * s_
    w = 1.0 - smoothstep(0.4 * wide, wide, np.abs(s_))
    h = h + (shore - h) * w
    band = np.abs(s_) < 0.4 * wide
    h = np.where(band, h, np.where(land, np.maximum(h, 0.08), np.minimum(h, -0.08)))

    D.h = h.astype(np.float32)
    D.land = land
    D.coast_d = coast_d
    return D
