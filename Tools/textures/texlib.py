"""Image helpers for the LA PLACE texture builder (numpy + Pillow only, no scipy / OpenCV).

Conventions used everywhere in Tools/textures:
- Images are float32 numpy arrays, shape (H, W) or (H, W, C), values 0..1. Colour is sRGB unless a name says `lin`.
- Row 0 is the top of the image; x runs right (U), y runs down (V as stored in the PNG / as Unreal samples it).
- Every filter that has a `periodic` flavour treats the image as a torus, so tiling textures stay seamless.
- Everything is deterministic: randomness always comes from an explicit numpy Generator.
"""
import numpy as np
from PIL import Image

# ----------------------------------------------------------------------------------------------- I/O


def load(path, mode="RGB"):
    im = Image.open(path).convert(mode)
    a = np.asarray(im).astype(np.float32) / 255.0
    return a


def to_u8(a):
    return (np.clip(a, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)


def save(path, a):
    a = np.asarray(a)
    if a.ndim == 2:
        Image.fromarray(to_u8(a), "L").save(path, optimize=False)
    elif a.shape[2] == 3:
        Image.fromarray(to_u8(a), "RGB").save(path, optimize=False)
    else:
        Image.fromarray(to_u8(a), "RGBA").save(path, optimize=False)


def resize(a, size, resample=Image.LANCZOS):
    """Float-precision resize (per channel through Pillow 'F' images). size = (w, h) or int."""
    if isinstance(size, int):
        size = (size, size)
    if a.ndim == 2:
        return np.asarray(Image.fromarray(a.astype(np.float32), "F").resize(size, resample), dtype=np.float32)
    return np.stack([resize(a[..., c], size, resample) for c in range(a.shape[2])], -1)


def resize_periodic(a, size):
    """Resize a tiling image without edge artefacts: pad by wrapping, resize, crop."""
    if isinstance(size, int):
        size = (size, size)
    h, w = a.shape[:2]
    p = max(8, h // 16)
    pad = [(p, p), (p, p)] + ([(0, 0)] if a.ndim == 3 else [])
    big = np.pad(a, pad, mode="wrap")
    sx, sy = size[0] / w, size[1] / h
    out = resize(big, (int(round((w + 2 * p) * sx)), int(round((h + 2 * p) * sy))))
    ox, oy = int(round(p * sx)), int(round(p * sy))
    return out[oy:oy + size[1], ox:ox + size[0]]


# ----------------------------------------------------------------------------------------------- colour


def srgb_to_lin(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4).astype(np.float32)


def lin_to_srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1 / 2.4) - 0.055).astype(np.float32)


def luma(a):
    """Rec.709 weights (on whatever encoding `a` is in)."""
    return a[..., 0] * 0.2126 + a[..., 1] * 0.7152 + a[..., 2] * 0.0722


def rgb_to_hsv(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mx = np.max(a, -1)
    mn = np.min(a, -1)
    d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-6
    rc = np.where(m, (mx - r) / np.maximum(d, 1e-6), 0)
    gc = np.where(m, (mx - g) / np.maximum(d, 1e-6), 0)
    bc = np.where(m, (mx - b) / np.maximum(d, 1e-6), 0)
    h = np.where(r == mx, bc - gc, np.where(g == mx, 2.0 + rc - bc, 4.0 + gc - rc))
    h = np.where(m, (h / 6.0) % 1.0, 0.0)
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0.0)
    return np.stack([h, s, mx], -1).astype(np.float32)


def saturate(a, k):
    """Scale saturation around the per-pixel luma (k < 1 desaturates)."""
    y = luma(a)[..., None]
    return np.clip(y + (a - y) * k, 0.0, 1.0)


def match_mean_lin(a, target_srgb, strength=1.0):
    """Scale each channel in linear light so that the image mean hits target (given in sRGB 0..1)."""
    lin = srgb_to_lin(a)
    cur = lin.reshape(-1, 3).mean(0)
    tgt = srgb_to_lin(np.asarray(target_srgb, np.float32))
    gain = (tgt / np.maximum(cur, 1e-5)) ** strength
    return lin_to_srgb(lin * gain)


def contrast_lin(a, k, pivot=None):
    """Contrast around the mean in linear light (multiplicative, keeps the mean)."""
    lin = srgb_to_lin(a)
    m = lin.reshape(-1, lin.shape[-1]).mean(0) if pivot is None else pivot
    out = m * np.power(np.maximum(lin, 1e-6) / np.maximum(m, 1e-6), k)
    out *= m / np.maximum(out.reshape(-1, out.shape[-1]).mean(0), 1e-6)
    return lin_to_srgb(out)


# ----------------------------------------------------------------------------------------------- filters


def _gauss_rfft(shape, sigma):
    h, w = shape
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.rfftfreq(w)[None, :]
    return np.exp(-2.0 * (np.pi ** 2) * (sigma ** 2) * (fx * fx + fy * fy)).astype(np.float32)


def blur_periodic(a, sigma):
    if sigma <= 0:
        return a.copy()
    k = _gauss_rfft(a.shape[:2], sigma)
    if a.ndim == 2:
        return np.fft.irfft2(np.fft.rfft2(a) * k, s=a.shape).astype(np.float32)
    return np.stack([blur_periodic(a[..., c], sigma) for c in range(a.shape[2])], -1)


def blur(a, sigma):
    """Gaussian blur with reflected borders (for non-tiling images)."""
    if sigma <= 0:
        return a.copy()
    p = int(min(max(a.shape[:2]) - 1, np.ceil(3 * sigma)))
    pad = [(p, p), (p, p)] + ([(0, 0)] if a.ndim == 3 else [])
    b = blur_periodic(np.pad(a, pad, mode="reflect"), sigma)
    return b[p:p + a.shape[0], p:p + a.shape[1]]


def box_periodic(a, r):
    """Box mean of radius r (window 2r+1) on a torus, via cumulative sums."""
    out = a
    for ax in (0, 1):
        n = out.shape[ax]
        pad = [(0, 0)] * out.ndim
        pad[ax] = (r + 1, r)
        c = np.cumsum(np.pad(out, pad, mode="wrap"), axis=ax, dtype=np.float64)
        hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=ax)
        lo = np.take(c, np.arange(0, n), axis=ax)
        out = ((hi - lo) / (2 * r + 1)).astype(np.float32)
    return out


def periodic_component(u):
    """Moisan (2011) periodic + smooth decomposition: returns the periodic component of u (per channel)."""
    if u.ndim == 3:
        return np.stack([periodic_component(u[..., c]) for c in range(u.shape[2])], -1)
    u = u.astype(np.float64)
    h, w = u.shape
    v = np.zeros_like(u)
    v[0, :] += u[-1, :] - u[0, :]
    v[-1, :] += u[0, :] - u[-1, :]
    v[:, 0] += u[:, -1] - u[:, 0]
    v[:, -1] += u[:, 0] - u[:, -1]
    q = np.arange(h)[:, None]
    r = np.arange(w)[None, :]
    den = 2 * np.cos(2 * np.pi * q / h) + 2 * np.cos(2 * np.pi * r / w) - 4
    den[0, 0] = 1.0
    s = np.fft.fft2(v) / den
    s[0, 0] = 0.0
    smooth = np.real(np.fft.ifft2(s))
    return (u - smooth).astype(np.float32)


def degrid(a, period=16, win=3):
    """Remove the faint decoder grid of AI images (spectral spikes at multiples of 1/period cycles per pixel, i.e.
    the 8 and 16 px harmonics): every harmonic bin is scaled down to the median magnitude of its neighbourhood
    (a (2*win+1)^2 window without its centre 3x3), keeping its phase. Natural content at those frequencies is only
    touched where it stands out as a spike. Works on any image; best on a periodic one."""
    if a.ndim == 3:
        return np.stack([degrid(a[..., c], period, win) for c in range(a.shape[2])], -1)
    h, w = a.shape
    F = np.fft.fft2(a.astype(np.float64))
    mag = np.abs(F)
    sy, sx = h // period, w // period
    for i in range(0, h, sy):
        for j in range(0, w, sx):
            if i == 0 and j == 0:
                continue
            ys = [(i + d) % h for d in range(-win, win + 1)]
            xs = [(j + d) % w for d in range(-win, win + 1)]
            block = mag[np.ix_(ys, xs)].copy()
            ring = np.ones_like(block, bool)
            ring[win - 1:win + 2, win - 1:win + 2] = False
            ref = np.median(block[ring])
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    y, x = (i + dy) % h, (j + dx) % w
                    if mag[y, x] > ref:
                        F[y, x] *= ref / mag[y, x]
    return np.real(np.fft.ifft2(F)).astype(np.float32)


def delight(a, sigma, strength=1.0, periodic=True, lum_only=False):
    """Remove low-frequency lighting / colour drift: divide by a heavy blur (in linear light), keep the mean."""
    lin = srgb_to_lin(a)
    bl = blur_periodic if periodic else blur
    if lum_only:
        y = luma(lin)
        b = bl(y, sigma)[..., None]
        m = y.mean()
    else:
        b = bl(lin, sigma)
        m = lin.reshape(-1, 3).mean(0)
    out = lin * np.power(m / np.maximum(b, 1e-4), strength)
    return lin_to_srgb(out)


def flatten_lowfreq(a, kc=2.5, width=1.5):
    """Remove tile-scale blotches and gradients from a periodic image without touching finer variation: in log
    linear light, every channel's Fourier modes with 0 < |k| <= kc cycles per tile are removed (cosine taper up to
    kc + width); the mean colour (DC) is kept. Block-to-block or clump-to-clump variation above that survives."""
    lin = srgb_to_lin(a)
    h, w = lin.shape[:2]
    ky = np.fft.fftfreq(h)[:, None] * h
    kx = np.fft.rfftfreq(w)[None, :] * w
    k = np.sqrt(kx * kx + ky * ky)
    keep = np.clip((k - kc) / max(width, 1e-6), 0.0, 1.0)
    keep = 0.5 - 0.5 * np.cos(np.pi * keep)
    keep[0, 0] = 1.0
    out = np.empty_like(lin)
    for c in range(lin.shape[2]):
        L = np.log(np.maximum(lin[..., c], 1e-4))
        out[..., c] = np.exp(np.fft.irfft2(np.fft.rfft2(L) * keep, s=(h, w)))
    # restore the exact linear mean per channel (the log filter keeps the geometric mean)
    out *= lin.reshape(-1, lin.shape[2]).mean(0) / np.maximum(out.reshape(-1, lin.shape[2]).mean(0), 1e-6)
    return lin_to_srgb(np.clip(out, 0.0, 1.0))


def replace_hue(a, hue_lo, hue_hi, target_srgb, strength=1.0, min_sat=0.06):
    """Recolour pixels whose hue lies in [hue_lo, hue_hi] (0..1 wheel) toward a target colour, keeping their
    relative brightness (e.g. unwanted purple flowers in a moss source -> pale lichen)."""
    hsv = rgb_to_hsv(a)
    hh, ss = hsv[..., 0], hsv[..., 1]
    inside = ((hh >= hue_lo) & (hh <= hue_hi)).astype(np.float32) * np.clip((ss - min_sat) / 0.1, 0.0, 1.0)
    inside = blur(inside, 1.0) * strength
    lin = srgb_to_lin(a)
    y = luma(lin)
    tgt = srgb_to_lin(np.asarray(target_srgb, np.float32) / 255.0)
    rec = tgt[None, None, :] * (y / max(float(luma(tgt[None, None, :])[0, 0]), 1e-4))[..., None]
    out = lin * (1 - inside[..., None]) + rec * inside[..., None]
    return lin_to_srgb(np.clip(out, 0.0, 1.0))


def highpass(a, sigma, periodic=True):
    bl = blur_periodic if periodic else blur
    return a - bl(a, sigma)


def normalize01(a, lo_pct=0.5, hi_pct=99.5):
    lo, hi = np.percentile(a, [lo_pct, hi_pct])
    return np.clip((a - lo) / max(hi - lo, 1e-6), 0.0, 1.0).astype(np.float32)


def seam_error(a):
    """Mean |jump| across the wrap seams divided by the mean neighbour difference (1.0 = invisible seam)."""
    dx = np.abs(np.diff(a, axis=1)).mean()
    dy = np.abs(np.diff(a, axis=0)).mean()
    sx = np.abs(a[:, 0] - a[:, -1]).mean()
    sy = np.abs(a[0] - a[-1]).mean()
    return float(sx / max(dx, 1e-6)), float(sy / max(dy, 1e-6))


def lowfreq_std(a, cells=8):
    """Std of block means of luma relative to the mean (in %): a proxy for visible blotches when tiled."""
    y = luma(a) if a.ndim == 3 else a
    h, w = y.shape
    b = y[:h // cells * cells, :w // cells * cells].reshape(cells, h // cells, cells, w // cells).mean((1, 3))
    return float(100.0 * b.std() / max(b.mean(), 1e-6))


def estimate_period(img, axis, lo, hi):
    """Dominant repeat length (px, sub-pixel) of an image along an axis (0 = rows / y, 1 = columns / x), from the
    autocorrelation of its high-passed luma. Candidates are interior local maxima in [lo, hi); the one standing
    out most from the autocorrelation around it (half a period either side) wins, so a slow trend (banding,
    lighting) cannot pull the answer onto the search boundary. Returns (period, prominence); prominence 0 means
    no period was found."""
    y = luma(img) if img.ndim == 3 else img
    y = y - blur_periodic(y, max(hi, 8))
    n = y.shape[axis]
    F = np.fft.fft(y, axis=axis)
    ac = np.real(np.fft.ifft(F * np.conj(F), axis=axis)).mean(axis=1 - axis)
    ac = ac / max(ac[0], 1e-12)
    best = (float(lo), 0.0)
    for k in range(max(lo, 2), min(hi, n // 2 - 1)):
        if not (ac[k] > ac[k - 1] and ac[k] >= ac[k + 1]):
            continue
        d = max(3, k // 2)
        if k + d >= n:
            continue
        prom = ac[k] - 0.5 * (ac[k - d] + ac[k + d])
        if prom > best[1]:
            a, b, c = ac[k - 1], ac[k], ac[k + 1]
            den = a - 2 * b + c
            off = 0.5 * (a - c) / den if abs(den) > 1e-12 else 0.0
            best = (float(k + np.clip(off, -0.5, 0.5)), float(prom))
    return best


# ----------------------------------------------------------------------------------------------- noise


def fbm_periodic(shape, rng, base_cycles, octaves=5, gain=0.5, lacunarity=2.0, aniso=(1.0, 1.0)):
    """Seamless fractal noise from band-limited random spectra with integer frequencies (always periodic).
    base_cycles: features per tile at the first octave. Returns zero-mean, unit-std float32."""
    h, w = shape
    fy = np.fft.fftfreq(h)[:, None] * h / aniso[1]
    fx = np.fft.rfftfreq(w)[None, :] * w / aniso[0]
    f = np.sqrt(fx * fx + fy * fy)
    spec = np.zeros(f.shape, np.float32)
    amp = 1.0
    c = base_cycles
    for _ in range(octaves):
        spec += amp * np.exp(-0.5 * ((f - c) / (0.45 * c)) ** 2)
        amp *= gain
        c *= lacunarity
    spec[0, 0] = 0.0
    ph = rng.standard_normal(f.shape) + 1j * rng.standard_normal(f.shape)
    n = np.fft.irfft2(ph * spec, s=(h, w))
    n = (n - n.mean()) / max(n.std(), 1e-8)
    return n.astype(np.float32)


def worley_periodic(size, cells, rng, jitter=1.0):
    """Seamless cellular noise on a size x size torus with cells x cells jittered points.
    Returns (F1, F2, cell_id) with distances in units of one cell."""
    pts = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells), indexing="ij"), -1)
           + 0.5 + (rng.random((cells, cells, 2)) - 0.5) * jitter)          # [i(y), j(x)] -> (y, x) in cells
    ys = (np.arange(size) + 0.5) * cells / size
    xs = (np.arange(size) + 0.5) * cells / size
    Y, X = np.meshgrid(ys, xs, indexing="ij")
    ci = np.floor(Y).astype(int)
    cj = np.floor(X).astype(int)
    f1 = np.full((size, size), 1e9, np.float32)
    f2 = np.full((size, size), 1e9, np.float32)
    idx = np.zeros((size, size), np.int32)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ii = (ci + di)
            jj = (cj + dj)
            p = pts[ii % cells, jj % cells]
            py = p[..., 0] + (ii - ii % cells)
            px = p[..., 1] + (jj - jj % cells)
            d = np.sqrt((Y - py) ** 2 + (X - px) ** 2).astype(np.float32)
            cid = ((ii % cells) * cells + (jj % cells)).astype(np.int32)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            idx = np.where(closer, cid, idx)
            f1 = np.where(closer, d, f1)
    return f1, f2, idx


# ----------------------------------------------------------------------------------------------- quilting


def _min_cut_vertical(err, periodic=False):
    """err: (H, O) overlap error. Returns the x index per row of the minimal-error vertical path.
    periodic=True forces the path to end where it starts (so a cut across a wrap-around image stays continuous)."""
    h, o = err.shape

    def run(first):
        E = err.astype(np.float64).copy()
        E[0] = first
        back = np.zeros((h, o), np.int8)
        for y in range(1, h):
            prev = E[y - 1]
            left = np.concatenate([[np.inf], prev[:-1]])
            right = np.concatenate([prev[1:], [np.inf]])
            stack = np.stack([left, prev, right])
            k = np.argmin(stack, 0)
            E[y] += stack[k, np.arange(o)]
            back[y] = k - 1
        return E, back

    E, back = run(err[0].astype(np.float64))
    end = int(np.argmin(E[-1]))
    if periodic:
        first = np.full(o, np.inf)
        first[end] = float(err[0, end])
        E, back = run(first)
    path = np.zeros(h, np.int32)
    path[-1] = end
    for y in range(h - 1, 0, -1):
        path[y - 1] = path[y] + back[y, path[y]]
    return np.clip(path, 0, o - 1)


DIHEDRAL = [(0, False), (1, False), (2, False), (3, False), (0, True), (1, True), (2, True), (3, True)]
FLIPS_ONLY = [(0, False), (0, True), (2, False), (2, True)]      # keeps horizontal / vertical structure axes
HFLIP_ONLY = [(0, False), (0, True)]                             # keeps "up" (rows, strata lit from above)


def variants(img, transforms):
    """Rotated / mirrored copies of a periodic image: transforms = [(quarter_turns, mirror_x), ...]."""
    out = []
    for k, f in transforms:
        a = np.rot90(img, k, axes=(0, 1))
        if f:
            a = a[:, ::-1]
        out.append(np.ascontiguousarray(a))
    return out


def quilt(sources, out_size, patch, overlap, rng, tol=0.08, match_scale=2, phase_y=None, avoid=0.0,
          feather=1.5):
    """Toroidal image quilting (Efros & Freeman 2001, min-error boundary cuts) into a seamless out_size^2 tile.

    sources: list of periodic float images (H, W, C) at the output texel scale (they are sampled with wrap-around).
    patch:   patch step in output pixels (out_size must be a multiple); patches are patch + overlap wide.
    tol:     candidates within (1 + tol) of the best overlap error are chosen at random.
    phase_y: (period, slack) - only accept source offsets whose y phase matches the output row phase (for rows /
             furrows that must stay continuous and periodic).
    avoid:   penalty weight against re-using source positions that were already used (spreads the sampling).
    Returns the quilted image and a list of the chosen (source, y, x) positions.
    """
    N = out_size
    assert N % patch == 0
    n = N // patch
    p = patch + overlap
    o = overlap
    C = sources[0].shape[2]
    out = np.zeros((N, N, C), np.float32)
    filled = np.zeros((N, N), bool)
    ms = match_scale
    # matching is done on block-averaged copies (ms x ms) for speed
    small = []
    for s in sources:
        hs, ws = s.shape[:2]
        sm = s[:hs // ms * ms, :ws // ms * ms].reshape(hs // ms, ms, ws // ms, ms, C).mean((1, 3))
        F = [np.fft.rfft2(sm[..., c]) for c in range(C)]
        F2 = [np.fft.rfft2(sm[..., c] ** 2) for c in range(C)]
        small.append((sm, F, F2))
    used = [np.zeros(s[0].shape[:2], np.float32) for s in small]
    picks = []
    ps = p // ms
    for i in range(n):
        for j in range(n):
            y0, x0 = i * patch, j * patch
            ys = (y0 + np.arange(p)) % N
            xs = (x0 + np.arange(p)) % N
            T = out[np.ix_(ys, xs)]
            M = filled[np.ix_(ys, xs)]
            best = None
            if not M.any():
                si = int(rng.integers(len(sources)))
                hs, ws = sources[si].shape[:2]
                sy = int(rng.integers(hs))
                if phase_y is not None:
                    per = phase_y[0]
                    sy = int(round((y0 % per) + per * rng.integers(max(1, int(hs // per))))) % hs
                best = (si, sy, int(rng.integers(ws)))
            else:
                Ts = T[:ps * ms, :ps * ms].reshape(ps, ms, ps, ms, C).mean((1, 3))
                Ms = M[:ps * ms, :ps * ms].reshape(ps, ms, ps, ms).mean((1, 3))
                Ms = (Ms > 0.99).astype(np.float32)
                cands = []
                for si, (sm, F, F2) in enumerate(small):
                    hs, ws = sm.shape[:2]
                    Mp = np.zeros((hs, ws), np.float32)
                    Mp[:ps, :ps] = Ms
                    FM = np.conj(np.fft.rfft2(Mp))
                    acc = None
                    for c in range(C):
                        TMp = np.zeros((hs, ws), np.float32)
                        TMp[:ps, :ps] = Ms * Ts[..., c]
                        term = F2[c] * FM - 2.0 * F[c] * np.conj(np.fft.rfft2(TMp))
                        acc = term if acc is None else acc + term
                    ssd = np.fft.irfft2(acc, s=(hs, ws)) + float((Ms[..., None] * Ts ** 2).sum())
                    ssd = np.maximum(ssd, 0.0) / max(Ms.sum(), 1.0)
                    if avoid > 0:
                        ssd = ssd * (1.0 + avoid * used[si])
                    if phase_y is not None:
                        per, slack = phase_y
                        yy = np.arange(hs) * ms
                        ph = (yy - (y0 % per)) % per
                        dph = np.minimum(ph, per - ph)
                        ssd = np.where((dph <= slack)[:, None], ssd, np.inf)
                    cands.append(ssd)
                mins = min(float(c.min()) for c in cands)
                thr = mins * (1.0 + tol) + 1e-9
                pool = []
                for si, c in enumerate(cands):
                    yy, xx = np.nonzero(c <= thr)
                    pool.extend((si, int(a), int(b)) for a, b in zip(yy, xx))
                si, yy, xx = pool[int(rng.integers(len(pool)))]
                best = (si, yy * ms + int(rng.integers(ms)), xx * ms + int(rng.integers(ms)))
            si, sy, sx = best
            src = sources[si]
            hs, ws = src.shape[:2]
            P = src[np.ix_((sy + np.arange(p)) % hs, (sx + np.arange(p)) % ws)]
            # mark usage (in the small grid) to spread later picks
            sm_h, sm_w = used[si].shape
            uy = ((sy // ms) + np.arange(ps)) % sm_h
            ux = ((sx // ms) + np.arange(ps)) % sm_w
            used[si][np.ix_(uy, ux)] += 1.0
            picks.append(best)
            new = np.ones((p, p), np.float32)
            if M.any():
                err = ((P - T) ** 2).sum(-1)
                if M[:, :o].all():                       # left
                    path = _min_cut_vertical(err[:, :o])
                    for y in range(p):
                        new[y, :path[y]] = 0.0
                if M[:o, :].all():                       # top
                    path = _min_cut_vertical(err[:o, :].T)
                    for x in range(p):
                        new[:path[x], x] = 0.0
                if M[:, p - o:].all():                   # right (wrap)
                    path = _min_cut_vertical(err[:, p - o:])
                    for y in range(p):
                        new[y, p - o + path[y] + 1:] = 0.0
                if M[p - o:, :].all():                   # bottom (wrap)
                    path = _min_cut_vertical(err[p - o:, :].T)
                    for x in range(p):
                        new[p - o + path[x] + 1:, x] = 0.0
                if feather > 0:
                    new = blur(new, feather)
                new = np.where(M, new, 1.0)
            w_ = new[..., None]
            out[np.ix_(ys, xs)] = w_ * P + (1.0 - w_) * T
            filled[np.ix_(ys, xs)] = True
    return out, picks


def _self_quilt_x(img, o, feather=1.5, margin=7):
    """Make the left/right wrap seamless by overlapping the image's two ends by o columns and joining them along the
    min-error vertical cut; the result is o columns narrower. The new wrap is a pair of natural neighbour columns,
    the cut stays `margin` px inside the strip, and it ends where it starts, so the top / bottom wrap (made
    seamless before) stays seamless too."""
    h, w = img.shape[:2]
    A = img[:, :o]
    B = img[:, w - o:]
    err = ((A - B) ** 2).sum(-1) if img.ndim == 3 else (A - B) ** 2
    err = err.astype(np.float64)
    pen = np.zeros(o)
    pen[:margin] = pen[-margin:] = 1e3 * (err.mean() + 1e-6)
    path = _min_cut_vertical(err + pen[None, :], periodic=True)
    m = (np.arange(o)[None, :] >= path[:, None]).astype(np.float32)   # 1 -> A (continues into the image body)
    if feather > 0:
        k = int(np.ceil(3 * feather))
        mp = np.pad(np.pad(m, ((k, k), (0, 0)), mode="wrap"), ((0, 0), (k, k)), mode="edge")
        m = blur_periodic(mp, feather)[k:k + h, k:k + o]
    if img.ndim == 3:
        m = m[..., None]
    joined = m * A + (1.0 - m) * B
    return np.concatenate([joined, img[:, o:w - o]], axis=1)


def _fit_axis(img, axis, period, min_ov):
    """For a lattice axis: returns (image, overlap) such that the image length is k whole periods + overlap.
    k = round(n / period); if k periods plus the minimum overlap do not fit, the image is upscaled a little along
    that axis (never more than about half a period) instead of dropping a whole row of tiles."""
    n = img.shape[axis]
    k = max(1, int(round(n / period)))
    need = int(np.ceil(k * period + min_ov))
    if need > n:
        size = (img.shape[1], need) if axis == 0 else (need, img.shape[0])
        img = resize(img, size)
        n = need
    return img, int(n - round(k * period))


def joint_profile(img, r, dark=True):
    """Row profile of thin horizontal lines: row mean of a black (dark lines) or white (bright lines) top-hat."""
    y = luma(img) if img.ndim == 3 else img
    a = y
    for op in (("max", "min") if dark else ("min", "max")):      # closing (dark lines) / opening (bright lines)
        for ax in (0, 1):
            pad = [(0, 0), (0, 0)]
            pad[ax] = (r, r)
            w = np.lib.stride_tricks.sliding_window_view(np.pad(a, pad, mode="reflect"), 2 * r + 1, axis=ax)
            a = (w.max(-1) if op == "max" else w.min(-1)).astype(np.float32)
    out = np.maximum(a - y, 0.0) if dark else np.maximum(y - a, 0.0)
    prof = out.mean(1)
    k = np.exp(-0.5 * (np.arange(-4, 5) / 1.5) ** 2)
    return np.convolve(np.pad(prof, 4, mode="edge"), k / k.sum(), mode="valid").astype(np.float32)


def joint_period(img, lo, hi, dark=True):
    """Course height from the autocorrelation of the joint-line profile (robust to sediment banding, stains and
    lighting, which fool a plain luma autocorrelation). Returns (period, prominence)."""
    prof = joint_profile(img, int(max(3, round(lo / 12))), dark)
    prof = prof - prof.mean()
    n = prof.size
    F = np.fft.fft(prof)
    ac = np.real(np.fft.ifft(F * np.conj(F)))
    ac = ac / max(ac[0], 1e-12)
    best = (float(lo), 0.0)
    for k in range(max(lo, 2), min(hi, n // 2 - 1)):
        if not (ac[k] > ac[k - 1] and ac[k] >= ac[k + 1]):
            continue
        d = max(3, k // 3)
        prom = ac[k] - 0.5 * (ac[k - d] + ac[min(k + d, n - 1)])
        if prom > best[1]:
            a, b, c = ac[k - 1], ac[k], ac[k + 1]
            den = a - 2 * b + c
            off = 0.5 * (a - c) / den if abs(den) > 1e-12 else 0.0
            best = (float(k + np.clip(off, -0.5, 0.5)), float(prom))
    return best


def find_joints(img, period, dark=True):
    """Rows of horizontal joints (mortar lines, course shadow lines) in a coursed texture: peaks of the joint-line
    profile picked greedily with non-maximum suppression (0.6 period). Returns sorted rows."""
    prof = joint_profile(img, int(max(3, round(period / 12))), dark)
    thr = float(np.median(prof) + 0.5 * prof.std())
    order = np.argsort(prof)[::-1]
    taken = []
    for i in order:
        if prof[i] < thr:
            break
        if all(abs(int(i) - t) > 0.6 * period for t in taken):
            taken.append(int(i))
    return sorted(taken)


def crop_to_joints(img, period, dark=True, even=False, min_frac=0.75):
    """Crop rows so the image spans a whole number of courses from one joint to another: the vertical wrap then
    falls exactly on a joint and courses stack naturally (no half blocks, no mismatched vertical joints).
    Joints extrapolated one period beyond the first / last detected joint count too when they fall within a
    joint's half-width of the image border. even=True keeps an even course count so staggered rows alternate
    across the wrap. The crop must keep at least min_frac of the height (limits the vertical rescale).
    Returns (cropped image, courses) or (None, 0) if no clean joint pair is found."""
    h = img.shape[0]
    joints = find_joints(img, period, dark)
    if not joints:
        return None, 0
    tol = 0.06 * period
    cand = list(joints)
    top = joints[0] - period
    bot = joints[-1] + period
    if -tol <= top <= tol:
        cand.insert(0, 0)
    if h - tol <= bot <= h + tol:
        cand.append(h)
    best = None
    for i, j0 in enumerate(cand):
        for j1 in cand[i + 1:]:
            n = int(round((j1 - j0) / period))
            if n < 2 or (even and n % 2) or (j1 - j0) < min_frac * h:
                continue
            if abs((j1 - j0) - n * period) > 0.2 * period:
                continue
            if best is None or (j1 - j0) > best[1] - best[0]:
                best = (j0, j1, n)
    if best is None:
        return None, 0
    j0, j1, n = best
    return img[j0:j1], n


def make_tileable(img, period_x=None, period_y=None, overlap=96, feather=1.5, min_ov=48, presmoothed=False,
                  y_done=False):
    """Seamless version of an almost-tileable image. The Moisan periodic component first removes the low-frequency
    mismatch between opposite borders (unless `presmoothed`); then each axis is closed by a min-cut self-overlap
    (lattice-aware for rows of tiles / courses / planks: the two overlapping strips are a whole number of periods
    apart). The final wrap pairs are natural neighbour pixels, so there is no seam and no smoothed line either.
    The result is a little smaller than the input; resize it afterwards.
    Returns (image, (repeats_x, repeats_y)) where repeats are None for non-lattice axes."""
    reps = [None, None]
    a = img if presmoothed else periodic_component(img)
    if not y_done:
        if period_y:
            a, oy = _fit_axis(a, 0, period_y, min_ov)
            reps[1] = int(round((a.shape[0] - oy) / period_y))
        else:
            oy = overlap
        a = np.swapaxes(_self_quilt_x(np.swapaxes(a, 0, 1), oy, feather), 0, 1)
    if period_x:
        a, ox = _fit_axis(a, 1, period_x, min_ov)
        reps[0] = int(round((a.shape[1] - ox) / period_x))
    else:
        ox = overlap
    a = _self_quilt_x(a, ox, feather)
    return a.astype(np.float32), tuple(reps)


# ----------------------------------------------------------------------------------------------- surface maps


def normal_from_height(h_m, texel_m, strength=1.0):
    """Tangent-space normal map (DirectX / Unreal convention) from a periodic height field in metres.

    Image x = +U (right), image y = +V (down). R = -dh/du, G = -dh/dv (v pointing down the image), B = 1,
    normalised and encoded as n * 0.5 + 0.5. A dome therefore gets a bright right edge in R and a bright bottom
    edge in G, which Unreal (Y- / DirectX) shades as a bump."""
    dhdx = (np.roll(h_m, -1, 1) - np.roll(h_m, 1, 1)) / (2.0 * texel_m)
    dhdy = (np.roll(h_m, -1, 0) - np.roll(h_m, 1, 0)) / (2.0 * texel_m)
    nx = -dhdx * strength
    ny = -dhdy * strength
    nz = np.ones_like(nx)
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    n = np.stack([nx * inv, ny * inv, nz * inv], -1)
    return (n * 0.5 + 0.5).astype(np.float32)


def ao_from_height(h01, radii_px, strength=1.0, periodic=True):
    """Cavity AO: how far each texel sits below its blurred neighbourhood at several radii."""
    bl = blur_periodic if periodic else blur
    occ = np.zeros_like(h01)
    for r in radii_px:
        occ += np.maximum(bl(h01, r) - h01, 0.0)
    occ /= len(radii_px)
    ref = np.percentile(occ, 99.0) + 1e-6
    ao = 1.0 - strength * np.clip(occ / ref, 0.0, 1.0)
    return np.clip(ao, 0.0, 1.0).astype(np.float32)


def dilate_colour(rgb, weight):
    """Push-pull fill: colour where weight == 0 becomes a smooth extrapolation of the weighted colour, so mip maps
    and bilinear filtering of alpha-tested cards never pull black / white fringes into the leaves.
    rgb: (H, W, 3) with power-of-two sides; weight: (H, W) in 0..1 (1 = trusted colour)."""
    C = rgb * weight[..., None]
    W = weight.astype(np.float32).copy()
    Cs, Ws = [], []
    while True:
        Cs.append(C)
        Ws.append(W)
        if min(C.shape[:2]) <= 1:
            break
        h2, w2 = C.shape[0] // 2, C.shape[1] // 2
        C = C.reshape(h2, 2, w2, 2, 3).sum((1, 3))
        W = W.reshape(h2, 2, w2, 2).sum((1, 3))
        s = np.maximum(W, 1.0)
        C = C / s[..., None]
        W = W / s
    fill = Cs[-1] / np.maximum(Ws[-1], 1e-6)[..., None]
    for C, W in zip(reversed(Cs[:-1]), reversed(Ws[:-1])):
        up = np.repeat(np.repeat(fill, 2, 0), 2, 1)
        if up.shape[0] >= 8:
            up = blur(up, 1.0)
        fill = C + (1.0 - W)[..., None] * up
    return fill.astype(np.float32)
