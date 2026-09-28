"""Scatters the nature kit over the generated world (trees, bushes, rocks, cliff rocks and rock landmarks).

Reads the world generator's outputs (SourceArt/World: Height.r16, Regions.png, Density/*.png; Content/Data/World.json)
and the nature kit manifest (SourceArt/Kit/manifest_nature.json: regions, scatter kind, size range, footprint), and
writes an instance set for Content/Python/mt_build_world.py (stage "instances"):

  SourceArt/World/Scatter/Foliage.json   {"Meshes": [UE paths], "Groups": [{Label, Offset, Count, CullStart, CullEnd,
                                          Collision, SpatiallyLoaded, CastShadow, Tags, HLODLayer}]}
  SourceArt/World/Scatter/Foliage.bin    float32 records (MeshIndex, X, Y, Z cm, Yaw, Pitch, Roll deg, Scale)

Grass, flowers, wheat, ferns and reeds are not scattered here: the landscape material spawns them near the camera
(landscape grass), which is far cheaper than millions of placed instances. Deterministic (fixed seed).

    ~/.venvs/mushoku-bpy311/bin/python Tools/world/scatter_foliage.py [--preview]
"""
import argparse
import json
import math
import os
import time

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WORLD_DIR = os.path.join(ROOT, "SourceArt", "World")
OUT_DIR = os.path.join(WORLD_DIR, "Scatter")
SEED = 20260928
TILE_CM = 102400.0  # streaming tiles: 1024 m

# Per scatter kind: instances per 12 m density cell at density 255, largest slope (rise / run), cull distances (cm),
# collision, shadows, and the minimum spacing factor (x the mean footprint of the two instances).
KINDS = {
    "Trees": dict(per_cell=0.85, max_slope=0.75, cull=(180000, 240000), collision=True, shadow=True, spacing=0.42),
    "Bushes": dict(per_cell=0.9, max_slope=0.9, cull=(35000, 45000), collision=False, shadow=True, spacing=0.55),
    "Rocks": dict(per_cell=0.45, max_slope=1.6, cull=(60000, 90000), collision=True, shadow=True, spacing=0.6),
}
# Kit categories drawn by the landscape grass system instead.
GRASS_KINDS = {"Grass", "Flowers"}


def load_world():
    with open(os.path.join(ROOT, "Content", "Data", "World.json"), "r", encoding="utf-8") as f:
        return json.load(f)


def load_manifest():
    with open(os.path.join(ROOT, "SourceArt", "Kit", "manifest_nature.json"), "r", encoding="utf-8") as f:
        data = json.load(f)
    return data["assets"] if isinstance(data, dict) else data


class Terrain:
    def __init__(self, world):
        ls = world["Landscape"]
        self.nx, self.ny = ls["VerticesX"], ls["VerticesY"]
        self.quad = ls["QuadSizeCm"]
        self.x0, self.y0 = ls["LocationCm"]["X"], ls["LocationCm"]["Y"]
        self.z0, self.zs = ls["LocationCm"]["Z"], ls["ScaleCm"]["Z"]
        raw = np.fromfile(os.path.join(WORLD_DIR, "Height.r16"), dtype="<u2").reshape(self.ny, self.nx)
        self.h = (self.z0 + (raw.astype(np.float32) - 32768.0) * self.zs / 128.0)  # cm
        self.width = (self.nx - 1) * self.quad
        self.height = (self.ny - 1) * self.quad

    def z(self, x, y):
        u = np.clip((x - self.x0) / self.quad, 0, self.nx - 1.001)
        v = np.clip((y - self.y0) / self.quad, 0, self.ny - 1.001)
        i, j = np.floor(u).astype(np.int64), np.floor(v).astype(np.int64)
        fu, fv = u - i, v - j
        h = self.h
        top = h[j, i] * (1 - fu) + h[j, i + 1] * fu
        bot = h[j + 1, i] * (1 - fu) + h[j + 1, i + 1] * fu
        return top * (1 - fv) + bot * fv

    def slope(self, x, y):
        d = self.quad
        gx = (self.z(x + d, y) - self.z(x - d, y)) / (2 * d)
        gy = (self.z(x, y + d) - self.z(x, y - d)) / (2 * d)
        return np.hypot(gx, gy), gx, gy


def load_map(name, terrain):
    im = np.asarray(Image.open(os.path.join(WORLD_DIR, name)).convert("L"), dtype=np.uint8)
    return im


def sample_map(im, terrain, x, y):
    h, w = im.shape
    px = np.clip(((x - terrain.x0) / terrain.width * w).astype(np.int64), 0, w - 1)
    py = np.clip(((y - terrain.y0) / terrain.height * h).astype(np.int64), 0, h - 1)
    return im[py, px]


class SpacingGrid:
    """Hash grid of placed discs (x, y, r) for minimum-spacing rejection."""

    def __init__(self, cell=1500.0):
        self.cell = cell
        self.cells = {}

    def _key(self, x, y):
        return int(math.floor(x / self.cell)), int(math.floor(y / self.cell))

    def free(self, x, y, r):
        cx, cy = self._key(x, y)
        reach = int(math.ceil((r + 2500.0) / self.cell))
        for gx in range(cx - reach, cx + reach + 1):
            for gy in range(cy - reach, cy + reach + 1):
                for (ox, oy, orad) in self.cells.get((gx, gy), ()):
                    if (ox - x) ** 2 + (oy - y) ** 2 < (r + orad) ** 2:
                        return False
        return True

    def add(self, x, y, r):
        self.cells.setdefault(self._key(x, y), []).append((x, y, r))


def block_cities(grid, terrain):
    """Reserve every city instance (buildings, walls, props, landmarks) in the spacing grid, and return a street mask
    sampler, so nothing grows inside a house or on a street (Tools/world/generate_cities.py outputs)."""
    base = os.path.join(WORLD_DIR, "Scatter")
    js, bn = os.path.join(base, "Cities.json"), os.path.join(base, "Cities.bin")
    if not (os.path.exists(js) and os.path.exists(bn)):
        return None
    with open(js, "r", encoding="utf-8") as f:
        cities = json.load(f)
    with open(os.path.join(ROOT, "SourceArt", "Kit", "manifest.json"), "r", encoding="utf-8") as f:
        kit = json.load(f)
    rows = {a["name"]: a for a in (kit["assets"] if isinstance(kit, dict) else kit)}
    radius_of = []
    for path in cities["Meshes"]:
        a = rows.get(path.rsplit("/", 1)[-1], {})
        fp = a.get("footprint", [4.0, 4.0])
        radius_of.append(0.5 * math.hypot(fp[0], fp[1]) * 100.0 + 250.0)
    recs = np.fromfile(bn, dtype="<f4").reshape(-1, 8)
    for r in recs:
        grid.add(float(r[1]), float(r[2]), radius_of[int(r[0])] * float(r[7]))
    masks = []
    for name in ("StreetCobble.png", "StreetDirt.png"):
        path = os.path.join(WORLD_DIR, "Cities", name)
        if os.path.exists(path):
            masks.append(np.asarray(Image.open(path).convert("L"), dtype=np.uint8))
    street = np.maximum.reduce(masks) if masks else None
    print("cities: %d instances reserved, street mask %s" % (len(recs), "yes" if street is not None else "no"))
    return street


def on_street(street, terrain, x, y):
    if street is None:
        return np.zeros(x.shape, bool)
    i = np.clip(np.round((x - terrain.x0) / terrain.quad).astype(np.int64), 0, terrain.nx - 1)
    j = np.clip(np.round((y - terrain.y0) / terrain.quad).astype(np.int64), 0, terrain.ny - 1)
    return street[j, i] > 40


def species_table(assets):
    table = {}
    for a in assets:
        kind = a.get("scatter")
        if kind in KINDS:
            table.setdefault(kind, []).append(a)
    return table


def asset_scale(asset, rng):
    lo, hi = asset.get("size_range", [1.0, 1.0])
    target = rng.uniform(lo, hi)
    if asset.get("size_rule") == "max_extent":
        base = max(asset["footprint"][0], asset["footprint"][1], asset["height"])
    else:
        base = asset["height"]
    return target / max(base, 0.01)


def scatter_kind(kind, cfg, species, terrain, density, regions, rng, grid, records, mesh_index, street=None):
    h, w = density.shape
    cell_x = terrain.width / w
    cell_y = terrain.height / h
    lam = density.astype(np.float32) / 255.0 * cfg["per_cell"]
    counts = rng.poisson(lam)
    ys, xs = np.nonzero(counts)
    n = counts[ys, xs]
    xs = np.repeat(xs, n)
    ys = np.repeat(ys, n)
    x = terrain.x0 + (xs + rng.random(xs.size)) * cell_x
    y = terrain.y0 + (ys + rng.random(ys.size)) * cell_y
    z = terrain.z(x, y)
    slope, gx, gy = terrain.slope(x, y)
    reg = sample_map(regions, terrain, x, y)
    keep = (z > 150.0) & (slope < cfg["max_slope"]) & (reg > 0) & ~on_street(street, terrain, x, y)
    x, y, z, slope, gx, gy, reg = x[keep], y[keep], z[keep], slope[keep], gx[keep], gy[keep], reg[keep]
    order = rng.permutation(x.size)
    by_region = {}
    for a in species:
        for r in a.get("regions", []):
            by_region.setdefault(r, []).append(a)
    placed = 0
    for k in order:
        choices = by_region.get(int(reg[k]))
        if not choices:
            continue
        # Big trees are rarer: weight species by 1 / footprint so giants do not crowd everything out.
        weights = np.array([1.0 / max(c["footprint"][0] * c["footprint"][1], 1.0) ** 0.35 for c in choices])
        a = choices[rng.choice(len(choices), p=weights / weights.sum())]
        scale = asset_scale(a, rng)
        radius = 0.5 * (a["footprint"][0] + a["footprint"][1]) * 0.5 * scale * 100.0 * cfg["spacing"]
        if not grid.free(x[k], y[k], radius):
            continue
        grid.add(x[k], y[k], radius)
        yaw = rng.uniform(0, 360)
        if kind == "Rocks":
            # Rocks settle into the slope and sink a little.
            pitch = math.degrees(math.atan(-gx[k])) * 0.6 + rng.normal(0, 4)
            roll = math.degrees(math.atan(gy[k])) * 0.6 + rng.normal(0, 4)
            zz = z[k] - 0.12 * scale * 100.0 * min(a["footprint"]) * 0.5
        else:
            pitch, roll = rng.normal(0, 1.5), rng.normal(0, 1.5)
            zz = z[k] - 5.0 - slope[k] * 30.0
        records.append((mesh_index[a["name"]], x[k], y[k], zz, yaw, pitch, roll, scale))
        placed += 1
    return placed


def scatter_cliffs(assets, terrain, regions, rng, grid, records, mesh_index):
    """Cliff rocks on steep ground in mountain regions; arches / pillars as rare landmarks."""
    cliffs = [a for a in assets if a.get("scatter") == "Cliffs"]
    marks = [a for a in assets if a.get("scatter") == "Landmark"]
    n = 60000
    x = terrain.x0 + rng.random(n) * terrain.width
    y = terrain.y0 + rng.random(n) * terrain.height
    z = terrain.z(x, y)
    slope, gx, gy = terrain.slope(x, y)
    reg = sample_map(regions, terrain, x, y)
    placed = 0
    for k in range(n):
        if z[k] < 300:
            continue
        r = int(reg[k])
        if slope[k] > 0.85 and cliffs:
            choices = [a for a in cliffs if r in a.get("regions", [])]
            if not choices or rng.random() > 0.35:
                continue
            a = choices[rng.integers(len(choices))]
        elif slope[k] < 0.25 and marks and rng.random() < 0.02:
            choices = [a for a in marks if r in a.get("regions", [])]
            if not choices:
                continue
            a = choices[rng.integers(len(choices))]
        else:
            continue
        scale = asset_scale(a, rng)
        radius = 0.5 * (a["footprint"][0] + a["footprint"][1]) * 0.5 * scale * 100.0 * 0.7
        if not grid.free(x[k], y[k], radius):
            continue
        grid.add(x[k], y[k], radius)
        # Face down-slope and sink into it so the rock reads as part of the mountain.
        yaw = math.degrees(math.atan2(-gy[k], -gx[k])) + rng.normal(0, 25)
        records.append((mesh_index[a["name"]], x[k], y[k], z[k] - scale * 100.0 * a["height"] * 0.18, yaw, rng.normal(0, 6), rng.normal(0, 6), scale))
        placed += 1
    return placed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview", action="store_true", help="also write Scatter/_Preview.png")
    args = parser.parse_args()
    t0 = time.time()
    rng = np.random.default_rng(SEED)
    world = load_world()
    assets = load_manifest()
    terrain = Terrain(world)
    regions = load_map("Regions.png", terrain)
    table = species_table(assets)
    used = [a for a in assets if a.get("scatter") in KINDS or a.get("scatter") in ("Cliffs", "Landmark")]
    mesh_index = {a["name"]: i for i, a in enumerate(used)}
    meshes = ["/Game/LaPlace/Kit/%s/%s" % (a["category"], a["name"]) for a in used]

    groups_out = []
    all_records = []
    grid = SpacingGrid()
    street = block_cities(grid, terrain)
    kind_records = {}
    for kind in ("Trees", "Rocks", "Bushes"):
        density = load_map(os.path.join("Density", kind + ".png"), terrain)
        recs = []
        placed = scatter_kind(kind, KINDS[kind], table.get(kind, []), terrain, density, regions, rng, grid, recs, mesh_index, street)
        kind_records[kind] = recs
        print("%-7s %8d instances (%.1f s)" % (kind, placed, time.time() - t0))
    recs = []
    placed = scatter_cliffs(assets, terrain, regions, rng, grid, recs, mesh_index)
    kind_records["Cliffs"] = recs
    print("Cliffs  %8d instances" % placed)

    cfg_for = dict(KINDS)
    cfg_for["Cliffs"] = dict(cull=(0, 0), collision=True, shadow=True)
    for kind, recs in kind_records.items():
        if not recs:
            continue
        arr = np.array(recs, dtype=np.float32)
        tx = np.floor((arr[:, 1] - terrain.x0) / TILE_CM).astype(np.int32)
        ty = np.floor((arr[:, 2] - terrain.y0) / TILE_CM).astype(np.int32)
        key = tx * 1000 + ty
        order = np.lexsort((arr[:, 0], key))
        arr, key, tx, ty = arr[order], key[order], tx[order], ty[order]
        starts = np.flatnonzero(np.r_[True, key[1:] != key[:-1]])
        ends = np.r_[starts[1:], key.size]
        cfg = cfg_for[kind]
        for s, e in zip(starts, ends):
            groups_out.append({
                "Label": "Nature_%s_X%02d_Y%02d" % (kind, tx[s], ty[s]),
                "Offset": sum(len(r) for r in all_records) + int(s),
                "Count": int(e - s),
                "CullStart": cfg["cull"][0], "CullEnd": cfg["cull"][1],
                "Collision": cfg["collision"], "SpatiallyLoaded": True, "CastShadow": cfg["shadow"],
                "Tags": ["MTNature", "MTNature_" + kind],
                "HLODLayer": "/Game/LaPlace/World/HLOD/HLOD_Nature.HLOD_Nature" if kind in ("Trees", "Cliffs") else "",
            })
        all_records.append(arr)
    data = np.concatenate(all_records).astype("<f4") if all_records else np.zeros((0, 8), "<f4")
    os.makedirs(OUT_DIR, exist_ok=True)
    data.tofile(os.path.join(OUT_DIR, "Foliage.bin"))
    with open(os.path.join(OUT_DIR, "Foliage.json"), "w", encoding="utf-8") as f:
        json.dump({"Generator": "Tools/world/scatter_foliage.py", "Seed": SEED, "Meshes": meshes, "Groups": groups_out}, f, indent=1)
    counts = {}
    for i in data[:, 0].astype(int):
        counts[used[i]["name"]] = counts.get(used[i]["name"], 0) + 1
    print("total %d instances in %d groups, %d meshes (%.1f s)" % (len(data), len(groups_out), len(meshes), time.time() - t0))
    for name, c in sorted(counts.items(), key=lambda kv: -kv[1]):
        print("  %-24s %7d" % (name, c))
    if args.preview:
        im = Image.open(os.path.join(WORLD_DIR, "WorldMap.png")).convert("RGB").resize((2048, 1536))
        px = ((data[:, 1] - terrain.x0) / terrain.width * 2048).astype(int)
        py = ((data[:, 2] - terrain.y0) / terrain.height * 1536).astype(int)
        a = np.asarray(im).copy()
        colors = {"Trees": (20, 90, 20), "Bushes": (120, 160, 40), "Rocks": (90, 90, 90), "Cliffs": (160, 60, 40), "Landmark": (200, 40, 200)}
        for i in range(len(data)):
            kind = used[int(data[i, 0])]["scatter"]
            if 0 <= px[i] < 2048 and 0 <= py[i] < 1536:
                a[py[i], px[i]] = colors.get(kind, (0, 0, 0))
        Image.fromarray(a).save(os.path.join(OUT_DIR, "_Preview.png"))


if __name__ == "__main__":
    main()
