"""Objective checks for the generated sounds + spectrogram / waveform contact sheets.

Loudness follows ITU-R BS.1770-4 / EBU R128: K-weighting (the libebur128 sample-rate independent
filter design), 400 ms momentary blocks, gated integrated loudness (-70 LUFS absolute, -10 LU
relative). Sounds shorter than a block are measured zero-padded to 400 ms.
"""
from __future__ import annotations

import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from dsp import SR, good_size, lin2db, oversample

# --------------------------------------------------------------------------------------------- loudness


def _kweight_sections():
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = math.tan(math.pi * f0 / SR)
    Vh = 10.0 ** (G / 20.0)
    Vb = Vh ** 0.4996667741545416
    a0 = 1.0 + K / Q + K * K
    pre = ((Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
           2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0)
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = math.tan(math.pi * f0 / SR)
    d = 1.0 + K / Q + K * K
    rlb = (1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / d, (1.0 - K / Q + K * K) / d)
    return pre, rlb


def k_weight(X):
    """K-weight a (ch, n) signal (frequency-domain exact IIR response, zero padded)."""
    n = X.shape[-1]
    nfft = good_size(n + SR // 2)
    z1 = np.exp(-1j * 2 * np.pi * np.arange(nfft // 2 + 1) / nfft)
    H = np.ones_like(z1)
    for b0, b1, b2, a1, a2 in _kweight_sections():
        H *= (b0 + z1 * (b1 + z1 * b2)) / (1.0 + z1 * (a1 + z1 * a2))
    return np.fft.irfft(np.fft.rfft(X, nfft, axis=-1) * H, nfft, axis=-1)[:, :n]


def _as2d(x):
    x = np.asarray(x, dtype=np.float64)
    return x if x.ndim == 2 else x[None, :]


def _block_power(Y, block_s: float, hop_s: float):
    """Mean-square (summed over channels) of K-weighted blocks."""
    n = Y.shape[-1]
    B = int(round(block_s * SR))
    H = max(int(round(hop_s * SR)), 1)
    if n < B:
        Y = np.pad(Y, ((0, 0), (0, B - n)))
        n = B
    c = np.concatenate([np.zeros((Y.shape[0], 1)), np.cumsum(Y * Y, axis=-1)], axis=-1)
    starts = np.arange(0, n - B + 1, H)
    return ((c[:, starts + B] - c[:, starts]) / B).sum(axis=0)


def _lufs(p):
    return -0.691 + 10.0 * np.log10(np.maximum(p, 1e-20))


def loudness(x) -> dict:
    """Integrated (gated), momentary max, short-term max (LUFS)."""
    X = _as2d(x)
    Y = k_weight(X)
    pb = _block_power(Y, 0.4, 0.1)
    lb = _lufs(pb)
    gated = pb[lb > -70.0]
    if gated.size:
        rel = _lufs(gated.mean()) - 10.0
        g2 = pb[(lb > -70.0) & (lb > rel)]
        integ = float(_lufs(g2.mean())) if g2.size else -70.0
    else:
        integ = -70.0
    mom = float(_lufs(_block_power(Y, 0.4, 0.01)).max())
    st = float(_lufs(_block_power(Y, 3.0, 0.1)).max())
    return {"lufs_i": round(integ, 2), "lufs_m_max": round(mom, 2), "lufs_s_max": round(st, 2)}


def true_peak_db(x) -> float:
    X = _as2d(x)
    pad = 512
    up = oversample(np.pad(X, ((0, 0), (pad, pad))), 4)
    return float(lin2db(max(np.max(np.abs(up)), np.max(np.abs(X)))))


BANDS = [("sub", 20, 60), ("bass", 60, 250), ("lowmid", 250, 800), ("mid", 800, 2500),
         ("himid", 2500, 6000), ("high", 6000, 12000), ("air", 12000, 22050)]


def band_balance(x) -> dict:
    """Share of spectral energy per band (percent)."""
    X = _as2d(x)
    m = X.mean(axis=0)
    P = np.abs(np.fft.rfft(m * np.hanning(m.shape[0]))) ** 2
    f = np.fft.rfftfreq(m.shape[0], 1.0 / SR)
    tot = P[(f >= 20)].sum() + 1e-30
    return {name: round(100.0 * P[(f >= lo) & (f < hi)].sum() / tot, 1) for name, lo, hi in BANDS}


def _third_octave_db(seg, lo_band: int = -10):
    """1/3-octave band levels (dB) of a (ch, n) segment, 100 Hz .. 16 kHz by default."""
    m = seg.mean(axis=0) * np.hanning(seg.shape[-1])
    P = np.abs(np.fft.rfft(m)) ** 2
    f = np.fft.rfftfreq(seg.shape[-1], 1.0 / SR)
    centres = 1000.0 * 2.0 ** (np.arange(lo_band, 13) / 3.0)
    out = []
    for c in centres:
        msk = (f >= c * 2 ** (-1 / 6)) & (f < c * 2 ** (1 / 6))
        out.append(10.0 * np.log10(P[msk].sum() + 1e-20))
    return np.array(out), centres


def seam_metrics(x, compare_points=None) -> dict:
    """How seamless is a loop? Compares the wrapped joint with ordinary windows of the same file.

    seam_step_ratio: |x[0] - x[-1]| / 99.9th percentile of |x[i+1] - x[i]| (<= 1: an ordinary step).
    seam_curv_ratio: same for the second difference across the joint (slope continuity).
    seam_hf_z: >5 kHz energy of a window centred on the joint as a z-score against 64 other windows
               (a click shows up as a large positive value).
    seam_timbre_pct: percentile rank (0-100) of the before/after 1/3-octave difference across the joint
               among 40 interior points (a timbre jump at the seam -> near 100; ~50 is typical).
    edge_level_step_db: level of the last 50 ms minus the first 50 ms."""
    X = _as2d(x)
    n = X.shape[-1]
    d1 = np.abs(np.diff(X, axis=-1))
    p999 = float(np.percentile(d1, 99.9)) + 1e-12
    step = float(np.max(np.abs(X[:, 0] - X[:, -1])))
    d2 = np.abs(X[:, 2:] - 2 * X[:, 1:-1] + X[:, :-2])
    q999 = float(np.percentile(d2, 99.9)) + 1e-12
    curv = float(max(np.max(np.abs(X[:, 0] - 2 * X[:, -1] + X[:, -2])), np.max(np.abs(X[:, 1] - 2 * X[:, 0] + X[:, -1]))))
    W = 8192 if n >= 6 * 8192 else 2048
    joint = np.concatenate([X[:, -W // 2:], X[:, : W // 2]], axis=-1)
    rng = np.random.default_rng(7)
    starts = rng.integers(W, max(n - 2 * W, W + 1), 64)

    def hf(seg):
        m = seg.mean(axis=0) * np.hanning(seg.shape[-1])
        P = np.abs(np.fft.rfft(m)) ** 2
        f = np.fft.rfftfreq(seg.shape[-1], 1.0 / SR)
        return 10.0 * np.log10(P[f > 5000].sum() + 1e-20)

    hfs = np.array([hf(X[:, s:s + W]) for s in starts])
    hz = float((hf(joint) - hfs.mean()) / (hfs.std() + 1e-6))
    # timbre continuity: 1/3-octave difference between the 0.25 s before and after a point, at the
    # joint versus 40 interior points (percentile rank; ~50 is typical, only >95 would be suspicious)
    B = min(int(0.25 * SR), n // 4)
    Xw = np.concatenate([X[:, -B:], X, X[:, :B]], axis=-1)

    def jump(p):  # p indexes the original signal; Xw is shifted by B
        a = _third_octave_db(Xw[:, p:p + B])[0]
        b = _third_octave_db(Xw[:, p + B:p + 2 * B])[0]
        return float(np.mean(np.abs(a - b)))

    pts = np.asarray(compare_points, dtype=int) if compare_points is not None else np.linspace(B, n - B, 40).astype(int)
    pts = pts[(pts >= B) & (pts <= n - B)]
    ds = np.array([jump(p) for p in pts])
    pct = float(np.mean(ds < jump(0)) * 100.0)
    e = int(0.05 * SR)
    lvl = lambda seg: 10.0 * np.log10(np.mean(seg * seg) + 1e-20)  # noqa: E731
    return {
        "seam_step_ratio": round(step / p999, 3),
        "seam_curv_ratio": round(curv / q999, 3),
        "seam_hf_z": round(hz, 2),
        "seam_timbre_pct": round(pct, 1),
        "edge_level_step_db": round(lvl(X[:, -e:]) - lvl(X[:, :e]), 2),
    }


def analyse(x, loop: bool = False, seam_points=None) -> dict:
    X = _as2d(x)
    out = {
        "peak_dbfs": round(float(lin2db(np.max(np.abs(X)))), 2),
        "true_peak_dbtp": round(true_peak_db(X), 2),
        "rms_dbfs": round(float(lin2db(math.sqrt(float(np.mean(X * X))))), 2),
        "dc_offset": float("%.2e" % float(np.max(np.abs(X.mean(axis=-1))))),
    }
    out.update(loudness(X))
    out["crest_db"] = round(out["peak_dbfs"] - out["rms_dbfs"], 2)
    out["bands_pct"] = band_balance(X)
    if X.shape[0] == 2:  # mono compatibility: L/R correlation and loudness lost by a mono downmix
        l, r = X[0] - X[0].mean(), X[1] - X[1].mean()
        out["stereo_corr"] = round(float(np.sum(l * r) / (np.sqrt(np.sum(l * l) * np.sum(r * r)) + 1e-20)), 3)
        m = 0.5 * (X[0] + X[1])
        out["mono_drop_db"] = round(loudness(np.stack([m, m]))["lufs_i"] - out["lufs_i"], 2)
    env = np.abs(X).max(axis=0)
    k = int(np.argmax(env))
    out["peak_time_s"] = round(k / SR, 3)
    if loop:
        out.update(seam_metrics(X, seam_points))
    else:
        tail = X[:, -int(0.01 * SR):]
        out["end_level_dbfs"] = round(float(lin2db(np.max(np.abs(tail)) + 1e-12)), 1)
        # level just before the mastering fade-out (4 % of the length, 10-250 ms) relative to the peak:
        # above about -30 dB the fade audibly truncates a still-ringing tail
        n = X.shape[-1]
        fo = int(min(max(0.04 * n / SR, 0.01), 0.25) * SR)
        seg = X[:, max(0, n - fo - int(0.03 * SR)): n - fo]
        out["prefade_tail_db"] = round(float(lin2db(np.max(np.abs(seg)) + 1e-12) - lin2db(np.max(np.abs(X)) + 1e-12)), 1)
    return out


def chroma(x, fmin: float = 60.0, fmax: float = 2000.0):
    """Energy per pitch class (C..B) from a long-window spectrum; used to check the music's mode."""
    X = _as2d(x)
    m = X.mean(axis=0)
    W = 16384
    hop = W // 2
    acc = np.zeros(W // 2 + 1)
    win = np.hanning(W)
    for s in range(0, m.shape[0] - W, hop):
        acc += np.abs(np.fft.rfft(m[s:s + W] * win)) ** 2
    f = np.fft.rfftfreq(W, 1.0 / SR)
    msk = (f >= fmin) & (f <= fmax)
    pc = np.mod(np.round(12.0 * np.log2(f[msk] / 440.0)) + 9, 12).astype(int)
    out = np.zeros(12)
    np.add.at(out, pc, acc[msk])
    return out / out.sum()


# --------------------------------------------------------------------------------------------- drawing
_FONT_CACHE = {}


def font(size: int, mono: bool = True):
    key = (size, mono)
    if key in _FONT_CACHE:
        return _FONT_CACHE[key]
    cands = (["/System/Library/Fonts/Menlo.ttc", "/System/Library/Fonts/Monaco.ttf",
              "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"] if mono else []) + [
        "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]
    f = None
    for c in cands:
        if os.path.exists(c):
            try:
                f = ImageFont.truetype(c, size)
                break
            except OSError:
                pass
    if f is None:
        f = ImageFont.load_default(size=size)
    _FONT_CACHE[key] = f
    return f


_MAGMA = np.array([[0, 0, 4], [28, 16, 68], [79, 18, 123], [129, 37, 129], [181, 54, 122],
                   [229, 80, 100], [251, 135, 97], [254, 194, 135], [252, 253, 191]], dtype=np.float64)


def colormap(v):
    v = np.clip(v, 0.0, 1.0) * (len(_MAGMA) - 1)
    i = np.minimum(np.floor(v).astype(int), len(_MAGMA) - 2)
    f = (v - i)[..., None]
    return (_MAGMA[i] * (1 - f) + _MAGMA[i + 1] * f).astype(np.uint8)


def spectrogram_rgb(mono, width: int, height: int, fmin: float = 25.0, fmax: float = 20000.0,
                    db_lo: float = -100.0, db_hi: float = -25.0, nfft: int = 2048):
    """Log-frequency spectrogram image (height x width x 3). Absolute dBFS scale (0 dB = full-scale sine)."""
    n = mono.shape[0]
    win = np.hanning(nfft)
    centres = (np.arange(width) + 0.5) * n / width
    starts = np.round(centres - nfft / 2).astype(int)
    xp = np.pad(mono, (nfft, nfft))
    idx = starts[:, None] + nfft + np.arange(nfft)[None, :]
    mags = np.abs(np.fft.rfft(xp[idx] * win, axis=1)) * 2.0 / win.sum()
    dbv = 20.0 * np.log10(mags + 1e-12)
    f = np.fft.rfftfreq(nfft, 1.0 / SR)
    rows = np.geomspace(fmax, fmin, height)
    fb = np.interp(rows, f, np.arange(f.shape[0]))
    i0 = np.floor(fb).astype(int)
    fr = fb - i0
    i1 = np.minimum(i0 + 1, f.shape[0] - 1)
    img = dbv[:, i0] * (1 - fr) + dbv[:, i1] * fr  # (width, height)
    v = (img.T - db_lo) / (db_hi - db_lo)
    return colormap(v)


def waveform_rgb(X, width: int, height: int, fg=(120, 200, 255), bg=(18, 20, 28)):
    """Min/max waveform per pixel column, channels stacked, plus a dB envelope line (-60..0 dBFS)."""
    ch = X.shape[0]
    img = np.zeros((height, width, 3), dtype=np.uint8)
    img[:] = bg
    hh = height // ch
    n = X.shape[-1]
    edges = np.linspace(0, n, width + 1).astype(int)
    for c in range(ch):
        x = X[c]
        mx = np.array([x[edges[i]:max(edges[i + 1], edges[i] + 1)].max() for i in range(width)])
        mn = np.array([x[edges[i]:max(edges[i + 1], edges[i] + 1)].min() for i in range(width)])
        y0 = c * hh
        mid = y0 + hh / 2.0
        top = np.clip(np.round(mid - mx * (hh / 2 - 1)), y0, y0 + hh - 1).astype(int)
        bot = np.clip(np.round(mid - mn * (hh / 2 - 1)), y0, y0 + hh - 1).astype(int)
        rr = np.arange(height)[:, None]
        mask = (rr >= top[None, :]) & (rr <= bot[None, :])
        img[mask] = fg
        img[int(mid), :] = (60, 70, 90)
        # dB envelope (peak per column), -60 dB at the bottom of the lane, 0 dB at the top
        pk = np.maximum(np.abs(mx), np.abs(mn))
        d = np.clip((lin2db(pk + 1e-9) + 60.0) / 60.0, 0, 1)
        ys = np.clip(np.round(y0 + hh - 1 - d * (hh - 2)), y0, y0 + hh - 1).astype(int)
        img[ys, np.arange(width)] = (255, 190, 60)
    return img


_BAND_COL = [(80, 40, 160), (40, 90, 200), (40, 170, 170), (60, 190, 90), (210, 200, 60), (240, 130, 50), (230, 70, 70)]


def _tile(entry, tile_w: int = 900, seam_w: int = 240):
    X = entry["data"]
    st = entry["stats"]
    loop = entry["loop"]
    W = tile_w
    width = tile_w - (seam_w if loop else 0)
    H = 22 + 22 + 64 + 150 + 16 + 8
    tile = Image.new("RGB", (W, H), (12, 13, 18))
    d = ImageDraw.Draw(tile)
    f1, f2 = font(15), font(12)
    dur = X.shape[-1] / SR
    head = "%s  %.2fs %s%s" % (entry["name"], dur, "stereo" if X.shape[0] == 2 else "mono", "  LOOP" if loop else "")
    d.text((6, 3), head, fill=(240, 240, 240), font=f1)
    s2 = "pk %.1f tp %.1f  M %.1f  I %.1f LUFS  crest %.1f  dc %.0e" % (
        st["peak_dbfs"], st["true_peak_dbtp"], st["lufs_m_max"], st["lufs_i"], st["crest_db"], st["dc_offset"])
    tgt = entry.get("target")
    if tgt:
        s2 += "  tgt %s %.1f" % (tgt[0], tgt[1])
    d.text((6, 24), s2, fill=(170, 180, 200), font=f2)
    y = 44
    mono = X.mean(axis=0)
    tile.paste(Image.fromarray(waveform_rgb(X, width, 64)), (0, y))
    y += 64
    tile.paste(Image.fromarray(spectrogram_rgb(mono, width, 150)), (0, y))
    for fq in (100, 1000, 10000):
        yy = y + int(round((math.log(20000.0) - math.log(fq)) / (math.log(20000.0) - math.log(25.0)) * 149))
        d.line([(0, yy), (width, yy)], fill=(90, 90, 110))
        d.text((3, yy - 13), "%dk" % (fq // 1000) if fq >= 1000 else "%d" % fq, fill=(200, 200, 220), font=f2)
    # time ticks every 0.5 s (or 5 s for long files)
    step = 0.5 if dur <= 6 else 5.0
    tt = step
    while tt < dur:
        xx = int(tt / dur * width)
        d.line([(xx, y + 146), (xx, y + 149)], fill=(230, 230, 230))
        tt += step
    y += 150
    # band balance bar
    bx = 0
    for (nm, _, _), col in zip(BANDS, _BAND_COL):
        w = int(round(st["bands_pct"][nm] / 100.0 * width))
        d.rectangle([bx, y + 2, bx + w, y + 13], fill=col)
        if w > 40:
            d.text((bx + 3, y + 1), "%s %d%%" % (nm, st["bands_pct"][nm]), fill=(0, 0, 0), font=f2)
        bx += w
    if loop:
        # seam panel: +-25 ms waveform around the joint and +-1 s spectrogram, joint marked in red
        x0 = width + 6
        sw = seam_w - 12
        k = int(0.025 * SR)
        seg = np.concatenate([X[:, -k:], X[:, :k]], axis=-1)
        tile.paste(Image.fromarray(waveform_rgb(seg, sw, 64)), (x0, 44))
        d.line([(x0 + sw // 2, 44), (x0 + sw // 2, 107)], fill=(255, 60, 60))
        k2 = min(int(1.0 * SR), X.shape[-1] // 2)
        seg2 = np.concatenate([X[:, -k2:], X[:, :k2]], axis=-1).mean(axis=0)
        tile.paste(Image.fromarray(spectrogram_rgb(seg2, sw, 150)), (x0, 108))
        d.line([(x0 + sw // 2, 108), (x0 + sw // 2, 257)], fill=(255, 60, 60))
        d.text((x0, 3), "seam step %.2f curv %.2f" % (st["seam_step_ratio"], st["seam_curv_ratio"]), fill=(240, 200, 200), font=f2)
        d.text((x0, 24), "hfz %.1f timbre %.0f%% lvl %.1f" % (st["seam_hf_z"], st["seam_timbre_pct"], st["edge_level_step_db"]), fill=(240, 200, 200), font=f2)
    return tile


def contact_sheet(entries, path: str, title: str, cols: int = 2):
    tiles = [_tile(e) for e in entries]
    tw = max(t.width for t in tiles)
    th = max(t.height for t in tiles)
    rows = int(math.ceil(len(tiles) / cols))
    W = cols * tw + (cols + 1) * 8
    H = 40 + rows * (th + 8) + 8
    img = Image.new("RGB", (W, H), (6, 6, 9))
    d = ImageDraw.Draw(img)
    d.text((10, 8), title, fill=(255, 255, 255), font=font(20, mono=False))
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        img.paste(t, (8 + c * (tw + 8), 40 + r * (th + 8)))
    save_png(img, path)
    return path


def save_png(img, path):
    """Palette-quantised PNG (the sheets are committed; this keeps them ~4x smaller)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    q = img.quantize(colors=160, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    q.save(path, optimize=True)
