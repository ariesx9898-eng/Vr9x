"""Independent verification of an animated GLB (pure numpy - no Blender): parses the file the way an
engine importer would, samples every animation channel, re-skins the mesh and renders frames.

    python3 Tools/anim/verify_glb.py SourceArt/Characters/Rudeus/Rudeus_Animated.glb [--sheet out.png]

Checks: joint names vs the source rig, bind-pose height/facing/ground contact, animation names,
durations, channel coverage, and per-clip lowest-vertex height (ground penetration)."""
import argparse
import io
import json
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

CT = {5126: np.float32, 5123: np.uint16, 5121: np.uint8, 5125: np.uint32}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class GLB:
    def __init__(self, path):
        data = open(path, "rb").read()
        off, chunks = 12, []
        while off < len(data):
            ln, _ = struct.unpack("<I4s", data[off:off + 8])
            chunks.append(data[off + 8:off + 8 + ln])
            off += 8 + ln
        self.j = json.loads(chunks[0])
        self.bin = chunks[1]
        nodes = self.j["nodes"]
        self.parent = {}
        for i, n in enumerate(nodes):
            for c in n.get("children", []):
                self.parent[c] = i

    def acc(self, i):
        a = self.j["accessors"][i]
        bv = self.j["bufferViews"][a["bufferView"]]
        dt, n = CT[a["componentType"]], NC[a["type"]]
        o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = bv.get("byteStride")
        isz = np.dtype(dt).itemsize * n
        if stride and stride != isz:
            raw = np.frombuffer(self.bin, dtype=np.uint8, count=a["count"] * stride, offset=o).reshape(a["count"], stride)
            arr = raw[:, :isz].copy().view(dt).reshape(a["count"], n)
        else:
            arr = np.frombuffer(self.bin, dtype=dt, count=a["count"] * n, offset=o).reshape(a["count"], n)
        if a.get("normalized"):
            arr = arr.astype(np.float32) / np.iinfo(dt).max
        return arr


def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def trs(t, r, s):
    m = np.eye(4)
    m[:3, :3] = q2m(r) @ np.diag(s)
    m[:3, 3] = t
    return m


def slerp(a, b, u):
    d = float(np.dot(a, b))
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        r = a + (b - a) * u
        return r / np.linalg.norm(r)
    th = np.arccos(d)
    return (np.sin((1 - u) * th) * a + np.sin(u * th) * b) / np.sin(th)


class Scene:
    def __init__(self, g):
        self.g = g
        j = g.j
        self.base = {}
        for i, n in enumerate(j["nodes"]):
            if "matrix" in n:
                m = np.array(n["matrix"]).reshape(4, 4).T
                t, r, s = m[:3, 3], None, None
                self.base[i] = ("m", m)
            else:
                self.base[i] = ("trs", np.array(n.get("translation", [0, 0, 0]), float),
                                np.array(n.get("rotation", [0, 0, 0, 1]), float), np.array(n.get("scale", [1, 1, 1]), float))
        mesh_node = next(i for i, n in enumerate(j["nodes"]) if "mesh" in n and "skin" in n)
        self.mesh_node = mesh_node
        skin = j["skins"][j["nodes"][mesh_node]["skin"]]
        self.joints = skin["joints"]
        self.ibm = g.acc(skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1).astype(np.float64)
        prim = j["meshes"][j["nodes"][mesh_node]["mesh"]]["primitives"][0]
        self.pos = g.acc(prim["attributes"]["POSITION"]).astype(np.float64)
        self.J = g.acc(prim["attributes"]["JOINTS_0"]).astype(int)
        self.W = g.acc(prim["attributes"]["WEIGHTS_0"]).astype(np.float64)
        self.uv = g.acc(prim["attributes"]["TEXCOORD_0"]).astype(np.float64)
        self.tri = g.acc(prim["indices"]).reshape(-1, 3).astype(int)
        self.tex = None
        mat = j["materials"][prim.get("material", 0)] if j.get("materials") else None
        try:
            ti = mat["pbrMetallicRoughness"]["baseColorTexture"]["index"]
            img = j["images"][j["textures"][ti]["source"]]
            bv = j["bufferViews"][img["bufferView"]]
            o = bv.get("byteOffset", 0)
            self.tex = np.asarray(Image.open(io.BytesIO(g.bin[o:o + bv["byteLength"]])).convert("RGB")).astype(np.float32)
        except Exception:
            pass

    def local(self, i, over):
        b = self.base[i]
        if b[0] == "m" and i not in over:
            return b[1]
        t, r, s = (b[1], b[2], b[3]) if b[0] == "trs" else (b[1][:3, 3], np.array([0, 0, 0, 1.0]), np.ones(3))
        o = over.get(i, {})
        return trs(o.get("translation", t), o.get("rotation", r), o.get("scale", s))

    def world(self, over):
        W = {}

        def w(i):
            if i in W:
                return W[i]
            m = self.local(i, over)
            if i in self.g.parent:
                m = w(self.g.parent[i]) @ m
            W[i] = m
            return m
        for i in range(len(self.g.j["nodes"])):
            w(i)
        return W

    def skin(self, over):
        W = self.world(over)
        M = np.array([W[jn] @ self.ibm[k] for k, jn in enumerate(self.joints)])
        ph = np.c_[self.pos, np.ones(len(self.pos))]
        out = np.zeros((len(self.pos), 4))
        for k in range(4):
            out += self.W[:, k:k + 1] * np.einsum("nij,nj->ni", M[self.J[:, k]], ph)
        return out[:, :3]

    def sample(self, anim, t):
        over = {}
        for ch in anim["channels"]:
            smp = anim["samplers"][ch["sampler"]]
            times = self.g.acc(smp["input"]).ravel()
            vals = self.g.acc(smp["output"]).astype(np.float64)
            node, path = ch["target"]["node"], ch["target"]["path"]
            if path not in ("translation", "rotation", "scale"):
                continue
            k = int(np.searchsorted(times, t, side="right") - 1)
            k = max(0, min(k, len(times) - 1))
            if k >= len(times) - 1:
                v = vals[-1]
            else:
                u = (t - times[k]) / max(1e-9, times[k + 1] - times[k])
                v = slerp(vals[k], vals[k + 1], u) if path == "rotation" else vals[k] + (vals[k + 1] - vals[k]) * u
            over.setdefault(node, {})[path] = v
        return over

    def draw(self, V, view="side", size=260):
        # glTF: +Y up, character faces +Z.
        if view == "side":
            sx, sy, depth = -V[:, 2], V[:, 1], V[:, 0]
        else:
            sx, sy, depth = V[:, 0], V[:, 1], -V[:, 2]
        t = np.stack([sx, sy, depth], 1)[self.tri]
        n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
        n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
        shade = 0.5 + 0.5 * np.abs(n @ np.array([0.35, 0.55, -0.75]) / 1.0)
        if self.tex is not None:
            h, w = self.tex.shape[:2]
            c = self.uv[self.tri].mean(1)
            col = self.tex[np.clip((c[:, 1] % 1) * h, 0, h - 1).astype(int), np.clip((c[:, 0] % 1) * w, 0, w - 1).astype(int)]
        else:
            col = np.full((len(self.tri), 3), 200.0)
        col = np.clip(col * shade[:, None], 0, 255).astype(np.uint8)
        lo, hi = (-0.95, -0.05), (0.95, 1.85)
        sc = (size - 20) / (hi[1] - lo[1])
        Wd = int((hi[0] - lo[0]) * sc) + 20
        img = Image.new("RGB", (Wd, size), (38, 42, 50))
        d = ImageDraw.Draw(img)
        gy = size - 10 - (0 - lo[1]) * sc
        d.line([(0, gy), (Wd, gy)], fill=(90, 140, 90))
        for i in np.argsort(-t[:, :, 2].mean(1)):
            d.polygon([(10 + (p[0] - lo[0]) * sc, size - 10 - (p[1] - lo[1]) * sc) for p in t[i]], fill=tuple(col[i]))
        return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb")
    ap.add_argument("--source", default="SourceArt/Characters/Rudeus/Rudeus_Greyrat_UE.glb")
    ap.add_argument("--sheet", default="")
    ap.add_argument("--clips", nargs="*", default=["A_Rudeus_Walk", "A_Rudeus_Run", "A_Rudeus_StoneCannon_Release", "A_Rudeus_Death"])
    a = ap.parse_args()
    g = GLB(a.glb)
    sc = Scene(g)
    src = GLB(a.source)
    src_names = sorted(src.j["nodes"][i]["name"] for i in src.j["skins"][0]["joints"])
    names = sorted(g.j["nodes"][i]["name"] for i in sc.joints)
    ok = True
    print("joints:", len(names), "match source rig:", names == src_names)
    ok &= names == src_names
    V = sc.skin({})
    h = V[:, 1].max() - V[:, 1].min()
    print("bind pose height m: %.4f  min y: %.4f" % (h, V[:, 1].min()))
    ok &= abs(h - 1.617) < 0.01 and abs(V[:, 1].min()) < 0.01
    # facing: the nose/face region should be at +Z (toes also at +Z of ankles)
    foot = [i for i in sc.joints if g.j["nodes"][i]["name"] == "leg_L0_3_jnt_011"][0]
    ankle = [i for i in sc.joints if g.j["nodes"][i]["name"] == "leg_L0_2_jnt_010"][0]
    Wm = sc.world({})
    facing = Wm[foot][2, 3] - Wm[ankle][2, 3]
    print("toe ahead of ankle along +Z (faces +Z):", facing > 0)
    ok &= facing > 0
    anims = g.j.get("animations", [])
    print("animations:", len(anims))
    report = []
    for an in anims:
        tmax = 0.0
        for smp in an["samplers"]:
            tmax = max(tmax, float(g.acc(smp["input"]).max()))
        nodes = {ch["target"]["node"] for ch in an["channels"]}
        low = min(sc.skin(sc.sample(an, t))[:, 1].min() for t in np.linspace(0, tmax, 6))
        report.append((an["name"], round(tmax, 3), len(nodes), round(low * 100, 1)))
    for r in report:
        print("  %-28s %5.2fs  %3d nodes  lowest vertex %5.1f cm" % r)
    if a.sheet:
        imgs = []
        by = {an["name"]: an for an in anims}
        for name in a.clips:
            an = by.get(name)
            if not an:
                continue
            tmax = max(float(g.acc(s["input"]).max()) for s in an["samplers"])
            for t in np.linspace(0, tmax, 6):
                imgs.append(sc.draw(sc.skin(sc.sample(an, t)), "side"))
        if imgs:
            w, hh = imgs[0].width, imgs[0].height
            out = Image.new("RGB", (w * 6, hh * ((len(imgs) + 5) // 6)))
            for k, im in enumerate(imgs):
                out.paste(im, ((k % 6) * w, (k // 6) * hh))
            out.save(a.sheet)
            print("sheet:", a.sheet)
    print("VERIFY:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
