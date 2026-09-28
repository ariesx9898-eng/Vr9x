#!/usr/bin/env python3
"""QA for the LA PLACE architecture / VFX kit GLBs (numpy only, no Blender needed).

Checks every GLB listed in SourceArt/Kit/manifest_architecture.json (or every GLB under SourceArt/Kit with --all):
  * parse + node transforms are identity (transforms applied)
  * triangle count and budget, bounds (Z-up metres), pivot (base centre on the ground / declared centre pivot)
  * material slot names are in the Spec palette (section 6) or 'VFX'
  * zero-area triangles, duplicate triangles (same 3 corners within 1 mm, any winding), loose vertices
  * coplanar overlapping triangles (same plane within 1 mm, overlap > 1 cm^2) = z-fighting risk, split into
    same-facing (visible z-fight) and back-to-back (hidden) overlaps; both count as failures
  * closed shells with inward normals (negative signed volume)
Writes SourceArt/Kit/QA_architecture.json and prints a summary. Exit code 1 if anything fails.

Usage: python Tools/kit/qa_architecture_kit.py [--all] [--only SUBSTR] [--verbose]
"""
import argparse
import json
import os
import struct
import sys
from collections import defaultdict

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
KIT = os.path.join(ROOT, "SourceArt", "Kit")
sys.path.insert(0, os.path.dirname(__file__))
from arch_palette import VALID_SLOTS  # noqa: E402

CT = {5126: np.float32, 5123: np.uint16, 5121: np.uint8, 5125: np.uint32, 5122: np.int16, 5120: np.int8}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def load_glb(path):
    data = open(path, "rb").read()
    magic, ver, length = struct.unpack("<4sII", data[:12])
    if magic != b"glTF":
        raise ValueError("not a GLB")
    off = 12
    js = None
    binc = b""
    while off < len(data):
        ln, typ = struct.unpack("<I4s", data[off:off + 8])
        chunk = data[off + 8:off + 8 + ln]
        if typ == b"JSON":
            js = json.loads(chunk)
        elif typ == b"BIN\x00":
            binc = chunk
        off += 8 + ln
    return js, binc


def accessor(js, binc, i):
    a = js["accessors"][i]
    bv = js["bufferViews"][a["bufferView"]]
    dt = CT[a["componentType"]]
    n = NC[a["type"]]
    o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    stride = bv.get("byteStride")
    isz = np.dtype(dt).itemsize
    if stride and stride != isz * n:
        raw = np.frombuffer(binc, dtype=np.uint8, count=a["count"] * stride, offset=o).reshape(a["count"], stride)
        arr = raw[:, :isz * n].copy().view(dt).reshape(a["count"], n)
    else:
        arr = np.frombuffer(binc, dtype=dt, count=a["count"] * n, offset=o).reshape(a["count"], n)
    return arr


def mesh_data(path):
    js, binc = load_glb(path)
    info = {"nodes": [], "prims": []}
    for n in js.get("nodes", []):
        info["nodes"].append({k: n.get(k) for k in ("name", "translation", "rotation", "scale", "matrix", "mesh")})
    mats = [m.get("name", f"mat{i}") for i, m in enumerate(js.get("materials", []))]
    for ni, node in enumerate(js.get("nodes", [])):
        if "mesh" not in node:
            continue
        for pr in js["meshes"][node["mesh"]]["primitives"]:
            if pr.get("mode", 4) != 4:
                raise ValueError("non-triangle primitive")
            pos = accessor(js, binc, pr["attributes"]["POSITION"]).astype(np.float64)
            idx = accessor(js, binc, pr["indices"]).astype(np.int64).reshape(-1, 3)
            uv = accessor(js, binc, pr["attributes"]["TEXCOORD_0"]) if "TEXCOORD_0" in pr["attributes"] else None
            nrm = accessor(js, binc, pr["attributes"]["NORMAL"]) if "NORMAL" in pr["attributes"] else None
            mat = mats[pr["material"]] if "material" in pr else None
            # glTF Y-up -> Blender Z-up: (x, y, z)_b = (x, -z, y)_g
            P = np.stack([pos[:, 0], -pos[:, 2], pos[:, 1]], 1)
            info["prims"].append({"pos": P, "idx": idx, "mat": mat, "uv": uv, "nrm": nrm})
    return info


def tri_overlap_area(a, b):
    """Area of the intersection of two 2D triangles (Sutherland-Hodgman)."""
    def clip(poly, p, q):
        out = []
        ex, ey = q[0] - p[0], q[1] - p[1]
        def side(r):
            return ex * (r[1] - p[1]) - ey * (r[0] - p[0])
        n = len(poly)
        for i in range(n):
            cur = poly[i]
            prv = poly[i - 1]
            sc = side(cur)
            sp = side(prv)
            if sc >= 0:
                if sp < 0:
                    t = sp / (sp - sc)
                    out.append((prv[0] + (cur[0] - prv[0]) * t, prv[1] + (cur[1] - prv[1]) * t))
                out.append(cur)
            elif sp >= 0:
                t = sp / (sp - sc)
                out.append((prv[0] + (cur[0] - prv[0]) * t, prv[1] + (cur[1] - prv[1]) * t))
        return out

    def area(p):
        s = 0.0
        for i in range(len(p)):
            s += p[i - 1][0] * p[i][1] - p[i][0] * p[i - 1][1]
        return s / 2

    if area(b) < 0:
        b = [b[0], b[2], b[1]]
    poly = list(a)
    for i in range(3):
        if not poly:
            return 0.0
        poly = clip(poly, b[i], b[(i + 1) % 3])
    return abs(area(poly)) if len(poly) >= 3 else 0.0


def coplanar_overlaps(T, keys, min_area=1e-4, dist_tol=1e-3, max_report=20):
    """T: (n,3,3) triangles. keys: (n,3) integer vertex ids (welded at 1 mm).
    Returns (same_facing_pairs, back_to_back_pairs, examples)."""
    e1 = T[:, 1] - T[:, 0]
    e2 = T[:, 2] - T[:, 0]
    N = np.cross(e1, e2)
    L = np.linalg.norm(N, axis=1)
    ok = L > 1e-9
    N[ok] /= L[ok, None]
    # canonical direction
    s = np.ones(len(N))
    flip = (N[:, 0] < -1e-6) | ((np.abs(N[:, 0]) <= 1e-6) & (N[:, 1] < -1e-6)) | \
           ((np.abs(N[:, 0]) <= 1e-6) & (np.abs(N[:, 1]) <= 1e-6) & (N[:, 2] < 0))
    s[flip] = -1
    Nc = N * s[:, None]
    D = np.einsum("ij,ij->i", Nc, T[:, 0])
    qn = np.round(Nc * 150).astype(np.int64)
    qd = np.floor(D / 0.004).astype(np.int64)
    buckets = defaultdict(list)
    for i in np.nonzero(ok)[0]:
        buckets[(qn[i, 0], qn[i, 1], qn[i, 2], qd[i])].append(i)
    same = 0
    b2b = 0
    examples = []
    seen = set()
    for key, lst in buckets.items():
        cand = list(lst)
        nb = buckets.get((key[0], key[1], key[2], key[3] + 1))
        if nb:
            cand = cand + nb
        if len(cand) < 2:
            continue
        idx = np.array(cand)
        n0 = Nc[idx[0]]
        # 2D basis
        ref = np.array([0, 0, 1.0]) if abs(n0[2]) < 0.9 else np.array([1.0, 0, 0])
        bu = np.cross(ref, n0)
        bu /= np.linalg.norm(bu)
        bv = np.cross(n0, bu)
        P2 = np.stack([T[idx] @ bu, T[idx] @ bv], -1)  # (k,3,2)
        mn = P2.min(1)
        mx = P2.max(1)
        k = len(idx)
        ov = (mn[:, None, 0] < mx[None, :, 0] - 1e-4) & (mx[:, None, 0] > mn[None, :, 0] + 1e-4) & \
             (mn[:, None, 1] < mx[None, :, 1] - 1e-4) & (mx[:, None, 1] > mn[None, :, 1] + 1e-4)
        ov &= np.abs(D[idx][:, None] - D[idx][None, :]) < dist_tol
        ov &= (Nc[idx] @ Nc[idx].T) > 0.9999
        ii, jj = np.nonzero(np.triu(ov, 1))
        for a, b in zip(ii, jj):
            ta, tb = idx[a], idx[b]
            pair = (min(ta, tb), max(ta, tb))
            if pair in seen:
                continue
            seen.add(pair)
            if len(set(keys[ta]) & set(keys[tb])) >= 2:
                continue
            ar = tri_overlap_area([tuple(p) for p in P2[a]], [tuple(p) for p in P2[b]])
            if ar > min_area:
                facing = s[ta] * s[tb] > 0
                if facing:
                    same += 1
                else:
                    b2b += 1
                if len(examples) < max_report:
                    c = T[ta].mean(0)
                    examples.append({"tri": [int(ta), int(tb)], "area_m2": round(float(ar), 5),
                                     "same_facing": bool(facing), "at": [round(float(x), 3) for x in c]})
    return same, b2b, examples


def shells(keys, T):
    """Connected components over shared vertex ids; returns list of (tri indices, closed?, signed volume)."""
    n = len(keys)
    parent = list(range(int(keys.max()) + 1)) if n else []

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b, c in keys:
        ra, rb, rc = find(a), find(b), find(c)
        if ra != rb:
            parent[rb] = ra
        rc = find(c)
        ra = find(a)
        if ra != rc:
            parent[rc] = ra
    comp = defaultdict(list)
    for t, (a, b, c) in enumerate(keys):
        comp[find(a)].append(t)
    out = []
    for root, tris in comp.items():
        edges = defaultdict(int)
        for t in tris:
            a, b, c = keys[t]
            for u, v in ((a, b), (b, c), (c, a)):
                edges[(u, v)] += 1
        closed = all(edges.get((v, u), 0) == cnt for (u, v), cnt in edges.items())
        tt = T[tris]
        vol = float(np.einsum("ij,ij->i", tt[:, 0], np.cross(tt[:, 1], tt[:, 2])).sum() / 6.0)
        out.append((tris, closed, vol))
    return out


def check_asset(path, meta, verbose=False):
    res = {"file": os.path.relpath(path, ROOT), "errors": [], "warnings": [], "info": {}}
    try:
        md = mesh_data(path)
    except Exception as e:  # noqa: BLE001
        res["errors"].append(f"parse failed: {e}")
        return res
    for n in md["nodes"]:
        t = n.get("translation") or [0, 0, 0]
        r = n.get("rotation") or [0, 0, 0, 1]
        sc = n.get("scale") or [1, 1, 1]
        if n.get("matrix") or max(abs(x) for x in t) > 1e-5 or max(abs(a - b) for a, b in zip(r, [0, 0, 0, 1])) > 1e-5 \
                or max(abs(x - 1) for x in sc) > 1e-5:
            res["errors"].append(f"node {n.get('name')} has a non-identity transform (transforms not applied)")
    if not md["prims"]:
        res["errors"].append("no mesh data")
        return res
    allP = []
    allT = []
    base = 0
    mats = []
    loose = 0
    zero = 0
    uvbad = 0
    for pr in md["prims"]:
        P, I = pr["pos"], pr["idx"]
        used = np.zeros(len(P), bool)
        used[I.ravel()] = True
        loose += int((~used).sum())
        mats.append(pr["mat"])
        if pr["mat"] not in VALID_SLOTS:
            res["errors"].append(f"material slot {pr['mat']!r} not in the Spec palette")
        if pr["uv"] is None:
            res["errors"].append(f"primitive {pr['mat']} has no UVs")
        elif not np.isfinite(pr["uv"]).all():
            uvbad += 1
        allP.append(P)
        allT.append(I + base)
        base += len(P)
    P = np.concatenate(allP)
    I = np.concatenate(allT)
    T = P[I]
    ntri = len(I)
    area = 0.5 * np.linalg.norm(np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]), axis=1)
    zero = int((area < 1e-8).sum())
    lo = P.min(0)
    hi = P.max(0)
    size = hi - lo
    res["info"].update({"tris": ntri, "verts": int(len(P)), "materials": sorted(set(m for m in mats if m)),
                        "bbox_min": [round(float(x), 3) for x in lo], "bbox_max": [round(float(x), 3) for x in hi],
                        "size": [round(float(x), 3) for x in size]})
    if loose:
        res["errors"].append(f"{loose} loose (unreferenced) vertices")
    if zero:
        res["errors"].append(f"{zero} zero-area triangles (< 1e-8 m^2)")
    if uvbad:
        res["errors"].append("non-finite UVs")
    # weld at 1 mm for topology checks
    q = np.round(P / 0.001).astype(np.int64)
    _, inv = np.unique(q, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    keys = inv[I]
    sk = np.sort(keys, axis=1)
    _, cnt = np.unique(sk, axis=0, return_counts=True)
    dup = int((cnt > 1).sum())
    if dup:
        res["errors"].append(f"{dup} duplicate triangles (coincident within 1 mm)")
    same, b2b, ex = coplanar_overlaps(T, keys)
    res["info"]["coplanar_same"] = same
    res["info"]["coplanar_back_to_back"] = b2b
    if same:
        res["errors"].append(f"{same} coplanar overlapping same-facing triangle pairs (z-fighting)")
    if b2b:
        res["errors"].append(f"{b2b} coplanar overlapping back-to-back triangle pairs (hidden coincident faces)")
    if ex and verbose:
        res["info"]["coplanar_examples"] = ex
    # consistent winding: a directed edge used by two triangles means one of them is flipped
    de = np.concatenate([keys[:, [0, 1]], keys[:, [1, 2]], keys[:, [2, 0]]])
    de = de[de[:, 0] != de[:, 1]]
    _, dcnt = np.unique(de, axis=0, return_counts=True)
    flipped = int((dcnt > 1).sum())
    res["info"]["repeated_directed_edges"] = flipped
    if flipped:
        res["errors"].append(f"{flipped} directed edges shared by two triangles (inconsistent winding / flipped faces)")
    sh = shells(keys, T)
    closed = [s for s in sh if s[1]]
    inverted = [s for s in closed if s[2] < -1e-6]
    res["info"]["shells"] = len(sh)
    res["info"]["closed_shells"] = len(closed)
    if inverted:
        res["errors"].append(f"{len(inverted)} closed shells with inward normals")
    # pivot / budget from the manifest
    if meta:
        pv = meta.get("pivot", "base")
        fnd = float(meta.get("foundation", 0.0))
        cx = (lo[0] + hi[0]) / 2
        cy = (lo[1] + hi[1]) / 2
        if pv in ("base", "base_point"):
            if lo[2] > 0.01:
                res["errors"].append(f"asset floats: min z = {lo[2]:.3f} > 0")
            if lo[2] < -fnd - 0.01:
                res["errors"].append(f"min z = {lo[2]:.3f} below the declared foundation depth -{fnd}")
            tol = max(0.05, 0.01 * max(size[0], size[1]))
            if pv == "base" and (abs(cx) > tol or abs(cy) > tol):
                res["errors"].append(f"pivot not at the footprint centre: bbox centre ({cx:.3f}, {cy:.3f})")
        elif pv == "center":
            cz = (lo[2] + hi[2]) / 2
            tol = max(0.02, 0.02 * float(np.max(size)))
            if max(abs(cx), abs(cy), abs(cz)) > tol:
                res["errors"].append(f"centre pivot expected, bbox centre ({cx:.3f}, {cy:.3f}, {cz:.3f})")
        b = meta.get("budget")
        if b:
            if ntri > b[1]:
                res["errors"].append(f"{ntri} tris over budget {b[1]}")
            elif ntri < b[0]:
                res["warnings"].append(f"{ntri} tris under the expected minimum {b[0]}")
        declared = set(meta.get("materials", []))
        if declared and declared != set(res["info"]["materials"]):
            res["errors"].append(f"manifest materials {sorted(declared)} != GLB {res['info']['materials']}")
        if meta.get("tris") and meta["tris"] != ntri:
            res["errors"].append(f"manifest tris {meta['tris']} != GLB {ntri}")
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true", help="check every GLB under SourceArt/Kit (not just the manifest)")
    ap.add_argument("--only", default=None)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--out", default=os.path.join(KIT, "QA_architecture.json"))
    a = ap.parse_args()
    man_path = os.path.join(KIT, "manifest_architecture.json")
    manifest = json.load(open(man_path)) if os.path.exists(man_path) else {"assets": []}
    metas = {m["name"]: m for m in manifest.get("assets", [])}
    files = []
    if a.all:
        for dp, _, fs in os.walk(KIT):
            for f in fs:
                if f.endswith(".glb"):
                    files.append(os.path.join(dp, f))
    else:
        for m in manifest.get("assets", []):
            files.append(os.path.join(ROOT, m["file"]))
    files.sort()
    report = {"assets": [], "summary": {}}
    nfail = 0
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        if a.only and a.only not in name:
            continue
        if not os.path.exists(f):
            report["assets"].append({"file": os.path.relpath(f, ROOT), "errors": ["missing file"], "warnings": [],
                                     "info": {}})
            nfail += 1
            continue
        r = check_asset(f, metas.get(name), a.verbose)
        r["name"] = name
        report["assets"].append(r)
        status = "FAIL" if r["errors"] else "ok"
        if r["errors"]:
            nfail += 1
        inf = r["info"]
        print(f"{status:4s} {name:34s} tris {inf.get('tris', 0):6d}  size {inf.get('size')}  "
              f"{'; '.join(r['errors'])}{' | ' + '; '.join(r['warnings']) if r['warnings'] else ''}")
        if a.verbose and inf.get("coplanar_examples"):
            for e in inf["coplanar_examples"][:8]:
                print("      ", e)
    report["summary"] = {"checked": len(report["assets"]), "failed": nfail}
    if not a.only:
        json.dump(report, open(a.out, "w"), indent=1)
    print(f"\n{len(report['assets'])} assets checked, {nfail} failed")
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
