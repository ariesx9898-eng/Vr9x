"""Landscape paint-layer textures: SourceArt/Textures/Terrain/T_Ground_<Layer>_{D,N,M}.png, 2048 px, seamless.

Pipeline per layer (all deterministic):
1. AI source(s) -> Moisan periodic component -> low-frequency delighting (divide by a heavy blur in linear light).
2. Each source is used at its native texel density: one 1024 px source covers half a tile, so a 2048 tile is
   synthesised by toroidal image quilting (min-error boundary cuts, wrap-around constraints on the last row and
   column) from the source and its rotated / mirrored copies (only mirrors for layers whose painted light or
   structure has a direction). Result: seamless by construction, no mirrored seams, no 2 x 2 repeat inside a tile.
3. A second delighting pass removes residual blotches (patch-choice drift) at scales above ~1/8 tile.
4. Height from the albedo (band-passed luma, crack grooves, or shape-from-shading for wind ripples), then
   normal (DirectX), AO and roughness; colour graded to the region art direction (Spec section 1).
"""
import numpy as np

import texlib as T
import surface as SF
from common import load_ai, rng_for, save_set

SIZE = 2048
XF = {"d": T.DIHEDRAL, "f": T.FLIPS_ONLY, "h": T.HFLIP_ONLY}

# delight / flatten sigmas are in output pixels; ao radii in metres; target = sRGB 0..255 mean colour
TERRAIN = {
    "Grass": dict(
        sources=["Terrain_Grass_A"], tile_m=4.0, xf="d", delight=48, flatten=56,
        sat=0.78, target=(88, 116, 48),
        height=dict(bands=[(0, 5, 1.0), (5, 24, 0.7)]), tilt=17, ao=([0.004, 0.012, 0.03], 0.55),
        rough=dict(base=0.84, h=-0.4, lo=0.6, hi=0.97)),
    "Farmland": dict(
        sources=["Terrain_Farmland_A"], tile_m=4.0, xf="h", rows=12, delight=64, flatten=80, overlap=96,
        sat=0.9, target=(104, 78, 56),
        height=dict(bands=[(0, 6, 0.8), (6, 24, 0.8)], row_relief=1.6), tilt=20,
        ao=([0.006, 0.02, 0.06], 0.6), rough=dict(base=0.9, h=-0.3, lo=0.7, hi=1.0)),
    "ForestFloor": dict(
        sources=["Terrain_ForestFloor_A"], tile_m=4.0, xf="d", delight=64, flatten=72,
        sat=0.88, target=(94, 71, 47),
        height=dict(bands=[(0, 8, 1.0), (8, 32, 0.8)], grooves=(3, 0.3, 0.7)), tilt=21,
        ao=([0.005, 0.015, 0.04], 0.6), rough=dict(base=0.86, lum=-0.3, lo=0.62, hi=0.98)),
    "Moss": dict(
        sources=["Terrain_Moss_A"], tile_m=4.0, xf="d", delight=64, flatten=72,
        sat=0.82, target=(64, 86, 32),
        height=dict(bands=[(0, 6, 0.6), (6, 40, 1.0)]), tilt=19, ao=([0.006, 0.02, 0.05], 0.65),
        rough=dict(base=0.93, lum=0.2, lo=0.72, hi=1.0)),
    "Snow": dict(
        sources=["Terrain_Snow_A"], tile_m=6.0, xf="h", delight=128, flatten=128, overlap=128,
        sat=1.0, target=(226, 232, 242),
        sfs=dict(d=(0.0, 1.0), hp=110, grain=0.18, grain_amt=0.8, tint=(0.74, 0.85, 1.0), tint_amt=0.42,
                 crest=0.04),
        tilt=9, ao=([0.02, 0.06], 0.35), rough=dict(base=0.62, h=0.25, noise=(0.05, 24), lo=0.45, hi=0.8)),
    "Sand": dict(
        sources=["Terrain_Sand_A"], tile_m=4.0, xf="d", delight=48, flatten=56,
        sat=0.9, target=(204, 186, 152),
        height=dict(bands=[(0, 3, 1.0), (3, 14, 0.6)]), tilt=8, ao=([0.003, 0.01], 0.35),
        rough=dict(base=0.9, lo=0.78, hi=1.0)),
    "Desert": dict(
        sources=["Terrain_Desert_A"], tile_m=8.0, xf="h", delight=96, flatten=112, overlap=128,
        sat=0.72, target=(212, 166, 106),
        sfs=dict(d=(0.0, 1.0), hp=40, grain=0.25, grain_amt=1.0, tint=(0.86, 0.78, 0.7), tint_amt=0.55,
                 crest=0.08),
        tilt=9, ao=([0.02, 0.08], 0.3), rough=dict(base=0.92, lo=0.82, hi=1.0)),
    "DemonSoil": dict(
        sources=["Terrain_DemonSoil_A"], tile_m=6.0, xf="d", delight=64, flatten=72,
        sat=0.95, target=(74, 45, 37),
        height=dict(bands=[(0, 6, 1.0), (6, 28, 0.8)], grooves=(4, 0.8, 0.8)), tilt=22,
        ao=([0.006, 0.02, 0.05], 0.6), rough=dict(base=0.9, lum=0.2, lo=0.7, hi=1.0)),
    "Rock": dict(
        # one 2048 px source that already covers the whole 8 m tile with bold, cliff-scale blocks and strata
        sources=["Terrain_Rock_C"], single=True, tile_m=8.0, delight=160, flatten=200, overlap=128,
        sat=0.85, target=(124, 120, 113),
        height=dict(bands=[(0, 4, 0.2), (4, 16, 0.45), (16, 64, 1.0), (64, 200, 1.0)], grooves=(6, 1.0, 1.0)),
        tilt=30, ao=([0.01, 0.04, 0.12], 0.7), rough=dict(base=0.84, h=-0.2, lo=0.62, hi=0.97)),
    "Road": dict(
        sources=["Terrain_Road_A"], tile_m=4.0, xf="d", delight=48, flatten=56,
        sat=0.85, target=(148, 120, 90),
        height=dict(bands=[(0, 4, 1.0), (4, 16, 0.6)]), tilt=14, ao=([0.004, 0.012, 0.03], 0.5),
        rough=dict(base=0.88, lum=-0.2, lo=0.7, hi=1.0)),
    "Mud": dict(
        sources=["Terrain_Mud_A"], tile_m=4.0, xf="d", delight=64, flatten=72,
        sat=0.95, target=(72, 55, 41),
        height=dict(bands=[(0, 6, 0.6), (6, 32, 1.0)]), tilt=13, ao=([0.006, 0.02, 0.05], 0.55),
        puddles=dict(frac=0.07, darken=0.55, rough=0.06),
        rough=dict(base=0.5, lum=-0.4, noise=(0.06, 12), lo=0.3, hi=0.72)),
}


def _prepare_source(name, R, size):
    a = load_ai(name)
    a = T.periodic_component(a)
    a = T.degrid(a)
    a = T.delight(a, R["delight"])
    # native texel density: one source image covers src_m metres (default half a terrain tile)
    src_m = R.get("src_m", R["tile_m"] / 2.0)
    scale = (size / R["tile_m"]) / (a.shape[1] / src_m)
    if abs(scale - 1.0) > 1e-3:
        a = T.resize_periodic(a, (int(round(a.shape[1] * scale)), int(round(a.shape[0] * scale))))
    if R.get("rows"):
        # rescale vertically so that an integer number of furrow rows fits the source exactly (period divides
        # both the source and the 2048 output), then quilting keeps the row phase
        per, _ = T.estimate_period(a, 0, 60, a.shape[0] // 3)
        rows_src = max(1, int(round(a.shape[0] / per)))
        target_per = size / R["rows"]
        new_h = int(round(rows_src * target_per))
        a = T.resize_periodic(a, (a.shape[1], new_h))
    return np.clip(a, 0.0, 1.0)


def _sfs(albedo, S):
    """Relief from painted directional shading. The albedo is then rebuilt without that shading: the source's mean
    colour, its fine grain (sparkle / sand grains, below ~2 px) and a gentle non-directional tint in the hollows
    (the Spec's 'subtle blue shading' for snow), so the terrain reacts correctly to any sun direction."""
    h = SF.height_sfs(albedo, S["d"], S["hp"])
    y = T.luma(albedo)
    grain = y - T.blur_periodic(y, 2.0)
    h = h + S["grain"] * grain / max(float(grain.std()), 1e-6)
    h01 = T.normalize01(h, 0.5, 99.5)
    lin = T.srgb_to_lin(albedo)
    yl = T.luma(lin)
    rel = yl / max(float(yl.mean()), 1e-6) - 1.0
    fine = rel - T.blur_periodic(rel, 2.0)
    base = lin.reshape(-1, 3).mean(0)
    lin = base[None, None, :] * (1.0 + S.get("grain_amt", 1.0) * fine)[..., None]
    hb = T.blur_periodic(h01, 3.0)
    w = S["tint_amt"] * np.clip((0.65 - hb) / 0.65, 0.0, 1.0) ** 1.2
    tint = np.asarray(S["tint"], np.float32)
    lin = lin * (1.0 + w[..., None] * (tint - 1.0))
    lin = lin * (1.0 + S.get("crest", 0.0) * np.clip((hb - 0.6) / 0.4, 0.0, 1.0))[..., None]
    return T.lin_to_srgb(lin), h01


def _puddles(albedo, h01, P):
    """Find the puddles the source already paints (smooth, slightly pale patches: muddy water with a baked sky
    reflection) and turn them into real water: dark wet albedo, a flat water level in the height, flat normals and
    very low roughness (so the sheen comes from the engine's reflections instead of the albedo)."""
    y = T.luma(albedo)
    hp = y - T.blur_periodic(y, 1.5)
    energy = np.sqrt(T.blur_periodic(hp * hp, 5.0))
    bright = T.blur_periodic(y, 6.0) - T.blur_periodic(y, 48.0)
    e = (energy - np.median(energy)) / max(float(energy.std()), 1e-6)
    b = (bright - np.median(bright)) / max(float(bright.std()), 1e-6)
    score = T.blur_periodic(-e + 0.35 * b, 3.0)
    thr = np.percentile(score, 100 - P["frac"] * 100)
    wet = np.clip((score - thr) / max(float(score.std()) * 0.35, 1e-6) + 0.5, 0.0, 1.0)
    wet = T.blur_periodic(wet, 2.0)
    wet = np.clip((wet - 0.25) / 0.5, 0.0, 1.0)
    wet = (wet * wet * (3 - 2 * wet)).astype(np.float32)
    level = float(np.percentile(h01, 18))
    hb = T.blur_periodic(h01, 10.0)
    water = np.minimum(hb, level)
    h_new = h01 * (1 - wet) + water * wet
    lin = T.srgb_to_lin(albedo)
    calm = T.blur_periodic(lin, 6.0) * P["darken"]
    lin = lin * (1 - wet[..., None]) + calm * wet[..., None]
    return T.lin_to_srgb(lin), h_new.astype(np.float32), wet


def build(name, R, log=print, size=SIZE, group="Terrain", prefix=None, entry_name=None):
    rng = rng_for(group + "_" + name)
    if R.get("single"):
        # the source covers the full tile at native density: only the wrap seams need removing
        a = T.degrid(T.periodic_component(load_ai(R["sources"][0])))
        a = np.clip(T.delight(a, R["delight"]), 0.0, 1.0)
        a, _ = T.make_tileable(a, overlap=R.get("overlap", 128), presmoothed=True)
        out = T.resize_periodic(a, (size, size))
        picks = []
    else:
        srcs = [_prepare_source(s, R, size) for s in R["sources"]]
        pool = []
        for s in srcs:
            pool.extend(T.variants(s, XF[R["xf"]]))
        phase = None
        if R.get("rows"):
            phase = (size / R["rows"], 6)
        out, picks = T.quilt(pool, size, R.get("patch", 256), R.get("overlap", 64), rng, tol=0.1, avoid=2.0,
                             phase_y=phase)
    out = np.clip(T.delight(out, R["flatten"]), 0.0, 1.0)
    wet = None
    if "sfs" in R:
        out, h01 = _sfs(out, R["sfs"])
    else:
        hs = R["height"]
        h = SF.height_luma(out, hs["bands"], hs.get("grooves"), hs.get("ridges"))
        if hs.get("row_relief"):
            # furrows: the clod-covered ridges are the brighter rows; their row-mean brightness (periodic, since
            # quilting kept the row phase) becomes a smooth ridge / furrow profile added to the height
            prof = T.luma(out).mean(1)
            prof = T.blur_periodic(np.repeat(prof[:, None], 8, 1), size / R["rows"] / 8.0)[:, 0]
            prof = (prof - prof.mean()) / max(float(prof.std()), 1e-6)
            h = h + hs["row_relief"] * prof[:, None]
        h01 = T.normalize01(h, 0.5, 99.5)
    if "puddles" in R:
        out, h01, wet = _puddles(out, h01, R["puddles"])
    out = T.saturate(out, R.get("sat", 1.0))
    out = T.match_mean_lin(out, np.asarray(R["target"], np.float32) / 255.0)
    rough = dict(R["rough"], seed=int(rng.integers(1 << 30)))
    N, M, info = SF.maps(out, h01, R["tile_m"], R["tilt"], R["ao"][0], R["ao"][1], rough, flat_mask=wet)
    if wet is not None:
        M[..., 1] = M[..., 1] * (1 - wet) + R["puddles"]["rough"] * wet
        info["roughness_range"] = [round(float(np.percentile(M[..., 1], 1)), 3),
                                   round(float(np.percentile(M[..., 1], 99)), 3)]
    files = save_set(group, prefix or f"T_Ground_{name}", out, N, M)
    info.update(seam=[round(v, 2) for v in T.seam_error(out)], seam_h=[round(v, 2) for v in T.seam_error(h01)],
                lowfreq_pct=round(T.lowfreq_std(out), 2),
                quilt_patches=len(picks))
    log(f"{group} {name}: tilt {info['mean_tilt_deg']} deg, height {info['height_m']} m, "
        f"lowfreq {info['lowfreq_pct']} %, seam {info['seam']} / height {info['seam_h']}")
    entry = dict(name=entry_name or f"Ground_{name}", group=group, files=files, tile_m=R["tile_m"],
                 size=size, source="AI", ai_sources=[f"SourceArt/AI/Textures/{s}.png" for s in R["sources"]],
                 metallic=0, roughness_range=info["roughness_range"], normal_strength=1.0,
                 height_m=info["height_m"], mean_albedo=SF.mean_albedo_linear(out))
    if group == "Terrain":
        entry["layer"] = name
    return entry, dict(D=out, N=N, M=M)
