"""Debug helpers: run the QA coplanar-overlap test directly on a Geo and name the parts involved."""
import numpy as np

import qa_architecture_kit as QA


def geo_triangles(g):
    tris = []
    fpart = []
    fmat = []
    part_of_face = {}
    for pi, (v0, v1, f0, f1, cl) in enumerate(g.parts):
        for f in range(f0, f1):
            part_of_face[f] = pi
    for fi, f in enumerate(g.F):
        for k in range(1, len(f) - 1):
            tris.append((f[0], f[k], f[k + 1]))
            fpart.append(part_of_face.get(fi, -1))
            fmat.append(g.FM[fi])
    return np.array(tris), fpart, fmat


def overlaps(g, limit=30):
    V = np.array(g.V)
    I, fpart, fmat = geo_triangles(g)
    T = V[I]
    q = np.round(V / 0.001).astype(np.int64)
    _, inv = np.unique(q, axis=0, return_inverse=True)
    keys = inv.reshape(-1)[I]
    same, b2b, ex = QA.coplanar_overlaps(T, keys, max_report=100000)
    print(f"{g.name}: same-facing {same}, back-to-back {b2b}")
    pairs = {}
    for e in ex:
        a, b = e["tri"]
        pa, pb = fpart[a], fpart[b]
        key = (min(pa, pb), max(pa, pb), e["same_facing"])
        pairs.setdefault(key, []).append(e)
    for (pa, pb, sf), lst in sorted(pairs.items(), key=lambda kv: -len(kv[1]))[:limit]:
        def desc(p):
            v0, v1, f0, f1, cl = g.parts[p]
            P = V[v0:v1]
            return f"part{p}[{g.FM[f0]}] {np.round(P.min(0), 2).tolist()}..{np.round(P.max(0), 2).tolist()}"
        e = lst[0]
        n = (T[e['tri'][0]][1] - T[e['tri'][0]][0])
        nn = np.cross(T[e['tri'][0]][1] - T[e['tri'][0]][0], T[e['tri'][0]][2] - T[e['tri'][0]][0])
        nn = nn / (np.linalg.norm(nn) + 1e-12)
        print(f"  {'SAME' if sf else 'B2B '} x{len(lst):3d} n={np.round(nn, 2).tolist()} at {e['at']}\n"
              f"      {desc(pa)}\n      {desc(pb)}")
    return same, b2b
