"""Site placement, footprint shaping, districts and landmarks.

Each Spec site starts at its (u, v) target and is moved to suitable land in its region:
  inland - the whole footprint (+margin) on dry land, away from rivers, lakes and the coast, least rough terrain;
  port   - the footprint straddles the shoreline (centre 25-70 % of the radius inland), sheltered, least rough;
  fixed  - the terrain was designed around the site (Rikarisu crater floor, Millishion beside its lake);
  gate   - labyrinth gates: the foot of the tallest rock face nearby, facing away from it.
The footprint becomes a plane (ground Z at the centre, tilt <= 2 %) inside 0.62 R, blending back to the natural
terrain by 1.45 R; landmark rises (Silver Palace, Greyrat hill, ...) are added as flat-topped mounds.
"""
import math
import zlib

import numpy as np

import wg_config as C
import wg_geo as GEO
import wg_grid as G
from wg_grid import smoothstep


class Site:
    pass


DISTRICTS = {
    "AsuraCapital": [("Palace Quarter & Noble District", 0.0, 0.34), ("Markets & Government", 0.34, 0.64),
                     ("Artisan & Residential Wards", 0.64, 0.92), ("City Walls & Gates", 0.92, 1.0)],
    "AsuraTown": [("Market Square & Keep", 0.0, 0.30), ("Town Streets", 0.30, 0.88), ("Town Walls", 0.88, 1.0)],
    "RuralVillage": [("Village Square", 0.0, 0.20), ("Timber Houses", 0.20, 0.70), ("Farmsteads & Barns", 0.70, 1.0)],
    "NorthernCity": [("Frost Plaza", 0.0, 0.16), ("Guild & Magic Shops", 0.16, 0.50),
                     ("Workshops & Residences", 0.50, 0.94), ("Stone Walls", 0.94, 1.0)],
    "DemonTown": [("Crater Market", 0.0, 0.30), ("Dense Streets", 0.30, 0.82), ("Adventurer Quarter", 0.55, 0.82),
                  ("Rock Dwellings", 0.82, 1.0)],
    "DemonPort": [("Harbour Market", 0.0, 0.35), ("Port Town", 0.35, 1.0)],
    "MillisPort": [("Harbour Market", 0.0, 0.35), ("Port Town", 0.35, 0.95), ("Sea Wall", 0.95, 1.0)],
    "MillisCapital": [("Cathedral Plaza", 0.0, 0.18), ("Temple District", 0.18, 0.42), ("Markets & Plazas", 0.42, 0.68),
                      ("Ordered Residential Streets", 0.68, 0.93), ("White Walls & Towers", 0.93, 1.0)],
    "DesertCity": [("Grand Bazaar", 0.0, 0.28), ("Adventurer Guild Quarter", 0.28, 0.55),
                   ("Inns & Caravan Yards", 0.55, 0.93), ("Sandstone Walls", 0.93, 1.0)],
    "LabyrinthGate": [("Gate Apron", 0.0, 1.0)],
    "NorthernVillage": [("Training Grounds", 0.0, 0.35), ("Village", 0.35, 1.0)],
}


def _uv_to_m(u, v):
    return u * C.WORLD_W_M, v * C.WORLD_H_M


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


def place_all(D, h, Hy, log=print):
    lv = G.L1
    cell = lv.cell
    H, W = h.shape
    region = D.region
    land = D.land
    water = Hy.water
    wet = water | ~land
    # distance to water (rivers / lakes / sea) in metres, 9 m grid
    dw, _, _ = G.jfa_nearest(wet)
    dw = dw * cell
    slope = G.slope(h, cell)
    sites = []
    taken = []
    for S in GEO.SITES:
        s = Site()
        s.spec = S
        s.id, s.name, s.region, s.r, s.style, s.mode = S["id"], S["name"], S["region"], S["r"], S["style"], S["mode"]
        tx, ty = _uv_to_m(*S["uv"])
        if s.mode == "fixed":
            s.x, s.y = tx, ty
        elif s.mode == "gate":
            s.x, s.y, s.gate_yaw, s.cliff_h = _place_gate(h, slope, region, land, wet, tx, ty, S, lv, taken)
        else:
            s.x, s.y = _search(h, slope, region, land, wet, dw, D.coast_d, tx, ty, S, lv, taken)
        taken.append((s.x, s.y, s.r))
        sl, d = _disk(lv, s.x, s.y, s.r * 0.6)
        vals = h[sl][d <= s.r * 0.6]
        s.ground = float(np.median(vals)) if len(vals) else float(h[int(s.y / cell), int(s.x / cell)])
        if s.mode == "gate":
            # the apron at the foot of the rock face
            s.ground = float(np.percentile(vals, 20)) if len(vals) else s.ground
        if s.mode == "port":
            s.ground = max(s.ground, 2.2)
        s.moved_m = math.hypot(s.x - tx, s.y - ty)
        sites.append(s)
        log("  site %-15s moved %5.0f m  ground %6.1f m" % (s.id, s.moved_m, s.ground))
    return sites


def _search(h, slope, region, land, wet, dw, coast_d, tx, ty, S, lv, taken):
    cell = lv.cell
    R = S["r"]
    step = max(cell * 2, R / 6.0)
    rng = S["search"]
    best, bx, by = 1e18, tx, ty
    port = S["mode"] == "port"
    for oy in np.arange(-rng, rng + 1e-6, step):
        for ox in np.arange(-rng, rng + 1e-6, step):
            dd = math.hypot(ox, oy)
            if dd > rng:
                continue
            cx, cy = tx + ox, ty + oy
            ci, cj = int(round(cy / cell)), int(round(cx / cell))
            if not (2 <= ci < lv.H - 2 and 2 <= cj < lv.W - 2):
                continue
            if any(math.hypot(cx - px, cy - py) < (R + pr) * 1.05 for px, py, pr in taken):
                continue
            sl, d = _disk(lv, cx, cy, R)
            inside = d <= R
            reg = region[sl][inside]
            if reg.size == 0:
                continue
            frac_reg = float(np.mean(reg == S["region"]))
            if frac_reg < (0.35 if port else 0.6) or region[ci, cj] != S["region"]:
                continue
            cd = float(coast_d[ci, cj])
            if port:
                if not (0.25 * R <= cd <= 0.7 * R):
                    continue
                inner = d <= 0.6 * R
                wet_in = wet[sl] & ~(coast_d[sl] < 0)          # rivers / lakes inside
                if np.any(wet_in & inside):
                    continue
                ls = land[sl] & inside
                rough = float(np.std(h[sl][ls])) + 4.0 * float(np.mean(slope[sl][ls]))
                score = rough + 6.0 * dd / rng
            else:
                sl2, d2 = _disk(lv, cx, cy, R + 60.0)
                if np.any(wet[sl2][d2 <= R + 60.0]):
                    continue
                if cd < R + 80.0:
                    continue
                hv = h[sl][inside]
                rough = float(np.std(hv)) + 6.0 * float(np.mean(slope[sl][inside]))
                near_w = float(dw[ci, cj])
                score = rough + 8.0 * dd / rng + (2.0 if near_w > 1200 else 0.0)
            if score < best:
                best, bx, by = score, cx, cy
    return bx, by


def _place_gate(h, slope, region, land, wet, tx, ty, S, lv, taken):
    """Find the base of a tall rock face near the target; returns (x, y, yaw_deg facing out of the rock, height)."""
    cell = lv.cell
    rng = S["search"]
    sl, d = _disk(lv, tx, ty, rng)
    sub_s = slope[sl]
    sub_h = h[sl]
    ok = (d <= rng) & land[sl] & ~wet[sl]
    # apron cells: flat, with a steep face within 45 m upslope
    gx, gy = G.gradient(h, cell)
    face = G.max_filter((slope > 0.9).astype(np.float32), 4)[sl] > 0
    flat = G.max_filter(sub_s, 2) < 0.2
    hmax = G.max_filter(h, 7)[sl]
    hmin = (-G.max_filter(-h, 7))[sl]
    rise = hmax - sub_h
    # open apron at the foot of the face: a face rises above it, nothing drops away beside it, away from the sea
    apron = (sub_h - hmin < 6.0) & (rise > 25.0) & (sub_h > 6.0)
    cand = ok & flat & face & apron
    if not cand.any():
        cand = ok & flat & (rise > 15.0) & (sub_h - hmin < 8.0) & (sub_h > 4.0)
    if not cand.any():
        cand = ok & (sub_s < 0.3)
    score = np.where(cand, rise - 0.04 * d, -1e9)
    k = int(np.argmax(score))
    iy, ix = divmod(k, sub_h.shape[1])
    y = (sl[0].start + iy) * cell
    x = (sl[1].start + ix) * cell
    # facing: away from the rock = down the smoothed gradient
    hb = G.blur(h[max(0, sl[0].start + iy - 8):sl[0].start + iy + 9, max(0, sl[1].start + ix - 8):sl[1].start + ix + 9], 2.5)
    gyy, gxx = np.gradient(hb)
    c = (hb.shape[0] // 2, hb.shape[1] // 2)
    vx, vy = -gxx[c], -gyy[c]
    yaw = math.degrees(math.atan2(vy, vx)) if (abs(vx) + abs(vy)) > 1e-6 else 0.0
    return x, y, yaw, float(rise[iy, ix])


def shape_footprints(h, sites, lv, yaw_hint=None):
    """Flatten site footprints: plane (tilt <= 2 %) inside 0.62 R, smooth blend to terrain by 1.45 R + 40 m.
    Also adds landmark rises. Works on any level (heights in metres)."""
    h = h.copy()
    cell = lv.cell
    for s in sites:
        R = s.r
        outer = 1.45 * R + 40.0
        sl, d = _disk(lv, s.x, s.y, outer)
        sub = h[sl]
        ys = (np.arange(sl[0].start, sl[0].stop) * cell)[:, None] - s.y
        xs = (np.arange(sl[1].start, sl[1].stop) * cell)[None, :] - s.x
        plane = s.ground + s.tilt[0] * xs + s.tilt[1] * ys
        # organic outline: the blend radius wobbles with direction; smootherstep has no crease at either end
        ang = np.arctan2(ys, xs)
        k = zlib.crc32(s.id.encode("utf-8"))
        wob = 1.0 + 0.16 * np.sin(3.0 * ang + (k % 7)) + 0.1 * np.sin(5.0 * ang + (k % 5))
        tt = np.clip((d - 0.62 * R) / (max(1.0, outer - 0.62 * R) * wob), 0.0, 1.0)
        w = 1.0 - tt * tt * tt * (tt * (tt * 6.0 - 15.0) + 10.0)
        if s.mode == "port":
            # never raise the harbour into the sea: seaward part keeps its depth, shore gets a quay level
            new = np.where(sub < -0.5, sub, sub + (plane - sub) * w)
        else:
            new = sub + (plane - sub) * w
        # landmark mounds
        for lm in s.landmarks:
            if lm.get("rise", 0.0) <= 0:
                continue
            lx, ly = lm["x"] - s.x, lm["y"] - s.y
            dl = np.hypot(xs - lx, ys - ly)
            top = lm["z"]
            tq = np.clip((dl - lm["r"]) / max(1.0, lm["ramp"] * 1.6), 0.0, 1.0)
            sm = tq * tq * tq * (tq * (tq * 6.0 - 15.0) + 10.0)
            mound = top - (top - new) * sm
            new = np.where(dl < lm["r"] + 1.6 * lm["ramp"], np.maximum(new, mound), new)
        h[sl] = new
    return h


def finalize(D, h, sites, roads_by_site, lv=None, log=print):
    """After roads: yaw (towards the main road), tilt, landmarks, districts."""
    lv = lv or G.L1
    cell = lv.cell
    for s in sites:
        # yaw: direction from the centre to where the first road leaves the footprint
        s.yaw = 0.0
        pts = roads_by_site.get(s.id)
        if pts is not None and len(pts):
            dd = np.hypot(pts[:, 0] - s.x, pts[:, 1] - s.y)
            k = int(np.argmin(np.abs(dd - s.r)))
            s.yaw = math.degrees(math.atan2(pts[k, 1] - s.y, pts[k, 0] - s.x))
            s.road_entry = (float(pts[k, 0]), float(pts[k, 1]))
        elif hasattr(s, "gate_yaw"):
            s.yaw = s.gate_yaw
        if s.mode == "gate" and hasattr(s, "gate_yaw"):
            s.yaw = s.gate_yaw
        s.landmarks = _landmarks(s, h, lv)
        s.districts = [dict(Name=n, InnerRadiusCm=round(a * s.r * 100.0), OuterRadiusCm=round(b * s.r * 100.0))
                       for n, a, b in DISTRICTS.get(s.style, [])]
    return sites


def compute_tilts(sites, h, lv=None):
    """Footprint plane: least-squares fit of the natural terrain inside 0.6 R, tilt clamped to 2 %."""
    lv = lv or G.L1
    cell = lv.cell
    for s in sites:
        s.landmarks = []
        sl, d = _disk(lv, s.x, s.y, s.r * 0.6)
        m = d <= s.r * 0.6
        ys = (np.arange(sl[0].start, sl[0].stop) * cell)[:, None] - s.y
        xs = (np.arange(sl[1].start, sl[1].stop) * cell)[None, :] - s.x
        zz = h[sl][m]
        A = np.stack([np.broadcast_to(xs, d.shape)[m], np.broadcast_to(ys, d.shape)[m], np.ones(m.sum())], axis=1)
        try:
            coef, *_ = np.linalg.lstsq(A, zz, rcond=None)
            gx, gy = float(coef[0]), float(coef[1])
        except Exception:
            gx, gy = 0.0, 0.0
        g = math.hypot(gx, gy)
        lim = 0.0 if s.mode in ("fixed", "port") or s.r < 60 else 0.02
        if g > lim and g > 0:
            gx, gy = gx * lim / g, gy * lim / g
        s.tilt = (gx, gy)
    return sites


def _high_dir(s, h, lv, r_in, r_out, avoid_yaw=None):
    """Direction (deg) of the highest natural ground in the ring r_in..r_out (optionally away from avoid_yaw)."""
    best, by = -1e9, 0.0
    for a in range(0, 360, 10):
        if avoid_yaw is not None:
            da = abs((a - avoid_yaw + 180) % 360 - 180)
            if da < 70:
                continue
        rr = 0.5 * (r_in + r_out)
        x = s.x + rr * math.cos(math.radians(a))
        y = s.y + rr * math.sin(math.radians(a))
        z = float(G.sample_bilinear(h, np.array([x / lv.cell]), np.array([y / lv.cell]))[0])
        if z > best:
            best, by = z, a
    return by


def _lm(name, s, dist, ang, r, rise=0.0, ramp=0.0, yaw=None, h=None, lv=None):
    x = s.x + dist * math.cos(math.radians(ang))
    y = s.y + dist * math.sin(math.radians(ang))
    z = s.ground + s.tilt[0] * (x - s.x) + s.tilt[1] * (y - s.y) + rise
    return dict(name=name, x=x, y=y, z=z, r=r, rise=rise, ramp=ramp,
                yaw=(ang + 180.0) % 360.0 if yaw is None else yaw)


def _landmarks(s, h, lv):
    R = s.r
    L = []
    road = s.yaw
    if s.id == "Ars":
        a = _high_dir(s, h, lv, 0.6 * R, 1.1 * R, avoid_yaw=road)
        L.append(_lm("Silver Palace", s, 0.93 * R, a, 0.14 * R, rise=18.0, ramp=46.0))
        L.append(_lm("Royal Market", s, 0.35 * R, road, 0.08 * R))
        L.append(_lm("Hall of Government", s, 0.3 * R, (a + 120) % 360, 0.07 * R))
        L.append(_lm("Main Gate", s, 0.96 * R, road, 0.04 * R, yaw=road))
    elif s.id == "Roa":
        a = _high_dir(s, h, lv, 0.4 * R, 0.8 * R, avoid_yaw=road)
        L.append(_lm("Boreas Manor", s, 0.6 * R, a, 0.16 * R))
        L.append(_lm("Roa Market", s, 0.0, 0.0, 0.1 * R))
        L.append(_lm("Town Gate", s, 0.95 * R, road, 0.05 * R, yaw=road))
    elif s.id == "Buena":
        a = _high_dir(s, h, lv, 1.0 * R, 1.6 * R, avoid_yaw=road)
        L.append(_lm("Greyrat House", s, 1.18 * R, a, 22.0, rise=5.0, ramp=45.0))
        L.append(_lm("Windmill", s, 1.12 * R, (a + 150) % 360, 10.0, rise=2.5, ramp=22.0))
        L.append(_lm("Village Square", s, 0.0, 0.0, 0.15 * R))
    elif s.id == "Sharia":
        a = _high_dir(s, h, lv, 0.5 * R, 0.9 * R, avoid_yaw=road)
        L.append(_lm("Ranoa University of Magic", s, 0.66 * R, a, 0.26 * R))
        L.append(_lm("Adventurer Guild", s, 0.3 * R, (a + 150) % 360, 0.06 * R))
        L.append(_lm("Magic Shops Row", s, 0.32 * R, (a + 220) % 360, 0.08 * R))
    elif s.id == "Millishion":
        L.append(_lm("Holy Cathedral", s, 0.0, 0.0, 0.12 * R))
        a = _high_dir(s, h, lv, 0.5 * R, 0.9 * R, avoid_yaw=road)
        L.append(_lm("Holy Knights Keep", s, 0.72 * R, a, 0.1 * R))
        L.append(_lm("Lakeside Promenade", s, 0.95 * R, 90.0, 0.06 * R))
    elif s.id == "Rikarisu":
        L.append(_lm("Crater Market", s, 0.0, 0.0, 0.14 * R))
        L.append(_lm("Adventurer Hall", s, 0.5 * R, road + 60.0, 0.07 * R))
    elif s.id == "KingDragon":
        a = _high_dir(s, h, lv, 0.4 * R, 0.8 * R, avoid_yaw=road)
        L.append(_lm("Dragon King Castle", s, 0.55 * R, a, 0.2 * R))
    elif s.id == "Rapan":
        L.append(_lm("Grand Bazaar", s, 0.0, 0.0, 0.12 * R))
        L.append(_lm("Caravan Yards", s, 0.75 * R, road, 0.12 * R))
    elif s.style in ("MillisPort", "DemonPort") or s.id == "EastPort":
        L.append(_lm("Harbour Docks", s, 0.55 * R, _sea_dir(s, h, lv), 0.12 * R))
    elif s.id == "SwordSanctuary":
        L.append(_lm("Sword God Dojo", s, 0.4 * R, _high_dir(s, h, lv, 0.3 * R, 0.8 * R), 0.2 * R))
    elif s.style == "LabyrinthGate":
        L.append(dict(name="Labyrinth Gate", x=s.x + 8.0 * math.cos(math.radians(s.yaw + 180)),
                      y=s.y + 8.0 * math.sin(math.radians(s.yaw + 180)), z=s.ground, r=12.0, rise=0.0, ramp=0.0,
                      yaw=s.yaw))
    return L


def _sea_dir(s, h, lv):
    best, by = 1e9, 0.0
    for a in range(0, 360, 10):
        x = s.x + s.r * 0.9 * math.cos(math.radians(a))
        y = s.y + s.r * 0.9 * math.sin(math.radians(a))
        z = float(G.sample_bilinear(h, np.array([x / lv.cell]), np.array([y / lv.cell]))[0])
        if z < best:
            best, by = z, a
    return by
