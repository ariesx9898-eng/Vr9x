"""WorldMap.png (painted parchment map, no text) and LaPlace_World_Preview.png (labelled review map).

WorldMap covers exactly the world rectangle: pixel (px, py) of 4096 x 3072 <-> u = px / 4095, v = py / 3071.
"""
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import wg_config as C
import wg_geo as GEO
import wg_grid as G
from wg_grid import smoothstep

MW, MH = C.WORLDMAP_W, C.WORLDMAP_H

BIOME_RGB = {
    0: (0, 0, 0), 1: (174, 186, 122), 2: (168, 186, 118), 3: (178, 162, 132), 4: (226, 228, 222),
    5: (190, 180, 128), 6: (126, 160, 98), 7: (214, 210, 204), 8: (196, 142, 94), 9: (184, 124, 82),
    10: (112, 150, 96), 11: (160, 184, 116), 12: (170, 160, 138), 13: (222, 198, 146), 14: (204, 168, 118),
    15: (178, 186, 130),
}


def _resize_f(a, w, h, method=Image.BILINEAR):
    return np.asarray(Image.fromarray(np.asarray(a, np.float32), mode="F").resize((w, h), method), np.float32)


def _hillshade(h, cell, az=315.0, alt=38.0, z=1.0):
    gx, gy = G.gradient(h * z, cell)
    a, e = math.radians(az), math.radians(alt)
    lx, ly, lz = math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)
    n = np.sqrt(gx * gx + gy * gy + 1.0)
    return np.clip((-gx * lx - gy * ly + lz) / n, 0.0, 1.0)


MAP_BAND = 256


def _l1_sampler(lv1=None):
    """Returns f(field, r0, r1) sampling an L1 (9 m) field at the centres of map rows [r0, r1)."""
    lv1 = lv1 or G.L1
    mx = C.WORLD_W_M / MW
    my = C.WORLD_H_M / MH
    xs = ((np.arange(MW) + 0.5) * mx / lv1.cell)[None, :]

    def f(field, r0, r1, nearest=False):
        ys = ((np.arange(r0, r1) + 0.5) * my / lv1.cell)[:, None]
        PX = np.broadcast_to(xs, (r1 - r0, MW))
        PY = np.broadcast_to(ys, (r1 - r0, MW))
        if nearest:
            return G.sample_nearest(field, PX, PY)
        return G.sample_bilinear(np.asarray(field, np.float32), PX, PY)
    return f


def _lake_sd(Hy, lv1=None):
    """Signed distance (m) to lake shores on the 9 m grid, computed only in each lake's window."""
    lv1 = lv1 or G.L1
    sd = np.full(lv1.shape, 1e4, np.float32)
    for lk in Hy.lakes:
        sl = lk["mask_sl"]
        m = lk["mask"]
        pad = 6
        big = np.pad(m, pad)
        s = G.signed_distance(big, lv1.cell)[pad:-pad, pad:-pad]
        sd[sl] = np.where(np.abs(s) < np.abs(sd[sl]), -s, sd[sl])
    return sd   # negative inside lakes


def world_map(D, h0, Hy, sites, roads, P, path, log=print):
    rng = np.random.default_rng(C.SEED + 77)
    nz = D.noise
    mpx = C.WORLD_W_M / MW
    samp = _l1_sampler()
    log("  world map: fields")
    hm = _resize_f(h0, MW, MH, Image.BOX)
    lake_sd1 = _lake_sd(Hy)
    reg1 = P.region.astype(np.float32)
    snow1 = P.snowline
    dune1 = getattr(D, "dune1", np.zeros(G.L1.shape, np.float32))
    heaven1 = D.heaven.astype(np.float32)
    stains = [(rng.uniform(0.05, 0.95) * MW, rng.uniform(0.05, 0.95) * MH, rng.uniform(90, 420), k)
              for k in range(9)]
    fox = [(rng.uniform(0, MW), rng.uniform(0, MH), rng.uniform(2, 8)) for _ in range(160)]
    lut = np.zeros((16, 3), np.float32)
    for k, c in BIOME_RGB.items():
        lut[k] = c
    X1, Y1 = G.L1.xy_full()
    wx, wy = nz.warp(X1, Y1, 700.0, 90.0, octaves=3, k0=545)
    regw = G.sample_nearest(reg1, wx / G.L1.cell, wy / G.L1.cell).astype(np.int64)
    regw = np.where(D.land, regw, P.region)
    biome1 = [G.blur(lut[regw][..., c], 5.0) for c in range(3)]
    del X1, Y1, wx, wy, regw
    out = np.zeros((MH, MW, 3), np.uint8)
    xs_px = np.arange(MW, dtype=np.float64)[None, :]
    log("  world map: painting")
    for r0 in range(0, MH, MAP_BAND):
        r1 = min(MH, r0 + MAP_BAND)
        n = r1 - r0
        Xp = np.broadcast_to(xs_px, (n, MW))
        Yp = np.broadcast_to(np.arange(r0, r1, dtype=np.float64)[:, None], (n, MW))
        # ---- paper
        n_big = nz.fbm(Xp * 4.0, Yp * 4.0, 2600.0, 4, k0=500)
        n_mid = nz.fbm(Xp * 4.0, Yp * 4.0, 420.0, 4, k0=501)
        fib = 0.5 * nz.fbm(Xp * 4.0, Yp * 16.0, 60.0, 2, k0=502) + 0.5 * nz.fbm(Xp * 16.0, Yp * 4.0, 60.0, 2, k0=503)
        tone = 1.0 + 0.05 * n_big + 0.025 * n_mid + 0.014 * fib
        paper_rgb = np.array([236, 222, 188], np.float32)[None, None, :] * tone[..., None]
        for (cx, cy, r, k) in stains:
            if cy + 1.4 * r < r0 or cy - 1.4 * r > r1:
                continue
            d = np.hypot(Xp - cx, Yp - cy) / r
            wob = 1.0 + 0.16 * nz.fbm(Xp * 3.0, Yp * 3.0, r * 6.0, 3, k0=504 + k)
            dd = d * wob
            rim = np.exp(-((dd - 1.0) / 0.05) ** 2) * 0.09 + (dd < 1.0) * 0.03
            paper_rgb *= (1.0 - rim[..., None] * np.array([0.3, 0.5, 0.85], np.float32)[None, None, :])
        for (cx, cy, r) in fox:
            if cy + 3 * r < r0 or cy - 3 * r > r1:
                continue
            d = np.hypot(Xp - cx, Yp - cy) / r
            paper_rgb *= (1.0 - 0.1 * np.exp(-d * d)[..., None] * np.array([0.3, 0.6, 1.0])[None, None, :])
        # fold creases (map folded in quarters)
        for fx in (MW / 2.0,):
            cr = np.exp(-((Xp - fx) / 1.2) ** 2) * 0.06 - np.exp(-((Xp - fx - 2.5) / 1.5) ** 2) * 0.03
            paper_rgb *= (1.0 - cr[..., None])
        cr = np.exp(-((Yp - MH / 2.0) / 1.2) ** 2) * 0.05 - np.exp(-((Yp - MH / 2.0 - 2.5) / 1.5) ** 2) * 0.025
        paper_rgb *= (1.0 - cr[..., None])
        # ---- fields for this band
        h = hm[r0:r1]
        ha = hm[max(0, r0 - 1):min(MH, r1 + 1)]
        shore = samp(D.shore_s, r0, r1)                 # m, >0 land
        lsd = samp(lake_sd1, r0, r1)                    # m, <0 inside lakes
        reg = samp(reg1, r0, r1, nearest=True).astype(np.int64)
        fo = samp(P.forest, r0, r1)
        fa = samp(P.farm, r0, r1)
        sn = samp(snow1, r0, r1)
        dn = samp(dune1, r0, r1)
        hv = samp(heaven1, r0, r1)
        land = shore > 0.0
        lake = lsd < 0.0
        # ---- sea
        depth = np.clip(-h, 0.0, 220.0) / 220.0
        dsea = np.maximum(-shore, 0.0)
        shallow = np.exp(-dsea / 90.0)
        sea_t = (np.array([164, 198, 196], np.float32) * (1 - depth[..., None])
                 + np.array([86, 124, 146], np.float32) * depth[..., None])
        sea_t = sea_t * (1 - 0.3 * shallow[..., None]) + np.array([206, 222, 206], np.float32) * 0.3 * shallow[..., None]
        sea_rgb = paper_rgb * 0.3 + sea_t * 0.7
        # engraved hatching in open water, coastal ripple lines, a pale halo at the shore
        hat = np.maximum(np.cos(2 * np.pi * (Yp + 1.5 * nz.fbm(Xp, Yp, 300.0, 2, k0=510)) / 6.0), 0.0) ** 10
        hat *= smoothstep(260.0, 900.0, dsea) * (0.55 + 0.45 * np.clip(0.5 + nz.fbm(Xp * 4, Yp * 4, 1800.0, 2, k0=511), 0, 1))
        lines = np.zeros((n, MW), np.float32)
        for k, dk in enumerate((32.0, 78.0, 132.0, 196.0, 270.0)):
            wob = dk + 10.0 * nz.fbm(Xp * 4.0, Yp * 4.0, 1300.0, 2, k0=530 + k)
            lines += np.exp(-((dsea - wob) / (mpx * 0.55)) ** 2) * (0.62 - 0.1 * k)
        ink_sea = np.array([66, 86, 92], np.float32)
        sea_rgb = sea_rgb * (1 - 0.1 * hat[..., None]) + ink_sea * 0.1 * hat[..., None]
        sea_rgb = sea_rgb * (1 - 0.55 * lines[..., None]) + ink_sea * 0.55 * lines[..., None]
        halo = np.exp(-dsea / 9.0) * (~land)
        sea_rgb = sea_rgb * (1 - 0.35 * halo[..., None]) + np.array([232, 234, 214], np.float32) * 0.35 * halo[..., None]
        # ---- land wash
        col = np.stack([samp(biome1[c], r0, r1) for c in range(3)], axis=-1)
        col = col * (1 - 0.62 * fo[..., None]) + np.array([104, 138, 86], np.float32) * 0.62 * fo[..., None]
        col = col * (1 - 0.42 * fa[..., None]) + np.array([206, 200, 134], np.float32) * 0.42 * fa[..., None]
        snow = smoothstep(sn - 40.0, sn + 60.0, h)
        snow = np.maximum(snow, hv * smoothstep(700.0, 800.0, h))
        col = col * (1 - 0.82 * snow[..., None]) + np.array([244, 244, 238], np.float32) * 0.82 * snow[..., None]
        rockish = smoothstep(260.0, 560.0, h) * (1 - snow) * (reg != 7)
        col = col * (1 - 0.42 * rockish[..., None]) + np.array([168, 148, 118], np.float32) * 0.42 * rockish[..., None]
        col = col * (1 - 0.25 * dn[..., None]) + np.array([232, 206, 150], np.float32) * 0.25 * dn[..., None]
        gran = 1.0 + 0.06 * nz.fbm(Xp * 4.0, Yp * 4.0, 140.0, 3, k0=540) + 0.035 * nz.fbm(Xp * 4.0, Yp * 4.0, 900.0, 3, k0=541)
        col *= gran[..., None]
        gx, gy = G.gradient(ha * 2.2, mpx)
        i0 = r0 - max(0, r0 - 1)
        gx, gy = gx[i0:i0 + n], gy[i0:i0 + n]
        a, e = math.radians(315.0), math.radians(38.0)
        lx, ly, lz = math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)
        hs = np.clip((-gx * lx - gy * ly + lz) / np.sqrt(gx * gx + gy * gy + 1.0), 0.0, 1.0)
        sh = hs - 0.72
        land_rgb = col + np.clip(sh, 0, 1)[..., None] * np.array([62, 50, 30], np.float32) \
            - np.clip(-sh, 0, 1)[..., None] * np.array([118, 116, 90], np.float32) * 1.1
        wet_edge = np.exp(-np.maximum(shore, 0.0) / 22.0)
        land_rgb *= (1 - 0.2 * wet_edge[..., None])
        land_rgb = paper_rgb * 0.2 + land_rgb * 0.8
        # ---- lakes
        lk_t = np.array([134, 166, 172], np.float32)
        lake_rgb = paper_rgb * 0.3 + lk_t * 0.7
        lr = np.exp(-((-lsd - 14.0) / (mpx * 0.6)) ** 2) * 0.35
        lake_rgb = lake_rgb * (1 - lr[..., None]) + ink_sea * lr[..., None]
        rgb = np.where(land[..., None], land_rgb, sea_rgb)
        rgb = np.where((land & lake)[..., None], lake_rgb, rgb)
        # ---- ink: coast + lake shores (from continuous fields -> smooth lines)
        cink = np.array([58, 44, 30], np.float32)
        coast = np.exp(-(shore / (mpx * 0.75)) ** 2)
        lk_edge = np.exp(-(lsd / (mpx * 0.6)) ** 2) * land
        rgb = rgb * (1 - 0.85 * coast[..., None]) + cink * 0.85 * coast[..., None]
        rgb = rgb * (1 - 0.6 * lk_edge[..., None]) + cink * 0.6 * lk_edge[..., None]
        # ---- vignette / burnt edge
        u = Xp / MW
        v = Yp / MH
        edge = np.minimum(np.minimum(u, 1 - u) * MW / MH, np.minimum(v, 1 - v))
        vig = smoothstep(0.0, 0.2, edge + 0.02 * n_mid)
        burn = (1.0 - vig)
        rgb = rgb * (1.0 - 0.3 * burn)[..., None] * np.array([1.0, 0.96, 0.9], np.float32)[None, None, :] \
            + np.array([96, 70, 40], np.float32) * (0.12 * burn)[..., None]
        out[r0:r1] = np.clip(rgb, 0, 255).astype(np.uint8)
    img = Image.fromarray(out, "RGB")
    del out
    d = ImageDraw.Draw(img, "RGBA")
    s = MW / C.WORLD_W_M
    log("  world map: rivers, roads, glyphs")
    # rivers
    for rv in Hy.rivers:
        p = rv["pts"] * s
        wv = np.maximum(1.2, rv["width"] * s * 1.3 + 0.6)
        for i in range(len(p) - 1):
            d.line([tuple(p[i]), tuple(p[i + 1])], fill=(60, 84, 104, 230), width=int(round(wv[i] + 1.6)))
        for i in range(len(p) - 1):
            d.line([tuple(p[i]), tuple(p[i + 1])], fill=(118, 156, 180, 255), width=max(1, int(round(wv[i]))))
    for db in Hy.drybeds:
        p = db["pts"] * s
        for i in range(0, len(p) - 1, 2):
            d.line([tuple(p[i]), tuple(p[min(i + 1, len(p) - 1)])], fill=(150, 116, 74, 140), width=2)
    # roads (dotted)
    for rd in roads:
        p = rd["pts"] * s
        acc = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(p[:, 0]), np.diff(p[:, 1])))])
        t = 0.0
        wd = 3 if rd["klass"] == "Highway" else 2
        while t < acc[-1]:
            t1 = min(acc[-1], t + 6.0)
            a_ = (np.interp(t, acc, p[:, 0]), np.interp(t, acc, p[:, 1]))
            b_ = (np.interp(t1, acc, p[:, 0]), np.interp(t1, acc, p[:, 1]))
            d.line([a_, b_], fill=(122, 58, 40, 225), width=wd)
            t += 11.0
    _dune_strokes(d, D, P, dune1, rng, s)
    _forest_glyphs(d, P, hm, lake_sd1, rng, s)
    _heaven_hachures(d, hm, heaven1, rng)
    _mountain_glyphs(img, hm, P, rng)
    d = ImageDraw.Draw(img, "RGBA")
    _wave_marks(d, D, rng, s)
    _compass(d, 0.63 * MW, 0.60 * MH, 150.0)
    for off, wdt, al in ((12, 4, 210), (22, 1, 170)):
        d.rectangle((off, off, MW - 1 - off, MH - 1 - off), outline=(70, 52, 34, al), width=wdt)
    for (cx, cy) in ((12, 12), (MW - 13, 12), (12, MH - 13), (MW - 13, MH - 13)):
        d.ellipse((cx - 16, cy - 16, cx + 16, cy + 16), outline=(70, 52, 34, 200), width=2)
        d.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), fill=(70, 52, 34, 200))
    img.save(path, compress_level=6)
    log("  wrote %s" % path)


def _poisson(test, spacing, rng, W=MW, H=MH, max_pts=80000):
    """Jittered-grid sampling; test(xs, ys) -> accept mask."""
    pts = []
    gx = np.arange(spacing / 2, W, spacing)
    for y in np.arange(spacing / 2, H, spacing):
        jy = y + rng.uniform(-0.45, 0.45, len(gx)) * spacing
        jx = gx + rng.uniform(-0.45, 0.45, len(gx)) * spacing
        ok = test(jx, jy)
        pts += list(zip(jx[ok].tolist(), jy[ok].tolist()))
    if len(pts) > max_pts:
        idx = rng.choice(len(pts), max_pts, replace=False)
        pts = [pts[i] for i in sorted(idx)]
    return pts


def _at(field, xs, ys, s):
    """Sample an L1 field at map pixel positions."""
    return G.sample_bilinear(np.asarray(field, np.float32), xs / s / G.L1.cell, ys / s / G.L1.cell)


def _forest_glyphs(d, P, hm, lake_sd1, rng, s):
    reg = P.region.astype(np.float32)

    def test(xs, ys):
        f = _at(P.forest, xs, ys, s)
        ok = (f > 0.45) & (rng.random(len(xs)) < np.clip((f - 0.3) * 1.7, 0, 1))
        ok &= _at(lake_sd1, xs, ys, s) > 8.0
        return ok
    pts = _poisson(test, 8.5, rng)
    pts.sort(key=lambda p: p[1])
    for x, y in pts:
        r = int(G.sample_nearest(reg, np.array([x / s / G.L1.cell]), np.array([y / s / G.L1.cell]))[0])
        z = hm[min(MH - 1, int(y)), min(MW - 1, int(x))]
        if z <= 0.5:
            continue
        conifer = r in (3, 4, 7, 12) or z > 380.0
        sz = rng.uniform(3.0, 4.6) * (1.25 if r == 10 else 1.0)
        if conifer:
            d.polygon([(x, y - 2.3 * sz), (x - 1.1 * sz, y + 0.45 * sz), (x + 1.1 * sz, y + 0.45 * sz)],
                      fill=(66, 98, 70, 240), outline=(40, 52, 36, 255))
            d.line([(x, y + 0.45 * sz), (x, y + 1.0 * sz)], fill=(58, 42, 28, 255), width=1)
        elif r in (8, 9):
            d.line([(x, y + sz), (x, y - 0.4 * sz)], fill=(58, 40, 40, 255), width=1)
            d.ellipse((x - 0.9 * sz, y - 1.3 * sz, x + 0.9 * sz, y - 0.1 * sz), fill=(110, 76, 110, 230),
                      outline=(56, 36, 50, 255))
        else:
            dark = (62, 96, 58, 240) if r != 6 else (50, 84, 52, 245)
            d.ellipse((x - sz, y - sz * 1.15, x + sz, y + sz * 0.75), fill=dark, outline=(40, 54, 36, 255))
            d.ellipse((x - 0.6 * sz, y - 1.0 * sz, x + 0.15 * sz, y - 0.25 * sz), fill=(128, 156, 100, 210))


def _dune_strokes(d, D, P, dune1, rng, s):
    wind = (-math.cos(math.radians(18.0)), math.sin(math.radians(18.0)))
    perp = (-wind[1], wind[0])

    def test(xs, ys):
        return _at(dune1, xs, ys, s) > 0.18
    for x, y in _poisson(test, 16.0, rng):
        L = rng.uniform(9, 17)
        bend = rng.uniform(2.5, 4.5)
        a = (x - perp[0] * L / 2, y - perp[1] * L / 2)
        b = (x + perp[0] * L / 2, y + perp[1] * L / 2)
        m = (x - wind[0] * bend, y - wind[1] * bend)
        d.line([a, m, b], fill=(150, 104, 58, 185), width=2)
        d.line([(m[0] + wind[0] * 2.0, m[1] + wind[1] * 2.0), (b[0] + wind[0] * 1.5, b[1] + wind[1] * 1.5)],
               fill=(250, 232, 190, 120), width=1)


def _heaven_hachures(d, hm, heaven1, rng):
    """Cliff hachures around the Heaven plateau: short strokes running down the cliff."""
    ys_, xs_ = np.nonzero(G.dilate_mask(_resize_f(heaven1, MW, MH) > 0.5, 3))
    if len(ys_) == 0:
        return
    y0, y1, x0, x1 = ys_.min(), ys_.max() + 1, xs_.min(), xs_.max() + 1
    sub = G.blur(hm[y0:y1, x0:x1], 1.5)
    gx, gy = G.gradient(sub, 1.0)
    gm = np.hypot(gx, gy)
    ok_map = (sub > 80.0) & (sub < 760.0) & (gm > 8.0)

    def test(xs, ys):
        xi = np.clip(xs.astype(int) - x0, 0, x1 - x0 - 1)
        yi = np.clip(ys.astype(int) - y0, 0, y1 - y0 - 1)
        inside = (xs >= x0) & (xs < x1) & (ys >= y0) & (ys < y1)
        return inside & ok_map[yi, xi]
    for x, y in _poisson(test, 5.0, rng, max_pts=200000):
        xi, yi = int(x) - x0, int(y) - y0
        g = gm[yi, xi]
        ux, uy = -gx[yi, xi] / g, -gy[yi, xi] / g
        L = rng.uniform(5.0, 9.0)
        d.line([(x, y), (x + ux * L, y + uy * L)], fill=(62, 50, 44, 190), width=1)


def _mountain_glyphs(img, hm, P, rng):
    """Peak glyphs at real summits (local maxima) sized by height, drawn back-to-front at 2x and downsampled."""
    SS = 2
    H, W = hm.shape
    hb = G.blur(hm, 2.0)
    mx = G.max_filter(hb, 14)
    reg_full = None
    peaks = (hb >= mx - 1e-3) & (hb > 260.0)
    ys, xs = np.nonzero(peaks)
    cand = sorted(zip(hb[ys, xs].tolist(), ys.tolist(), xs.tolist()), reverse=True)
    chosen = []
    grid = {}

    def free(x, y, rad):
        gxk, gyk = int(x // 40), int(y // 40)
        for ix in range(gxk - 2, gxk + 3):
            for iy in range(gyk - 2, gyk + 3):
                for (cx, cy, cr) in grid.get((ix, iy), ()):
                    if (x - cx) ** 2 + (y - cy) ** 2 < (0.78 * (rad + cr)) ** 2:
                        return False
        return True

    def add(z, y, x, rad):
        chosen.append((z, y, x, rad))
        grid.setdefault((int(x // 40), int(y // 40)), []).append((x, y, rad))
    for z, y, x in cand:
        rad = 8.0 + z / 38.0
        if free(x, y, rad):
            add(z, y, x, rad)
        if len(chosen) > 1600:
            break
    ridge = (hb > 320.0) & (hb >= G.max_filter(hb, 5) - 0.5)
    ys, xs = np.nonzero(ridge)
    for k in rng.permutation(len(ys))[:8000]:
        y, x = int(ys[k]), int(xs[k])
        z = float(hb[y, x])
        rad = 6.0 + z / 55.0
        if free(x, y, rad):
            add(z, y, x, rad)
    chosen.sort(key=lambda c: c[1] + c[3] * 0.4)
    reg1 = P.region.astype(np.float32)
    ov = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    dd = ImageDraw.Draw(ov)
    snow1 = P.snowline
    sx = G.L1.cell * MW / C.WORLD_W_M
    for z, y, x, rad in chosen:
        r = int(G.sample_nearest(reg1, np.array([x / sx]), np.array([y / sx]))[0])
        if r == 7:
            continue                      # the Heaven plateau gets cliff hachures instead of peaks
        snl = float(G.sample_bilinear(snow1, np.array([x / sx]), np.array([y / sx]))[0])
        snowy = z > snl - 30.0
        demon = r in (8, 9)
        w = rad * 1.8 * SS
        hgt = rad * (1.25 + min(1.0, z / 900.0) * 0.85) * SS
        cx, by = x * SS, y * SS + rad * 0.5 * SS
        jx = rng.uniform(-0.12, 0.12) * w
        # silhouette: left base, shoulder, apex, notch, optional secondary peak, right base
        sil = [(cx - w / 2, by),
               (cx - w * rng.uniform(0.28, 0.36), by - hgt * rng.uniform(0.38, 0.55)),
               (cx - w * rng.uniform(0.12, 0.2), by - hgt * rng.uniform(0.7, 0.8)),
               (cx + jx, by - hgt)]
        if rng.random() < 0.55:
            sil += [(cx + jx + w * 0.14, by - hgt * rng.uniform(0.66, 0.74)),
                    (cx + w * rng.uniform(0.26, 0.32), by - hgt * rng.uniform(0.76, 0.86)),
                    (cx + w * 0.4, by - hgt * 0.42)]
        else:
            sil += [(cx + jx + w * 0.2, by - hgt * rng.uniform(0.55, 0.65)), (cx + w * 0.38, by - hgt * 0.3)]
        sil.append((cx + w / 2, by))
        apex = sil[3]
        ridge = [apex, (apex[0] + w * rng.uniform(0.04, 0.1), by - hgt * 0.55), (cx + w * rng.uniform(0.02, 0.12), by)]
        if demon:
            lit, shd, ink = (192, 134, 96, 255), (120, 76, 56, 255), (50, 30, 22, 255)
        else:
            lit, shd, ink = (230, 214, 182, 255), (146, 124, 98, 255), (58, 44, 30, 255)
        dd.ellipse((cx - w * 0.56, by - hgt * 0.07, cx + w * 0.64, by + hgt * 0.09), fill=(40, 30, 20, 40))
        dd.polygon(sil[:4] + [ridge[1], ridge[2]], fill=lit)
        dd.polygon([apex] + sil[4:] + [ridge[2], ridge[1]], fill=shd)
        # hachures on the shadow face
        for k in range(1, 8):
            t = k / 8.0
            i_s = min(len(sil) - 1, 4 + int(t * (len(sil) - 5)))
            p0 = sil[i_s] if k % 2 else ((sil[i_s][0] + sil[max(4, i_s - 1)][0]) / 2, (sil[i_s][1] + sil[max(4, i_s - 1)][1]) / 2)
            q = (ridge[1][0] + (ridge[2][0] - ridge[1][0]) * t, ridge[1][1] + (ridge[2][1] - ridge[1][1]) * t)
            p1 = (p0[0] + (q[0] - p0[0]) * 0.55, p0[1] + (q[1] - p0[1]) * 0.55)
            dd.line([p0, p1], fill=ink, width=max(1, SS))
        if snowy:
            cap_y = by - hgt * 0.72
            cap = [p for p in sil if p[1] <= cap_y]
            if len(cap) >= 1:
                lx_ = min(p[0] for p in cap) - w * 0.06
                rx_ = max(p[0] for p in cap) + w * 0.06
                rag = [(rx_, cap_y + hgt * 0.02)]
                for j in range(1, 5):
                    fx = rx_ + (lx_ - rx_) * j / 5.0
                    rag.append((fx, cap_y + hgt * (0.08 if j % 2 else -0.02)))
                rag.append((lx_, cap_y))
                dd.polygon(cap + rag, fill=(248, 248, 244, 255))
        dd.line(sil, fill=ink, width=max(2, int(1.4 * SS)), joint="curve")
        dd.line(ridge, fill=ink, width=max(1, SS))
    ov = ov.resize((W, H), Image.LANCZOS)
    img.paste(ov, (0, 0), ov)


def _wave_marks(d, D, rng, s):
    def test(xs, ys):
        return _at(D.shore_s, xs, ys, s) < -700.0
    for x, y in _poisson(test, 140.0, rng):
        if rng.random() < 0.4:
            continue
        w = rng.uniform(10, 16)
        for k in range(2):
            ox = x + k * w * 0.85
            d.arc((ox - w / 2, y - w / 4, ox + w / 2, y + w / 4), 200, 340, fill=(66, 86, 92, 150), width=2)


def _compass(d, cx, cy, R):
    ink = (66, 50, 34, 235)
    fill_a = (222, 204, 166, 235)
    fill_b = (124, 94, 64, 235)
    d.ellipse((cx - R * 1.08, cy - R * 1.08, cx + R * 1.08, cy + R * 1.08), outline=ink, width=3)
    d.ellipse((cx - R * 0.98, cy - R * 0.98, cx + R * 0.98, cy + R * 0.98), outline=ink, width=1)
    d.ellipse((cx - R * 0.62, cy - R * 0.62, cx + R * 0.62, cy + R * 0.62), outline=(66, 50, 34, 140), width=1)
    for k in range(64):
        a = k * math.pi / 32
        r0 = R * (0.98 if k % 2 else 0.93)
        d.line([(cx + r0 * math.cos(a), cy + r0 * math.sin(a)),
                (cx + R * 1.08 * math.cos(a), cy + R * 1.08 * math.sin(a))], fill=ink, width=1)
    for k in (8, 4, 2):
        for j in range(k):
            a = -math.pi / 2 + j * 2 * math.pi / k + (math.pi / k if k == 8 else 0.0)
            if k == 4:
                a = -math.pi / 2 + j * math.pi / 2 + math.pi / 4
            if k == 2:
                continue
            L = R * (0.52 if k == 8 else 0.75)
            wdt = R * 0.08
            tip = (cx + L * math.cos(a), cy + L * math.sin(a))
            l_ = (cx + wdt * math.cos(a - math.pi / 2), cy + wdt * math.sin(a - math.pi / 2))
            r_ = (cx + wdt * math.cos(a + math.pi / 2), cy + wdt * math.sin(a + math.pi / 2))
            d.polygon([(cx, cy), l_, tip], fill=fill_a, outline=ink)
            d.polygon([(cx, cy), tip, r_], fill=fill_b, outline=ink)
    for j in range(4):
        a = -math.pi / 2 + j * math.pi / 2
        L = R * 1.0
        wdt = R * 0.13
        tip = (cx + L * math.cos(a), cy + L * math.sin(a))
        l_ = (cx + wdt * math.cos(a - math.pi / 2), cy + wdt * math.sin(a - math.pi / 2))
        r_ = (cx + wdt * math.cos(a + math.pi / 2), cy + wdt * math.sin(a + math.pi / 2))
        d.polygon([(cx, cy), l_, tip], fill=fill_a, outline=ink)
        d.polygon([(cx, cy), tip, r_], fill=fill_b, outline=ink)
    # north marker: a small fleur-like diamond above N
    d.polygon([(cx, cy - R * 1.28), (cx - 9, cy - R * 1.14), (cx, cy - R * 1.08), (cx + 9, cy - R * 1.14)],
              fill=fill_b, outline=ink)
    d.ellipse((cx - 8, cy - 8, cx + 8, cy + 8), fill=(66, 50, 34, 255))
    d.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill=(222, 204, 166, 255))


# ---------------------------------------------------------------------------------------------- preview
def _font(size, bold=False):
    cands = ["/System/Library/Fonts/Supplemental/Georgia Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Georgia.ttf",
             "/System/Library/Fonts/Helvetica.ttc", "/Library/Fonts/Arial.ttf"]
    for c in cands:
        try:
            return ImageFont.truetype(c, size)
        except Exception:
            pass
    try:
        return ImageFont.load_default(size=size)
    except Exception:
        return ImageFont.load_default()


def preview(D, h0, Hy, sites, roads, P, path, log=print):
    PW, PH = 3048, 2286
    s = (PW - 1) / C.WORLD_W_M
    hm = _resize_f(h0, PW, PH, Image.BOX)
    cellm = C.WORLD_W_M / (PW - 1)
    reg = np.asarray(Image.fromarray(P.region.astype(np.uint8), mode="L").resize((PW, PH), Image.NEAREST))
    fo = _resize_f(P.forest, PW, PH)
    sn = _resize_f(P.snowline, PW, PH)
    lut = np.zeros((16, 3), np.float32)
    for k, c in BIOME_RGB.items():
        lut[k] = c
    col = lut[reg]
    col = col * (1 - 0.55 * fo[..., None]) + np.array([96, 132, 80], np.float32) * 0.55 * fo[..., None]
    snow = smoothstep(sn - 40.0, sn + 60.0, hm)
    col = col * (1 - 0.85 * snow[..., None]) + np.array([245, 245, 245], np.float32) * 0.85 * snow[..., None]
    rockish = smoothstep(260.0, 560.0, hm) * (1 - snow)
    col = col * (1 - 0.4 * rockish[..., None]) + np.array([150, 132, 108], np.float32) * 0.4 * rockish[..., None]
    hs = _hillshade(hm, cellm, z=1.6)
    img = col * (0.45 + 0.75 * hs[..., None])
    depth = np.clip(-hm, 0, 200) / 200.0
    sea = np.array([96, 150, 178], np.float32) * (1 - depth[..., None]) + np.array([34, 70, 112], np.float32) * depth[..., None]
    lake = _resize_f(Hy.lake_mask.astype(np.float32), PW, PH) > 0.5
    img = np.where((hm <= 0)[..., None], sea, img)
    img = np.where(lake[..., None], np.array([70, 130, 175], np.float32), img)
    out = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB")
    d = ImageDraw.Draw(out, "RGBA")
    for rv in Hy.rivers:
        p = rv["pts"] * s
        d.line([tuple(q) for q in p], fill=(40, 100, 200, 255), width=3)
    for rd in roads:
        p = rd["pts"] * s
        d.line([tuple(q) for q in p], fill=(150, 40, 30, 255), width=3 if rd["klass"] == "Highway" else 2)
    f_site = _font(26, True)
    f_small = _font(20)
    f_reg = _font(34, True)
    # region labels at region centroids
    region = D.region
    f_reg_s = _font(24, True)
    offsets = {9: (0, 70), 12: (60, 40), 3: (0, -30), 2: (-40, 30)}
    for rid, (key, name, cont, biome) in C.REGIONS.items():
        if rid in (0, 15):
            continue
        m = region == rid
        if m.sum() < 50:
            continue
        ys, xs = np.nonzero(m[::4, ::4])
        cy, cx = np.median(ys) * 4, np.median(xs) * 4
        x, y = cx * G.L1.cell * s, cy * G.L1.cell * s
        ox, oy = offsets.get(rid, (0, 0))
        x, y = x + ox, y + oy
        f = f_reg if m.sum() > 40000 else f_reg_s
        tw = d.textlength(name.upper(), font=f)
        d.text((x - tw / 2 + 2, y + 2), name.upper(), font=f, fill=(0, 0, 0, 120))
        d.text((x - tw / 2, y), name.upper(), font=f, fill=(255, 250, 235, 215))
    for st in sites:
        x, y = st.x * s, st.y * s
        r = max(5.0, st.r * s)
        d.ellipse((x - r, y - r, x + r, y + r), outline=(255, 230, 80, 255), width=3)
        d.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(255, 60, 40, 255))
        label = st.name if len(st.name) < 22 else st.id
        d.text((x + r + 5, y - 14), label, font=f_site, fill=(0, 0, 0, 255), stroke_width=3, stroke_fill=(255, 255, 255, 220))
    for rv in Hy.rivers[:]:
        p = rv["pts"][len(rv["pts"]) // 2] * s
        d.text((p[0] + 6, p[1]), rv["name"], font=f_small, fill=(20, 60, 140, 255), stroke_width=2,
               stroke_fill=(255, 255, 255, 180))
    for key, P_ in GEO.PASSES.items():
        x, y = P_["uv"][0] * (PW - 1), P_["uv"][1] * (PH - 1)
        d.polygon([(x, y - 12), (x - 10, y + 8), (x + 10, y + 8)], outline=(20, 20, 20, 255), fill=(255, 255, 255, 200))
        d.text((x + 12, y - 6), key + " pass", font=f_small, fill=(0, 0, 0, 255), stroke_width=2,
               stroke_fill=(255, 255, 255, 200))
    out.save(path)
    log("  wrote %s" % path)
