"""QA for the LA PLACE nature kit GLBs (pure numpy GLB reader, no Blender needed).

    ~/.venvs/mushoku-bpy311/bin/python Tools/kit/qa_nature_kit.py [--strict] [names...]

Checks every asset listed in SourceArt/Kit/manifest_nature.json (Spec sections 6-7):
  tris within the asset's budget          - bounds: height inside the asset's expected range
  pivot at the base centre on the ground  - no node transforms (transforms applied)
  every material slot is in the palette   - no zero-area / degenerate triangles
  no loose (unreferenced) vertices        - no duplicate / coincident triangles
  normals present, unit length and agreeing with the triangle winding (outward shading)
  solids: closed-ish shells have positive signed volume (outward), UV texel density ~1 UV/m
  cards: UVs inside 0..1 and spanning the whole texture, never more than 5 cm below the ground
Writes SourceArt/Kit/QA_nature.json and exits non-zero if any asset fails.
"""
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
KIT = os.path.join(ROOT, "SourceArt", "Kit")

NATURE = ["MT_BarkOak", "MT_BarkBirch", "MT_BarkPine", "MT_BarkDead", "MT_BarkGiant", "MT_BarkDemon", "MT_LeavesOak",
          "MT_LeavesBirch", "MT_NeedlesPine", "MT_NeedlesSnow", "MT_LeavesGiant", "MT_LeavesDemon", "MT_LeavesPalm",
          "MT_Grass", "MT_GrassDry", "MT_Wheat", "MT_Flowers", "MT_Fern", "MT_Reeds", "MT_MushroomCap",
          "MT_MushroomStem", "MT_Rock", "MT_RockMossy", "MT_RockSnow", "MT_RockDesert", "MT_RockDemon", "MT_RockPale"]
ARCH = ["MT_Plaster", "MT_PlasterTan", "MT_Timber", "MT_WoodPlanks", "MT_Stone", "MT_StoneWhite", "MT_Cobble",
        "MT_Sandstone", "MT_DemonRock", "MT_RoofRed", "MT_RoofBlue", "MT_RoofDark", "MT_RoofThatch", "MT_RoofSilver",
        "MT_RoofGreen", "MT_Snow", "MT_ClothRed", "MT_ClothBlue", "MT_ClothGreen", "MT_ClothTan", "MT_Hide", "MT_Iron",
        "MT_Gold", "MT_Glass", "MT_Crystal", "MT_Bone"]
PALETTE = set(NATURE) | set(ARCH) | {"VFX"}
CARDS = {"MT_LeavesOak", "MT_LeavesBirch", "MT_NeedlesPine", "MT_NeedlesSnow", "MT_LeavesGiant", "MT_LeavesDemon",
         "MT_LeavesPalm", "MT_Grass", "MT_GrassDry", "MT_Wheat", "MT_Flowers", "MT_Fern", "MT_Reeds"}

CT = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def read_glb(path):
    data = open(path, "rb").read()
    magic, ver, length = struct.unpack("<4sII", data[:12])
    assert magic == b"glTF" and ver == 2, "not a glTF 2 binary"
    off = 12
    js, bin_ = None, b""
    while off < len(data):
        ln, typ = struct.unpack("<I4s", data[off:off + 8])
        chunk = data[off + 8:off + 8 + ln]
        if typ == b"JSON":
            js = json.loads(chunk)
        elif typ == b"BIN\x00":
            bin_ = chunk
        off += 8 + ln
    return js, bin_


def accessor(js, bin_, i):
    a = js["accessors"][i]
    bv = js["bufferViews"][a["bufferView"]]
    dt = CT[a["componentType"]]
    n = NC[a["type"]]
    o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    stride = bv.get("byteStride")
    size = np.dtype(dt).itemsize * n
    if stride and stride != size:
        raw = np.frombuffer(bin_, np.uint8, a["count"] * stride, o).reshape(a["count"], stride)
        return raw[:, :size].copy().view(dt).reshape(a["count"], n)
    return np.frombuffer(bin_, dt, a["count"] * n, o).reshape(a["count"], n)


def load(path):
    js, bin_ = read_glb(path)
    mats = [m.get("name", "?") for m in js.get("materials", [])]
    double = [bool(m.get("doubleSided", False)) for m in js.get("materials", [])]
    prims = []
    for node in js.get("nodes", []):
        if "mesh" not in node:
            continue
        for p in js["meshes"][node["mesh"]]["primitives"]:
            at = p["attributes"]
            pos = accessor(js, bin_, at["POSITION"]).astype(np.float64)
            # glTF Y-up -> Blender Z-up: (x, y, z) -> (x, -z, y)
            pos = np.stack([pos[:, 0], -pos[:, 2], pos[:, 1]], 1)
            nrm = accessor(js, bin_, at["NORMAL"]).astype(np.float64) if "NORMAL" in at else None
            if nrm is not None:
                nrm = np.stack([nrm[:, 0], -nrm[:, 2], nrm[:, 1]], 1)
            uv = accessor(js, bin_, at["TEXCOORD_0"]).astype(np.float64) if "TEXCOORD_0" in at else None
            idx = accessor(js, bin_, p["indices"]).astype(np.int64).reshape(-1, 3)
            mi = p.get("material")
            prims.append(dict(pos=pos, nrm=nrm, uv=uv, idx=idx, mat=mats[mi] if mi is not None else None,
                              double=double[mi] if mi is not None else False))
    nodes = js.get("nodes", [])
    return js, prims, nodes


def check_asset(entry, strict=False):
    path = os.path.join(ROOT, entry["file"])
    res = dict(name=entry["name"], errors=[], warnings=[], stats={})
    E, W = res["errors"], res["warnings"]
    if not os.path.exists(path):
        E.append("missing file")
        return res
    js, prims, nodes = load(path)
    # transforms applied
    for n in nodes:
        for k, ident in (("translation", [0, 0, 0]), ("rotation", [0, 0, 0, 1]), ("scale", [1, 1, 1])):
            if k in n and not np.allclose(n[k], ident, atol=1e-6):
                E.append("node %s has %s %s (transforms not applied)" % (n.get("name"), k, n[k]))
        if "matrix" in n and not np.allclose(n["matrix"], np.eye(4).flatten(), atol=1e-6):
            E.append("node %s has a matrix" % n.get("name"))
    if len([n for n in nodes if "mesh" in n]) != 1:
        W.append("expected exactly one mesh node, found %d" % len([n for n in nodes if "mesh" in n]))
    allpos = np.concatenate([p["pos"] for p in prims])
    tris = int(sum(len(p["idx"]) for p in prims))
    lo, hi = allpos.min(0), allpos.max(0)
    H = float(hi[2])
    res["stats"] = dict(tris=tris, min=lo.round(3).tolist(), max=hi.round(3).tolist(),
                        footprint=[round(float(hi[0] - lo[0]), 2), round(float(hi[1] - lo[1]), 2)], height=round(H, 2),
                        materials=sorted({p["mat"] for p in prims}))
    if not np.all(np.isfinite(allpos)):
        E.append("non-finite positions")
    b0, b1 = entry.get("budget", [0, 10 ** 9])
    if not (b0 <= tris <= b1):
        E.append("tris %d outside budget %s" % (tris, [b0, b1]))
    h0, h1 = entry.get("size_range", entry.get("height_range", [0, 1e9]))
    if entry.get("size_rule") == "max_extent":
        size, what = max(hi[0] - lo[0], hi[1] - lo[1], H), "size (max extent)"
    else:
        size, what = H, "height"
    if not (h0 - 1e-6 <= size <= h1 + 1e-6):
        E.append("%s %.2f m outside expected %s" % (what, size, [h0, h1]))
    # pivot on the ground at the base centre
    if lo[2] > 0.02:
        E.append("mesh floats: lowest point %.3f m above the pivot" % lo[2])
    if lo[2] < -max(0.4, 0.25 * H):
        W.append("sinks %.2f m below ground (%.0f%% of height)" % (-lo[2], 100 * -lo[2] / max(H, 1e-6)))
    if hi[2] <= 0:
        E.append("nothing above ground")
    band = allpos[(allpos[:, 2] < min(0.3, 0.15 * H) + 0.0) & (allpos[:, 2] > lo[2] - 1)]
    if len(band):
        c = (band[:, :2].min(0) + band[:, :2].max(0)) * 0.5
        span = max(hi[0] - lo[0], hi[1] - lo[1])
        if np.linalg.norm(c) > 0.2 * span + 0.05:
            E.append("base centre (%.2f, %.2f) is off the pivot (footprint %.1f m)" % (c[0], c[1], span))
    # materials
    for p in prims:
        if p["mat"] not in PALETTE:
            E.append("material slot %r not in the Spec palette" % p["mat"])
    # per-primitive geometry checks
    n_zero = n_degen = n_loose = n_dup = n_flip = 0
    n_badnrm = 0
    tri_keys = {}
    solid_vol = {}
    texel = []
    for p in prims:
        pos, idx, nrm, uv = p["pos"], p["idx"], p["nrm"], p["uv"]
        if idx.max() >= len(pos):
            E.append("index out of range in %s" % p["mat"])
            continue
        n_degen += int(np.sum((idx[:, 0] == idx[:, 1]) | (idx[:, 1] == idx[:, 2]) | (idx[:, 0] == idx[:, 2])))
        used = np.zeros(len(pos), bool)
        used[idx.ravel()] = True
        n_loose += int((~used).sum())
        a, b, c = pos[idx[:, 0]], pos[idx[:, 1]], pos[idx[:, 2]]
        cr = np.cross(b - a, c - a)
        area = 0.5 * np.linalg.norm(cr, axis=1)
        n_zero += int(np.sum(area < 1e-7))
        # coincident duplicates (same three corner positions, any order/winding)
        q = np.round(np.stack([a, b, c], 1) / 1e-4).astype(np.int64)
        for t in range(len(q)):
            key = tuple(sorted(map(tuple, q[t])))
            tri_keys[key] = tri_keys.get(key, 0) + 1
        if nrm is None:
            E.append("no normals on %s" % p["mat"])
        else:
            ln = np.linalg.norm(nrm, axis=1)
            n_badnrm += int(np.sum(np.abs(ln - 1) > 1e-2))
            fn = cr / np.maximum(np.linalg.norm(cr, axis=1, keepdims=True), 1e-20)
            vn = nrm[idx].mean(1)
            agree = np.einsum("ij,ij->i", fn, vn)
            n_flip += int(np.sum((agree < 0) & (area > 1e-6)))
        if uv is None:
            E.append("no UVs on %s" % p["mat"])
        elif p["mat"] in CARDS:
            if pos[:, 2].min() < -0.05:
                E.append("%s cards reach %.2f m below ground" % (p["mat"], pos[:, 2].min()))
            if uv.min() < -1e-3 or uv.max() > 1 + 1e-3:
                E.append("card UVs of %s leave 0..1 (%.3f..%.3f)" % (p["mat"], uv.min(), uv.max()))
            if uv[:, 0].max() - uv[:, 0].min() < 0.99 or uv[:, 1].max() - uv[:, 1].min() < 0.99:
                E.append("card UVs of %s do not span the whole texture" % p["mat"])
        else:
            ua, ub, uc = uv[idx[:, 0]], uv[idx[:, 1]], uv[idx[:, 2]]
            uva = 0.5 * np.abs((ub[:, 0] - ua[:, 0]) * (uc[:, 1] - ua[:, 1]) - (uc[:, 0] - ua[:, 0]) * (ub[:, 1] - ua[:, 1]))
            ok = area > 1e-5
            if ok.any():
                texel.append((np.sqrt(uva[ok] / area[ok]), area[ok]))
            vol = float(np.sum(np.einsum("ij,ij->i", a, np.cross(b, c))) / 6.0)
            solid_vol[p["mat"]] = solid_vol.get(p["mat"], 0.0) + vol
    n_dup = sum(v - 1 for v in tri_keys.values() if v > 1)
    res["stats"].update(zero_area=n_zero, degenerate=n_degen, loose_verts=n_loose, duplicate_tris=n_dup,
                        winding_vs_normal_disagree=n_flip, bad_normals=n_badnrm)
    if n_zero:
        E.append("%d zero-area triangles" % n_zero)
    if n_degen:
        E.append("%d degenerate triangles" % n_degen)
    if n_loose:
        E.append("%d loose vertices" % n_loose)
    if n_dup:
        E.append("%d duplicate/coincident triangles" % n_dup)
    if n_badnrm:
        E.append("%d non-unit normals" % n_badnrm)
    if n_flip > 0.002 * tris + 2:
        E.append("%d triangles whose winding disagrees with their normals" % n_flip)
    elif n_flip:
        W.append("%d triangles whose winding disagrees with their normals" % n_flip)
    if texel:
        d = np.concatenate([t[0] for t in texel])
        w = np.concatenate([t[1] for t in texel])
        order = np.argsort(d)
        cum = np.cumsum(w[order])
        med = float(d[order][np.searchsorted(cum, cum[-1] * 0.5)])
        res["stats"]["texel_uv_per_m"] = round(med, 3)
        if not (0.5 <= med <= 2.0):
            E.append("solid texel density %.2f UV/m (expected ~1)" % med)
    for m, v in solid_vol.items():
        res["stats"].setdefault("signed_volume", {})[m] = round(v, 3)
    total_solid = sum(solid_vol.values())
    if solid_vol and total_solid < 0:
        E.append("solid shells have negative signed volume (inward normals)")
    if strict and W:
        E.extend("strict: " + w for w in W)
    return res


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    strict = "--strict" in argv
    names = [a for a in argv if not a.startswith("--")]
    man = json.load(open(os.path.join(KIT, "manifest_nature.json")))
    entries = [e for e in man["assets"] if not names or e["name"] in names]
    results = []
    fails = 0
    for e in entries:
        r = check_asset(e, strict)
        results.append(r)
        s = r["stats"]
        status = "FAIL" if r["errors"] else ("warn" if r["warnings"] else "ok")
        fails += bool(r["errors"])
        print("%-4s %-26s %6s tris  h=%6.2f  min_z=%6.2f  %s" % (
            status, r["name"], s.get("tris", "-"), s.get("height", 0), (s.get("min") or [0, 0, 0])[2],
            "; ".join(r["errors"] + r["warnings"])))
    out = dict(generator="Tools/kit/qa_nature_kit.py", checked=len(results), failed=fails,
               passed=len(results) - fails, results=results)
    if not names:
        with open(os.path.join(KIT, "QA_nature.json"), "w") as f:
            json.dump(out, f, indent=1)
    print("\n%d assets checked, %d failed" % (len(results), fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
