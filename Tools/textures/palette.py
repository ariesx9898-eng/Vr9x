"""Mesh material palette (Spec section 6): SourceArt/Textures/Palette/T_<Name>_{D,N,M}.png, 1024 px, seamless.

AI-sourced materials: degrid -> lattice-aware min-cut seam removal (rows of roof tiles, masonry courses and planks
stay whole across the wrap) -> Moisan periodic component -> low-frequency delighting -> colour grade to the Spec
palette colour -> height (band-passed luma + joints / cracks as grooves) -> normal / AO / roughness.
Cloth colours share one neutral woven source; Snow reuses the terrain snow pipeline at mesh scale; Iron, Gold, Glass
and Crystal are procedural (procedural.py). Cards (alpha-masked foliage) live in cards.py.

UV conventions they are made for (Tools/kit): solids are box-projected at 1 UV = 1 m; roof slopes use 'slope' UVs
(u along the eave, v up the slope) so roof images have their rows horizontal with the ridge at the top; bark is
cylindrical with v along the branch and u wrapping whole 1 m tiles, so bark tiles are 1 m and grain runs along V.
"""
import numpy as np

import texlib as T
import surface as SF
import terrain
import procedural
from common import load_ai, rng_for, save_set

SIZE = 1024

# rows / cols: ('est', lo_px, hi_px[, 'stagger']) lattice period searched in the source (stagger doubles the row
# period so staggered slates / scales stay consistent across the wrap), or None (free min-cut seam).
# target: sRGB 0..255 mean colour (Spec palette colours, toned for 'rich but not neon'); tile_m in metres.
ARCH = {
    "Plaster": dict(src="Palette_Plaster_A", tile_m=2.0, delight=220, sat=0.9, target=(222, 212, 190),
                    soften=(6, 140, 0.55),
                    height=dict(bands=[(0, 3, 0.6), (3, 40, 1.0)], grooves=(2, 0.6, 0.6)), tilt=5,
                    ao=([0.004, 0.02], 0.3), rough=dict(base=0.9, lum=-0.2, lo=0.78, hi=0.98)),
    "PlasterTan": dict(src="Palette_PlasterTan_A", tile_m=2.0, delight=220, sat=0.78, target=(204, 168, 122),
                       height=dict(bands=[(0, 3, 0.6), (3, 40, 1.0)], grooves=(2, 0.7, 0.6)), tilt=6,
                       ao=([0.004, 0.02], 0.35), rough=dict(base=0.92, lo=0.8, hi=1.0)),
    "Timber": dict(src="Palette_Timber_A", tile_m=1.0, delight=200, sat=0.9, target=(80, 56, 38),
                   height=dict(bands=[(0, 2, 0.7), (2, 20, 1.0)], grooves=(2, 0.9, 0.5)), tilt=13,
                   ao=([0.002, 0.01], 0.45), rough=dict(base=0.76, lum=-0.3, lo=0.6, hi=0.9)),
    "WoodPlanks": dict(src="Palette_WoodPlanks_A", tile_m=2.0, rows=("est", 70, 150), delight=260, sat=0.85,
                       target=(140, 103, 66),
                       height=dict(bands=[(0, 2, 0.5), (2, 16, 0.8)], grooves=(4, 1.2, 0.8)), tilt=11,
                       ao=([0.003, 0.012], 0.5), rough=dict(base=0.8, lum=-0.2, lo=0.62, hi=0.95)),
    "Stone": dict(src="Palette_Stone_A", tile_m=2.0, rows=("est", 110, 200), delight=300, sat=0.8,
                  target=(136, 134, 127),
                  height=dict(bands=[(0, 3, 0.5), (3, 30, 0.7)], lines=[("bright", 7, -1.3, 1.2)]), tilt=15,
                  ao=([0.004, 0.02, 0.05], 0.55), rough=dict(base=0.84, h=-0.3, lo=0.66, hi=0.97)),
    "StoneWhite": dict(src="Palette_StoneWhite_A", tile_m=2.0, rows=("est", 200, 320), delight=300, sat=0.7,
                       target=(228, 225, 215),
                       height=dict(bands=[(0, 3, 0.4), (3, 40, 0.5)], grooves=(4, 1.2, 0.7)), tilt=6,
                       ao=([0.004, 0.02], 0.4), rough=dict(base=0.58, lum=0.2, lo=0.42, hi=0.75)),
    "Cobble": dict(src="Palette_Cobble_A", tile_m=2.0, delight=260, sat=0.85, target=(120, 115, 107),
                   height=dict(bands=[(0, 3, 0.4), (3, 28, 1.0)], grooves=(8, 1.2, 1.0)), tilt=18,
                   ao=([0.004, 0.02, 0.05], 0.6), rough=dict(base=0.8, h=-0.3, lo=0.6, hi=0.95)),
    "Sandstone": dict(src="Palette_Sandstone_A", tile_m=2.0, rows=("est", 150, 300), delight=320, sat=0.72,
                      target=(212, 178, 128),
                      height=dict(bands=[(0, 3, 0.5), (3, 40, 0.7)], grooves=(5, 1.2, 0.8)), tilt=9,
                      ao=([0.004, 0.02], 0.45), rough=dict(base=0.9, lo=0.78, hi=1.0)),
    "DemonRock": dict(src="Palette_DemonRock_A", tile_m=2.0, delight=300, sat=0.9, target=(92, 50, 40),
                      height=dict(bands=[(0, 3, 0.6), (3, 30, 1.0)], grooves=(6, 1.0, 0.8)), tilt=22,
                      ao=([0.004, 0.02, 0.05], 0.6), rough=dict(base=0.86, h=-0.2, lo=0.66, hi=0.98)),
    "RoofRed": dict(src="Palette_RoofRed_A", tile_m=2.0, rows=("est", 120, 300), cols=("est", 60, 200),
                    delight=400, sat=0.85, target=(176, 86, 54),
                    height=dict(bands=[(0, 3, 0.25), (3, 16, 0.5), (8, 70, 1.6)], grooves=(4, 0.8, 0.8)),
                    tilt=24, ao=([0.004, 0.02, 0.06], 0.6), rough=dict(base=0.72, lum=-0.3, lo=0.55, hi=0.9)),
    "RoofBlue": dict(src="Palette_RoofBlue_A", tile_m=2.0, rows=("est", 200, 300, "stagger"), cols=("est", 150, 260),
                     delight=400, sat=0.8, target=(86, 102, 128),
                     height=dict(bands=[(0, 3, 0.4), (3, 30, 0.8)], grooves=(4, 1.2, 0.8)), tilt=15,
                     ao=([0.004, 0.02, 0.05], 0.55), rough=dict(base=0.55, lum=0.2, lo=0.4, hi=0.75)),
    "RoofDark": dict(src="Palette_RoofDark_A", tile_m=2.0, rows=("est", 230, 330, "stagger"), delight=400, sat=0.8,
                     target=(54, 57, 66),
                     height=dict(bands=[(0, 3, 0.4), (3, 30, 0.8)], grooves=(4, 1.2, 0.8)), tilt=16,
                     ao=([0.004, 0.02, 0.05], 0.55), rough=dict(base=0.62, lum=0.2, lo=0.45, hi=0.8)),
    "RoofThatch": dict(src="Palette_RoofThatch_A", tile_m=2.0, rows=("est", 150, 300), delight=400, sat=0.8,
                       target=(166, 136, 84),
                       height=dict(bands=[(0, 2, 0.6), (2, 12, 0.8), (12, 80, 1.0)]), tilt=24,
                       ao=([0.003, 0.015, 0.06], 0.65), rough=dict(base=0.94, lo=0.82, hi=1.0)),
    "RoofSilver": dict(src="Palette_RoofSilver_A", tile_m=2.0, rows=("est", 200, 330, "stagger"),
                       cols=("est", 100, 220), delight=400, sat=0.7, target=(196, 204, 216), metallic=1,
                       height=dict(bands=[(0, 3, 0.2), (3, 40, 1.0)], grooves=(4, 1.0, 0.8)), tilt=16,
                       ao=([0.004, 0.02, 0.05], 0.5),
                       rough=dict(base=0.34, lum=-0.2, noise=(0.05, 6), lo=0.2, hi=0.5)),
    "RoofGreen": dict(src="Palette_RoofGreen_A", tile_m=2.0, cols=("est", 100, 260), delight=400, sat=0.8,
                      target=(96, 160, 138),
                      height=dict(bands=[(0, 3, 0.3), (3, 30, 0.5)], ridges=(5, 1.2, 1.0)), tilt=9,
                      ao=([0.004, 0.02], 0.4), rough=dict(base=0.62, lum=0.2, lo=0.45, hi=0.8)),
    "Hide": dict(src="Palette_Hide_A", tile_m=1.0, delight=220, sat=0.85, target=(148, 112, 80),
                 height=dict(bands=[(0, 2, 0.6), (2, 30, 1.0)], grooves=(2, 0.6, 0.6)), tilt=8,
                 ao=([0.002, 0.01], 0.35), rough=dict(base=0.72, lum=-0.3, lo=0.55, hi=0.88)),
    "Bone": dict(src="Palette_Bone_A", tile_m=1.0, delight=220, sat=0.7, target=(218, 206, 174),
                 height=dict(bands=[(0, 2, 0.6), (2, 30, 1.0)], grooves=(2, 0.8, 0.5)), tilt=8,
                 ao=([0.002, 0.01], 0.4), rough=dict(base=0.62, lum=-0.2, lo=0.45, hi=0.8)),
}

# one neutral woven source, four dyes (colourised in linear light, detail kept)
CLOTH_BASE = dict(src="Palette_Cloth_A", tile_m=1.0, delight=160, height=dict(bands=[(0, 1.5, 1.0), (1.5, 8, 0.6)]),
                  tilt=12, ao=([0.001, 0.004], 0.4), rough=dict(base=0.88, lo=0.78, hi=0.98))
CLOTH = {"ClothRed": (166, 46, 38), "ClothBlue": (48, 76, 140), "ClothGreen": (62, 112, 64),
         "ClothTan": (202, 176, 128)}

NATURE = {
    "BarkOak": dict(src="Palette_BarkOak_A", tile_m=1.0, delight=240, sat=0.85, target=(92, 74, 58),
                    height=dict(bands=[(0, 3, 0.5), (3, 30, 1.0)], grooves=(6, 1.0, 0.8)), tilt=28,
                    ao=([0.003, 0.015, 0.04], 0.7), rough=dict(base=0.9, lo=0.78, hi=1.0)),
    "BarkBirch": dict(src="Palette_BarkBirch_A", tile_m=1.0, delight=240, sat=0.7, target=(214, 210, 198),
                      height=dict(bands=[(0, 3, 0.6), (3, 30, 0.6)], grooves=(4, 1.2, 0.6)), tilt=10,
                      ao=([0.003, 0.012], 0.45), rough=dict(base=0.8, lum=0.2, lo=0.62, hi=0.95)),
    "BarkPine": dict(src="Palette_BarkPine_A", tile_m=1.0, delight=240, sat=0.85, target=(116, 76, 52),
                     height=dict(bands=[(0, 3, 0.5), (3, 30, 1.0)], grooves=(6, 1.0, 0.8)), tilt=26,
                     ao=([0.003, 0.015, 0.04], 0.7), rough=dict(base=0.88, lo=0.75, hi=1.0)),
    "BarkDead": dict(src="Palette_BarkDead_A", tile_m=1.0, delight=240, sat=0.6, target=(142, 132, 118),
                     height=dict(bands=[(0, 3, 0.5), (3, 24, 0.8)], grooves=(4, 1.1, 0.6)), tilt=16,
                     ao=([0.003, 0.012, 0.03], 0.55), rough=dict(base=0.86, lo=0.72, hi=0.98)),
    "BarkGiant": dict(src="Palette_BarkGiant_A", tile_m=1.0, delight=240, sat=0.85, target=(92, 86, 60),
                      height=dict(bands=[(0, 3, 0.5), (3, 36, 1.0)], grooves=(7, 1.0, 0.8)), tilt=28,
                      ao=([0.003, 0.015, 0.05], 0.7), rough=dict(base=0.9, lo=0.78, hi=1.0)),
    "BarkDemon": dict(src="Palette_BarkDemon_A", tile_m=1.0, delight=240, sat=0.9, target=(62, 38, 50),
                      height=dict(bands=[(0, 3, 0.5), (3, 30, 1.0)], grooves=(5, 1.0, 0.8)), tilt=24,
                      ao=([0.003, 0.015, 0.04], 0.65), rough=dict(base=0.74, lum=-0.3, lo=0.55, hi=0.9)),
    "MushroomCap": dict(src="Palette_MushroomCap_A", tile_m=2.0, delight=260, sat=0.85, target=(158, 72, 64),
                        height=dict(bands=[(0, 3, 0.5), (3, 40, 1.0)]), tilt=8, ao=([0.004, 0.02], 0.35),
                        rough=dict(base=0.6, lum=0.2, lo=0.45, hi=0.8)),
    "MushroomStem": dict(src="Palette_MushroomStem_A", tile_m=1.0, delight=240, sat=0.7, target=(218, 206, 178),
                         height=dict(bands=[(0, 2, 0.6), (2, 24, 1.0)]), tilt=9, ao=([0.002, 0.01], 0.35),
                         rough=dict(base=0.7, lo=0.55, hi=0.85)),
    "Rock": dict(src="Palette_Rock_A", tile_m=2.0, delight=280, sat=0.8, target=(122, 118, 112),
                 height=dict(bands=[(0, 3, 0.4), (3, 16, 0.7), (16, 80, 1.0)], grooves=(5, 0.9, 0.8)), tilt=26,
                 ao=([0.004, 0.02, 0.06], 0.65), rough=dict(base=0.86, h=-0.2, lo=0.66, hi=0.98)),
    "RockMossy": dict(src="Palette_RockMossy_A", tile_m=2.0, delight=280, sat=0.85, target=(98, 112, 72),
                      height=dict(bands=[(0, 3, 0.5), (3, 16, 0.7), (16, 80, 1.0)]), tilt=22,
                      ao=([0.004, 0.02, 0.06], 0.65), rough=dict(base=0.9, lo=0.72, hi=1.0)),
    "RockSnow": dict(src="Palette_RockSnow_A", tile_m=2.0, delight=280, sat=0.75, target=(104, 108, 116),
                     height=dict(bands=[(0, 3, 0.4), (3, 16, 0.7), (16, 80, 1.0)], grooves=(5, 0.9, 0.8)),
                     tilt=26, ao=([0.004, 0.02, 0.06], 0.65), rough=dict(base=0.82, lum=-0.3, lo=0.55, hi=0.95)),
    "RockDesert": dict(src="Palette_RockDesert_A", tile_m=2.0, delight=280, sat=0.78, target=(190, 146, 104),
                       height=dict(bands=[(0, 3, 0.4), (3, 16, 0.7), (16, 80, 1.0)], grooves=(5, 0.9, 0.8)),
                       tilt=22, ao=([0.004, 0.02, 0.06], 0.6), rough=dict(base=0.9, lo=0.75, hi=1.0)),
    "RockDemon": dict(src="Palette_RockDemon_A", tile_m=2.0, delight=280, sat=0.9, target=(56, 38, 40),
                      height=dict(bands=[(0, 3, 0.4), (3, 20, 0.8), (20, 80, 1.0)], grooves=(5, 0.8, 0.8)),
                      tilt=20, ao=([0.004, 0.02, 0.06], 0.55), rough=dict(base=0.36, lum=0.3, lo=0.18, hi=0.6)),
    "RockPale": dict(src="Palette_RockPale_A", tile_m=2.0, delight=280, sat=0.7, target=(200, 196, 186),
                     height=dict(bands=[(0, 3, 0.4), (3, 16, 0.7), (16, 80, 1.0)], grooves=(5, 0.8, 0.8)),
                     tilt=20, ao=([0.004, 0.02, 0.06], 0.55), rough=dict(base=0.84, lo=0.66, hi=0.98)),
}

SNOW = dict(sources=["Terrain_Snow_A"], tile_m=2.0, src_m=3.0, xf="h", delight=128, flatten=128, overlap=96,
            sat=1.0, target=(230, 235, 244),
            sfs=dict(d=(0.0, 1.0), hp=110, grain=0.18, grain_amt=0.8, tint=(0.76, 0.86, 1.0), tint_amt=0.35,
                     crest=0.04),
            tilt=9, ao=([0.01, 0.04], 0.3), rough=dict(base=0.6, h=0.25, noise=(0.05, 8), lo=0.45, hi=0.78))

ARCH_ORDER = ["Plaster", "PlasterTan", "Timber", "WoodPlanks", "Stone", "StoneWhite", "Cobble", "Sandstone",
              "DemonRock", "RoofRed", "RoofBlue", "RoofDark", "RoofThatch", "RoofSilver", "RoofGreen", "Snow",
              "ClothRed", "ClothBlue", "ClothGreen", "ClothTan", "Hide", "Iron", "Gold", "Glass", "Crystal", "Bone"]
NATURE_ORDER = ["BarkOak", "BarkBirch", "BarkPine", "BarkDead", "BarkGiant", "BarkDemon", "MushroomCap",
                "MushroomStem", "Rock", "RockMossy", "RockSnow", "RockDesert", "RockDemon", "RockPale"]
NAMES = ARCH_ORDER + NATURE_ORDER
KIND = {**{n: "Architecture" for n in ARCH_ORDER}, **{n: "Nature" for n in NATURE_ORDER}}


def _period(a, spec, axis):
    if not spec:
        return None
    _, lo, hi = spec[:3]
    p, strength = T.estimate_period(a, axis, lo, hi)
    return p if strength > 0.08 else None


def prepare(src, R, size=SIZE):
    """AI source -> seamless, delit, resized image. Returns (image, info)."""
    a = load_ai(src)
    a = T.degrid(a)
    py = _period(a, R.get("rows"), 0)
    px = _period(a, R.get("cols"), 1)
    a, reps = T.make_tileable(a, px, py, overlap=R.get("overlap", 96))
    a = T.resize_periodic(a, (size, size))
    a = np.clip(T.delight(a, R["delight"] * size / 1024.0), 0.0, 1.0)
    return a, dict(repeats_x=reps[0], repeats_y=reps[1])


def finish(name, R, D, rng, extra=None, src_names=(), size=SIZE):
    hs = R["height"]
    h = SF.height_luma(D, hs["bands"], hs.get("grooves"), hs.get("ridges"), lines=hs.get("lines", ()))
    h01 = T.normalize01(h, 0.5, 99.5)
    rough = dict(R["rough"], seed=int(rng.integers(1 << 30)))
    N, M, info = SF.maps(D, h01, R["tile_m"], R["tilt"], R["ao"][0], R["ao"][1], rough)
    files = save_set("Palette", f"T_{name}", D, N, M)
    entry = dict(name=name, group="Palette", kind=KIND[name], files=files, tile_m=R["tile_m"], size=size,
                 source="AI", ai_sources=[f"SourceArt/AI/Textures/{s}.png" for s in src_names],
                 metallic=int(R.get("metallic", 0)), roughness_range=info["roughness_range"], normal_strength=1.0,
                 height_m=info["height_m"], mean_albedo=SF.mean_albedo_linear(D),
                 seam=[round(v, 2) for v in T.seam_error(D)])
    if extra:
        entry.update(extra)
    return entry, dict(D=D, N=N, M=M)


def soften_band(a, s_lo, s_hi, amount):
    """Reduce blotchy stains between two scales (px) by `amount` (0..1), in linear light, keeping fine detail."""
    lin = T.srgb_to_lin(a)
    y = T.luma(lin)
    band = T.blur_periodic(y, s_lo) - T.blur_periodic(y, s_hi)
    ref = np.maximum(T.blur_periodic(y, s_hi), 1e-4)
    lin = lin * np.clip(1.0 - amount * band / ref, 0.3, 3.0)[..., None]
    return T.lin_to_srgb(lin)


def colourise(a, target_srgb, detail=1.0):
    """Neutral source -> dyed cloth: keep the luminance detail, replace the chroma, hit the target mean."""
    lin = T.srgb_to_lin(a)
    y = T.luma(lin)
    rel = (y / max(float(y.mean()), 1e-6)) ** detail
    tgt = T.srgb_to_lin(np.asarray(target_srgb, np.float32) / 255.0)
    out = rel[..., None] * tgt[None, None, :]
    return T.lin_to_srgb(out)


def build(name, log=print):
    rng = rng_for("Palette_" + name)
    if name in procedural.BUILDERS:
        return procedural.build(name, KIND[name])
    if name == "Snow":
        e, imgs = terrain.build("Snow", SNOW, log=log, size=SIZE, group="Palette", prefix="T_Snow", entry_name="Snow")
        e["kind"] = "Architecture"
        e.pop("layer", None)
        return e, imgs
    if name in CLOTH:
        R = dict(CLOTH_BASE)
        D, info = prepare(R["src"], R)
        D = colourise(D, CLOTH[name])
        info["note"] = "dyed from the shared neutral woven source"
        return finish(name, R, D, rng, info, [R["src"]])
    R = ARCH.get(name) or NATURE[name]
    D, info = prepare(R["src"], R)
    if R.get("soften"):
        D = soften_band(D, *R["soften"])
    D = T.saturate(D, R.get("sat", 1.0))
    D = T.match_mean_lin(D, np.asarray(R["target"], np.float32) / 255.0)
    entry, imgs = finish(name, R, D, rng, info, [R["src"]])
    log(f"Palette {name}: tilt {entry.get('height_m')} m, seam {entry['seam']}, repeats {info}")
    return entry, imgs
