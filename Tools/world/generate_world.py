#!/usr/bin/env python3
"""LA PLACE world generator (Docs/LaPlace/Spec.md sections 2-5; method in Docs/LaPlace/World_Generation.md).

Run:   ~/.venvs/mushoku-bpy311/bin/python Tools/world/generate_world.py [--skip-maps] [--skip-layers]
Deterministic (fixed seed in wg_config.SEED), numpy + Pillow only.

Outputs
  SourceArt/World/Height.r16              6097 x 4573 uint16 LE (Z_cm = 72400 + (h - 32768) * 400 / 128)
  SourceArt/World/Layers/<Layer>.png      6097 x 4573 8-bit paint layers, summing to 255 per vertex
  SourceArt/World/Regions.png             1524 x 1143 region ids
  SourceArt/World/Density/<Kind>.png      1524 x 1143 foliage densities
  SourceArt/World/WorldMap.png            4096 x 3072 painted parchment map (no text)
  Docs/Images/LaPlace_World_Preview.png   labelled hill-shaded review map
  Content/Data/World.json                 constants, regions, sites, roads, rivers, lakes, bridges
  Content/Data/Locations.json             spawn / fast-travel / quest locations
"""
import argparse
import json
import os
import resource
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import wg_config as C          # noqa: E402
import wg_geo as GEO           # noqa: E402
import wg_grid as G            # noqa: E402
import wg_terrain              # noqa: E402
import wg_erosion              # noqa: E402
import wg_hydro                # noqa: E402
import wg_sites                # noqa: E402
import wg_roads                # noqa: E402
import wg_fullres              # noqa: E402
import wg_paint                # noqa: E402
import wg_maps                 # noqa: E402
import wg_export               # noqa: E402


class Clock:
    def __init__(self):
        self.t0 = time.time()
        self.c0 = self.cpu()
        self.last = self.t0
        self.rows = []

    @staticmethod
    def cpu():
        r = resource.getrusage(resource.RUSAGE_SELF)
        return r.ru_utime + r.ru_stime

    def stage(self, name):
        now = time.time()
        self.rows.append((name, now - self.last))
        print("[%6.1f s] %s done (%.1f s)" % (now - self.t0, name, now - self.last), flush=True)
        self.last = now


def peak_rss_mb():
    r = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return r / (1024.0 * 1024.0) if sys.platform == "darwin" else r / 1024.0


def region_stats(h0, region1, cont1, land0):
    """Per-region height stats and per-continent land area from the final full-resolution heights."""
    reg0 = np.repeat(np.repeat(region1, 3, axis=0), 3, axis=1)[:C.FULL_H, :C.FULL_W]
    out = {"regions": {}, "continents": {}}
    cell_km2 = (C.QUAD_M / 1000.0) ** 2
    hs = h0[::2, ::2]
    rs = reg0[::2, ::2]
    for rid in range(C.N_REGIONS):
        m = rs == rid
        if not m.any():
            continue
        v = hs[m]
        out["regions"][rid] = {"area_km2": float(m.sum()) * cell_km2 * 4.0, "min": round(float(v.min()), 1),
                               "median": round(float(np.median(v)), 1), "max": round(float(v.max()), 1)}
    cont0 = np.repeat(np.repeat(cont1, 3, axis=0), 3, axis=1)[:C.FULL_H, :C.FULL_W][::2, ::2]
    total = float(hs.size)
    for name, cid in GEO.CONTINENT_IDS.items():
        m = cont0 == cid
        out["continents"][name] = {"land_km2": float((m & (hs > 0)).sum()) * cell_km2 * 4.0,
                                   "land_percent_of_map": 100.0 * float((m & (hs > 0)).sum()) / total}
    out["land_percent"] = 100.0 * float((hs > 0).sum()) / total
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-maps", action="store_true")
    ap.add_argument("--skip-layers", action="store_true")
    ap.add_argument("--erosion-iters", type=int, default=80)
    ap.add_argument("--maps-only", action="store_true",
                    help="recompute in memory and redraw WorldMap.png + the review preview only (no world data written)")
    ap.add_argument("--preview-only", action="store_true",
                    help="recompute in memory and redraw only Docs/Images/LaPlace_World_Preview.png")
    args = ap.parse_args()
    redraw = args.maps_only or args.preview_only
    clk = Clock()
    os.makedirs(C.OUT_WORLD, exist_ok=True)
    os.makedirs(C.OUT_DOC_IMG, exist_ok=True)
    print("LA PLACE world generator, seed %d" % C.SEED, flush=True)

    print("design (9 m grid)", flush=True)
    D = wg_terrain.build(C.SEED, log=print)
    clk.stage("design")

    print("landscape evolution (uplift + stream power + talus)", flush=True)
    h1, einfo = wg_erosion.erode_design(D, iters=args.erosion_iters, log=print)
    clk.stage("erosion")

    print("hydrology", flush=True)
    Hy = wg_hydro.build(D, h1, log=print)
    h1 = Hy.h
    clk.stage("hydrology")

    print("sites", flush=True)
    sites = wg_sites.place_all(D, h1, Hy, log=print)
    wg_sites.compute_tilts(sites, h1)
    h1s = wg_sites.shape_footprints(h1, sites, G.L1)
    clk.stage("sites")

    print("roads", flush=True)
    roads = wg_roads.build(D, h1s, Hy, sites, log=print)
    roads, bridges = wg_roads.profiles(roads, h1s, Hy, sites, log=print)
    roads_by_site = {}
    for rd in roads:
        for sid in rd["stops"]:
            if sid not in roads_by_site:
                roads_by_site[sid] = rd["pts"]
    wg_sites.finalize(D, h1s, sites, roads_by_site, log=print)
    clk.stage("roads")

    print("full resolution terrain", flush=True)
    h0, road_w, lake_level0 = wg_fullres.build(D, h1, Hy, sites, roads, bridges, log=print)
    enc = wg_fullres.encode(h0)
    if redraw:
        # redraw mode never writes world data; it only checks that the data on disk is what this code produces
        hp = os.path.join(C.OUT_WORLD, "Height.r16")
        same = os.path.exists(hp) and np.array_equal(np.fromfile(hp, dtype="<u2").reshape(enc.shape), enc)
        print("Height.r16 on disk %s the recomputed terrain" % ("matches" if same else "DOES NOT MATCH"), flush=True)
        P = wg_paint.prepare(D, h1, Hy, sites, roads, log=print)
        P.region_hard = D.region
        if args.maps_only:
            wg_maps.world_map(D, h0, Hy, sites, roads, P, os.path.join(C.OUT_WORLD, "WorldMap.png"), log=print)
        wg_maps.preview(D, h0, Hy, sites, roads, P, os.path.join(C.OUT_DOC_IMG, "LaPlace_World_Preview.png"), log=print)
        clk.stage("redraw")
        return
    enc.tofile(os.path.join(C.OUT_WORLD, "Height.r16"))
    clk.stage("full-res height")

    print("paint layers, densities, regions", flush=True)
    P = wg_paint.prepare(D, h1, Hy, sites, roads, log=print)
    P.region_hard = D.region
    if not args.skip_layers:
        wg_paint.layers(D, h0, road_w, lake_level0, P, Hy, sites, C.OUT_LAYERS, log=print)
    dens, excl = wg_paint.densities(D, P, h0, road_w, Hy, sites, C.OUT_DENSITY, log=print)
    wg_paint.region_png(P, os.path.join(C.OUT_WORLD, "Regions.png"))
    clk.stage("layers + densities")

    stats = region_stats(h0, D.region, D.cont, D.land)
    lakes_poly = wg_export.lake_polygons(Hy.lakes, lake_level0)
    wg_export.write_all(D, h0, Hy, sites, roads, bridges, lakes_poly, stats, P,
                        os.path.join(C.OUT_DATA, "World.json"), os.path.join(C.OUT_DATA, "Locations.json"), log=print)
    clk.stage("json")

    if not args.skip_maps:
        print("maps", flush=True)
        wg_maps.world_map(D, h0, Hy, sites, roads, P, os.path.join(C.OUT_WORLD, "WorldMap.png"), log=print)
        wg_maps.preview(D, h0, Hy, sites, roads, P, os.path.join(C.OUT_DOC_IMG, "LaPlace_World_Preview.png"), log=print)
        clk.stage("maps")

    total = time.time() - clk.t0
    summary = {"wall_s": round(total, 1), "cpu_s": round(clk.cpu() - clk.c0, 1), "peak_rss_mb": round(peak_rss_mb()),
               "stages": [(n, round(t, 1)) for n, t in clk.rows], "stats": stats}
    with open(os.path.join(C.OUT_WORLD, "generation_log.json"), "w") as f:
        json.dump(summary, f, indent=1, default=str)
    print("done: %.1f s wall, %.1f s cpu, peak RSS %.0f MB" % (total, clk.cpu() - clk.c0, peak_rss_mb()), flush=True)


if __name__ == "__main__":
    main()
