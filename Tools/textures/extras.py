"""Extras: terrain macro noise, ocean wave normal map, shoreline foam (all procedural, seamless, 1024 px).

- T_Macro_Noise.png: R, G, B = three independent smooth noise fields (about 3, 7 and 16 features per tile), each
  histogram-equalised to a flat 0..1 distribution so thresholds behave the same everywhere. The terrain material
  samples the texture at ~150 m, 40 m and 12 m per repeat to break up tiling and tint the ground.
- T_Ocean_N.png: small wind waves from a directional spectrum with integer wave numbers (exactly periodic),
  normals from the analytic slopes (DirectX convention like every _N here). Suggested repeat: 12 m.
- T_Foam_D.png: white shoreline foam, RGB = foam colour (dilated), A = foam coverage (lacy cells + streaks).
"""
import numpy as np

import texlib as T
from common import rng_for, out_path

SIZE = 1024
NAMES = ["Macro_Noise", "Ocean", "Foam"]


def _equalize(a):
    order = np.argsort(a, axis=None)
    ranks = np.empty(a.size, np.float32)
    ranks[order] = np.linspace(0.0, 1.0, a.size, dtype=np.float32)
    return ranks.reshape(a.shape)


def macro_noise():
    rng = rng_for("Terrain_Macro_Noise")
    chans = []
    for cycles, oct_, gain in ((2, 3, 0.35), (4, 3, 0.35), (8, 3, 0.35)):
        n = T.fbm_periodic((SIZE, SIZE), rng, cycles, oct_, gain)
        n = T.blur_periodic(n, SIZE / (cycles * 24.0))          # smooth: no detail finer than ~1/24 feature
        chans.append(_equalize(n))
    img = np.stack(chans, -1)
    T.save(out_path("Terrain", "T_Macro_Noise.png"), img)
    corr = np.corrcoef(img.reshape(-1, 3).T)
    entry = dict(name="Macro_Noise", group="Terrain", files={"D": "Terrain/T_Macro_Noise.png"}, size=SIZE,
                 source="Procedural", linear=True, tile_m=[150.0, 40.0, 12.0],
                 channels=dict(R="broad smooth noise, ~2 cycles per repeat (e.g. sampled at ~150 m)",
                               G="medium smooth noise, ~4 cycles per repeat (e.g. sampled at ~40 m)",
                               B="fine smooth noise, ~8 cycles per repeat (e.g. sampled at ~12 m)"),
                 note="import as linear (sRGB off); each channel is histogram-equalised (uniform 0..1)",
                 channel_correlation=[round(float(corr[0, 1]), 3), round(float(corr[0, 2]), 3),
                                      round(float(corr[1, 2]), 3)],
                 seam=[round(v, 2) for v in T.seam_error(img)])
    return entry, dict(D=img)


def ocean():
    """Directional wind-sea spectrum (Phillips-like with a cos^4 spread), integer wave numbers only."""
    rng = rng_for("Water_Ocean")
    tile_m = 12.0
    k = np.fft.fftfreq(SIZE) * SIZE                     # cycles per tile (integers)
    kx, ky = np.meshgrid(k, k)
    kk = np.sqrt(kx * kx + ky * ky)
    kmag = 2 * np.pi * kk / tile_m                      # rad / m
    wind = np.array([np.cos(0.35), np.sin(0.35)])
    L = 1.1                                             # ~ peak wavelength scale (m) for a light breeze
    with np.errstate(divide="ignore", invalid="ignore"):
        cosang = (kx * wind[0] + ky * wind[1]) / np.maximum(kk, 1e-9)
        P = np.exp(-1.0 / np.maximum((kmag * L) ** 2, 1e-9)) / np.maximum(kmag, 1e-9) ** 4
        P *= np.maximum(cosang, 0.0) ** 4 * 0.85 + 0.15    # some cross-wind chop
        P *= np.exp(-(kmag * 0.035) ** 2)                   # damp capillary waves below ~3 cm
    P[0, 0] = 0.0
    P[kk > SIZE / 2.2] = 0.0
    amp = np.sqrt(P)
    H = (rng.standard_normal((SIZE, SIZE)) + 1j * rng.standard_normal((SIZE, SIZE))) * amp
    # slopes analytically: dh/dx = IFFT(i kx H), with kx in rad/m
    kxr = 2 * np.pi * kx / tile_m
    kyr = 2 * np.pi * ky / tile_m
    h = np.real(np.fft.ifft2(H))
    sx = np.real(np.fft.ifft2(1j * kxr * H))
    sy = np.real(np.fft.ifft2(1j * kyr * H))
    s = np.sqrt(sx * sx + sy * sy)
    scale = np.tan(np.radians(11.0)) / max(float(np.median(s)), 1e-9)   # median slope ~11 deg
    sx, sy = sx * scale, sy * scale
    n = np.stack([-sx, -sy, np.ones_like(sx)], -1)       # y = image down: G = -dh/dv (DirectX)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    N = (n * 0.5 + 0.5).astype(np.float32)
    T.save(out_path("Water", "T_Ocean_N.png"), N)
    tilt = float(np.degrees(np.arccos(np.clip(n[..., 2], -1, 1))).mean())
    entry = dict(name="Ocean", group="Water", files={"N": "Water/T_Ocean_N.png"}, tile_m=tile_m, size=SIZE,
                 source="Procedural", normal_strength=1.0, mean_tilt_deg=round(tilt, 1),
                 wind_direction_uv=[round(float(wind[0]), 3), round(float(wind[1]), 3)],
                 note="pan two copies in different directions / scales for motion; height_rms_m "
                      f"{float(h.std() * scale):.3f} at the stated tile size",
                 seam=[round(v, 2) for v in T.seam_error(N)])
    return entry, dict(N=N)


def _sample_wrap(img, X, Y):
    """Bilinear lookup on a torus (X, Y in pixels)."""
    h, w = img.shape
    x0 = np.floor(X).astype(int)
    y0 = np.floor(Y).astype(int)
    fx, fy = X - x0, Y - y0
    x0 %= w
    y0 %= h
    x1, y1 = (x0 + 1) % w, (y0 + 1) % h
    return ((img[y0, x0] * (1 - fx) + img[y0, x1] * fx) * (1 - fy) +
            (img[y1, x0] * (1 - fx) + img[y1, x1] * fx) * fy)


def _bubbles(rng, cells, rmin, rmax, density, warp):
    """Foam between round bubbles: every cell point gets a random radius (shrunk where foam is dense)."""
    f1, _, cid = T.worley_periodic(SIZE, cells, rng, jitter=1.0)
    radius = rng.uniform(rmin, rmax, cells * cells).astype(np.float32)[cid]
    radius = radius * (1.15 - 0.55 * density)
    solid = np.clip((f1 - radius) / 0.05, 0.0, 1.0)            # 0 inside a bubble, 1 in the foam between
    return _sample_wrap(solid, *warp)


def foam():
    """Sea foam: foam between round bubbles of two sizes (domain-warped so nothing looks cellular), thickening
    into solid foam in dense patches and breaking up into drifting streaks; A = coverage."""
    rng = rng_for("Water_Foam")
    Y, X = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
    wx = T.fbm_periodic((SIZE, SIZE), rng, 5, 3, 0.5) * 9.0
    wy = T.fbm_periodic((SIZE, SIZE), rng, 5, 3, 0.5) * 9.0
    warp = (X + wx, Y + wy)
    patches = T.normalize01(T.fbm_periodic((SIZE, SIZE), rng, 3, 4, 0.5, aniso=(1.8, 1.0)), 1, 99)
    streak = T.normalize01(T.fbm_periodic((SIZE, SIZE), rng, 9, 3, 0.45, aniso=(4.0, 1.0)), 1, 99)
    density = np.clip(0.8 * patches + 0.3 * streak - 0.1, 0.0, 1.0)
    big = _bubbles(rng, 26, 0.35, 0.62, density, warp)
    small = _bubbles(rng, 70, 0.25, 0.5, density, warp)
    lace = big * (0.35 + 0.65 * small)
    a = lace * np.clip((density - 0.22) / 0.3, 0.0, 1.0)
    a = T.blur_periodic(a, 0.8)
    a = np.clip((a - 0.2) / 0.55, 0.0, 1.0)
    a = (a * a * (3 - 2 * a)).astype(np.float32)
    tone = 0.92 + 0.08 * T.normalize01(T.fbm_periodic((SIZE, SIZE), rng, 8, 3))
    tone = tone * (0.94 + 0.06 * a)                                # thin lace slightly greyer than solid foam
    rgb = np.stack([tone * 0.965, tone * 0.985, tone], -1).astype(np.float32)
    D = np.concatenate([rgb, a[..., None]], -1)
    T.save(out_path("Water", "T_Foam_D.png"), D)
    entry = dict(name="Foam", group="Water", files={"D": "Water/T_Foam_D.png"}, tile_m=6.0, size=SIZE,
                 source="Procedural", translucent=True, coverage=round(float(a.mean()), 3),
                 note="RGB = foam colour (sRGB), A = foam opacity; scale A by a shoreline / wave-crest mask",
                 mean_albedo=[round(float(x), 4) for x in T.srgb_to_lin(rgb).reshape(-1, 3).mean(0)],
                 seam=[round(v, 2) for v in T.seam_error(D)])
    return entry, dict(D=D)


def build(name):
    return {"Macro_Noise": macro_noise, "Ocean": ocean, "Foam": foam}[name]()
