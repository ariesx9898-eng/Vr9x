#!/usr/bin/env python3
"""Build the LA PLACE architecture + VFX mesh kit (Docs/LaPlace/Spec.md sections 1, 4, 6, 7).

Run with the Blender-as-a-module venv (Python 3.11, bpy 5.0):
    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/build_architecture_kit.py            # everything
    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/build_architecture_kit.py --only 'SM_Asura_*' --views /tmp/v

Outputs (deterministic, no network):
    SourceArt/Kit/<Category>/<Name>.glb          one GLB per asset (metres, Z-up in Blender, glTF Y-up)
    SourceArt/Kit/<Category>/_Preview.png        contact sheet per category (EEVEE render of the exported GLBs)
    SourceArt/Kit/manifest_architecture.json     asset list (footprint, height, tris, slots, pivot, attach points)
    SourceArt/Kit/QA_architecture.json           QA report (Tools/kit/qa_architecture_kit.py)
    Docs/LaPlace/Kit.md                          'Architecture kit' and 'VFX meshes' sections (regenerated between
                                                 markers; other sections of the file are left untouched)
"""
import argparse
import fnmatch
import json
import os
import random
import subprocess
import sys
import time
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(ROOT, "SourceArt", "Kit")

import bpy  # noqa: E402

import arch_registry as R  # noqa: E402
from arch_geo import Geo, to_blender_object  # noqa: E402

MODULES = ["arch_asura", "arch_noble", "arch_rural", "arch_north", "arch_millis", "arch_demon", "arch_desert",
           "arch_walls", "arch_props", "arch_landmarks", "arch_vfx"]
CATEGORY_ORDER = ["Asura", "AsuraNoble", "Rural", "North", "Millis", "Demon", "Desert", "Walls", "Props",
                  "Landmarks", "VFX"]


def load_modules():
    import importlib
    for m in MODULES:
        if os.path.exists(os.path.join(HERE, m + ".py")):
            importlib.import_module(m)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def export_glb(obj, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLB", use_selection=True, export_apply=True, export_yup=True,
        export_texcoords=True, export_normals=True, export_tangents=False, export_materials="EXPORT",
        export_image_format="NONE", export_animations=False, export_skins=False, export_morph=False,
        export_cameras=False, export_lights=False, export_extras=False, export_vertex_color="NONE",
        check_existing=False)


def build_one(ad):
    g = Geo(ad.name)
    rng = random.Random(zlib.crc32(ad.name.encode()))
    extra = ad.fn(g, rng) or {}
    lo, hi = g.bounds()
    if ad.pivot == "center":
        g.translate_all((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -(lo[2] + hi[2]) / 2))
        lo, hi = g.bounds()
    elif ad.recentre and ad.pivot != "base_point":
        g.translate_all((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, 0.0))
        lo, hi = g.bounds()
    obj = to_blender_object(g, ad.name)
    me = obj.data
    me.calc_loop_triangles()
    tris = len(me.loop_triangles)
    mats = sorted({m.name for m in me.materials})
    rel = os.path.join("SourceArt", "Kit", ad.category, ad.name + ".glb")
    export_glb(obj, os.path.join(ROOT, rel))
    entry = {
        "name": ad.name, "category": ad.category, "style": ad.style, "kind": ad.kind, "file": rel,
        "footprint": [round(hi[0] - lo[0], 2), round(hi[1] - lo[1], 2)],
        "height": round(hi[2], 2), "min_z": round(lo[2], 2), "size": [round(hi[i] - lo[i], 3) for i in range(3)],
        "tris": tris, "materials": mats,
        "pivot": ad.pivot, "foundation": ad.foundation, "front": "-Y", "budget": list(ad.budget),
        "bounds": [[round(v, 3) for v in lo], [round(v, 3) for v in hi]],
    }
    if ad.notes:
        entry["notes"] = ad.notes
    if g.anchors:
        entry["anchors"] = {k: [round(c, 3) for c in v] for k, v in g.anchors.items()}
    if extra:
        entry.update(extra)
    bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.meshes.remove(me)
    return entry


def write_manifest(entries, replace_all):
    path = os.path.join(KIT, "manifest_architecture.json")
    old = {}
    if os.path.exists(path) and not replace_all:
        for e in json.load(open(path)).get("assets", []):
            old[e["name"]] = e
    for e in entries:
        old[e["name"]] = e
    known = set(R.ORDER)
    assets = [old[n] for n in R.ORDER if n in old] + [e for n, e in old.items() if n not in known]
    man = {
        "generator": "Tools/kit/build_architecture_kit.py",
        "spec": "Docs/LaPlace/Spec.md sections 6-7",
        "units": "metres; Blender Z-up (glTF files are Y-up, the exporter converts); +X right, fronts face -Y",
        "pivot": "base: centre of the XY bounding box at ground level z = 0 (buildings have a buried foundation "
                 "skirt down to -foundation); base_point: the base attachment point at z = 0 (see notes); "
                 "center: bounding-box centre; hub: see anchors",
        "uv": "box-projected, 1 UV unit = 1 m (roof slopes planar along the slope); VFX meshes use the UVs "
              "described in their notes",
        "count": len(assets),
        "assets": assets,
    }
    os.makedirs(KIT, exist_ok=True)
    with open(path, "w") as f:
        json.dump(man, f, indent=1)
    return man


def render_sheets(man, categories):
    import arch_render as AR
    for cat in categories:
        items = [a for a in man["assets"] if a["category"] == cat]
        if not items:
            continue
        entries = []
        for a in items:
            ad = R.REG.get(a["name"])
            sz = a.get("size", [a["footprint"][0], a["footprint"][1], a["height"]])
            lines = [a["name"], f"{sz[0]:.2f} x {sz[1]:.2f} x {sz[2]:.2f} m, {a['tris']} tris"]
            opts = {"ground": ad.ground if ad else True}
            if ad and ad.view:
                opts["view"] = ad.view
            entries.append((os.path.join(ROOT, a["file"]), lines, opts))
        cols = 4 if len(entries) > 6 else 3
        if len(entries) > 16:
            cols = 5
        out = os.path.join(KIT, cat, "_Preview.png")
        reset_scene()
        AR.contact_sheet(entries, out, cols=cols, title=f"LA PLACE kit / {cat} ({len(entries)} assets)")
        print("preview", os.path.relpath(out, ROOT))


def render_debug_views(names, outdir, views):
    import arch_render as AR
    os.makedirs(outdir, exist_ok=True)
    for n in names:
        ad = R.REG[n]
        path = os.path.join(KIT, ad.category, n + ".glb")
        reset_scene()
        objs = AR.import_glb(path)
        AR.render_views(objs, os.path.join(outdir, n + ".png"), views=views, ground=ad.ground,
                        res=tuple(int(x) for x in os.environ.get("KIT_VIEW_RES", "640x480").split("x")))
        print("views", os.path.join(outdir, n + ".png"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default=None, help="comma-separated glob patterns of asset names")
    ap.add_argument("--category", default=None, help="comma-separated categories")
    ap.add_argument("--no-sheets", action="store_true")
    ap.add_argument("--no-qa", action="store_true")
    ap.add_argument("--no-docs", action="store_true")
    ap.add_argument("--views", default=None, help="directory for multi-view debug renders of the built assets")
    ap.add_argument("--view-angles", default="-35:24,145:24,-90:62,35:12")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    load_modules()
    names = list(R.ORDER)
    if a.list:
        for n in names:
            print(R.REG[n].category, n)
        return 0
    if a.category:
        cats = a.category.split(",")
        names = [n for n in names if R.REG[n].category in cats]
    if a.only:
        pats = a.only.split(",")
        names = [n for n in names if any(fnmatch.fnmatch(n, p) for p in pats)]
    full = not a.only and not a.category
    reset_scene()
    entries = []
    t0 = time.time()
    for n in names:
        t = time.time()
        e = build_one(R.REG[n])
        entries.append(e)
        print(f"built {n:34s} {e['tris']:6d} tris  {e['footprint'][0]:6.1f} x {e['footprint'][1]:6.1f} x "
              f"{e['height']:5.1f} m  {time.time() - t:5.2f}s", flush=True)
    man = write_manifest(entries, replace_all=full)
    print(f"{len(entries)} assets in {time.time() - t0:.1f}s; manifest {man['count']} entries")
    if a.views:
        views = [tuple(float(x) for x in v.split(":")) for v in a.view_angles.split(",")]
        render_debug_views(names, a.views, views)
    if not a.no_sheets:
        cats = [c for c in CATEGORY_ORDER if any(R.REG[n].category == c for n in names)]
        render_sheets(man, cats)
    if not a.no_docs and full:
        import arch_docs
        arch_docs.write_docs(man)
    rc = 0
    if not a.no_qa:
        cmd = [sys.executable, os.path.join(HERE, "qa_architecture_kit.py")]
        if a.only:
            pass
        rc = subprocess.call(cmd)
    return rc


if __name__ == "__main__":
    sys.exit(main())
