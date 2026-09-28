"""Build the LA PLACE procedural NATURE kit (trees, plants, rocks) as GLBs + contact sheets.

Run from the repo root with the Blender-as-a-module venv (Python 3.11, bpy 5.0):
    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/build_nature_kit.py                 # everything (~4 min)
    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/build_nature_kit.py --only SM_Tree_Oak_A,SM_Rock_Small_*
    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/build_nature_kit.py --category Rocks --no-preview
    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/build_nature_kit.py --dev <dir> --views 3 --only ...  # look-dev

Outputs (Docs/LaPlace/Spec.md section 7):
    SourceArt/Kit/<Category>/<Name>.glb          one GLB per asset (Trees / Plants / Rocks)
    SourceArt/Kit/<Category>/_Preview.png        contact sheet per category (Cycles CPU)
    SourceArt/Kit/manifest_nature.json           nature asset list (same schema as manifest_architecture.json)
    SourceArt/Kit/manifest.json                  shared manifest: nature entries merged in under a file lock
    SourceArt/Kit/QA_nature.json                 written by qa_nature_kit.py (run automatically at the end)
Deterministic: every asset is seeded from its name; no network, no randomness from the clock.
"""
import argparse
import contextlib
import fcntl
import json
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402,F401

import nature_lib as L  # noqa: E402
import nature_trees as TR  # noqa: E402
import nature_plants as PL  # noqa: E402
import nature_rocks as RK  # noqa: E402

ROOT = L.ROOT
KIT = L.KIT_DIR
CATEGORIES = ["Trees", "Plants", "Rocks"]
TILE_CACHE = os.path.join(tempfile.gettempdir(), "laplace_nature_tiles")

NOTES = {
    "SM_Tree_Oak_A": "Classic broad oak: short stout bole, five spreading limbs, lumpy rounded crown.",
    "SM_Tree_Oak_B": "Tall upright oak with a central leader and an oval crown.",
    "SM_Tree_Oak_C": "Low, wide, gnarled field oak; near-horizontal limbs under a flat-bottomed crown.",
    "SM_Tree_Birch_A": "Single slender white-barked birch, airy narrow crown of drooping twigs.",
    "SM_Tree_Birch_B": "Three-stem birch clump leaning apart.",
    "SM_Tree_Pine_A": "Dense spruce cone with boughs almost to the ground (folded needle strips).",
    "SM_Tree_Pine_B": "Scots-pine type: tall bare orange trunk, flat cloud-pruned needle pads.",
    "SM_Tree_Pine_C": "Tall slender fir for mountain slopes.",
    "SM_Tree_PineSnow_A": "Snow-laden spruce: MT_NeedlesSnow boughs with solid MT_Snow pillows on every tier.",
    "SM_Tree_PineSnow_B": "Taller snow-laden fir, same construction.",
    "SM_Tree_Dead_A": "Dead, gnarled oak with broken limbs and fine twig tips (no leaves).",
    "SM_Tree_Dead_B": "Tall dead snag with a broken top and a few spiky limbs.",
    "SM_Tree_Giant_A": "Great Forest giant: ~4 m buttressed trunk, surface roots, canopy of cloud masses.",
    "SM_Tree_Giant_B": "Tallest Great Forest giant: ~5 m trunk, two rings of canopy masses.",
    "SM_Tree_Demon_A": "Twisted rope-bark demon tree with a wide claw crown and sparse purple foliage.",
    "SM_Tree_Demon_B": "Corkscrew-trunk demon tree with drooping claw branches.",
    "SM_Tree_Demon_C": "Squat split-trunk demon tree, spiky.",
    "SM_Tree_Palm_A": "Oasis palm: ringed, gently curved trunk, arching fronds, dry frond skirt (MT_GrassDry).",
    "SM_Tree_Palm_B": "Twin-trunk oasis palm.",
    "SM_Tree_Jungle_A": "Southern jungle emergent with a broad flat umbrella canopy and buttress flare.",
    "SM_Tree_Jungle_B": "Taller leaning jungle tree with a layered two-tier canopy.",
    "SM_Bush_A": "Round leafy bush.",
    "SM_Bush_B": "Low, wide spreading bush (lighter birch-green leaves).",
    "SM_Bush_C": "Flowering bush: leaf clumps sprinkled with MT_Flowers cards.",
    "SM_Bush_D": "Tall upright shrub (hedgerow filler).",
    "SM_Bush_Snow_A": "Evergreen shrub under snow: MT_NeedlesSnow clumps with MT_Snow lumps on top.",
    "SM_Bush_Desert_A": "Round dry desert scrub: twiggy MT_BarkDead stems with dry tufts.",
    "SM_Bush_Desert_B": "Wider creosote-like dry scrub.",
    "SM_Plant_DemonGiant_A": "Alien tentacle-frond rosette with a fiddlehead stalk on a swollen bulb.",
    "SM_Plant_DemonGiant_B": "Lantern stalks: curled stalks ending in drooping purple tassels.",
    "SM_Plant_DemonGiant_C": "Spiky purple rosette with a tall flowering spike.",
    "SM_Mushroom_Giant_A": "Single 5.6 m mushroom: flared stem, ring skirt, cap with gill underside.",
    "SM_Mushroom_Giant_B": "Cluster of four giant mushrooms (1.5-3.9 m).",
    "SM_Fern_A": "Fern clump, arched fronds.",
    "SM_Fern_B": "Large Great Forest fern.",
    "SM_Grass_A": "Meadow grass clump (crossed + fanned blade cards).",
    "SM_Grass_B": "Tall grass clump.",
    "SM_Grass_C": "Low, wide lawn patch.",
    "SM_Grass_Dry_A": "Dry grass tuft.",
    "SM_Grass_Dry_B": "Tall dry steppe grass.",
    "SM_Grass_Snow_A": "Frosted dry grass poking out of a low MT_Snow mound.",
    "SM_Wheat_A": "Dense 2 x 2 m wheat patch, ~1 m tall (tiles edge to edge).",
    "SM_Wheat_B": "Wind-bent 2 x 2 m wheat patch.",
    "SM_Flowers_A": "Low wildflower clump with grass.",
    "SM_Flowers_B": "Tall wildflowers.",
    "SM_Flowers_C": "Wide meadow flower patch (three sub-clumps).",
    "SM_Reeds_A": "Reed clump for river banks.",
    "SM_Reeds_B": "Dense tall cattail bed.",
    "SM_Rock_Small_A": "Small chiselled rock.",
    "SM_Rock_Small_B": "Small angular rock.",
    "SM_Rock_Small_C": "Cluster of three stones (path / stream scatter).",
    "SM_Rock_Medium_A": "Medium boulder.",
    "SM_Rock_Medium_B": "Medium upright boulder.",
    "SM_Rock_Medium_C": "Medium flat boulder with a mossy top (MT_RockMossy).",
    "SM_Rock_Large_A": "Large boulder with chiselled facets.",
    "SM_Rock_Large_B": "Large low outcrop with a mossy top.",
    "SM_Rock_Cliff_A": "14 m cliff chunk: blocky massif, vertical fractures, subtle strata.",
    "SM_Rock_Cliff_B": "24 m tall fractured cliff chunk (mountain faces, canyon walls).",
    "SM_Rock_Cliff_C": "28 m massive wide cliff chunk.",
    "SM_Rock_Snow_A": "Boulder (MT_RockSnow) with a separate MT_Snow cap sheet.",
    "SM_Rock_Snow_B": "Large layered rock with snow on the top and ledges.",
    "SM_Rock_Desert_A": "Layered sandstone boulder.",
    "SM_Rock_Desert_B": "Flat-topped sandstone mesa outcrop.",
    "SM_Rock_Desert_C": "Sandstone slab with tilted strata.",
    "SM_Rock_Arch_A": "~19 m natural sandstone arch on two footings.",
    "SM_Rock_Pillar_A": "~19 m sandstone hoodoo: flared base, thin neck, cap rock.",
    "SM_Rock_Pillar_B": "~37 m weathered granite spire with buttresses and cracks.",
    "SM_Rock_Demon_A": "Cluster of tall obsidian shards on a dark core (flat-shaded, razor edges).",
    "SM_Rock_Demon_B": "Jagged 12 m obsidian crag.",
    "SM_Rock_Demon_C": "Low jagged obsidian outcrop.",
    "SM_Rock_Pale_A": "Wind-rounded pale boulder (Heaven plateau).",
    "SM_Rock_Pale_B": "Tall chiselled pale slab.",
    "SM_Log_A": "Fallen oak log, sunk into the ground, splintered ends (MT_BarkDead) and branch stubs.",
    "SM_Stump_A": "Broken stump with root flare and surface roots.",
}

KIND = {"Tree": "tree", "Bush": "bush", "Plant": "plant", "Mushroom": "plant", "Fern": "groundcover",
        "Grass": "groundcover", "Wheat": "groundcover", "Flowers": "groundcover", "Reeds": "groundcover",
        "Rock": "rock", "Log": "deadwood", "Stump": "deadwood"}

# name -> dict(category, style, regions, scatter, budget, hrange, fn)
ASSETS = {}
for _cat, _mod in (("Trees", TR), ("Plants", PL), ("Rocks", RK)):
    for _n, _m in _mod.META.items():
        ASSETS[_n] = dict(category=_cat, style=_m["style"], regions=_m["regions"],
                          scatter=_m.get("scatter", "Trees"), budget=_m["budget"], hrange=_m["hrange"],
                          rule=_m.get("rule", "height"), fn=_mod.SPECIES[_n])


def kind_of(name):
    part = name.split("_")[1]
    if name.startswith("SM_Rock_Cliff") or name.startswith("SM_Rock_Arch") or name.startswith("SM_Rock_Pillar"):
        return "formation"
    return KIND.get(part, "prop")


def fit_height(geo, a, margin=0.01):
    """Trees / plants: if the generated height lands outside the asset's declared range (a stray card
    above the crown, say), scale uniformly about the pivot back inside it. Deterministic; the factor
    stays close to 1 so bark texel density is essentially unchanged. Rocks are sized by their builders."""
    if a["category"] == "Rocks" or a.get("rule", "height") != "height":
        return 1.0
    h0, h1 = a["hrange"]
    top = max(v[2] for v in geo.V)
    k = 1.0
    if top > h1:
        k = h1 * (1 - margin) / top
    elif top < h0:
        k = h0 * (1 + margin) / top
    if k != 1.0:
        geo.transform(L.Matrix.Diagonal((k, k, k, 1.0)))
        print("[nature]   fit %s: height %.2f m -> %.2f m (x%.3f)" % (a["category"], top, top * k, k))
    return k


def build_one(name, out_root):
    a = ASSETS[name]
    L.reset_scene()
    t0 = time.time()
    res = a["fn"](name)
    geo, info = res if isinstance(res, tuple) else (res, {})
    fit = fit_height(geo, a)
    L.clamp_cards_floor(geo, -0.03)
    ob, geo = L.build_object(geo, name, sharp_angle=info.get("sharp_angle"))
    path = os.path.join(out_root, a["category"], name + ".glb")
    L.export_glb(ob, path)
    st = L.mesh_stats(geo)
    st.update(name=name, category=a["category"], style=a["style"], path=path, seconds=round(time.time() - t0, 2),
              tris_by_slot=geo.tris_by_mat())
    print("[nature] %-26s %6d tris  %5.1f x %5.1f x %5.1f m  %s  (%.1fs)" % (
        name, st["tris"], st["footprint"][0], st["footprint"][1], st["height"], ",".join(st["slots"]),
        st["seconds"]), flush=True)
    return st


def manifest_entry(st):
    a = ASSETS[st["name"]]
    return {
        "name": st["name"],
        "category": st["category"],
        "style": st["style"],
        "kind": kind_of(st["name"]),
        "file": os.path.relpath(st["path"], ROOT),
        "footprint": st["footprint"],
        "height": st["height"],
        "min_z": st["min"][2],
        "tris": st["tris"],
        "materials": st["slots"],
        "tris_by_material": dict(sorted(st["tris_by_slot"].items())),
        "pivot": "base",
        "budget": list(a["budget"]),
        "size_range": list(a["hrange"]),
        "size_rule": a["rule"],
        "bounds": [st["min"], st["max"]],
        "regions": a["regions"],
        "scatter": a["scatter"],
        "notes": NOTES.get(st["name"], ""),
    }


@contextlib.contextmanager
def locked(path):
    """Exclusive advisory lock (fcntl) for a shared JSON file. The lock file lives in the system temp dir
    (laplace_kit_<file>.lock) so the repo stays clean; any kit builder can take the same lock."""
    lock_path = os.path.join(tempfile.gettempdir(), "laplace_kit_%s.lock" % os.path.basename(path))
    with open(lock_path, "w") as lf:
        fcntl.flock(lf, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(lf, fcntl.LOCK_UN)


def write_json_atomic(path, data):
    tmp = path + ".tmp%d" % os.getpid()
    with open(tmp, "w") as f:
        json.dump(data, f, indent=1)
        f.write("\n")
    os.replace(tmp, path)


def write_manifests(entries):
    """manifest_nature.json (all nature assets, subset rebuilds merge in) + merge into manifest.json."""
    os.makedirs(KIT, exist_ok=True)
    own = os.path.join(KIT, "manifest_nature.json")
    with locked(own):
        old = json.load(open(own)) if os.path.exists(own) else {"assets": []}
        by_name = {e["name"]: e for e in old.get("assets", []) if e.get("name") in ASSETS}
        for e in entries:
            by_name[e["name"]] = e
        order = [n for n in ASSETS if n in by_name]
        data = {
            "generator": "Tools/kit/build_nature_kit.py",
            "spec": "Docs/LaPlace/Spec.md sections 6-7",
            "units": "metres; Blender Z-up (glTF files are Y-up, the exporter converts)",
            "pivot": "base: trunk / rock base centre on the ground at z = 0; roots, trunks and rocks continue "
                     "below ground (min_z) so nothing floats on slopes",
            "uv": "solids: 1 UV unit = 1 m (rocks box-projected; bark wraps an integer number of 1 m tiles around "
                  "the trunk, V = metres along the branch); cards (leaves, needles, grass, wheat, flowers, fern, "
                  "reeds, palm fronds) use the whole 0..1 texture, V up / along the frond",
            "normals": "tree and bush leaf cards carry custom normals blended toward the crown volume (soft "
                       "painterly shading); card winding always agrees with those normals",
            "count": len(order),
            "assets": [by_name[n] for n in order],
        }
        write_json_atomic(own, data)
    shared = os.path.join(KIT, "manifest.json")
    with locked(shared):
        if os.path.exists(shared):
            try:
                man = json.load(open(shared))
            except ValueError:
                man = None
        else:
            man = None
        if man is None:
            man = {"spec": "Docs/LaPlace/Spec.md section 7", "units": "metres, Blender Z-up", "assets": []}
        if isinstance(man, list):
            man = {"assets": man}
        assets = man.setdefault("assets", [])
        mine = {e["name"] for e in data["assets"]}
        assets[:] = [a for a in assets if a.get("name") not in mine]
        assets.extend(data["assets"])
        gens = man.get("generators", [])
        if "Tools/kit/build_nature_kit.py" not in gens:
            gens.append("Tools/kit/build_nature_kit.py")
        man["generators"] = gens
        man["count"] = len(assets)
        write_json_atomic(shared, man)
    print("[nature] manifest_nature.json: %d assets; manifest.json now lists %d assets" % (
        len(data["assets"]), len(man["assets"])))
    return data


def render_previews(categories, out_root, views, size, samples, dev):
    import nature_preview as PV
    stage = PV.setup_stage(samples)
    man_path = os.path.join(KIT, "manifest_nature.json")
    man = {e["name"]: e for e in json.load(open(man_path))["assets"]} if (not dev and os.path.exists(man_path)) else {}
    for cat in categories:
        tiles = []
        names = [n for n in ASSETS if ASSETS[n]["category"] == cat and
                 os.path.exists(os.path.join(out_root, cat, n + ".glb"))]
        for n in names:
            glb = os.path.join(out_root, cat, n + ".glb")
            png = os.path.join(TILE_CACHE if not dev else os.path.join(out_root, cat, "_tiles"), n + ".png")
            os.makedirs(os.path.dirname(png), exist_ok=True)
            ims = PV.render_asset(stage, glb, png, style=ASSETS[n]["style"], size=size, views=views)
            e = man.get(n)
            if e:
                dims = "%.1f x %.1f x %.1f m   %d tris" % (e["footprint"][0], e["footprint"][1], e["height"], e["tris"])
            else:
                dims = ""
            for i, im in enumerate(ims):
                tiles.append((im, [n, dims] if i == 0 else [n + " (view %d)" % i, ""]))
        if not tiles:
            continue
        out = os.path.join(out_root, cat, "_Preview.png" if not dev else "_Dev.png")
        PV.contact_sheet(tiles, out, "LA PLACE nature kit - %s (%d assets)" % (cat, len(names)),
                         cols=6 if len(views) == 1 else len(views) * 2)
        print("[nature] preview", os.path.relpath(out, ROOT) if not dev else out, flush=True)
        if cat == "Trees" and not dev and len(names) == len([n for n in ASSETS if ASSETS[n]["category"] == cat]):
            glbs = {n: os.path.join(out_root, cat, n + ".glb") for n in names}
            giants = [n for n in names if "Giant" in n or "Jungle" in n] + ["SM_Tree_Oak_A"]
            normal = [n for n in names if n not in giants or n == "SM_Tree_Oak_A"]
            out = os.path.join(out_root, cat, "_Lineup.png")
            PV.render_lineup([("Trees at true scale, seen from {d:.0f} m (eye height 1.7 m)", [glbs[n] for n in normal],
                               75.0, 7),
                              ("Great Forest giants and jungle trees with SM_Tree_Oak_A for scale, seen from {d:.0f} m",
                               [glbs[n] for n in giants], 110.0, 10)], out)
            print("[nature] lineup", os.path.relpath(out, ROOT), flush=True)
            stage = PV.setup_stage(samples)     # the lineup resets the scene: rebuild the preview stage


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="", help="comma list of names, trailing * allowed")
    ap.add_argument("--category", default="")
    ap.add_argument("--no-preview", action="store_true")
    ap.add_argument("--no-qa", action="store_true")
    ap.add_argument("--dev", default="", help="write GLBs/previews to this folder instead of SourceArt/Kit")
    ap.add_argument("--views", type=int, default=1)
    ap.add_argument("--size", type=int, default=420)
    ap.add_argument("--samples", type=int, default=32)
    args = ap.parse_args(argv)

    names = list(ASSETS)
    if args.only:
        want = [s.strip() for s in args.only.split(",") if s.strip()]
        names = [n for n in names if any(n == w or (w.endswith("*") and n.startswith(w[:-1])) for w in want)]
    if args.category:
        names = [n for n in names if ASSETS[n]["category"] == args.category]
    out_root = args.dev or KIT
    t0 = time.time()
    stats = [build_one(n, out_root) for n in names]
    if not args.dev:
        write_manifests([manifest_entry(s) for s in stats])
    if not args.no_preview:
        views = [(-35, 12), (55, 10), (-35, 62)][:max(1, args.views)]
        cats = [c for c in CATEGORIES if any(ASSETS[n]["category"] == c for n in names)]
        render_previews(cats, out_root, views, args.size, args.samples, bool(args.dev))
    print("[nature] %d assets built in %.0fs" % (len(stats), time.time() - t0))
    rc = 0
    if not args.dev and not args.no_qa:
        import qa_nature_kit
        rc = qa_nature_kit.main([])
    if not args.dev:
        import nature_docs
        nature_docs.write()
    return rc


if __name__ == "__main__":
    sys.exit(main())
