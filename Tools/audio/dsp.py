"""LA PLACE procedural audio: numpy-only DSP toolkit.

Everything here is deterministic and depends on numpy only. Conventions:

* ``SR = 44100``. Mono signals are 1-D float64 arrays, stereo signals are ``(2, n)`` arrays.
* All randomness goes through :func:`make_rng` (seeded from names), never the global RNG.
* Static filters (RBJ biquads, Butterworth cascades, one-poles) are applied in the frequency domain
  with their *exact* complex IIR response. With ``circular=True`` the convolution wraps around, which
  is precisely the steady-state response of a periodic signal: a seamless loop stays seamless.
* Time-varying filters (sweeps, swirls, formant morphs) use a Hann-windowed STFT with zero-phase
  gain curves (:func:`stft_filter`, :func:`sweep`), or the per-sample TPT state-variable filter
  :func:`svf` when a truly resonant IIR character is wanted.
"""
from __future__ import annotations

import math
import re
import zlib
from functools import lru_cache

import numpy as np

SR = 44100
NYQ = SR / 2.0
SQ2 = 1.0 / math.sqrt(2.0)
TAU = 2.0 * math.pi


# --------------------------------------------------------------------------------------------- basics
def ns(sec: float) -> int:
    """Seconds -> whole samples."""
    return int(round(float(sec) * SR))


def tvec(n: int) -> np.ndarray:
    return np.arange(int(n), dtype=np.float64) / SR


def make_rng(*keys) -> np.random.Generator:
    """Deterministic generator from string / int keys (stable across processes, unlike hash())."""
    seq = []
    for k in keys:
        if isinstance(k, str):
            seq.append(zlib.crc32(k.encode("utf-8")))
        else:
            seq.append(int(k) & 0xFFFFFFFF)
    return np.random.default_rng(seq)


def db2lin(d):
    return 10.0 ** (np.asarray(d, dtype=np.float64) / 20.0)


def lin2db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def peak(x) -> float:
    return float(np.max(np.abs(x))) if np.size(x) else 0.0


def rms(x) -> float:
    return float(np.sqrt(np.mean(np.square(x)))) if np.size(x) else 0.0


def norm_peak(x, level_db: float = 0.0):
    p = peak(x)
    return x * (float(db2lin(level_db)) / p) if p > 0 else x


def norm_rms(x, level_db: float = 0.0):
    r = rms(x)
    return x * (float(db2lin(level_db)) / r) if r > 0 else x


def fit(x, n: int):
    """Pad with zeros / trim the last axis to n samples."""
    m = x.shape[-1]
    if m == n:
        return x
    if m > n:
        return x[..., :n].copy()
    pad = [(0, 0)] * (x.ndim - 1) + [(0, n - m)]
    return np.pad(x, pad)


@lru_cache(maxsize=None)
def good_size(n: int) -> int:
    """Smallest 2^a 3^b 5^c >= n (fast sizes for numpy's pocketfft)."""
    n = max(int(n), 1)
    best = 1 << (n - 1).bit_length()
    p5 = 1
    while p5 < best:
        p35 = p5
        while p35 < best:
            p = p35
            while p < n:
                p *= 2
            best = min(best, p)
            p35 *= 3
        p5 *= 5
    return best


def place(buf: np.ndarray, x: np.ndarray, start: int, wrap: bool = False) -> None:
    """Add x into buf (in place) at sample index start. wrap=True folds overflow circularly (loops)."""
    n = buf.shape[-1]
    m = x.shape[-1]
    if m == 0:
        return
    if wrap:
        start %= n
        pos = 0
        while pos < m:
            s = (start + pos) % n
            k = min(m - pos, n - s)
            buf[..., s:s + k] += x[..., pos:pos + k]
            pos += k
        return
    if start >= n or start + m <= 0:
        return
    a0 = max(0, -start)
    b0 = max(0, start)
    k = min(m - a0, n - b0)
    buf[..., b0:b0 + k] += x[..., a0:a0 + k]


def at(buf: np.ndarray, x: np.ndarray, t: float, wrap: bool = False) -> None:
    place(buf, x, ns(t), wrap)


# --------------------------------------------------------------------------------------------- envelopes
def _curve(u, c):
    if abs(c) < 1e-6:
        return u
    return (1.0 - np.exp(c * u)) / (1.0 - math.exp(c))


def env(points, n: int, curve=0.0) -> np.ndarray:
    """Piecewise envelope from [(t_sec, value), ...].

    curve: one number or one per segment (SuperCollider convention: 0 linear, >0 starts slowly and
    accelerates, <0 starts quickly and decelerates). Values before/after the points are held.
    """
    pts = sorted(points, key=lambda p: p[0])
    out = np.empty(n)
    idx = [min(max(ns(p[0]), 0), n) for p in pts]
    out[: idx[0]] = pts[0][1]
    curves = curve if isinstance(curve, (list, tuple)) else [curve] * (len(pts) - 1)
    for i in range(len(pts) - 1):
        a, b = idx[i], idx[i + 1]
        if b <= a:
            continue
        u = np.arange(b - a) / float(b - a)
        v0, v1 = pts[i][1], pts[i + 1][1]
        out[a:b] = v0 + (v1 - v0) * _curve(u, curves[i])
    out[idx[-1]:] = pts[-1][1]
    return out


def perc(n: int, t0: float = 0.0, attack: float = 0.001, tau: float = 0.1, hold: float = 0.0,
         power: float = 1.0) -> np.ndarray:
    """Attack ramp (raised cosine) then exponential decay with time constant tau."""
    t = tvec(n) - t0
    e = np.zeros(n)
    a = max(attack, 1.0 / SR)
    m1 = (t >= 0) & (t < a)
    e[m1] = 0.5 - 0.5 * np.cos(np.pi * t[m1] / a)
    m2 = t >= a
    td = np.maximum(t[m2] - a - hold, 0.0)
    e[m2] = np.exp(-td / tau)
    return e ** power if power != 1.0 else e


def bell_env(n: int, t_start: float, t_peak: float, t_end: float, rise_curve=2.0, fall_curve=-3.0):
    """0 -> 1 -> 0 envelope (rise then fall) with curvature."""
    return env([(t_start, 0.0), (t_peak, 1.0), (t_end, 0.0)], n, [rise_curve, fall_curve])


def fade(x, fin: float = 0.0, fout: float = 0.0):
    """Raised-cosine fade in / out (seconds)."""
    y = np.array(x, dtype=np.float64, copy=True)
    n = y.shape[-1]
    a = min(ns(fin), n)
    if a > 0:
        y[..., :a] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(a) / a)
    b = min(ns(fout), n)
    if b > 0:
        y[..., n - b:] *= 0.5 + 0.5 * np.cos(np.pi * (np.arange(b) + 1) / b)
    return y


def smooth(x, ms: float, passes: int = 2):
    """Zero-phase smoothing by repeated centred moving averages (box -> triangle -> ~gaussian)."""
    w = max(int(ms * SR / 1000.0), 1)
    y = np.asarray(x, dtype=np.float64)
    if w <= 1:
        return y.copy()
    for _ in range(passes):
        y = _centred_avg(y, w)
    return y


def causal_avg(x, ms: float):
    """Causal moving average (window ending at each sample)."""
    w = max(int(ms * SR / 1000.0), 1)
    c = np.cumsum(np.concatenate([np.zeros(w), np.asarray(x, dtype=np.float64)]))
    return (c[w:] - c[:-w]) / w


# --------------------------------------------------------------------------------------------- noise
_SLOPES = {"white": 0.0, "pink": -3.0, "brown": -6.0, "red": -6.0, "blue": 3.0, "violet": 6.0}


def colored_noise(n: int, rng: np.random.Generator, slope_db_oct: float = -3.0, fmin: float = 15.0):
    """Gaussian noise with a spectral slope (dB/octave), unit RMS. Built by FFT, hence periodic in n."""
    n = int(n)
    if n <= 1:
        return np.zeros(n)
    X = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1.0 / SR)
    expo = slope_db_oct / (20.0 * math.log10(2.0))
    X *= (np.maximum(f, fmin) / 1000.0) ** expo
    X[0] = 0.0
    y = np.fft.irfft(X, n)
    r = rms(y)
    return y / r if r > 0 else y


def noise(n: int, rng: np.random.Generator, color: str = "white"):
    if color == "white":
        return rng.standard_normal(int(n))
    return colored_noise(n, rng, _SLOPES[color])


def smooth_random(n: int, rng: np.random.Generator, rate_hz: float, positive: bool = False):
    """Band-limited random control curve (unit std, zero mean). Periodic in n (loop safe).

    rate_hz is the -ish bandwidth: 0.1 = slow gusts, 5 = flutter, 30 = fast turbulence.
    positive=True maps to 0..1 through a logistic squash."""
    n = int(n)
    rate = max(rate_hz, 1.5 * SR / max(n, 1))
    X = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1.0 / SR)
    X *= np.exp(-0.5 * (f / rate) ** 2)
    X[0] = 0.0
    y = np.fft.irfft(X, n)
    s = np.std(y)
    y = y / s if s > 0 else y
    if positive:
        y = 1.0 / (1.0 + np.exp(-1.6 * y))
    return y


def periodic_lfo(n: int, cycles: float, shape: str = "sine", phase: float = 0.0, sharp: float = 1.0):
    """LFO with an exact number of cycles over n samples (integer cycles => loop safe). Range -1..1."""
    ph = cycles * np.arange(n) / n + phase
    s = np.sin(TAU * ph)
    if shape == "sine":
        return s
    if shape == "pulse":  # raised-cosine pulses, sharpness controls the duty
        return 2.0 * ((0.5 + 0.5 * s) ** sharp) - 1.0
    if shape == "tri":
        return 2.0 * np.abs(2.0 * (ph % 1.0) - 1.0) - 1.0
    raise ValueError(shape)


# --------------------------------------------------------------------------------------------- static filters
def butter_q(order: int):
    """Q of each 2nd-order section of an even-order Butterworth filter."""
    return [1.0 / (2.0 * math.cos((2 * k + 1) * math.pi / (2 * order))) for k in range(order // 2)]


def rbj(kind: str, f0: float, q: float = SQ2, gain_db: float = 0.0):
    """RBJ audio-EQ-cookbook biquad. Returns normalised (b0, b1, b2, a1, a2)."""
    f0 = min(max(float(f0), 2.0), 0.4995 * SR)
    A = 10.0 ** (gain_db / 40.0)
    w0 = TAU * f0 / SR
    cw, sw = math.cos(w0), math.sin(w0)
    alpha = sw / (2.0 * q)
    if kind == "lp":
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "hp":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "bp":  # constant 0 dB peak gain
        b = [alpha, 0.0, -alpha]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "notch":
        b = [1.0, -2 * cw, 1.0]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "ap":
        b = [1 - alpha, -2 * cw, 1 + alpha]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    elif kind == "peak":
        b = [1 + alpha * A, -2 * cw, 1 - alpha * A]
        a = [1 + alpha / A, -2 * cw, 1 - alpha / A]
    elif kind == "ls":
        sa = 2 * math.sqrt(A) * alpha
        b = [A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sa)]
        a = [(A + 1) + (A - 1) * cw + sa, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa]
    elif kind == "hs":
        sa = 2 * math.sqrt(A) * alpha
        b = [A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sa)]
        a = [(A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa]
    else:
        raise ValueError(kind)
    a0 = a[0]
    return (b[0] / a0, b[1] / a0, b[2] / a0, a[1] / a0, a[2] / a0)


def one_pole(kind: str, fc: float):
    """Bilinear one-pole low/high-pass as a (degenerate) biquad section."""
    fc = min(max(float(fc), 2.0), 0.4995 * SR)
    K = math.tan(math.pi * fc / SR)
    a1 = (K - 1.0) / (K + 1.0)
    if kind == "lp":
        return (K / (1 + K), K / (1 + K), 0.0, a1, 0.0)
    return (1 / (1 + K), -1 / (1 + K), 0.0, a1, 0.0)


def _sections(spec):
    """Expand ('lp4', 800) / ('peak', 300, 1.0, -4) / ('bp', 1200, 3.0) into biquad sections."""
    kind = spec[0]
    f0 = spec[1]
    q = spec[2] if len(spec) > 2 and spec[2] is not None else None
    g = spec[3] if len(spec) > 3 else 0.0
    m = re.match(r"([a-z]+)(\d*)$", kind)
    base, order = m.group(1), int(m.group(2) or 2)
    if base in ("lp", "hp"):
        if order == 1:
            return [one_pole(base, f0)]
        if order == 2:
            return [rbj(base, f0, q or SQ2)]
        qs = butter_q(order - (order % 2))
        if q:
            qs[-1] *= q / SQ2
        secs = [rbj(base, f0, qq) for qq in qs]
        if order % 2:
            secs.append(one_pole(base, f0))
        return secs
    if base in ("bp", "notch", "ap"):
        return [rbj(base, f0, q or SQ2)] * max(order // 2, 1)
    if base in ("peak", "ls", "hs"):
        return [rbj(base, f0, q or SQ2, g)]
    raise ValueError(kind)


def _section_resp(sec, nfft: int):
    b0, b1, b2, a1, a2 = sec
    z1 = np.exp(-1j * TAU * np.arange(nfft // 2 + 1) / nfft)
    return (b0 + z1 * (b1 + z1 * b2)) / (1.0 + z1 * (a1 + z1 * a2))


def _section_decay(sec) -> int:
    """Samples for the section's impulse response to fall ~80 dB (from its pole radius)."""
    _, _, _, a1, a2 = sec
    r = float(np.max(np.abs(np.roots([1.0, a1, a2])))) if a2 != 0.0 else abs(a1)
    if r <= 1e-9:
        return 4
    if r >= 1.0:
        return 3 * SR
    return int(min(math.log(1e-4) / math.log(r), 3 * SR)) + 4


def eq(x, *specs, circular: bool = False):
    """Apply a cascade of static filters with their exact IIR response (via FFT).

    specs: ('lp', fc[, q]) ('hp', fc[, q]) ('lp4', fc) ('hp8', fc) ('lp1', fc) ('bp', fc, q)
           ('notch', fc, q) ('peak', fc, q, dB) ('ls', fc, q, dB) ('hs', fc, q, dB)
    circular=True: periodic (loop) processing, the output wraps exactly like a steady-state loop.
    """
    x = np.asarray(x, dtype=np.float64)
    secs = [s for sp in specs if sp is not None for s in _sections(sp)]
    if not secs:
        return x.copy()
    n = x.shape[-1]
    if circular:
        nfft = n
    else:
        tail = sum(_section_decay(s) for s in secs)
        nfft = good_size(n + min(tail, 3 * SR) + 64)
    H = np.ones(nfft // 2 + 1, dtype=np.complex128)
    for s in secs:
        H *= _section_resp(s, nfft)
    X = np.fft.rfft(x, nfft, axis=-1)
    return np.fft.irfft(X * H, nfft, axis=-1)[..., :n]


def shape_spectrum(x, gain_fn, circular: bool = True):
    """Zero-phase static spectral shaping: gain_fn(freqs_hz) -> linear gains."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    nfft = n if circular else good_size(n + 8192)
    f = np.fft.rfftfreq(nfft, 1.0 / SR)
    X = np.fft.rfft(x, nfft, axis=-1)
    return np.fft.irfft(X * gain_fn(f), nfft, axis=-1)[..., :n]


def biquad_td(x, sec):
    """Reference time-domain biquad (transposed direct form II). Slow; used for validation."""
    b0, b1, b2, a1, a2 = sec
    xs = np.asarray(x, dtype=np.float64).tolist()
    out = [0.0] * len(xs)
    z1 = z2 = 0.0
    for i, v in enumerate(xs):
        y = b0 * v + z1
        z1 = b1 * v - a1 * y + z2
        z2 = b2 * v - a2 * y
        out[i] = y
    return np.array(out)


# --------------------------------------------------------------------------------------------- time-varying filters
def svf(x, fc, q=SQ2, mode: str = "lp"):
    """Topology-preserving-transform state-variable filter with per-sample cutoff / Q.

    mode 'lp' | 'bp' | 'hp' | 'notch' | 'peak' or a tuple of those (returns a tuple).
    Pure python loop (~1.5 us / sample): use for short resonant gestures."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    fcv = np.clip(np.broadcast_to(np.asarray(fc, dtype=np.float64), (n,)), 5.0, 0.45 * SR)
    qv = np.maximum(np.broadcast_to(np.asarray(q, dtype=np.float64), (n,)), 0.05)
    g = np.tan(np.pi * fcv / SR)
    k = 1.0 / qv
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    xs, A1, A2, A3 = x.tolist(), a1.tolist(), a2.tolist(), a3.tolist()
    lp = [0.0] * n
    bp = [0.0] * n
    ic1 = ic2 = 0.0
    for i in range(n):
        v3 = xs[i] - ic2
        v1 = A1[i] * ic1 + A2[i] * v3
        v2 = ic2 + A2[i] * ic1 + A3[i] * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        lp[i] = v2
        bp[i] = v1
    lp = np.array(lp)
    bp = np.array(bp)
    outs = {"lp": lp, "bp": bp * k, "hp": x - k * bp - lp, "notch": x - k * bp,
            "peak": lp - (x - k * bp - lp), "bpq": bp}
    if isinstance(mode, (tuple, list)):
        return tuple(outs[m] for m in mode)
    return outs[mode]


def _time_fn(v, n: int, circular: bool = False):
    """Scalar / per-sample array / callable(t) -> callable(t_array)."""
    if callable(v):
        return v
    arr = np.asarray(v, dtype=np.float64)
    if arr.ndim == 0:
        c = float(arr)
        return lambda t: np.full(np.shape(t), c)
    grid = np.arange(arr.shape[-1]) / SR
    if circular:
        L = arr.shape[-1] / SR
        return lambda t: np.interp(np.mod(t, L), grid, arr, period=L)
    return lambda t: np.interp(t, grid, arr)


def _hann(frame: int):
    return 0.5 - 0.5 * np.cos(TAU * np.arange(frame) / frame)


def stft_filter(x, gain_fn, frame: int = 2048, nfft: int | None = None, circular: bool = False,
                chunk: int = 256):
    """Zero-phase time-varying filter: gain_fn(times[M], freqs[K]) -> (M, K) linear gains.

    Hann analysis windows at 75 % overlap (constant overlap-add), frames zero-padded (centred) to nfft
    so each frame is a true linear convolution. circular=True treats x as a loop (n % hop == 0 is
    arranged automatically by choosing a hop that divides n)."""
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 2:
        return np.stack([stft_filter(c, gain_fn, frame, nfft, circular, chunk) for c in x])
    n = x.shape[0]
    hop = frame // 4
    if circular and n % hop:
        cands = [h for h in range(max(hop // 2, 32), hop * 2) if n % h == 0]
        if not cands:
            raise ValueError("circular stft: no hop divides %d" % n)
        hop = min(cands, key=lambda h: abs(h - hop))
        frame = 4 * hop
    nfft = nfft or good_size(2 * frame)
    nfft = max(nfft, frame)
    win = _hann(frame)
    off = (nfft - frame) // 2
    freqs = np.fft.rfftfreq(nfft, 1.0 / SR)
    if circular:
        M = n // hop
        starts = np.arange(M) * hop - frame // 2
        out = np.zeros(n)
    else:
        M = int(math.ceil((n + frame) / hop)) + 1
        starts = np.arange(M) * hop - (frame - hop)
        base = frame + off  # offset of sample 0 inside the padded buffers
        xp = np.concatenate([np.zeros(base), x, np.zeros(frame + nfft)])
        out = np.zeros(xp.shape[0] + nfft)
    centers = starts + frame / 2.0
    for c0 in range(0, M, chunk):
        c1 = min(M, c0 + chunk)
        st = starts[c0:c1]
        idx = st[:, None] + np.arange(frame)[None, :]
        if circular:
            fr = x[np.mod(idx, n)]
        else:
            fr = xp[idx + base]
        buf = np.zeros((c1 - c0, nfft))
        buf[:, off:off + frame] = fr * win
        G = gain_fn(centers[c0:c1] / SR, freqs)
        Y = np.fft.irfft(np.fft.rfft(buf, axis=1) * G, nfft, axis=1)
        for j in range(c1 - c0):
            p = int(st[j]) - off
            if circular:
                place(out, Y[j], p, wrap=True)
            else:
                out[p + base: p + base + nfft] += Y[j]
    out *= 0.5  # Hann at 75 % overlap sums to 2
    if circular:
        return out
    return out[base:base + n]


def mag_response(kind: str, f, fc, q=SQ2, order: int = 2, gain_db=0.0):
    """Analog-prototype magnitude of lp/hp/bp/notch/peak (broadcasts f against fc, q)."""
    r = np.maximum(f, 1e-3) / np.maximum(fc, 1e-3)
    den = np.sqrt((1.0 - r * r) ** 2 + (r / q) ** 2)
    if kind == "lp":
        h = 1.0 / den
    elif kind == "hp":
        h = r * r / den
    elif kind == "bp":
        h = (r / q) / den
    elif kind == "notch":
        h = np.abs(1.0 - r * r) / den
    elif kind == "peak":
        g = 10.0 ** (np.asarray(gain_db) / 20.0)
        return 1.0 + (g - 1.0) * ((r / q) / den) ** (order / 2.0)
    else:
        raise ValueError(kind)
    return h ** (order / 2.0)


def sweep(x, kind: str, fc, q=SQ2, order: int = 2, frame: int = 2048, nfft: int | None = None,
          circular: bool = False, gain_db=0.0):
    """Time-varying lp / hp / bp / notch / peak filter. fc, q: scalars, per-sample arrays or f(t)."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    ffc = _time_fn(fc, n, circular)
    fq = _time_fn(q, n, circular)
    fg = _time_fn(gain_db, n, circular)

    def gfn(times, f):
        c = np.clip(ffc(times), 5.0, 0.49 * SR)[:, None]
        qq = np.maximum(fq(times), 0.05)[:, None]
        gg = fg(times)[:, None]
        return mag_response(kind, f[None, :], c, qq, order, gg)

    return stft_filter(x, gfn, frame=frame, nfft=nfft, circular=circular)


def formant_filter(x, formants, frame: int = 2048, circular: bool = False, floor: float = 0.0):
    """Parallel resonant band-passes: formants = [(fc, q, gain_db), ...] where each entry may be a
    scalar, per-sample array or f(t). Gives vowel / howl / resonant-body colouring."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    fs = [(_time_fn(a, n, circular), _time_fn(b, n, circular), _time_fn(c, n, circular)) for a, b, c in formants]

    def gfn(times, f):
        G = np.full((len(times), f.shape[0]), floor)
        for ffc, fq, fg in fs:
            c = np.clip(ffc(times), 5.0, 0.49 * SR)[:, None]
            qq = np.maximum(fq(times), 0.1)[:, None]
            gg = 10.0 ** (fg(times)[:, None] / 20.0)
            G += gg * mag_response("bp", f[None, :], c, qq)
        return G

    return stft_filter(x, gfn, frame=frame, circular=circular)


# --------------------------------------------------------------------------------------------- oscillators
def phase(freq, n: int, phase0: float = 0.0):
    """Running phase in cycles for a (possibly time-varying) frequency."""
    f = np.broadcast_to(np.asarray(freq, dtype=np.float64), (int(n),))
    ph = np.empty(int(n))
    ph[0] = 0.0
    np.cumsum(f[:-1] / SR, out=ph[1:])
    return ph + phase0


def sine(freq, n: int, phase0: float = 0.0):
    return np.sin(TAU * phase(freq, n, phase0))


def _blep(ph, dt):
    y = np.zeros_like(ph)
    m = ph < dt
    t = ph[m] / dt[m]
    y[m] = t + t - t * t - 1.0
    m = ph > 1.0 - dt
    t = (ph[m] - 1.0) / dt[m]
    y[m] = t * t + t + t + 1.0
    return y


def saw(freq, n: int, phase0: float = 0.0):
    """Band-limited (PolyBLEP) sawtooth."""
    f = np.broadcast_to(np.asarray(freq, dtype=np.float64), (int(n),))
    dt = np.minimum(np.abs(f) / SR, 0.49)
    ph = np.mod(phase(f, n, phase0), 1.0)
    return 2.0 * ph - 1.0 - _blep(ph, dt)


def pulse(freq, n: int, width: float = 0.5, phase0: float = 0.0):
    """Band-limited (PolyBLEP) pulse / square wave."""
    f = np.broadcast_to(np.asarray(freq, dtype=np.float64), (int(n),))
    dt = np.minimum(np.abs(f) / SR, 0.49)
    ph = np.mod(phase(f, n, phase0), 1.0)
    ph2 = np.mod(ph + (1.0 - width), 1.0)
    y = np.where(ph < width, 1.0, -1.0)
    return y + _blep(ph, dt) - _blep(ph2, dt)


def tri(freq, n: int, phase0: float = 0.0):
    ph = np.mod(phase(freq, n, phase0), 1.0)
    return 1.0 - 4.0 * np.abs(ph - 0.5)


def supersaw(freq, n: int, rng: np.random.Generator, voices: int = 5, detune_cents: float = 12.0):
    """Detuned saw stack (random start phases), unit-ish peak. freq may be an array."""
    out = np.zeros(int(n))
    for i in range(voices):
        c = 0.0 if voices == 1 else detune_cents * (2.0 * i / (voices - 1) - 1.0)
        out += saw(np.asarray(freq) * 2.0 ** (c / 1200.0), n, rng.random())
    return out / math.sqrt(voices)


def fm(carrier, modulator, index, n: int, phase_c: float = 0.0, phase_m: float = 0.0, feedback: float = 0.0):
    """Two-operator FM (phase modulation). carrier / modulator / index may be arrays."""
    pm = TAU * phase(modulator, n, phase_m)
    mod = np.sin(pm)
    return np.sin(TAU * phase(carrier, n, phase_c) + np.asarray(index) * mod)


def glide(n: int, f_from: float, f_to: float, t0: float, t1: float, curve: float = 0.0):
    """Frequency curve gliding exponentially (in pitch) from f_from to f_to between t0 and t1."""
    e = env([(t0, 0.0), (t1, 1.0)], n, curve)
    return f_from * (f_to / f_from) ** e


def midi_hz(m):
    return 440.0 * 2.0 ** ((np.asarray(m, dtype=np.float64) - 69.0) / 12.0)


_NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def note_midi(name: str) -> int:
    """'D4', 'F#5', 'Bb3' -> MIDI number."""
    m = re.match(r"([A-G])([#b]?)(-?\d)$", name)
    v = _NOTE[m.group(1)] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    return 12 * (int(m.group(3)) + 1) + v


def note_hz(name: str) -> float:
    return float(midi_hz(note_midi(name)))


# --------------------------------------------------------------------------------------------- resampling
def oversample(x, factor: int):
    """Band-limited upsampling by FFT zero-padding (treats x as periodic; fine for loops / padded shots)."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    X = np.fft.rfft(x, axis=-1)
    m = n * factor
    Y = np.zeros(x.shape[:-1] + (m // 2 + 1,), dtype=np.complex128)
    k = X.shape[-1]
    Y[..., :k] = X
    if n % 2 == 0:  # split the Nyquist bin
        Y[..., k - 1] *= 0.5
    return np.fft.irfft(Y, m, axis=-1) * factor


def downsample(y, factor: int, n: int):
    Y = np.fft.rfft(y, axis=-1)
    X = Y[..., : n // 2 + 1].copy()
    return np.fft.irfft(X, n, axis=-1) / factor


def resample(x, n_out: int):
    """FFT resampling of the last axis to n_out samples."""
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    X = np.fft.rfft(x, axis=-1)
    Y = np.zeros(x.shape[:-1] + (n_out // 2 + 1,), dtype=np.complex128)
    k = min(X.shape[-1], Y.shape[-1])
    Y[..., :k] = X[..., :k]
    return np.fft.irfft(Y, n_out, axis=-1) * (n_out / n)


def hermite(x, pos):
    """4-point cubic Hermite interpolation of x at fractional sample positions (zero outside)."""
    x = np.asarray(x, dtype=np.float64)
    xp = np.concatenate([[0.0, 0.0], x, [0.0, 0.0, 0.0]])
    pos = np.asarray(pos, dtype=np.float64)
    i = np.floor(pos).astype(np.int64)
    f = pos - i
    valid = (i >= -1) & (i < x.shape[0] + 1)
    i = np.clip(i, -2, x.shape[0]) + 2
    xm1, x0, x1, x2 = xp[i - 1], xp[i], xp[i + 1], xp[i + 2]
    c1 = 0.5 * (x1 - xm1)
    c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2
    c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1)
    y = ((c3 * f + c2) * f + c1) * f + x0
    return np.where(valid, y, 0.0)


def varispeed(x, rate, n_out: int | None = None):
    """Tape-style playback with a per-sample rate curve (pitch + time change, Doppler, slow-downs)."""
    n_out = n_out or x.shape[-1]
    r = np.broadcast_to(np.asarray(rate, dtype=np.float64), (n_out,))
    pos = np.concatenate([[0.0], np.cumsum(r[:-1])])
    return hermite(x, pos)


# --------------------------------------------------------------------------------------------- convolution
def convolve(x, h, circular: bool = False):
    """FFT convolution, output length = len(x). Mono x with stereo h gives stereo."""
    x = np.asarray(x, dtype=np.float64)
    h = np.asarray(h, dtype=np.float64)
    n = x.shape[-1]
    m = h.shape[-1]
    if circular:
        if m > n:  # fold the kernel onto the loop length
            hf = np.zeros(h.shape[:-1] + (n,))
            for s in range(0, m, n):
                seg = h[..., s:s + n]
                hf[..., : seg.shape[-1]] += seg
            h = hf
        nfft = n
    else:
        nfft = good_size(n + m)
    X = np.fft.rfft(x, nfft, axis=-1)
    H = np.fft.rfft(h, nfft, axis=-1)
    if X.ndim == 1 and H.ndim == 2:
        X = X[None, :]
    if X.ndim == 2 and H.ndim == 1:
        H = H[None, :]
    return np.fft.irfft(X * H, nfft, axis=-1)[..., :n]


# --------------------------------------------------------------------------------------------- nonlinear
def saturate(x, drive_db: float = 6.0, asym: float = 0.0, os: int = 2, mix: float = 1.0):
    """tanh soft clipper with unity small-signal gain: signals above ~-drive_db dBFS get squashed.
    asym adds even harmonics. Oversampled to keep aliasing down."""
    x = np.asarray(x, dtype=np.float64)
    g = float(db2lin(drive_db))
    n = x.shape[-1]
    y = oversample(x, os) if os > 1 else x
    y = (np.tanh(g * y + asym) - math.tanh(asym)) / (g * (1.0 - math.tanh(asym) ** 2))
    if os > 1:
        y = downsample(y, os, n)
    return y if mix >= 1.0 else (1.0 - mix) * x + mix * y


def soft_clip(x, threshold_db: float, knee_db: float = 2.0, os: int = 4):
    """Oversampled soft clipper for transients: unity gain below (threshold - knee), then a tanh
    shoulder that approaches the threshold. Used on noisy impacts to shave the first 1-3 dB of
    millisecond transients instead of letting a limiter duck the body that follows."""
    x = np.asarray(x, dtype=np.float64)
    t = float(db2lin(threshold_db))
    k = t * float(db2lin(-knee_db))
    if peak(x) <= k:
        return x.copy()
    n = x.shape[-1]
    pad = 256
    xp = np.pad(x, [(0, 0)] * (x.ndim - 1) + [(pad, pad)])
    y = oversample(xp, os)
    a = np.abs(y)
    over = a > k
    y[over] = np.sign(y[over]) * (k + (t - k) * np.tanh((a[over] - k) / (t - k)))
    return downsample(y, os, xp.shape[-1])[..., pad:pad + n]


def fold(x, drive: float = 2.0):
    """Sine wavefolder (bright, glassy / crunchy harmonics)."""
    return np.sin(0.5 * math.pi * drive * np.asarray(x))


# --------------------------------------------------------------------------------------------- dynamics
def _sliding_min(a, w: int, circular: bool = False):
    """Centred sliding minimum over an odd window w."""
    h = w // 2
    if circular:
        ap = np.concatenate([a[-h:], a, a[:h]]) if h else a
    else:
        ap = np.pad(a, (h, h), mode="edge")
    return np.lib.stride_tricks.sliding_window_view(ap, w).min(axis=-1)


def _centred_avg(a, w: int, circular: bool = False):
    h = w // 2
    w = 2 * h + 1
    ap = np.concatenate([a[-h:], a, a[:h]]) if circular and h else np.pad(a, (h, h), mode="edge")
    c = np.concatenate([[0.0], np.cumsum(ap)])
    return (c[w:] - c[:-w]) / w


def true_peak_env(X):
    """Per-sample true-peak estimate (4x oversampled, max over channels). X is (ch, n)."""
    n = X.shape[-1]
    pad = 256
    Xp = np.pad(X, ((0, 0), (pad, pad)))
    up = np.abs(oversample(Xp, 4))[:, 4 * pad: 4 * (pad + n)]
    tp = up.reshape(X.shape[0], n, 4).max(axis=2).max(axis=0)
    return np.maximum(tp, np.abs(X).max(axis=0))


def limiter(x, ceiling_db: float = -1.0, lookahead_ms: float = 1.5, release_ms: float = 80.0,
            circular: bool = False, true_peak: bool = True):
    """Look-ahead peak limiter (zero latency offline). Linked channels; guarantees |y| <= ceiling
    at the samples (and, with true_peak, at the 4x-oversampled inter-sample peaks estimate).

    Returns (y, max_gain_reduction_db)."""
    x = np.asarray(x, dtype=np.float64)
    X = x if x.ndim == 2 else x[None, :]
    n = X.shape[-1]
    ceil = float(db2lin(ceiling_db))
    if true_peak:
        if circular:
            up = np.abs(oversample(X, 4)).reshape(X.shape[0], n, 4).max(axis=2).max(axis=0)
            pk = np.maximum(up, np.abs(X).max(axis=0))
        else:
            pk = true_peak_env(X)
    else:
        pk = np.abs(X).max(axis=0)
    g = np.minimum(1.0, ceil / np.maximum(pk, 1e-12))
    if g.min() >= 1.0:
        return x.copy(), 0.0
    w = max(int(lookahead_ms * SR / 1000.0), 1)
    gmin = _sliding_min(g, 2 * w + 1, circular)
    gs = np.minimum(_centred_avg(gmin, w, circular), g)
    # release smoothing at control rate: gain may fall instantly (gs already looks ahead) but
    # recovers exponentially with release_ms.
    B = 16
    nb = int(math.ceil(n / B))
    gp = np.pad(gs, (0, nb * B - n), mode="edge").reshape(nb, B).min(axis=1)
    coef = math.exp(-B / (release_ms * SR / 1000.0))
    gl = gp.tolist()
    st = 1.0
    passes = 2 if circular else 1
    res = [0.0] * nb
    for _ in range(passes):
        for i in range(nb):
            tgt = gl[i]
            st = tgt if tgt < st else tgt + (st - tgt) * coef
            res[i] = st
    centres = (np.arange(nb) + 0.5) * B
    gr = np.interp(np.arange(n), centres, np.array(res), period=n if circular else None)
    gain = np.minimum(gs, gr)
    y = X * gain[None, :]
    y = y if x.ndim == 2 else y[0]
    return y, float(-lin2db(gain.min()))


def compress(x, threshold_db: float = -18.0, ratio: float = 3.0, attack_ms: float = 10.0,
             release_ms: float = 150.0, knee_db: float = 6.0, makeup_db: float = 0.0, block: int = 64,
             circular: bool = False):
    """Feed-forward RMS compressor (control rate). Returns compressed signal."""
    x = np.asarray(x, dtype=np.float64)
    X = x if x.ndim == 2 else x[None, :]
    n = X.shape[-1]
    nb = int(math.ceil(n / block))
    p = np.pad(np.mean(X * X, axis=0), (0, nb * block - n)).reshape(nb, block).mean(axis=1)
    lvl = 10.0 * np.log10(np.maximum(p, 1e-12)) + 3.01  # sine-RMS referenced to peak
    over = lvl - threshold_db
    gr = np.where(over <= -knee_db / 2, 0.0,
                  np.where(over >= knee_db / 2, over * (1.0 - 1.0 / ratio),
                           (1.0 - 1.0 / ratio) * (over + knee_db / 2) ** 2 / (2.0 * knee_db)))
    ca = math.exp(-block / (attack_ms * SR / 1000.0))
    cr = math.exp(-block / (release_ms * SR / 1000.0))
    st = 0.0
    out = [0.0] * nb
    grl = gr.tolist()
    for _ in range(2 if circular else 1):
        for i in range(nb):
            tgt = grl[i]
            c = ca if tgt > st else cr
            st = tgt + (st - tgt) * c
            out[i] = st
    centres = (np.arange(nb) + 0.5) * block
    g_db = np.interp(np.arange(n), centres, np.array(out), period=n if circular else None)
    gain = db2lin(makeup_db - g_db)
    y = X * gain[None, :]
    return y if x.ndim == 2 else y[0]


def transient_shape(x, attack_db: float = 4.0, sustain_db: float = 0.0, fast_ms: float = 1.0,
                    slow_ms: float = 30.0):
    """Differential-envelope transient shaper: boosts (or cuts) onsets and/or the sustain."""
    x = np.asarray(x, dtype=np.float64)
    mono = np.abs(x).max(axis=0) if x.ndim == 2 else np.abs(x)
    fast = causal_avg(mono, fast_ms)
    slow = causal_avg(mono, slow_ms)
    tr = np.clip((fast - slow) / (fast + 1e-9), 0.0, 1.0)
    tr = smooth(tr, 0.7, passes=1)
    g = db2lin(attack_db * tr + sustain_db * (1.0 - tr))
    return x * g


# --------------------------------------------------------------------------------------------- stereo
def pan(x, p):
    """Equal-power pan of a mono signal to stereo. p in [-1, 1] (scalar or per-sample)."""
    th = (np.asarray(p, dtype=np.float64) + 1.0) * (math.pi / 4.0)
    return np.stack([np.cos(th) * x, np.sin(th) * x])


def to_stereo(x):
    x = np.asarray(x, dtype=np.float64)
    return x if x.ndim == 2 else np.stack([x, x]) * SQ2


def ms_width(st, width: float):
    """Mid/side width: 0 = mono, 1 = unchanged, >1 wider."""
    m = 0.5 * (st[0] + st[1])
    s = 0.5 * (st[0] - st[1]) * width
    return np.stack([m + s, m - s])


def decorrelator(rng: np.random.Generator, ms: float = 18.0):
    """Short near-allpass FIR with random phase (flat magnitude) for stereo decorrelation."""
    L = max(ns(ms / 1000.0), 64)
    nfft = good_size(4 * L)
    k = nfft // 2 + 1
    ph = np.cumsum(rng.standard_normal(k)) * 0.35  # small random-walk steps keep the IR compact
    H = np.exp(1j * ph)
    H[0] = 1.0
    h = np.fft.irfft(H, nfft)
    h = np.roll(h, L // 3)[:L] * np.exp(-np.arange(L) / (L / 2.5))
    return h / math.sqrt(np.sum(h * h))


def stereoize(x, rng: np.random.Generator, amount: float = 0.6, ms: float = 18.0, circular: bool = False):
    """Mono -> stereo by mixing in two different decorrelated copies (keeps mono compatibility)."""
    dl = convolve(x, decorrelator(rng, ms), circular)
    dr = convolve(x, decorrelator(rng, ms), circular)
    a = amount
    L = math.sqrt(1 - a * a) * x + a * dl
    R = math.sqrt(1 - a * a) * x + a * dr
    return np.stack([L, R]) * SQ2


def haas(x, delay_ms: float = 8.0, side: float = 1.0, level_db: float = -3.0):
    """Precedence-effect widening of a mono signal (delayed copy on one side)."""
    d = ns(delay_ms / 1000.0)
    dly = np.concatenate([np.zeros(d), x[:-d]]) * db2lin(level_db) if d > 0 else x
    if side >= 0:
        return np.stack([x, dly])
    return np.stack([dly, x])


# --------------------------------------------------------------------------------------------- space
def make_ir(rt60: float, rng: np.random.Generator, predelay: float = 0.012, stereo: bool = True,
            hf_ratio: float = 0.45, lf_ratio: float = 1.15, er_level: float = 0.5, er_n: int = 10,
            er_window=(0.004, 0.06), length: float | None = None, diffusion: float = 0.025,
            lowcut: float = 80.0, highcut: float = 10000.0):
    """Synthetic room impulse response: sparse early reflections + a diffuse exponentially decaying
    noise tail whose decay time depends on frequency (hf_ratio: darker tails). Unit energy."""
    L = length if length is not None else rt60 * 1.3 + predelay + 0.05
    n = ns(L)
    t = tvec(n)
    chans = []
    lf = np.log([60.0, 250.0, 1000.0, 4000.0, 10000.0, 22050.0])
    rts = np.array([lf_ratio, 0.5 * (lf_ratio + 1), 1.0, 0.5 * (1 + hf_ratio), hf_ratio, hf_ratio * 0.6]) * rt60

    def gfn(times, f):
        rt = np.interp(np.log(np.maximum(f, 20.0)), lf, rts)
        tt = np.maximum(times - predelay, 0.0)[:, None]
        return np.exp(-6.9078 * tt / rt[None, :])

    for _ in range(2 if stereo else 1):
        tail = stft_filter(rng.standard_normal(n), gfn, frame=1024)
        on = np.clip((t - predelay) / max(diffusion, 1e-3), 0.0, None)
        tail *= np.where(t >= predelay, 1.0 - np.exp(-on * 3.0), 0.0)
        tail = eq(tail, ("hp", lowcut), ("lp", highcut))
        tail /= math.sqrt(np.sum(tail * tail)) + 1e-12
        er = np.zeros(n)
        taps = np.sort(rng.uniform(er_window[0], er_window[1], er_n))
        for tt in taps:
            k = ns(tt)
            if k < n:
                fall = 1.0 - 0.7 * (tt - er_window[0]) / max(er_window[1] - er_window[0], 1e-6)
                er[k] += (0.4 + 0.6 * rng.random()) * fall * (1 if rng.random() < 0.5 else -1)
        er = eq(er, ("lp", 6500.0), ("hp", 120.0))
        er_e = math.sqrt(np.sum(er * er)) + 1e-12
        ir = tail + er / er_e * er_level
        chans.append(ir / (math.sqrt(np.sum(ir * ir)) + 1e-12))
    return np.stack(chans) if stereo else chans[0]


def reverb(x, ir, wet_db: float = -12.0, dry: float = 1.0, circular: bool = False):
    """dry * x + wet * (x conv ir). A mono x with a stereo ir returns stereo."""
    wet = convolve(x, ir, circular) * float(db2lin(wet_db))
    xs = np.asarray(x, dtype=np.float64)
    if wet.ndim == 2 and xs.ndim == 1:
        xs = np.stack([xs, xs]) * SQ2
        wet = wet * SQ2
    return dry * xs + wet


def early_reflections(x, rng: np.random.Generator, n_taps: int = 7, t_range=(0.005, 0.045),
                      level_db: float = -9.0, lp: float = 5000.0, circular: bool = False, stereo: bool = False):
    """Short sparse reflections (ground / walls) that add weight and size without a long tail."""
    L = ns(t_range[1]) + 64
    outs = []
    for _ in range(2 if stereo else 1):
        h = np.zeros(L)
        taps = np.sort(rng.uniform(t_range[0], t_range[1], n_taps))
        for i, tt in enumerate(taps):
            h[ns(tt)] += (0.55 + 0.45 * rng.random()) * (0.85 ** i) * (1 if rng.random() < 0.6 else -1)
        h = eq(h, ("lp", lp), ("hp", 90.0))
        h *= float(db2lin(level_db)) / (math.sqrt(np.sum(h * h)) + 1e-12)
        h[0] += 1.0
        outs.append(h)
    return convolve(x, np.stack(outs) if stereo else outs[0], circular)


def slap_echoes(x, delays=(0.11, 0.23, 0.37), gains_db=(-14.0, -18.0, -23.0), lp: float = 2500.0,
                circular: bool = False):
    """Discrete distant echoes (terrain / canyon slaps) for big outdoor events."""
    L = ns(max(delays)) + 8
    h = np.zeros(L)
    for d, g in zip(delays, gains_db):
        h[ns(d)] += float(db2lin(g))
    wet = eq(convolve(x, h, circular), ("lp", lp), ("hp", 60.0), circular=circular)
    return np.asarray(x) + wet


# --------------------------------------------------------------------------------------------- events
def poisson_times(rate, t0: float, t1: float, rng: np.random.Generator):
    """Inhomogeneous Poisson event times on [t0, t1). rate: events/s or callable(t)."""
    if t1 <= t0:
        return np.zeros(0)
    if callable(rate):
        grid = np.linspace(t0, t1, 1024)
        rmax = float(np.max(rate(grid))) * 1.1 + 1e-9
    else:
        rmax = float(rate)
    count = rng.poisson(rmax * (t1 - t0))
    ts = np.sort(rng.uniform(t0, t1, count))
    if callable(rate) and count:
        ts = ts[rng.uniform(0.0, rmax, count) < rate(ts)]
    return ts


def impulses(n: int, times, amps, wrap: bool = False):
    buf = np.zeros(int(n))
    idx = np.round(np.asarray(times) * SR).astype(np.int64)
    amps = np.broadcast_to(np.asarray(amps, dtype=np.float64), idx.shape)
    if wrap:
        idx = np.mod(idx, n)
        np.add.at(buf, idx, amps)
    else:
        m = (idx >= 0) & (idx < n)
        np.add.at(buf, idx[m], amps[m])
    return buf


def heavy_tail(rng: np.random.Generator, count: int, alpha: float = 2.2, cap: float = 10.0):
    """Many small, few large amplitudes (Pareto) in (0, 1]; the largest events reach 1."""
    a = np.minimum(rng.pareto(alpha, count) + 1.0, cap)
    return a / cap


def modal(freqs, decays, amps, dur: float | None = None, attack: float = 0.0004, rng=None):
    """Sum of exponentially decaying sinusoids (struck objects, bells, wood, glass)."""
    freqs = np.asarray(freqs, dtype=np.float64)
    decays = np.asarray(decays, dtype=np.float64)
    amps = np.asarray(amps, dtype=np.float64)
    L = dur if dur is not None else float(np.max(decays)) * 7.0
    t = tvec(max(ns(L), 8))
    ph = rng.random(len(freqs)) * TAU if rng is not None else np.zeros(len(freqs))
    y = np.zeros_like(t)
    for f, d, a, p in zip(freqs, decays, amps, ph):
        if f >= 0.48 * SR:
            continue
        y += a * np.exp(-t / d) * np.sin(TAU * f * t + p)
    if attack > 0:
        y *= 1.0 - np.exp(-t / attack)
    return y


def fm_bell(f: float, dur: float, ratio: float = 1.4, index: float = 3.0, amp_tau: float = 0.8,
            index_tau: float = 0.25, attack: float = 0.002):
    """Chowning-style FM bell / chime (inharmonic ratio -> bell, integer ratio -> glassy chime)."""
    n = ns(dur)
    t = tvec(n)
    I = index * np.exp(-t / index_tau)
    y = np.sin(TAU * f * t + I * np.sin(TAU * f * ratio * t))
    e = np.exp(-t / amp_tau) * (1.0 - np.exp(-t / max(attack, 1e-4)))
    return y * e


def bubble(f0: float, rise: float = 0.3, decay_mult: float = 1.0, max_len: float = 0.4):
    """One bubble (van den Doel model): damped sine whose pitch rises as it nears the surface."""
    d = (0.13 * f0 + 0.0072 * f0 ** 1.5) / decay_mult
    L = min(6.0 / d, max_len)
    t = tvec(max(ns(L), 16))
    f = f0 * (1.0 + rise * d * t)
    ph = TAU * np.cumsum(f) / SR
    return np.sin(ph) * np.exp(-d * t) * (1.0 - np.exp(-t / 0.0006))


def droplet(f0: float, tau: float = 0.012, glide_to: float = 2.2, click: float = 0.3,
            rng: np.random.Generator | None = None):
    """Water droplet 'plink': fast upward chirp with a tiny impact click."""
    L = tau * 7.0
    t = tvec(max(ns(L), 16))
    f = f0 * (1.0 + (glide_to - 1.0) * (1.0 - np.exp(-t / 0.004)))
    y = np.sin(TAU * np.cumsum(f) / SR) * np.exp(-t / tau) * (1.0 - np.exp(-t / 0.0005))
    if click > 0 and rng is not None:
        k = min(len(t), 48)
        y[:k] += click * rng.standard_normal(k) * np.exp(-np.arange(k) / 8.0)
    return y


def pluck(f0: float, dur: float, rng: np.random.Generator, t60: float = 3.0, t60_hf: float = 0.5,
          hf_ref: float = 3000.0, pick: float = 0.15, bright: float = 5000.0, amp: float = 1.0):
    """Karplus-Strong string evaluated exactly in the frequency domain.

    The KS loop y[n] = x[n] + g(f) y[n - P] with a *fractional* period P = SR / f0 is applied as
    H(f) = 1 / (1 - g(f) e^{-j 2 pi f P / SR}), so tuning is exact at any pitch. g(f) is derived from
    the wanted decay time per frequency: T60(f) = 1 / (1/t60 + (f/hf_ref)^2 / t60_hf), i.e. the
    upper partials die away faster like a real string. Excitation = one period of noise shaped by
    a pick-position comb and a brightness low-pass."""
    n = ns(dur)
    nfft = good_size(n + ns(min(0.35 * t60, 2.5)))
    P = SR / f0
    f = np.fft.rfftfreq(nfft, 1.0 / SR)
    rate = 1.0 / t60 + (f / hf_ref) ** 2 / t60_hf
    g = 10.0 ** (-3.0 * rate / f0)
    H = 1.0 / (1.0 - g * np.exp(-1j * TAU * f * P / SR))
    L = max(int(math.ceil(P)), 2)
    e = rng.uniform(-1.0, 1.0, L) * np.sin(np.pi * (np.arange(L) + 0.5) / L)
    E = np.fft.rfft(e, nfft)
    E *= 1.0 - np.exp(-1j * TAU * f * pick * P / SR)  # pick position comb
    E /= 1.0 + 1j * f / max(bright, 50.0)  # brightness (one-pole)
    y = np.fft.irfft(E * H, nfft)[:n]
    y = fade(y, 0.0, min(0.05, dur * 0.2))
    p = peak(y)
    return y * (amp / p) if p > 0 else y
