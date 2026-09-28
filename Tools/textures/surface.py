"""Surface maps from an albedo: height (B), normal (_N, DirectX), ambient occlusion (R) and roughness (G).

Height is always derived from the processed albedo, so it lines up with what the eye sees:
- `height_luma`: band-passed luma (bright = high), plus optional grooves for thin dark lines (cracks / joints) found
  with a black top-hat, or ridges for thin bright lines (light mortar) with a white top-hat.
- `height_sfs`: for sources painted with a directional light (wind-rippled snow and sand) the luma is treated as the
  slope along the light direction and integrated in the Fourier domain (a linearised shape-from-shading), so the
  recovered relief matches the painted relief instead of being shifted by a quarter wave.
Normal strength is set by a target mean surface tilt (degrees), which keeps every material in a sensible range
whatever its texel size; the physical height scale that results is reported in the manifest.
"""
import numpy as np

import texlib as T


def _bl(periodic):
    return T.blur_periodic if periodic else T.blur


def minmax_filter(a, r, op, periodic=True):
    """Separable square min / max filter of radius r (window 2r+1)."""
    mode = "wrap" if periodic else "reflect"
    out = a
    for ax in (0, 1):
        pad = [(0, 0), (0, 0)]
        pad[ax] = (r, r)
        p = np.pad(out, pad, mode=mode)
        w = np.lib.stride_tricks.sliding_window_view(p, 2 * r + 1, axis=ax)
        out = (w.min(-1) if op == "min" else w.max(-1)).astype(np.float32)
    return out


def tophat(y, r, dark=True, periodic=True):
    """Black top-hat (closing - y) for thin dark lines, white top-hat (y - opening) for thin bright lines."""
    if dark:
        c = minmax_filter(minmax_filter(y, r, "max", periodic), r, "min", periodic)
        return np.maximum(c - y, 0.0)
    o = minmax_filter(minmax_filter(y, r, "min", periodic), r, "max", periodic)
    return np.maximum(y - o, 0.0)


def height_luma(albedo, bands, grooves=None, ridges=None, periodic=True, invert=False, lines=()):
    """bands: [(sigma_fine, sigma_coarse, weight)] band-passes of luma (in px). grooves / ridges:
    (radius_px, weight, blur_px) thin dark lines -> grooves / thin bright lines -> ridges. lines: extra
    [(kind 'dark' | 'bright', radius_px, signed weight, blur_px)], e.g. ('bright', 6, -1.0, 1.0) turns light
    recessed mortar into grooves."""
    y = T.luma(albedo)
    bl = _bl(periodic)
    h = np.zeros_like(y)
    for s0, s1, w in bands:
        f = bl(y, s0) if s0 > 0 else y
        h += w * (f - bl(y, s1))
    if invert:
        h = -h
    sd = max(float(h.std()), 1e-6)
    h /= sd
    specs = list(lines)
    if grooves:
        specs.append(("dark", grooves[0], -grooves[1], grooves[2]))
    if ridges:
        specs.append(("bright", ridges[0], ridges[1], ridges[2]))
    for kind, r, w, b in specs:
        t = tophat(y, r, kind == "dark", periodic)
        if b > 0:
            t = bl(t, b)
        t /= max(float(np.percentile(t, 99.5)), 1e-6)
        h += w * 2.0 * np.clip(t, 0.0, 1.5)
    return h.astype(np.float32)


def height_sfs(albedo, d, sigma_hp, eps=0.02, periodic=True):
    """Integrate the painted shading along the light travel direction d = (dx, dy) (image axes, y down).
    Light from the top of the image -> d = (0, 1). Returns an unnormalised periodic height."""
    y = T.luma(T.srgb_to_lin(albedo))
    s = y / max(float(y.mean()), 1e-6) - 1.0
    s = s - _bl(periodic)(s, sigma_hp)
    h_, w_ = s.shape
    ky = np.fft.fftfreq(h_)[:, None]
    kx = np.fft.fftfreq(w_)[None, :]
    kd = 2.0 * np.pi * (kx * d[0] + ky * d[1])
    k2 = (2.0 * np.pi) ** 2 * (kx * kx + ky * ky)
    F = np.fft.fft2(s)
    # regularised inverse of the directional derivative; eps scales with |k|^2 so that frequencies nearly
    # perpendicular to the light (which carry no shading information) are damped instead of blowing up
    H = F * np.conj(1j * kd) / (kd * kd + eps * k2 + 1e-12)
    H[0, 0] = 0.0
    h = np.real(np.fft.ifft2(H)).astype(np.float32)
    return h / max(float(h.std()), 1e-6)


def tilt_amplitude(h01, texel_m, target_deg, blur_px=0.8):
    """Height amplitude (m) for which the mean surface tilt of h01 * amplitude equals target_deg."""
    hb = T.blur_periodic(h01, blur_px) if blur_px > 0 else h01
    gx = (np.roll(hb, -1, 1) - np.roll(hb, 1, 1)) / (2 * texel_m)
    gy = (np.roll(hb, -1, 0) - np.roll(hb, 1, 0)) / (2 * texel_m)
    g = np.sqrt(gx * gx + gy * gy)
    lo, hi = 1e-6, 10.0
    tgt = np.radians(target_deg)
    for _ in range(50):
        mid = np.sqrt(lo * hi)
        if np.arctan(mid * g).mean() < tgt:
            lo = mid
        else:
            hi = mid
    return float(np.sqrt(lo * hi))


def maps(albedo, h01, tile_m, tilt_deg, ao_radii_m, ao_strength, rough, normal_blur_px=0.8, periodic=True,
         flat_mask=None):
    """Returns (normal RGB, M RGB = AO / roughness / height, info dict).
    rough = dict(base, h=var with height, lum=var with albedo luma, lo, hi, noise=(amp, cycles), seed)."""
    n = h01.shape[0]
    texel = tile_m / n
    amp = tilt_amplitude(h01, texel, tilt_deg, normal_blur_px)
    hn = T.blur_periodic(h01, normal_blur_px) if normal_blur_px > 0 else h01
    if flat_mask is not None:            # water in puddles: flatten the normal
        hn = hn * (1 - flat_mask) + T.blur_periodic(hn, 6.0) * flat_mask
    N = T.normal_from_height(hn * amp, texel)
    if flat_mask is not None:
        flat = np.array([0.5, 0.5, 1.0], np.float32)
        N = N * (1 - flat_mask[..., None]) + flat * flat_mask[..., None]
        N = _renorm(N)
    ao = T.ao_from_height(h01, [max(0.7, r / texel) for r in ao_radii_m], ao_strength, periodic)
    y = T.luma(albedo)
    yn = (y - y.mean()) / max(float(y.std()), 1e-6)
    hc = (h01 - h01.mean()) / max(float(h01.std()), 1e-6)
    g = rough["base"] + rough.get("h", 0.0) * hc * 0.1 + rough.get("lum", 0.0) * yn * 0.1
    if rough.get("noise"):
        a, cyc = rough["noise"]
        g = g + a * T.fbm_periodic(h01.shape, np.random.default_rng(rough.get("seed", 7)), cyc, 4)
    g = np.clip(g, rough.get("lo", 0.05), rough.get("hi", 1.0)).astype(np.float32)
    M = np.stack([ao, g, h01], -1)
    tilt = float(np.degrees(np.arccos(np.clip(N[..., 2] * 2 - 1, -1, 1))).mean())
    info = dict(height_m=round(amp, 4), mean_tilt_deg=round(tilt, 1),
                roughness_range=[round(float(np.percentile(g, 1)), 3), round(float(np.percentile(g, 99)), 3)])
    return N, M, info


def _renorm(N):
    v = N * 2 - 1
    v /= np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-6)
    return (v * 0.5 + 0.5).astype(np.float32)


def mean_albedo_linear(albedo, alpha=None):
    lin = T.srgb_to_lin(albedo)
    if alpha is not None:
        w = (alpha > 0.5).astype(np.float32)
        m = (lin * w[..., None]).reshape(-1, 3).sum(0) / max(float(w.sum()), 1.0)
    else:
        m = lin.reshape(-1, 3).mean(0)
    return [round(float(x), 4) for x in m]
