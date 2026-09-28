"""Landscape paint layers (6097 x 4573, sum 255), foliage density maps and the region map.

Layer weights per pixel = region affinity (soft, noise-warped boundaries) modulated by slope, height, aspect,
moisture and distances to water / roads / towns, plus explicit masks (beaches, riverbanks, snow line, patchwork
farmland fields, forests, roads). Weights are sharpened (power 2.2) for crisp but blended transitions, then quantised
so the eleven layers sum to exactly 255 at every vertex.
"""
import math

import numpy as np
from PIL import Image

import wg_config as C
import wg_geo as GEO
import wg_grid as G
import wg_sites
from wg_grid import smoothstep
from wg_fullres import upsample_rows, BAND

L = {n: i for i, n in enumerate(C.LAYERS)}
NL = len(C.LAYERS)

# region -> base affinity per layer (Grass, Farmland, ForestFloor, Moss, Snow, Sand, Desert, DemonSoil, Rock, Road, Mud)
AFF = np.zeros((C.N_REGIONS, NL), np.float32)


def _aff(rid, **kw):
    for k, v in kw.items():
        AFF[rid, L[k]] = v


_aff(0, Sand=1.0, Mud=0.6, Rock=0.2)
_aff(1, Grass=1.0, Moss=0.05, Mud=0.03)
_aff(2, Grass=1.0, Moss=0.08, Mud=0.03)
_aff(3, Grass=0.55, Rock=0.45, Moss=0.12, ForestFloor=0.05)
_aff(4, Snow=1.0, Grass=0.12, Moss=0.15, Rock=0.08)
_aff(5, Grass=0.75, Mud=0.2, Sand=0.12, Farmland=0.05, Desert=0.08)
_aff(6, Grass=0.6, ForestFloor=0.35, Moss=0.3, Mud=0.1)
_aff(7, Snow=0.7, Rock=0.45, Moss=0.12, Grass=0.05)
_aff(8, DemonSoil=1.0, Rock=0.18, Sand=0.06)
_aff(9, DemonSoil=1.0, Rock=0.25, Mud=0.05)
_aff(10, ForestFloor=0.9, Moss=0.75, Grass=0.2, Mud=0.1)
_aff(11, Grass=1.0, Moss=0.1, Mud=0.04)
_aff(12, Grass=0.5, Rock=0.45, Moss=0.15, ForestFloor=0.08)
_aff(13, Desert=1.0, Sand=0.15, Rock=0.05)
_aff(14, Desert=0.7, Rock=0.45, Sand=0.1)
_aff(15, Sand=0.35, Grass=0.7, ForestFloor=0.2, Moss=0.1)

# forest coverage threshold per region (lower = more forest) and tree density scale
FOREST = {1: (0.22, 0.85), 2: (0.20, 0.85), 3: (0.05, 0.6), 4: (0.12, 0.7), 5: (0.40, 0.3), 6: (-0.35, 0.95),
          7: (2.0, 0.0), 8: (0.72, 0.16), 9: (0.8, 0.05), 10: (-0.8, 1.0), 11: (0.18, 0.8), 12: (0.02, 0.6),
          13: (0.9, 0.0), 14: (0.9, 0.0), 15: (0.05, 0.55)}
FIELD_ANGLE = {1: -12.0, 2: 22.0, 5: 5.0, 6: -30.0, 11: 35.0}


class PaintFields:
    pass


def prepare(D, h1, Hy, sites, roads, log=print):
    """L1 (9 m) helper fields shared by layers and densities."""
    lv = G.L1
    nz = D.noise
    X, Y = lv.xy_full()
    P = PaintFields()
    reg = D.region
    # soft region weights through warped lookup of the hard map, then per-layer affinity
    wx, wy = nz.warp(X, Y, 420.0, 70.0, octaves=3, k0=300)
    reg_w = G.sample_nearest(reg.astype(np.float32), wx / lv.cell, wy / lv.cell).astype(np.int64)
    reg_w = np.where(D.land, reg_w, 0)
    base = AFF[reg_w]                        # H x W x NL
    P.base = [G.blur(base[..., k], 4.0) for k in range(NL)]
    del base
    P.region = reg_w
    # moisture from drainage area and distance to water
    A = Hy.A.reshape(h1.shape)
    wet = Hy.water
    dwat, _, _ = G.jfa_nearest(wet)
    dwat *= lv.cell
    P.dwater = dwat
    moist = np.clip((np.log10(np.maximum(A, 1.0)) - 3.6) / 2.4, 0, 1) * 0.6 + 0.6 * (1.0 - smoothstep(0.0, 160.0, dwat))
    P.moist = G.blur(np.clip(moist, 0, 1).astype(np.float32), 1.5)
    # distance to towns and to roads (9 m)
    town = np.zeros(h1.shape, bool)
    dtown = np.full(h1.shape, 1e6, np.float32)
    for s in sites:
        sl, d = wg_sites._disk(lv, s.x, s.y, s.r + 2600.0)
        dtown[sl] = np.minimum(dtown[sl], np.maximum(d - s.r, 0.0).astype(np.float32))
        town[sl] |= d < s.r
    P.dtown = dtown
    P.town = town
    rmask = np.zeros(h1.shape, bool)
    for rd in roads:
        pts = rd["pts"][::2]
        ii = np.clip(np.rint(pts[:, 1] / lv.cell).astype(int), 0, lv.H - 1)
        jj = np.clip(np.rint(pts[:, 0] / lv.cell).astype(int), 0, lv.W - 1)
        rmask[ii, jj] = True
    droad, _, _ = G.jfa_nearest(rmask)
    P.droad = droad * lv.cell
    slope1 = G.slope(h1, lv.cell)
    P.slope1 = slope1
    # forest mask (0..1) per region coverage, excluding towns, roads, steep and high ground
    fx, fy = nz.warp(X, Y, 1500.0, 350.0, octaves=3, k0=310)
    fn = nz.fbm(fx, fy, 1100.0, 5, gain=0.55, k0=311)
    thr = np.zeros(h1.shape, np.float32)
    dens = np.zeros(h1.shape, np.float32)
    for rid, (t, dsc) in FOREST.items():
        thr[reg_w == rid] = t * 0.35
        dens[reg_w == rid] = dsc
    thr = G.blur(thr, 6.0)
    dens = G.blur(dens, 6.0)
    forest = smoothstep(thr - 0.04, thr + 0.05, fn)
    treeline = np.where(reg_w == 4, 480.0, np.where(np.isin(reg_w, (7,)), 2000.0, 640.0))
    forest *= 1.0 - smoothstep(treeline - 90.0, treeline, h1)
    forest *= 1.0 - smoothstep(0.55, 0.95, slope1)
    forest *= smoothstep(40.0, 160.0, dtown)
    forest *= smoothstep(8.0, 26.0, P.droad)
    forest *= smoothstep(2.0, 18.0, dwat)
    forest *= D.land
    # farmland zone: gentle land near settlements in farming regions
    farm_reg = np.isin(reg_w, (1, 2, 11)) * 1.0 + np.isin(reg_w, (5, 6)) * 0.45
    near = np.zeros(h1.shape, np.float32)
    for s in sites:
        if s.style in ("AsuraCapital", "AsuraTown", "RuralVillage", "MillisCapital", "MillisPort"):
            reach = {"AsuraCapital": 2300.0, "MillisCapital": 2100.0, "RuralVillage": 1500.0}.get(s.style, 1300.0)
            sl, d = wg_sites._disk(lv, s.x, s.y, s.r + reach)
            near[sl] = np.maximum(near[sl], 1.0 - smoothstep(s.r + 0.45 * reach, s.r + reach, d))
    farm = farm_reg * near * (1.0 - smoothstep(0.07, 0.16, slope1)) * smoothstep(8.0, 30.0, dwat)
    farm *= (1.0 - np.clip(D.mesa * 4.0, 0, 1)) * D.land * (~town)
    farm = G.blur(farm.astype(np.float32), 1.5)
    forest *= 1.0 - smoothstep(0.25, 0.6, farm)
    P.forest = G.blur(forest.astype(np.float32), 1.0)
    P.tree_dens = dens
    P.farm = farm
    # oasis vegetation halo
    oasis = np.zeros(h1.shape, np.float32)
    for lk in Hy.lakes:
        if lk["kind"] == "Oasis":
            sl = lk["mask_sl"]
            m = G.blur(G.dilate_mask(lk["mask"], 6).astype(np.float32), 3.0)
            oasis[sl] = np.maximum(oasis[sl], m)
    P.oasis = oasis
    P.snowline = G.blur(np.where(D.snowline > 5000, 9999.0, D.snowline).astype(np.float32), 20.0)
    P.coast_d = D.coast_d
    P.ravine = D.ravine
    P.mesa = D.mesa
    return P


def _field_pattern(X, Y, rid_map, nz, cell_m=120.0):
    """Patchwork fields: jittered rotated grid Voronoi. Returns (crop 0/1 per field, border strength 0..1)."""
    ang = np.zeros(X.shape, np.float32)
    for rid, a in FIELD_ANGLE.items():
        ang[rid_map == rid] = math.radians(a)
    c, s = np.cos(ang), np.sin(ang)
    u = (X * c + Y * s) / (cell_m * 1.7)
    v = (-X * s + Y * c) / cell_m
    iu, iv = np.floor(u), np.floor(v)
    best = np.full(X.shape, 1e9, np.float32)
    second = np.full(X.shape, 1e9, np.float32)
    bid = np.zeros(X.shape, np.int64)
    for du in (-1, 0, 1):
        for dv in (-1, 0, 1):
            cu, cv = iu + du, iv + dv
            hsh = (cu.astype(np.int64) * 73856093) ^ (cv.astype(np.int64) * 19349663)
            hsh = (hsh ^ (hsh >> 13)) * 1274126177
            jx = ((hsh & 1023) / 1023.0) * 0.55 + 0.225
            jy = (((hsh >> 10) & 1023) / 1023.0) * 0.55 + 0.225
            ddu = (cu + jx - u) * 1.7
            ddv = cv + jy - v
            dd = np.sqrt(ddu * ddu + ddv * ddv).astype(np.float32)
            closer = dd < best
            second = np.where(closer, best, np.minimum(second, dd))
            best = np.where(closer, dd, best)
            bid = np.where(closer, hsh, bid)
    crop = ((bid >> 20) & 7) < 5          # ~5/8 of fields cultivated
    tone = ((bid >> 24) & 3).astype(np.float32) / 3.0
    border = 1.0 - smoothstep(0.0, 0.05, second - best)
    return crop.astype(np.float32), border, tone


def layers(D, h0, road_w, lake_level0, P, Hy, sites, out_dir, log=print, compress=3):
    lv0, lv1 = G.L0, G.L1
    nz = D.noise
    H0, W0 = h0.shape
    out = np.zeros((NL, H0, W0), np.uint8)
    from wg_hydro import river_mask
    bank1 = river_mask(lv1, Hy.rivers, extra=10.0).astype(np.float32)
    river1 = river_mask(lv1, Hy.rivers, extra=0.0).astype(np.float32)
    dry1 = river_mask(lv1, [dict(pts=d["pts"], width=d["width"]) for d in Hy.drybeds], extra=4.0).astype(np.float32)
    lake1 = G.dilate_mask(Hy.lake_mask, 1).astype(np.float32)
    region_f = P.region.astype(np.float32)
    for r0 in range(0, H0, BAND):
        r1 = min(H0, r0 + BAND)
        n = r1 - r0
        ys = (np.arange(r0, r1, dtype=np.float64) * lv0.cell)[:, None]
        xs = (np.arange(W0, dtype=np.float64) * lv0.cell)[None, :]
        X = np.broadcast_to(xs, (n, W0))
        Y = np.broadcast_to(ys, (n, W0))
        # heights & slope with a 1-row halo
        a0, a1 = max(0, r0 - 1), min(H0, r1 + 1)
        hb = h0[a0:a1]
        gx, gy = G.gradient(hb, lv0.cell)
        sl_ = np.sqrt(gx * gx + gy * gy)[r0 - a0:r0 - a0 + n]
        gy = gy[r0 - a0:r0 - a0 + n]
        h = h0[r0:r1]
        wts = [upsample_rows(P.base[k], r0, r1) for k in range(NL)]
        moist = upsample_rows(P.moist, r0, r1)
        forest = upsample_rows(P.forest, r0, r1)
        farm = upsample_rows(P.farm, r0, r1)
        oasis = upsample_rows(P.oasis, r0, r1)
        snowl = upsample_rows(P.snowline, r0, r1)
        bank = upsample_rows(bank1, r0, r1)
        riv = upsample_rows(river1, r0, r1)
        dry = upsample_rows(dry1, r0, r1)
        lake = upsample_rows(lake1, r0, r1)
        rav = upsample_rows(P.ravine, r0, r1)
        mesa = upsample_rows(P.mesa, r0, r1)
        rid = G.sample_nearest(region_f, np.broadcast_to(np.arange(W0) / 3.0, (n, W0)),
                               np.broadcast_to((np.arange(r0, r1) / 3.0)[:, None], (n, W0))).astype(np.int64)
        n1 = nz.fbm(X, Y, 45.0, 3, k0=320)
        n2 = nz.fbm(X, Y, 260.0, 3, k0=321)
        brk = 0.5 + 0.5 * np.clip(n1 * 1.6 + n2, -1, 1)          # 0..1 breakup
        g = wts
        # forests
        fo = smoothstep(0.35, 0.65, forest + 0.12 * n1)
        g[L["ForestFloor"]] += 1.6 * fo * (1.0 - smoothstep(0.6, 0.95, sl_))
        g[L["Moss"]] += 0.45 * fo * moist
        g[L["Grass"]] *= 1.0 - 0.75 * fo
        # farmland patchwork
        fz = farm > 0.02
        if fz.any():
            crop, border, tone = _field_pattern(X, Y, rid, nz)
            fm = smoothstep(0.25, 0.55, farm + 0.1 * n2)
            g[L["Farmland"]] += 2.6 * fm * crop * (1.0 - border)
            g[L["Grass"]] += 1.2 * fm * (1.0 - crop * (1.0 - border))
        # moisture: meadows get mossier / muddier in wet hollows
        g[L["Mud"]] += 0.35 * smoothstep(0.75, 1.0, moist) * (1.0 - smoothstep(0.2, 0.4, sl_)) * (h > 0.5)
        # rock on steep slopes (breakup noise), mesas and cliffs
        steep = smoothstep(0.55, 1.0, sl_ + 0.18 * (brk - 0.5))
        g[L["Rock"]] += 3.2 * steep + 1.2 * mesa * smoothstep(0.35, 0.7, sl_)
        for k in range(NL):
            if k != L["Rock"]:
                g[k] *= 1.0 - 0.85 * steep
        # alpine: rock and thin grass above the tree line, snow above the snow line (less on cliffs, more on
        # north-facing slopes)
        alpine = smoothstep(snowl - 260.0, snowl - 80.0, h)
        g[L["Rock"]] += 0.9 * alpine * brk
        g[L["Grass"]] *= 1.0 - 0.5 * alpine
        g[L["ForestFloor"]] *= 1.0 - alpine
        north_face = smoothstep(-0.05, 0.25, gy)
        snow = smoothstep(snowl - 30.0 - 60.0 * north_face, snowl + 60.0, h + 25.0 * (brk - 0.5))
        snow *= 1.0 - smoothstep(0.75, 1.15, sl_)
        heaven_top = (rid == 7) * smoothstep(760.0, 820.0, h) * (1.0 - smoothstep(0.55, 0.9, sl_))
        snow = np.maximum(snow, heaven_top * smoothstep(0.35, 0.7, brk + 0.2))
        g[L["Snow"]] += 3.0 * snow
        north = rid == 4
        g[L["Snow"]] *= np.where(north, 1.0 - 0.6 * smoothstep(0.4, 0.8, sl_), 1.0)
        # beaches, shallow sea floor, riverbanks, lake shores
        beach = (1.0 - smoothstep(1.8, 4.5, h + 1.5 * n1)) * (1.0 - smoothstep(0.25, 0.5, sl_))
        beach *= (h > -12.0)
        not_heaven = rid != 7
        g[L["Sand"]] += 3.5 * beach * not_heaven
        for k in (L["Grass"], L["Farmland"], L["ForestFloor"], L["Snow"], L["DemonSoil"], L["Desert"]):
            g[k] *= 1.0 - 0.9 * beach * not_heaven
        deep = smoothstep(-8.0, -30.0, h)
        g[L["Mud"]] += 1.5 * deep
        g[L["Sand"]] *= 1.0 - 0.6 * deep
        bank_only = np.clip(bank - riv, 0, 1)
        g[L["Mud"]] += 1.8 * bank_only * (0.6 + 0.4 * brk) + 2.5 * riv
        g[L["Sand"]] += 1.2 * bank_only * (1.0 - brk) + 1.0 * riv * (1.0 - brk)
        g[L["Sand"]] += 2.5 * dry
        g[L["Desert"]] *= 1.0 - 0.7 * dry
        lk = (1.0 - smoothstep(0.0, 1.0, lake * 0 + 0.0)) * 0.0
        ll = lake_level0[r0:r1]
        near_lake = lake * (1.0 - np.isnan(ll))
        g[L["Mud"]] += 1.4 * lake * (0.5 + 0.5 * brk)
        g[L["Sand"]] += 0.8 * lake * (1.0 - brk) * (rid != 4)
        # demon ravine floors dusty, oases green
        g[L["Sand"]] += 1.2 * rav * (rid >= 8) * (rid <= 9)
        g[L["Grass"]] += 2.2 * oasis
        g[L["Moss"]] += 0.6 * oasis
        g[L["Desert"]] *= 1.0 - 0.8 * oasis
        # roads dominate their footprint
        rw = road_w[r0:r1].astype(np.float32) / 255.0
        for k in range(NL):
            g[k] *= 1.0 - rw
        g[L["Road"]] += 6.0 * rw
        edge = smoothstep(0.02, 0.3, rw) * (1.0 - smoothstep(0.6, 0.95, rw))
        g[L["Mud"]] += 0.6 * edge
        # sharpen + normalise to exactly 255
        wsum = np.zeros((n, W0), np.float32)
        for k in range(NL):
            np.maximum(g[k], 0.0, out=g[k])
            g[k] = g[k] ** 2.2
            wsum += g[k]
        wsum = np.maximum(wsum, 1e-12)
        q = np.zeros((NL, n, W0), np.int32)
        best = np.zeros((n, W0), np.int32)
        bestv = np.full((n, W0), -1.0, np.float32)
        tot = np.zeros((n, W0), np.int32)
        for k in range(NL):
            v = g[k] / wsum * 255.0
            q[k] = np.floor(v).astype(np.int32)
            tot += q[k]
            better = v > bestv
            bestv = np.where(better, v, bestv)
            best = np.where(better, k, best)
        rem = 255 - tot
        for k in range(NL):
            q[k] += np.where(best == k, rem, 0)
        out[:, r0:r1] = q.astype(np.uint8)
    import os
    os.makedirs(out_dir, exist_ok=True)
    for k, name in enumerate(C.LAYERS):
        Image.fromarray(out[k], mode="L").save(os.path.join(out_dir, name + ".png"), compress_level=compress)
    log("  wrote %d layer PNGs" % NL)
    return out


def densities(D, P, h0, road_w, Hy, sites, out_dir, log=print):
    """Foliage densities on the 1524 x 1143 (12 m) grid, zero on roads, water, towns and cliffs."""
    import os
    os.makedirs(out_dir, exist_ok=True)
    lv1 = G.L1
    Wd, Hd = C.REGION_W, C.REGION_H
    # pixel centres -> full-res vertex 4p + 2 -> L1 coords (4p + 2) / 3
    px1 = (np.arange(Wd) * 4 + 2) / 3.0
    py1 = (np.arange(Hd) * 4 + 2) / 3.0
    PX = np.broadcast_to(px1[None, :], (Hd, Wd))
    PY = np.broadcast_to(py1[:, None], (Hd, Wd))
    s1 = lambda a: G.sample_bilinear(a.astype(np.float32), PX, PY)
    forest = s1(P.forest)
    tdens = s1(P.tree_dens)
    moist = s1(P.moist)
    farm = s1(P.farm)
    oasis = s1(P.oasis)
    slope = s1(P.slope1)
    reg = G.sample_nearest(P.region.astype(np.float32), PX, PY).astype(np.int64)
    h = s1(G.downsample(h0, 3)) if False else G.sample_bilinear(h0, PX * 3.0, PY * 3.0)
    snowl = s1(P.snowline)
    dtown = s1(P.dtown)
    # exclusion from full-res masks: any road / water inside the 4x4 block of the pixel
    blk = lambda a: a[:Hd * 4, :Wd * 4].reshape(Hd, 4, Wd, 4).max(axis=(1, 3))
    road_blk = blk(road_w) > 0
    wat = G.sample_bilinear(G.dilate_mask(Hy.water, 1).astype(np.float32), PX, PY) > 0.05
    hb_min = -blk(-h0)
    wat |= hb_min < 0.35
    town = np.zeros((Hd, Wd), bool)
    for s in sites:
        d = np.hypot(PX * lv1.cell - s.x, PY * lv1.cell - s.y)
        town |= d < s.r + 8.0
    excl = road_blk | wat | town | (slope > 1.0)
    nz = D.noise
    X, Y = PX * lv1.cell, PY * lv1.cell
    n1 = nz.fbm(X, Y, 90.0, 3, k0=400)
    n2 = nz.fbm(X, Y, 400.0, 3, k0=401)
    treeline = np.where(reg == 4, 480.0, 640.0)
    alpine = smoothstep(treeline - 60.0, treeline + 40.0, h)
    fo = smoothstep(0.3, 0.7, forest)
    lone = 0.06 * smoothstep(0.35, 0.6, n1) * np.isin(reg, (1, 2, 5, 11, 6)) * (1 - fo)
    trees = fo * tdens * (0.75 + 0.25 * n1) + lone + 0.85 * oasis * (reg >= 13) * (reg <= 14)
    trees *= 1.0 - alpine
    trees *= 1.0 - smoothstep(0.6, 0.9, slope)
    trees *= 1.0 - 0.9 * smoothstep(0.3, 0.6, farm)
    edge = fo * (1.0 - fo) * 4.0
    bushes = 0.7 * edge + 0.25 * fo * tdens + 0.35 * (reg == 5) * smoothstep(-0.1, 0.4, n2) \
        + 0.25 * (reg == 8) * smoothstep(0.1, 0.5, n2) + 0.5 * oasis + 0.2 * (reg == 6) \
        + 0.15 * np.isin(reg, (3, 12)) * (1 - alpine) * smoothstep(0.2, 0.5, n2)
    bushes *= 1.0 - smoothstep(0.7, 1.0, slope)
    grass_base = {1: 0.85, 2: 0.9, 3: 0.45, 4: 0.12, 5: 0.6, 6: 0.55, 7: 0.1, 8: 0.12, 9: 0.08, 10: 0.35,
                  11: 0.9, 12: 0.45, 13: 0.02, 14: 0.05, 15: 0.6}
    gb = np.zeros((Hd, Wd), np.float32)
    for k, v in grass_base.items():
        gb[reg == k] = v
    grass = gb * (0.7 + 0.3 * n1) * (1.0 - 0.6 * fo) * (1.0 - smoothstep(0.6, 1.0, slope)) + 0.9 * oasis
    grass *= 1.0 - 0.7 * alpine * (reg != 3)
    grass = np.maximum(grass, 0.35 * farm)          # field margins
    rocks = 0.9 * smoothstep(0.45, 0.95, slope) + 0.25 * np.isin(reg, (8, 9, 14)) + 0.1 * (reg == 13) \
        + 0.35 * alpine + 0.2 * (reg == 7)
    rocks *= 0.6 + 0.4 * n2
    fl_base = {1: 0.45, 2: 0.6, 11: 0.5, 3: 0.2, 12: 0.2, 6: 0.25, 8: 0.18, 15: 0.2, 10: 0.15}
    fb = np.zeros((Hd, Wd), np.float32)
    for k, v in fl_base.items():
        fb[reg == k] = v
    flowers = fb * smoothstep(0.1, 0.5, n2 + 0.3 * n1) * (1.0 - fo) * (1.0 - smoothstep(0.3, 0.6, farm)) \
        * (1.0 - smoothstep(0.35, 0.7, slope)) + 0.4 * oasis
    maps = dict(Trees=trees, Bushes=bushes, Grass=grass, Rocks=rocks, Flowers=flowers)
    res = {}
    for k in C.DENSITY_KINDS:
        a = np.clip(maps[k], 0.0, 1.0)
        a = np.where(excl, 0.0, a)
        a8 = np.rint(a * 255.0).astype(np.uint8)
        Image.fromarray(a8, mode="L").save(os.path.join(out_dir, k + ".png"))
        res[k] = a8
    log("  wrote density maps")
    return res, excl


def region_png(P, path):
    Wd, Hd = C.REGION_W, C.REGION_H
    px1 = (np.arange(Wd) * 4 + 2) / 3.0
    py1 = (np.arange(Hd) * 4 + 2) / 3.0
    PX = np.broadcast_to(px1[None, :], (Hd, Wd))
    PY = np.broadcast_to(py1[:, None], (Hd, Wd))
    r = G.sample_nearest(P.region_hard.astype(np.float32), PX, PY).astype(np.uint8)
    Image.fromarray(r, mode="L").save(path)
    return r
