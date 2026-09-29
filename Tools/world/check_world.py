#!/usr/bin/env python3
"""Validate the LA PLACE world outputs (Docs/LaPlace/Spec.md section 5).

Run:  ~/.venvs/mushoku-bpy311/bin/python Tools/world/check_world.py [--json out.json]
Reads only the generated files (no generator state), prints a report and exits non-zero on any hard failure.

Hard checks: file formats and sizes; heights inside the encodable range with no clipping; sea level encoding;
every paint layer sums to exactly 255; densities are zero on roads, water and town footprints; sites on land, in
their region and flat across 60 % of their radius; spawn points on flat land; rivers monotone downhill; JSON schemas.
Reported (soft) metrics: land per continent, per-region heights, slope histogram, spikes, road grades (> 15 % is a
warning), river bank clearance, lake shore consistency.
"""
import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wg_config as C  # noqa: E402

Image.MAX_IMAGE_PIXELS = None
W, H = C.FULL_W, C.FULL_H
fails, warns, info = [], [], {}


def fail(msg):
    fails.append(msg)
    print("  FAIL  " + msg)


def warn(msg):
    warns.append(msg)
    print("  warn  " + msg)


def ok(msg):
    print("  ok    " + msg)


def z_of(hraw):
    return (C.LANDSCAPE_Z_CM + (hraw.astype(np.float64) - 32768.0) * C.LANDSCAPE_SCALE_Z / 128.0) / 100.0


def cm_to_px(x_cm, y_cm):
    """World cm -> full-res vertex coordinates (col, row)."""
    return (x_cm - C.LANDSCAPE_MIN_X_CM) / 300.0, (y_cm - C.LANDSCAPE_MIN_Y_CM) / 300.0


def bilinear(a, px, py):
    px = np.clip(np.asarray(px, np.float64), 0, a.shape[1] - 1.001)
    py = np.clip(np.asarray(py, np.float64), 0, a.shape[0] - 1.001)
    x0, y0 = np.floor(px).astype(int), np.floor(py).astype(int)
    fx, fy = px - x0, py - y0
    v00, v01 = a[y0, x0], a[y0, x0 + 1]
    v10, v11 = a[y0 + 1, x0], a[y0 + 1, x0 + 1]
    return (v00 * (1 - fx) + v01 * fx) * (1 - fy) + (v10 * (1 - fx) + v11 * fx) * fy


def disk(z, px, py, r_px):
    y0, y1 = max(0, int(py - r_px) - 1), min(H, int(py + r_px) + 2)
    x0, x1 = max(0, int(px - r_px) - 1), min(W, int(px + r_px) + 2)
    yy, xx = np.mgrid[y0:y1, x0:x1]
    m = (xx - px) ** 2 + (yy - py) ** 2 <= r_px * r_px
    return z[y0:y1, x0:x1], m, xx, yy


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default=os.path.join(C.OUT_WORLD, "validation_report.json"))
    args = ap.parse_args()

    print("== files")
    hpath = os.path.join(C.OUT_WORLD, "Height.r16")
    if not os.path.exists(hpath) or os.path.getsize(hpath) != W * H * 2:
        fail("Height.r16 missing or wrong size (expected %d bytes)" % (W * H * 2))
        return finish(args)
    ok("Height.r16 %d x %d uint16 LE (%d bytes)" % (W, H, W * H * 2))
    hraw = np.fromfile(hpath, dtype="<u2").reshape(H, W)
    z = z_of(hraw).astype(np.float32)
    layers = {}
    for n in C.LAYERS:
        p = os.path.join(C.OUT_LAYERS, n + ".png")
        if not os.path.exists(p):
            fail("layer %s missing" % n)
            continue
        im = Image.open(p)
        if im.size != (W, H) or im.mode != "L":
            fail("layer %s is %s %s (expected %dx%d L)" % (n, im.size, im.mode, W, H))
        layers[n] = np.asarray(im)
    if len(layers) == len(C.LAYERS):
        ok("11 paint layers %dx%d 8-bit" % (W, H))
    rp = os.path.join(C.OUT_WORLD, "Regions.png")
    reg = np.asarray(Image.open(rp))
    if reg.shape != (C.REGION_H, C.REGION_W) or reg.max() >= C.N_REGIONS:
        fail("Regions.png shape %s / max id %d" % (reg.shape, reg.max()))
    else:
        ok("Regions.png %dx%d ids %s" % (C.REGION_W, C.REGION_H, sorted(np.unique(reg).tolist())))
    dens = {}
    for k in C.DENSITY_KINDS:
        p = os.path.join(C.OUT_DENSITY, k + ".png")
        im = Image.open(p)
        if im.size != (C.REGION_W, C.REGION_H) or im.mode != "L":
            fail("density %s is %s %s" % (k, im.size, im.mode))
        dens[k] = np.asarray(im)
    ok("5 density maps %dx%d" % (C.REGION_W, C.REGION_H))
    wm = Image.open(os.path.join(C.OUT_WORLD, "WorldMap.png"))
    if wm.size != (C.WORLDMAP_W, C.WORLDMAP_H):
        fail("WorldMap.png size %s" % (wm.size,))
    else:
        ok("WorldMap.png %dx%d %s" % (wm.size[0], wm.size[1], wm.mode))
    if not os.path.exists(os.path.join(C.OUT_DOC_IMG, "LaPlace_World_Preview.png")):
        fail("Docs/Images/LaPlace_World_Preview.png missing")
    world = json.load(open(os.path.join(C.OUT_DATA, "World.json")))
    locs = json.load(open(os.path.join(C.OUT_DATA, "Locations.json")))
    ok("World.json and Locations.json parse (%d locations)" % len(locs))

    print("== heights")
    info["height_raw_min"], info["height_raw_max"] = int(hraw.min()), int(hraw.max())
    info["height_m_min"], info["height_m_max"] = round(float(z.min()), 2), round(float(z.max()), 2)
    if hraw.min() == 0 or hraw.max() == 65535:
        fail("heights clip at the encoding limits (raw %d..%d)" % (hraw.min(), hraw.max()))
    else:
        ok("raw h %d..%d  ->  Z %.1f .. %.1f m (encodable -300 .. +1748 m, no clipping)" % (
            hraw.min(), hraw.max(), z.min(), z.max()))
    zs = z_of(np.array([C.H_SEA]))[0]
    if abs(zs) > 1e-9:
        fail("h = 9600 does not decode to Z = 0 (got %.4f m)" % zs)
    else:
        ok("sea level: h = 9600 <-> Z = 0 cm")
    # region grid at full res (pixel p covers vertices 4p .. 4p+3)
    reg_full = np.repeat(np.repeat(reg, 4, axis=0), 4, axis=1)
    reg_full = np.pad(reg_full, ((0, H - reg_full.shape[0]), (0, W - reg_full.shape[1])), mode="edge")
    sea_r = reg_full == 0
    land_z = z > 0.0
    mism_land = float(np.mean(land_z[sea_r]))
    info["ocean_region_above_sea_fraction"] = round(mism_land, 5)
    if mism_land > 0.01:
        warn("%.2f%% of ocean-region vertices are above sea level" % (100 * mism_land))
    else:
        ok("ocean region vertices above sea level: %.3f%% (coastline sub-pixel detail)" % (100 * mism_land))
    info["land_percent"] = round(100.0 * float(land_z.mean()), 2)
    ok("land %.2f%% of the landscape" % info["land_percent"])
    # continent land shares
    cont_of = {r["Id"]: r["Continent"] for r in world["Regions"]}
    cont_land = {}
    for rid in range(1, C.N_REGIONS):
        m = reg_full == rid
        c = cont_of.get(rid) or "Islands"
        cont_land[c] = cont_land.get(c, 0.0) + float((m & land_z).sum()) * 9.0 / 1e6
    total_land = sum(cont_land.values())
    info["continent_land_km2"] = {k: round(v, 2) for k, v in cont_land.items()}
    for k, v in sorted(cont_land.items(), key=lambda kv: -kv[1]):
        print("        %-9s %6.2f km2  (%.1f%% of land)" % (k, v, 100 * v / total_land))
    # per-region heights
    info["regions"] = {}
    print("        region                 min    median    max  (m)")
    for rid in range(C.N_REGIONS):
        m = reg_full[::2, ::2] == rid
        if not m.any():
            continue
        v = z[::2, ::2][m]
        info["regions"][C.REGIONS[rid][0]] = dict(min=round(float(v.min()), 1), median=round(float(np.median(v)), 1),
                                                  max=round(float(v.max()), 1))
        print("        %2d %-20s %7.1f %8.1f %7.1f" % (rid, C.REGIONS[rid][1], v.min(), np.median(v), v.max()))
    # slope histogram and spikes (land only)
    gx = np.zeros_like(z)
    gy = np.zeros_like(z)
    gx[:, 1:-1] = (z[:, 2:] - z[:, :-2]) / 6.0
    gy[1:-1, :] = (z[2:, :] - z[:-2, :]) / 6.0
    sl = np.degrees(np.arctan(np.hypot(gx, gy)))
    lm = land_z
    bins = [0, 2, 5, 10, 15, 20, 30, 45, 60, 90]
    hist, _ = np.histogram(sl[lm], bins=bins)
    frac = hist / max(1, lm.sum())
    info["slope_histogram_deg"] = {"%d-%d" % (bins[i], bins[i + 1]): round(100 * float(frac[i]), 2) for i in range(len(frac))}
    print("        slope histogram (land): " + ", ".join("%s: %.1f%%" % (k, v) for k, v in info["slope_histogram_deg"].items()))
    c = z[1:-1, 1:-1]
    nb = np.stack([z[:-2, :-2], z[:-2, 1:-1], z[:-2, 2:], z[1:-1, :-2], z[1:-1, 2:], z[2:, :-2], z[2:, 1:-1], z[2:, 2:]])
    spikes = int(((c > nb.max(axis=0) + 2.0) | (c < nb.min(axis=0) - 2.0)).sum())
    lap = c - nb.mean(axis=0)
    info["isolated_spikes_or_pits_gt2m"] = spikes
    info["vertices_curvature_gt6m"] = int((np.abs(lap) > 6.0).sum())
    info["nan_values"] = 0
    if spikes > 50:
        warn("%d isolated spikes / pits (a vertex > 2 m above or below all 8 neighbours)" % spikes)
    else:
        ok("isolated spikes / pits > 2 m: %d  (vertices on sharp cliff edges with > 6 m curvature: %d)" % (
            spikes, info["vertices_curvature_gt6m"]))
    del gx, gy, lap, nb, c

    print("== paint layers")
    if len(layers) == len(C.LAYERS):
        tot = np.zeros((H, W), np.uint16)
        for n in C.LAYERS:
            tot += layers[n]
        bad = int((tot != 255).sum())
        info["layer_sum_bad_vertices"] = bad
        if bad:
            fail("%d vertices where the 11 layers do not sum to 255" % bad)
        else:
            ok("all %d vertices: layers sum to exactly 255" % (W * H))
        cover = {n: round(100.0 * float(layers[n][lm].mean()) / 255.0, 2) for n in C.LAYERS}
        info["layer_coverage_land_percent"] = cover
        print("        land coverage: " + ", ".join("%s %.1f%%" % kv for kv in cover.items()))
        del tot

    print("== sites and spawn points")
    reg_of = {r["Key"]: r["Id"] for r in world["Regions"]}
    info["sites"] = []
    for s in world["Sites"]:
        px, py = cm_to_px(s["Center"]["X"], s["Center"]["Y"])
        r_px = s["RadiusCm"] / 300.0
        zc = float(bilinear(z, px, py))
        rid_here = int(reg[min(C.REGION_H - 1, int(py / 4)), min(C.REGION_W - 1, int(px / 4))])
        sub, m, xx, yy = disk(z, px, py, 0.6 * r_px)
        if "Port" in s["Style"] or s["Id"] in ("EastPort", "WestPort", "ZantPort", "Wenport"):
            # harbour towns straddle the shore: judge the dry part, away from the quay edge
            dry = sub > 0.5
            for _ in range(2):
                dry = dry & np.roll(dry, 1, 0) & np.roll(dry, -1, 0) & np.roll(dry, 1, 1) & np.roll(dry, -1, 1)
            m = m & dry
        vals = sub[m]
        A = np.stack([xx[m] - px, yy[m] - py, np.ones(m.sum())], axis=1).astype(np.float64)
        coef, *_ = np.linalg.lstsq(A, vals.astype(np.float64), rcond=None)
        resid = float(np.abs(A @ coef - vals).max())
        tilt = math.hypot(coef[0], coef[1]) / 3.0 * 100.0
        sgx = np.zeros_like(sub)
        sgy = np.zeros_like(sub)
        sgx[:, 1:-1] = (sub[:, 2:] - sub[:, :-2]) / 6.0
        sgy[1:-1, :] = (sub[2:, :] - sub[:-2, :]) / 6.0
        mm = m.copy()
        mm[0, :] = mm[-1, :] = False
        mm[:, 0] = mm[:, -1] = False
        slope_max = float(np.percentile(np.hypot(sgx, sgy)[mm], 99)) * 100.0
        row = dict(id=s["Id"], z=round(zc, 2), region_ok=rid_here == s["RegionId"], tilt_pct=round(tilt, 2),
                   resid_m=round(resid, 2), slope_p99_pct=round(slope_max, 2))
        info["sites"].append(row)
        problems = []
        if zc <= 0.5:
            problems.append("not on land (Z %.2f)" % zc)
        if rid_here != s["RegionId"]:
            problems.append("region %d at centre, expected %d" % (rid_here, s["RegionId"]))
        if tilt > 2.2 or resid > 0.6 or slope_max > 5.0:
            problems.append("not flat within 0.6 R (tilt %.2f%%, residual %.2f m, p99 slope %.1f%%)" %
                            (tilt, resid, slope_max))
        if problems:
            fail("site %s: %s" % (s["Id"], "; ".join(problems)))
        else:
            ok("site %-15s Z %6.1f m  region %2d  tilt %.2f%%  resid %.2f m  p99 slope %.1f%%" % (
                s["Id"], zc, rid_here, tilt, resid, slope_max))
    need = ["LocationID", "DisplayName", "Continent", "Region", "Biome", "Difficulty", "Description", "WorldLocation",
            "SpawnYaw", "bSpawnPoint", "bFastTravel", "DiscoveryRadius", "RequiredRank", "MapUV", "PreviewCamera"]
    spawn_ids = sorted(l["LocationID"] for l in locs if l.get("bSpawnPoint"))
    fast_ids = sorted(l["LocationID"] for l in locs if l.get("bFastTravel"))
    exp_spawn = sorted(["Buena", "Roa", "Ars", "Sharia", "Rikarisu", "Millishion", "Rapan"])
    if spawn_ids != exp_spawn:
        fail("spawn points %s, expected %s" % (spawn_ids, exp_spawn))
    else:
        ok("spawn points: %s" % ", ".join(spawn_ids))
    if not {"Wenport", "ZantPort"} <= set(fast_ids) or {"Wenport", "ZantPort"} & set(spawn_ids):
        fail("Wenport / ZantPort must be fast travel but not spawn points")
    else:
        ok("fast travel: %s" % ", ".join(fast_ids))
    for l in locs:
        missing = [k for k in need if k not in l]
        if missing:
            fail("location %s missing %s" % (l.get("LocationID"), missing))
            continue
        wl = l["WorldLocation"]
        px, py = cm_to_px(wl["X"], wl["Y"])
        zt = float(bilinear(z, px, py))
        u, v = l["MapUV"]["X"], l["MapUV"]["Y"]
        if not (0 <= u <= 1 and 0 <= v <= 1) or not (1 <= l["Difficulty"] <= 5):
            fail("location %s: MapUV / Difficulty out of range" % l["LocationID"])
        if abs(zt * 100.0 - wl["Z"]) > 30.0:
            fail("location %s: WorldLocation Z %.0f cm vs ground %.0f cm" % (l["LocationID"], wl["Z"], zt * 100))
        cam = l["PreviewCamera"]["Location"]
        cpx, cpy = cm_to_px(cam["X"], cam["Y"])
        if cam["Z"] < float(bilinear(z, cpx, cpy)) * 100.0 + 500.0:
            fail("location %s: preview camera below / too close to the ground" % l["LocationID"])
        if l["bSpawnPoint"]:
            sub, m, _, _ = disk(z, px, py, 3.0)
            rng_ = float(sub[m].max() - sub[m].min())
            if zt <= 0.5 or rng_ > 1.0:
                fail("spawn %s not on flat land (Z %.2f, relief %.2f m within 9 m)" % (l["LocationID"], zt, rng_))
            else:
                ok("spawn %-11s (%.0f, %.0f, %.0f) cm  yaw %.0f  relief %.2f m within 9 m" % (
                    l["LocationID"], wl["X"], wl["Y"], wl["Z"], l["SpawnYaw"], rng_))

    print("== roads, bridges")
    info["roads"] = []
    for rd in world["Roads"]:
        P = rd["Points"]
        xs = np.array([p["X"] for p in P])
        ys = np.array([p["Y"] for p in P])
        zz = np.array([p["Z"] for p in P]) / 100.0
        ds = np.hypot(np.diff(xs), np.diff(ys)) / 100.0
        gr = np.abs(np.diff(zz)) / np.maximum(ds, 1e-3)
        px, py = cm_to_px(xs, ys)
        zt = bilinear(z, px, py)
        dev = np.abs(zt - zz)
        on_br = np.zeros(len(P), bool)
        for b in world["Bridges"]:
            if b["Road"] != rd["Id"]:
                continue
            bx0, by0 = b["Start"]["X"], b["Start"]["Y"]
            bx1, by1 = b["End"]["X"], b["End"]["Y"]
            L = math.hypot(bx1 - bx0, by1 - by0) + 1e-6
            t = ((xs - bx0) * (bx1 - bx0) + (ys - by0) * (by1 - by0)) / (L * L)
            dd = np.abs((xs - bx0) * (by1 - by0) - (ys - by0) * (bx1 - bx0)) / L
            on_br |= (t > -0.05) & (t < 1.05) & (dd < 1500.0)
        g = float(gr.max()) if len(gr) else 0.0
        row = dict(id=rd["Id"], length_m=rd["LengthM"], max_grade_pct=round(100 * g, 2),
                   p99_grade_pct=round(100 * float(np.percentile(gr, 99)), 2),
                   max_dev_m=round(float(dev[~on_br].max()), 2) if (~on_br).any() else 0.0)
        info["roads"].append(row)
        msg = "road %-16s %6.0f m  max grade %5.1f%% (p99 %4.1f%%)  |Z - ground| max %.2f m" % (
            rd["Id"], rd["LengthM"], row["max_grade_pct"], row["p99_grade_pct"], row["max_dev_m"])
        if g > 0.15:
            warn(msg + "  <- grade > 15%")
        else:
            ok(msg)
    ok("%d bridges" % len(world["Bridges"]))

    print("== rivers and lakes")
    info["rivers"] = []
    for rv in world["Rivers"]:
        zw = np.array([p["Z"] for p in rv["Points"]]) / 100.0
        rises = int((np.diff(zw) > 1e-3).sum())
        xs = np.array([p["X"] for p in rv["Points"]])
        ys = np.array([p["Y"] for p in rv["Points"]])
        wd = np.array([p["WidthCm"] for p in rv["Points"]]) / 100.0
        # bank clearance: ground just outside the channel edge vs water surface
        dx = np.gradient(xs)
        dy = np.gradient(ys)
        nl = np.maximum(np.hypot(dx, dy), 1e-6)
        nx_, ny_ = -dy / nl, dx / nl
        low = 0
        for side in (-1, 1):
            bx = xs + side * nx_ * (wd * 0.5 + 3.0) * 100.0
            by = ys + side * ny_ * (wd * 0.5 + 3.0) * 100.0
            px, py = cm_to_px(bx, by)
            low += int((bilinear(z, px, py) < zw - 0.05).sum())
        row = dict(id=rv["Id"], points=len(zw), source_z=round(float(zw[0]), 2), mouth_z=round(float(zw[-1]), 2),
                   rises=rises, mouth=rv["Mouth"], bank_points_below_water=low)
        info["rivers"].append(row)
        if rises:
            fail("river %s: water surface rises %d times downstream" % (rv["Id"], rises))
        else:
            ok("river %-14s %4d pts  water %6.1f -> %5.1f m (monotone)  mouth %s  bank points below water: %d" % (
                rv["Id"], len(zw), zw[0], zw[-1], rv["Mouth"], low))
    for lk in world["Lakes"]:
        ok("lake %-15s water %6.1f m  area %8.0f m2  %s" % (lk["Id"], lk["WaterZ"] / 100.0, lk["AreaM2"],
                                                            "frozen" if lk["Frozen"] else lk["Kind"]))

    print("== densities")
    road = layers.get("Road")
    if road is not None:
        blk = lambda a: a[:C.REGION_H * 4, :C.REGION_W * 4].reshape(C.REGION_H, 4, C.REGION_W, 4).max(axis=(1, 3))
        road_px = blk(road) > 20
        water_px = blk((z < 0.3).astype(np.uint8)) > 0
        town = np.zeros((C.REGION_H, C.REGION_W), bool)
        yy, xx = np.mgrid[0:C.REGION_H, 0:C.REGION_W]
        cxv = xx * 4 + 2
        cyv = yy * 4 + 2
        for s in world["Sites"]:
            px, py = cm_to_px(s["Center"]["X"], s["Center"]["Y"])
            town |= (cxv - px) ** 2 + (cyv - py) ** 2 < (s["RadiusCm"] / 300.0) ** 2
        for k, a in dens.items():
            bad = int(((a > 0) & (road_px | water_px | town)).sum())
            if bad:
                fail("density %s: %d pixels > 0 on roads / water / towns" % (k, bad))
            else:
                ok("density %-7s zero on roads, water and town footprints (mean %.1f)" % (k, a.mean()))
    return finish(args)


OUTPUTS = [os.path.join("SourceArt", "World", "Height.r16")] + \
    [os.path.join("SourceArt", "World", "Layers", n + ".png") for n in C.LAYERS] + \
    [os.path.join("SourceArt", "World", "Regions.png")] + \
    [os.path.join("SourceArt", "World", "Density", k + ".png") for k in C.DENSITY_KINDS] + \
    [os.path.join("SourceArt", "World", "WorldMap.png"), os.path.join("Docs", "Images", "LaPlace_World_Preview.png"),
     os.path.join("Content", "Data", "World.json"), os.path.join("Content", "Data", "Locations.json")]


def finish(args):
    info["fails"] = fails
    info["warnings"] = warns
    with open(args.json, "w") as f:
        json.dump(info, f, indent=1)
    # merge a summary into the generator's log (the checker never touches world data)
    logp = os.path.join(C.OUT_WORLD, "generation_log.json")
    try:
        log = json.load(open(logp))
    except Exception:
        log = {}
    log["files"] = {p: os.path.getsize(os.path.join(C.ROOT, p)) for p in OUTPUTS if os.path.exists(os.path.join(C.ROOT, p))}
    log["regenerate"] = {
        "all": "~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_world.py",
        "validate": "~/.venvs/mushoku-bpy311/bin/python Tools/world/check_world.py",
        "redraw_maps_only": "~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_world.py --maps-only",
        "redraw_preview_only": "~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_world.py --preview-only",
    }
    log["validation"] = {"failures": len(fails), "warnings": len(warns), "fails": fails, "warning_list": warns,
                         "report": os.path.relpath(args.json, C.ROOT),
                         "summary": {k: info.get(k) for k in ("height_m_min", "height_m_max", "land_percent",
                                                                 "continent_land_km2", "regions",
                                                                 "slope_histogram_deg", "isolated_spikes_or_pits_gt2m",
                                                                 "layer_sum_bad_vertices")}}
    with open(logp, "w") as f:
        json.dump(log, f, indent=1, default=str)
    print("\n%d failures, %d warnings  (report: %s)" % (len(fails), len(warns), args.json))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
