"""Final world data for Unreal, after Tools/world/generate_world.py (terrain, paint, densities) and
Tools/world/generate_cities.py (city streets):

  SourceArt/World/LayersFinal/<Layer>.png + layers.json   the generator's paint layers with the city streets merged in:
                                                          Road := max(Road, StreetDirt), a new Cobble layer from
                                                          StreetCobble, the other layers rescaled so every vertex still
                                                          sums to 255
  SourceArt/World/Macro/RegionTint.png                    RGB albedo multiplier for vegetated ground per region
                                                          (128 = x1.0), blurred so regions blend over ~100 m
  SourceArt/World/Macro/GrassMix.png                      RGBA landscape-grass densities: R lush grass, G dry grass,
                                                          B snowy grass, A flowers (world density maps x region kind)
  Content/Data/WorldRegions.png                           region ids (1524 x 1143) for the game's region / weather
                                                          lookups (committed; small)

    ~/.venvs/mushoku-bpy311/bin/python Tools/world/finalize_world.py
"""
import json
import os
import shutil
import time

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WORLD = os.path.join(ROOT, "SourceArt", "World")

# Region id -> (linear albedo multiplier for vegetated ground, grass kind).
REGION_LOOK = {
    0: ((1.00, 1.00, 1.00), None),
    1: ((1.06, 1.03, 0.84), "lush"),    # Asura: warm, golden green
    2: ((1.00, 1.06, 0.88), "lush"),    # Fittoa: fresh green
    3: ((0.95, 0.97, 0.95), "alpine"),  # Red Wyrm Mountains
    4: ((0.86, 0.95, 1.06), "snow"),    # Northern Territories
    5: ((1.28, 1.05, 0.66), "dry"),     # Strife Zone: dry, yellow
    6: ((0.84, 1.10, 0.78), "lush"),    # Southern Central jungle
    7: ((0.95, 0.97, 1.06), "snow"),    # Heaven
    8: ((1.12, 0.80, 0.92), "dry"),     # Demon Continent: red / purple cast
    9: ((1.12, 0.84, 0.92), "dry"),     # Rikarisu crater
    10: ((0.80, 1.06, 0.80), "lush"),   # Great Forest: deep green
    11: ((0.90, 1.10, 0.88), "lush"),   # Millis lowlands: lush
    12: ((0.92, 0.99, 1.02), "alpine"), # Blue Wyrm Mountains
    13: ((1.18, 1.00, 0.76), "dry"),    # Begaritt desert
    14: ((1.16, 0.95, 0.78), "dry"),    # Begaritt badlands
    15: ((0.95, 1.08, 0.90), "lush"),   # Islands
}


def load_grey(path):
    return np.asarray(Image.open(path).convert("L"), dtype=np.uint8)


def save_grey(path, arr):
    Image.fromarray(arr.astype(np.uint8), "L").save(path, optimize=False, compress_level=6)


def merge_layers(world):
    names = list(world["Landscape"]["LayerNames"])
    cob_path = os.path.join(WORLD, "Cities", "StreetCobble.png")
    dirt_path = os.path.join(WORLD, "Cities", "StreetDirt.png")
    out_dir = os.path.join(WORLD, "LayersFinal")
    if not (os.path.exists(cob_path) or os.path.exists(dirt_path)):
        print("no city street masks: LayersFinal not written (the landscape uses the generator's layers)")
        if os.path.isdir(out_dir):
            shutil.rmtree(out_dir)
        return
    layers = {n: load_grey(os.path.join(WORLD, "Layers", n + ".png")) for n in names}
    h, w = layers[names[0]].shape
    cob = load_grey(cob_path) if os.path.exists(cob_path) else np.zeros((h, w), np.uint8)
    dirt = load_grey(dirt_path) if os.path.exists(dirt_path) else np.zeros((h, w), np.uint8)
    others = [n for n in names if n != "Road"]
    out = {n: np.zeros((h, w), np.uint8) for n in names + ["Cobble"]}
    for r0 in range(0, h, 512):
        r1 = min(h, r0 + 512)
        c = cob[r0:r1].astype(np.int32)
        road = np.minimum(np.maximum(layers["Road"][r0:r1].astype(np.int32), dirt[r0:r1].astype(np.int32)), 255 - c)
        rest = 255 - c - road
        stack = np.stack([layers[n][r0:r1].astype(np.float32) for n in others])
        total = stack.sum(axis=0)
        scale = np.where(total > 0, rest / np.maximum(total, 1.0), 0.0)
        scaled = np.floor(stack * scale).astype(np.int32)
        remainder = rest - scaled.sum(axis=0)
        # Rounding remainder to the strongest layer; pixels with no other layer give it to the road.
        best = np.argmax(stack, axis=0)
        for k in range(len(others)):
            scaled[k] += np.where((best == k) & (total > 0), remainder, 0)
        road += np.where(total > 0, 0, remainder)
        for k, n in enumerate(others):
            out[n][r0:r1] = scaled[k]
        out["Road"][r0:r1] = road
        out["Cobble"][r0:r1] = c
    os.makedirs(out_dir, exist_ok=True)
    for n, arr in out.items():
        save_grey(os.path.join(out_dir, n + ".png"), arr)
    check = sum(out[n].astype(np.int32) for n in out)
    final = names + ["Cobble"]
    with open(os.path.join(out_dir, "layers.json"), "w", encoding="utf-8") as f:
        json.dump({"Layers": final, "Files": ["SourceArt/World/LayersFinal/%s.png" % n for n in final]}, f, indent=1)
    print("LayersFinal: %d layers, sum min %d max %d, cobble %.2f%%, road %.2f%%" % (
        len(final), check.min(), check.max(), (out["Cobble"] > 0).mean() * 100, (out["Road"] > 0).mean() * 100))


def blur(arr, radius):
    im = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "L")
    return np.asarray(im.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32)


def macro_maps(world):
    regions = load_grey(os.path.join(WORLD, "Regions.png"))
    h, w = regions.shape
    ls = world["Landscape"]
    raw = np.fromfile(os.path.join(WORLD, "Height.r16"), dtype="<u2").reshape(ls["VerticesY"], ls["VerticesX"])
    alt_m = (ls["LocationCm"]["Z"] + (np.asarray(Image.fromarray(raw.astype(np.float32)).resize((w, h), Image.BILINEAR)) - 32768.0)
             * ls["ScaleCm"]["Z"] / 128.0) / 100.0
    tint = np.ones((h, w, 3), np.float32)
    kinds = {"lush": np.zeros((h, w), np.float32), "dry": np.zeros((h, w), np.float32), "snow": np.zeros((h, w), np.float32)}
    for rid, (rgb, kind) in REGION_LOOK.items():
        mask = regions == rid
        tint[mask] = rgb
        if kind == "alpine":
            kinds["lush"][mask & (alt_m < 420)] = 1.0
            kinds["snow"][mask & (alt_m >= 420)] = 1.0
        elif kind:
            kinds[kind][mask] = 1.0
    out_dir = os.path.join(WORLD, "Macro")
    os.makedirs(out_dir, exist_ok=True)
    enc = np.stack([blur(np.clip(tint[..., i] * 128.0, 0, 255), 6) for i in range(3)], axis=-1)
    Image.fromarray(np.clip(enc + 0.5, 0, 255).astype(np.uint8), "RGB").save(os.path.join(out_dir, "RegionTint.png"))
    grass = load_grey(os.path.join(WORLD, "Density", "Grass.png")).astype(np.float32)
    flowers = load_grey(os.path.join(WORLD, "Density", "Flowers.png")).astype(np.float32)
    # Densities stay in the generator's 0..255 scale; kinds blend over ~50 m.
    r = blur(kinds["lush"] * 255, 4) / 255 * grass
    g = blur(kinds["dry"] * 255, 4) / 255 * grass
    b = blur(kinds["snow"] * 255, 4) / 255 * grass
    a = flowers
    mix = np.stack([r, g, b, a], axis=-1)
    Image.fromarray(np.clip(mix + 0.5, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(out_dir, "GrassMix.png"))
    shutil.copyfile(os.path.join(WORLD, "Regions.png"), os.path.join(ROOT, "Content", "Data", "WorldRegions.png"))
    print("macro maps: RegionTint, GrassMix (%dx%d); Content/Data/WorldRegions.png" % (w, h))


def main():
    t0 = time.time()
    with open(os.path.join(ROOT, "Content", "Data", "World.json"), "r", encoding="utf-8") as f:
        world = json.load(f)
    merge_layers(world)
    macro_maps(world)
    print("done in %.1f s" % (time.time() - t0))


if __name__ == "__main__":
    main()
