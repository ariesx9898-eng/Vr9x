#!/usr/bin/env python3
"""Build every LA PLACE texture (Docs/LaPlace/Spec.md section 8, Docs/LaPlace/Textures.md).

Inputs:  SourceArt/AI/Textures/*.png (committed Higgsfield sources) + procedural code in Tools/textures/.
Outputs: SourceArt/Textures/Terrain/T_Ground_<Layer>_{D,N,M}.png, T_Macro_Noise.png
         SourceArt/Textures/Palette/T_<Name>_{D,N,M}.png (+ T_Crystal_E.png)
         SourceArt/Textures/Water/T_Ocean_N.png, T_Foam_D.png
         SourceArt/Textures/manifest.json, Docs/Images/LaPlace_Textures_{Terrain,Palette,Cards}.jpg

Usage (Python 3.11 with numpy + Pillow, e.g. ~/.venvs/mushoku-bpy311/bin/python):
  build_textures.py                     everything (deterministic; a few minutes on 12 cores)
  build_textures.py --only Grass,RoofRed,LeavesOak    subset (the manifest keeps the other entries)
  build_textures.py --group Terrain     one group: Terrain, Palette, Cards, Extras
  build_textures.py --no-sheets         skip the contact sheets
  build_textures.py --jobs 4            worker processes (default: CPU count)
  build_textures.py --selftest          normal-map convention test only
"""
import argparse
import json
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numpy as np  # noqa: E402

import texlib as T  # noqa: E402
import terrain  # noqa: E402
import palette  # noqa: E402
import cards  # noqa: E402
import extras  # noqa: E402
import sheets  # noqa: E402
from common import OUT_DIR  # noqa: E402

MANIFEST = os.path.join(OUT_DIR, "manifest.json")


def selftest():
    """Synthetic dome: the DirectX normal map must have a bright right edge in R and a bright bottom edge in G, and
    shading it the way Unreal reads it (tangent +Y = down the image) with light from the top of the image must light
    the top of the dome (a bump, not a dent)."""
    n = 128
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    h = np.sqrt(np.maximum(0.0, 30.0 ** 2 - ((x - 64) ** 2 + (y - 64) ** 2))) / n
    N = T.normal_from_height(h, 1.0 / n)
    assert N[64, 88, 0] > 0.7 and N[64, 40, 0] < 0.3, "red channel must be -dh/du"
    assert N[88, 64, 1] > 0.7 and N[40, 64, 1] < 0.3, "green channel must be -dh/dv (v down, DirectX)"
    d = N * 2 - 1
    L = np.array([0.0, -1.0, 1.0]) / np.sqrt(2.0)
    s = np.clip((d * L).sum(-1), 0, 1)
    assert s[40, 64] > s[88, 64] + 0.5, "dome lit from the top must be bright on top"
    return True


def _job(args):
    group, name = args
    t0 = time.time()
    try:
        return _build_one(group, name, t0)
    except FileNotFoundError as e:           # an AI source is missing: skip this material, keep the rest
        return group, name, {"job": f"{group}/{name}", "missing": str(e), "build_s": round(time.time() - t0, 1)}


def _build_one(group, name, t0):
    if group == "Terrain":
        entry, _ = terrain.build(name, terrain.TERRAIN[name])
    elif group == "Palette":
        entry, _ = palette.build(name)
    elif group == "Cards":
        entry, _ = cards.build(name)
    else:
        entry, _ = extras.build(name)
    entry["build_s"] = round(time.time() - t0, 1)
    entry["job"] = f"{group}/{name}"
    return group, name, entry


def all_jobs():
    jobs = [("Terrain", n) for n in terrain.TERRAIN]
    jobs += [("Palette", n) for n in palette.NAMES]
    jobs += [("Cards", n) for n in cards.NAMES]
    jobs += [("Extras", n) for n in extras.NAMES]
    return jobs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", default="")
    ap.add_argument("--group", default="")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--no-sheets", action="store_true")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    selftest()
    print("normal-map convention self-test: OK (DirectX, green = -dh/dv, v down)")
    if a.selftest:
        return
    jobs = all_jobs()
    if a.group:
        groups = {g.strip() for g in a.group.split(",")}
        jobs = [j for j in jobs if j[0] in groups]
    if a.only:
        only = {o.strip() for o in a.only.split(",")}
        jobs = [j for j in jobs if j[1] in only]
    if not jobs:
        sys.exit("nothing to build")
    t0 = time.time()
    # longest jobs first keeps the pool busy
    order = {"Terrain": 0, "Extras": 1, "Palette": 2, "Cards": 3}
    jobs.sort(key=lambda j: order[j[0]])
    results = []
    if a.jobs > 1 and len(jobs) > 1:
        with Pool(min(a.jobs, len(jobs))) as pool:
            for r in pool.imap_unordered(_job, jobs):
                results.append(r)
                print(f"  done {r[0]}/{r[1]} ({r[2]['build_s']} s)", flush=True)
    else:
        for j in jobs:
            results.append(_job(j))
    missing = [r for r in results if "missing" in r[2]]
    for r in missing:
        print(f"  SKIPPED {r[0]}/{r[1]}: {r[2]['missing']}")
    results = [r for r in results if "missing" not in r[2]]
    manifest = {"entries": []}
    if os.path.exists(MANIFEST):
        manifest = json.load(open(MANIFEST))
    by_key = {e["job"]: e for e in manifest.get("entries", []) if "job" in e}
    for group, name, entry in results:
        entry.pop("build_s", None)
        by_key[entry["job"]] = entry
    index = {f"{g}/{n}": i for i, (g, n) in enumerate(all_jobs())}
    entries = sorted((e for e in by_key.values() if e["job"] in index), key=lambda e: index[e["job"]])
    manifest = dict(
        spec="Docs/LaPlace/Spec.md section 8",
        generator="Tools/textures/build_textures.py",
        conventions=dict(
            D="albedo, sRGB; RGB (+ A = alpha mask for cards)",
            N="tangent-space normal, DirectX / Unreal: R = -dh/du, G = -dh/dv with v pointing down the image",
            M="linear: R = ambient occlusion, G = roughness, B = height 0..1",
            E="emissive mask, linear (Crystal only)",
            tile_m="physical size of one texture repeat in metres; with 1 UV = 1 m meshes scale UVs by 1 / tile_m",
            mean_albedo="average albedo in linear RGB (alpha-weighted for cards), for far-distance colour"),
        entries=entries)
    with open(MANIFEST, "w") as f:
        json.dump(manifest, f, indent=1)
    print(f"built {len(results)} materials in {time.time() - t0:.1f} s -> {MANIFEST}")
    if not a.no_sheets:
        sheets.build_all(manifest)
        print("contact sheets written to Docs/Images/")


if __name__ == "__main__":
    main()
