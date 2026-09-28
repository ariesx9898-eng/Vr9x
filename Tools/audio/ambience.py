"""Region ambience beds (stereo, 24-30 s, seamless loops).

Every layer is built circularly: FFT-coloured noise, circular filtering (eq / sweep / formant with
circular=True), periodic modulation curves (smooth_random / periodic_lfo), events placed with
wrap=True (a bird call that starts near the end finishes at the start) and circular convolution
reverb. Oscillators with drifting pitch get their frequency nudged so the loop holds a whole number
of cycles. The result is exactly periodic: the joint is just another sample."""
from __future__ import annotations

import math

import numpy as np

from dsp import (SR, TAU, db2lin, env, eq, fm, formant_filter, make_ir, modal, noise, norm_peak, norm_rms, ns, pan,
                 periodic_lfo, perc, phase, place, reverb, saw, smooth, smooth_random, sweep, tvec)
from layers import crack, crackle, fpath, turbulence
from reg import sound

G = lambda d: float(db2lin(d))  # noqa: E731
AMB = -25.0  # integrated LUFS for the beds


# --------------------------------------------------------------------------------------------- helpers
def st_noise(rng, n, color="pink", corr=0.25):
    """Stereo noise with partial inter-channel correlation (0 = fully wide, 1 = mono)."""
    a = noise(n, rng, color)
    b = noise(n, rng, color)
    if corr > 0:
        c = noise(n, rng, color)
        a = math.sqrt(corr) * c + math.sqrt(1 - corr) * a
        b = math.sqrt(corr) * c + math.sqrt(1 - corr) * b
    return np.stack([a, b])


def gust(rng, n, rate=0.08, floor=0.2, sharp=1.0):
    """Periodic gust strength curve in [floor, 1]."""
    g = 1.0 / (1.0 + np.exp(-1.8 * smooth_random(n, rng, rate)))
    g = g ** sharp
    return floor + (1.0 - floor) * (g - g.min()) / (g.max() - g.min() + 1e-12)


def loop_phase(freq, n):
    """Running phase (cycles) with the frequency nudged so a whole number of cycles fits the loop."""
    f = np.array(np.broadcast_to(np.asarray(freq, dtype=np.float64), (n,)))
    total = f.sum() / SR
    f += (round(total) - total) * SR / n
    return phase(f, n)


def wet(x, rng, rt60, wet_db, hf=0.4, predelay=0.02):
    ir = make_ir(rt60, rng, predelay=predelay, stereo=True, hf_ratio=hf, er_level=0.3)
    return reverb(x, ir, wet_db, circular=True)


def bird_call(rng, kind):
    """FM bird calls. kind: 'trill' (lark-like warble run), 'teeoo' (two-note whistle),
    'chip' (short sharp chirps), 'warble' (soft song-thrush-like phrase)."""
    parts = []
    if kind == "trill":
        base = rng.uniform(3200, 4400)
        for i in range(int(rng.integers(5, 11))):
            d = rng.uniform(0.045, 0.07)
            m = ns(d)
            t = tvec(m)
            f = base * (1.0 + 0.16 * t / d) * (1.0 + 0.03 * i * rng.uniform(-1, 1))
            y = fm(f, 55.0, 1.8, m) * env([(0.0, 0.0), (0.006, 1.0), (d, 0.0)], m, [0.0, -2.0])
            parts.append((y, rng.uniform(0.06, 0.09)))
    elif kind == "teeoo":
        for f0, f1, d in ((rng.uniform(4000, 4400), rng.uniform(4500, 4800), 0.15), (rng.uniform(3300, 3500), rng.uniform(2900, 3100), 0.2)):
            m = ns(d)
            t = tvec(m)
            f = (f0 + (f1 - f0) * t / d) * (1.0 + 0.012 * np.sin(TAU * 22.0 * t))
            y = np.sin(TAU * np.cumsum(f) / SR) * env([(0.0, 0.0), (0.03, 1.0), (d * 0.7, 0.8), (d, 0.0)], m, [1.0, 0.0, -2.0])
            parts.append((y, d + 0.06))
    elif kind == "chip":
        for _ in range(int(rng.integers(1, 4))):
            d = rng.uniform(0.02, 0.035)
            m = ns(d)
            t = tvec(m)
            f = rng.uniform(5000, 6200) * (1.0 - 0.3 * t / d)
            y = np.sin(TAU * np.cumsum(f) / SR) * env([(0.0, 0.0), (0.003, 1.0), (d, 0.0)], m, [0.0, -2.0])
            parts.append((y, rng.uniform(0.09, 0.2)))
    else:  # warble
        for _ in range(int(rng.integers(3, 7))):
            d = rng.uniform(0.08, 0.16)
            m = ns(d)
            t = tvec(m)
            f0 = rng.uniform(2200, 3600)
            f = f0 * (1.0 + rng.uniform(-0.25, 0.25) * np.sin(np.pi * t / d))
            y = fm(f, rng.uniform(25, 60), rng.uniform(0.5, 1.5), m) * env([(0.0, 0.0), (0.015, 1.0), (d, 0.0)], m, [0.0, -1.5])
            parts.append((y, d + rng.uniform(0.03, 0.1)))
    total = sum(gap for _, gap in parts) + 0.3
    out = np.zeros(ns(total))
    t = 0.0
    for y, gap in parts:
        place(out, y, ns(t))
        t += gap
    return eq(out, ("hp", 1500))


def ev(x, peak_db):
    """Sparse event layer, peak-referenced: peak_db is relative to a bed normalised to RMS 1 (whose
    noise peaks sit around +12 dB). RMS-normalising sparse events would blow their peaks up."""
    return norm_peak(x) * G(peak_db)


def scatter(n, rng, count, maker, level_db, pan_range=0.8, min_gap=0.0):
    """Place `count` events from maker(rng) at random times (wrapped), randomly panned."""
    T = n / SR
    out = np.zeros((2, n))
    times = np.sort(rng.uniform(0.0, T, count))
    last = -1e9
    for t in times:
        if t - last < min_gap:
            continue
        last = t
        y = maker(rng)
        place(out, pan(y * rng.uniform(0.5, 1.0), float(rng.uniform(-pan_range, pan_range))), ns(t), wrap=True)
    return norm_peak(out) * G(level_db)


# --------------------------------------------------------------------------------------------- beds
@sound("Ambience", 24.0, loop=True, stereo=True, target=AMB, use="Plains / Asura / Fittoa fields: gentle breeze, grass, distant birds")
def amb_plains_wind(rng, n):
    g = gust(rng, n, 0.07, 0.25)
    bed = st_noise(rng, n, "pink", 0.3)
    bed = sweep(bed, "lp", 350.0 + 1000.0 * g, 0.7, circular=True) * (0.35 + 0.65 * g)
    rustle = eq(st_noise(rng, n, "white", 0.1), ("hp", 2000), ("lp", 7000), circular=True)
    rustle = rustle * (g ** 2.2) * np.maximum(1.0 + 0.5 * smooth_random(n, rng, 12.0), 0.0)
    low = eq(st_noise(rng, n, "brown", 0.6), ("lp", 120), ("hp", 30), circular=True)
    birds = scatter(n, rng, 14, lambda r: bird_call(r, ["trill", "teeoo", "chip", "warble"][int(r.integers(0, 4))]),
                    0.0, min_gap=0.8)
    birds = eq(birds, ("lp", 6500), circular=True)
    birds = wet(birds, rng, 1.6, -6.0, hf=0.5)
    y = norm_rms(bed) + norm_rms(rustle) * G(-13) + norm_rms(low) * G(-20) + ev(birds, 3.0)
    return y


@sound("Ambience", 24.0, loop=True, stereo=True, target=AMB, use="Northern Territories / Ranoa / high snow: howling cold wind")
def amb_snow_wind(rng, n):
    g = gust(rng, n, 0.09, 0.15, sharp=1.3)
    voices = np.zeros((2, n))
    for base, q, p in ((380.0, 16.0, -0.6), (520.0, 18.0, 0.5), (690.0, 20.0, -0.2), (900.0, 14.0, 0.7)):
        fc = base * (1.0 + 0.22 * smooth_random(n, rng, 0.07)) * (0.85 + 0.3 * g)
        h = formant_filter(noise(n, rng, "white"), [(fc, q, 0.0)], frame=4096, circular=True)
        a = np.maximum(0.2 + 0.8 * (0.5 + 0.5 * smooth_random(n, rng, 0.12)), 0.0) * g
        voices += pan(norm_rms(h) * a, p)
    body = st_noise(rng, n, "pink", 0.3)
    body = sweep(body, "lp", 400.0 + 900.0 * g, 0.7, circular=True) * g
    hiss = eq(st_noise(rng, n, "white", 0.0), ("hp", 5000), ("lp", 12000), circular=True) * g ** 1.5
    low = eq(st_noise(rng, n, "brown", 0.7), ("lp", 100), ("hp", 25), circular=True) * (0.5 + 0.5 * g)
    y = norm_rms(voices) * G(-2) + norm_rms(body) + norm_rms(hiss) * G(-16) + norm_rms(low) * G(-16)
    return wet(y, rng, 1.2, -16.0, hf=0.5)


@sound("Ambience", 24.0, loop=True, stereo=True, target=AMB, use="Begaritt desert: dry gusty wind with blowing sand hiss")
def amb_desert_wind(rng, n):
    T = n / SR
    g = gust(rng, n, 0.16, 0.12, sharp=1.8)
    wind = eq(st_noise(rng, n, "pink", 0.3), ("hp", 180), circular=True)
    wind = sweep(wind, "lp", 900.0 + 1800.0 * g, 0.7, circular=True) * (0.25 + 0.75 * g)
    sand = np.stack([crackle(rng, n, 6000.0, 0.0, T, band=(3000, 12000), tick_ms=(0.05, 0.3), pop_prob=0.0, wrap=True)
                     for _ in range(2)])
    sand = sand * g ** 1.6
    swirls = np.zeros((2, n))
    for tc in rng.uniform(0.0, T, 3):
        m = ns(1.6)
        tt = tvec(m)
        s = noise(m, rng, "white") * env([(0.0, 0.0), (0.7, 1.0), (1.6, 0.0)], m, [2.0, -2.0])
        s = sweep(s, "bp", fpath([(0.0, 1500), (0.7, 4200), (1.6, 2000)]), 2.0)
        p0 = float(rng.uniform(-0.8, 0.8))
        place(swirls, pan(norm_peak(s), np.clip(p0 + 0.9 * np.sin(np.pi * tt / 1.6) * np.sign(-p0), -1, 1)), ns(tc), wrap=True)
    low = eq(st_noise(rng, n, "brown", 0.6), ("lp", 150), ("hp", 30), circular=True) * (0.4 + 0.6 * g)
    return norm_rms(wind) + norm_rms(sand) * G(-9) + ev(swirls, 2.0) + norm_rms(low) * G(-17)


def _creak(rng):
    d = rng.uniform(0.6, 1.4)
    m = ns(d)
    t = tvec(m)
    rate = rng.uniform(15, 22) + rng.uniform(15, 25) * np.sin(np.pi * t / d) ** 1.5
    ph = np.cumsum(rate) / SR
    ticks = np.zeros(m)
    idx = np.nonzero(np.diff(np.floor(ph)) > 0)[0]
    ticks[idx] = rng.uniform(0.5, 1.0, idx.size)
    f0 = rng.uniform(320, 650)
    body = modal([f0, f0 * 2.1, f0 * 3.3], [0.025, 0.015, 0.01], [1.0, 0.6, 0.3], rng=rng)
    y = np.convolve(ticks, body)[:m] * env([(0.0, 0.0), (0.1, 1.0), (d * 0.8, 0.8), (d, 0.0)], m, [1.0, 0.0, -1.0])
    return eq(y, ("hp", 200))


def _drip(rng):
    from dsp import droplet
    y = droplet(rng.uniform(1300, 3000), rng.uniform(0.012, 0.028), rng.uniform(1.5, 2.4), 0.2, rng)
    if rng.random() < 0.3:
        y2 = droplet(rng.uniform(1300, 3000), 0.015, 2.0, 0.2, rng)
        out = np.zeros(y.shape[0] + ns(0.25))
        out[: y.shape[0]] += y
        k = ns(rng.uniform(0.12, 0.22))
        out[k:k + y2.shape[0]] += 0.6 * y2
        return out
    return y


@sound("Ambience", 28.0, loop=True, stereo=True, target=AMB, use="Forests (Great Forest, Fittoa woods): dense leaf rustle, drips, creaks, soft birds")
def amb_forest(rng, n):
    T = n / SR
    g = gust(rng, n, 0.1, 0.2)
    rust = eq(st_noise(rng, n, "white", 0.05), ("hp", 1500), ("lp", 6500), circular=True)
    rust = rust * (0.15 + g ** 2) * np.stack([turbulence(rng, n, ((9.0, 0.6), (25.0, 0.4))) for _ in range(2)])
    leaves = np.stack([crackle(rng, n, lambda t: 250.0 + 500.0 * np.interp(t, tvec(n), g), 0.0, T, band=(1500, 7000),
                               tick_ms=(1.5, 6.0), pop_prob=0.0, wrap=True) for _ in range(2)])
    canopy = eq(st_noise(rng, n, "pink", 0.3), ("lp", 900), ("hp", 60), circular=True) * g
    drips = scatter(n, rng, 16, _drip, 0.0, pan_range=0.9)
    creaks = scatter(n, rng, 3, _creak, 0.0, pan_range=0.7, min_gap=4.0)
    birds = scatter(n, rng, 20, lambda r: bird_call(r, ["chip", "warble", "trill"][int(r.integers(0, 3))]), 0.0, min_gap=0.5)
    birds = eq(birds, ("lp", 7000), circular=True)
    events = ev(drips, 2.0) + ev(creaks, -2.0) + ev(birds, 2.0)
    events = wet(events, rng, 1.3, -5.0, hf=0.35)
    y = norm_rms(rust) + norm_rms(leaves) * G(-6) + norm_rms(canopy) * G(-6) + events
    return wet(y, rng, 1.0, -18.0, hf=0.35)


@sound("Ambience", 30.0, loop=True, stereo=True, target=AMB, use="Demon Continent: eerie low drone, dusty gusts, distant rumbles")
def amb_demon(rng, n):
    T = n / SR
    t = tvec(n)
    k = 1.0 / T  # all drone frequencies are whole multiples of 1/T -> exact loop
    drone = np.zeros((2, n))
    for f, gain, p in ((round(55.0 / k) * k, 1.0, -0.3), (round(55.0 / k + 1) * k, 0.8, 0.3), (round(77.78 / k) * k, 0.55, 0.0),
                       (round(110.0 / k) * k, 0.45, -0.2), (round(110.0 / k + 1) * k, 0.4, 0.2)):
        drone += pan(saw(f, n, rng.random()) * gain, p)
    drone = sweep(drone, "lp", 260.0 + 160.0 * (1.0 + np.sin(TAU * t / T)), 0.9, order=4, circular=True)
    wf = 1250.0 * (1.0 + 0.12 * smooth_random(n, rng, 0.06))
    whistle = np.sin(TAU * loop_phase(wf, n)) * np.maximum(smooth_random(n, rng, 0.05), 0.0) ** 3
    g = gust(rng, n, 0.08, 0.1, sharp=1.5)
    wind = eq(st_noise(rng, n, "pink", 0.3), ("hp", 150), ("lp", 1200), circular=True) * g
    rum = np.zeros((2, n))
    for tc in rng.uniform(0.0, T, 3):
        m = ns(3.2)
        r = eq(noise(m, rng, "brown"), ("lp", float(rng.uniform(90, 150))), ("hp", 25)) * env([(0.0, 0.0), (0.6, 1.0), (3.2, 0.0)], m, [1.0, -2.0])
        place(rum, pan(norm_peak(r), float(rng.uniform(-0.7, 0.7))), ns(tc), wrap=True)
    cracks = np.zeros((2, n))
    for tc in rng.uniform(0.0, T, 2):
        c = eq(crack(rng, ns(0.3), 0.0, fc=900, tau_ms=20, drive_db=8), ("lp", 1000))
        place(cracks, pan(c, float(rng.uniform(-0.8, 0.8))), ns(tc), wrap=True)
    breath = np.zeros((2, n))
    tb = float(rng.uniform(0, T))
    m = ns(3.0)
    b = formant_filter(noise(m, rng, "pink"), [(600.0, 5.0, 0.0), (1000.0, 6.0, -4.0)], floor=0.02)
    b *= env([(0.0, 0.0), (1.4, 1.0), (3.0, 0.0)], m, [2.0, -2.0])
    place(breath, pan(norm_peak(b), 0.4), ns(tb), wrap=True)
    y = (norm_rms(drone) + pan(norm_rms(whistle), -0.4) * G(-24) + norm_rms(wind) * G(-4) + norm_rms(rum) * G(-3) +
         ev(cracks, 5.0) + ev(breath, -2.0))
    return wet(y, rng, 3.0, -10.0, hf=0.3, predelay=0.04)


def _wave_layer(rng, m, L, tc):
    swell = eq(noise(m, rng, "brown"), ("lp", 200), ("hp", 30)) * env([(0.0, 0.0), (tc, 1.0), (tc + 1.0, 0.3), (L, 0.0)], m, [2.0, -1.0, -2.0])
    crash = noise(m, rng, "pink") * env([(tc - 0.35, 0.0), (tc, 1.0), (tc + 3.5, 0.15), (L, 0.0)], m, [2.5, -1.8, -1.5])
    crash = sweep(crash, "lp", fpath([(tc - 0.35, 400), (tc, 5500), (tc + 3.5, 1000)]), 0.7)
    foam = crackle(rng, m, lambda t: 3000.0 * np.exp(-np.maximum(t - tc, 0) / 1.2) * (t > tc - 0.1), tc - 0.1, L,
                   band=(2500, 10000), tick_ms=(0.05, 0.3), pop_prob=0.0)
    drag = crackle(rng, m, lambda t: 350.0 * np.exp(-((t - tc - 2.8) / 1.0) ** 2), tc + 1.2, L, band=(800, 4000),
                   tick_ms=(0.4, 2.0), pop_prob=0.2, pop_band=(400, 1200))
    return norm_rms(swell) * G(-8) + norm_rms(crash) + norm_rms(foam) * G(-10) + norm_rms(drag) * G(-16)


def _wave(rng, size):
    """One wave on the shore: swell, break, crash, long wash with foam fizz and pebble drag. A shared
    (centre) layer plus an independent layer per side gives width with a stable centre image."""
    L = 7.5
    m = ns(L)
    tc = 1.5
    centre = _wave_layer(rng, m, L, tc)
    out = np.stack([0.75 * centre + 0.66 * _wave_layer(rng, m, L, tc), 0.75 * centre + 0.66 * _wave_layer(rng, m, L, tc)])
    return norm_peak(out) * size


@sound("Ambience", 28.0, loop=True, stereo=True, target=AMB, use="Coasts / ports (Wenport, Zant Port, beaches): waves breaking on the shore")
def amb_ocean(rng, n):
    T = n / SR
    waves = np.zeros((2, n))
    starts = [0.4, 7.4, 13.9, 20.8]
    for i, t0 in enumerate(starts):
        w = _wave(rng, float(rng.uniform(0.65, 1.0)))
        p = float(rng.uniform(-0.35, 0.35))
        w = np.stack([w[0] * math.sqrt(0.5 - 0.5 * p), w[1] * math.sqrt(0.5 + 0.5 * p)]) * math.sqrt(2.0)
        place(waves, w, ns(t0 + rng.uniform(-0.3, 0.3)), wrap=True)
    bed = eq(st_noise(rng, n, "pink", 0.2), ("lp", 500), ("hp", 40), circular=True)
    bed = bed * np.maximum(1.0 + 0.3 * smooth_random(n, rng, 0.3), 0.2)
    low = eq(st_noise(rng, n, "brown", 0.6), ("lp", 150), ("hp", 25), circular=True)
    wind = eq(st_noise(rng, n, "pink", 0.2), ("hp", 400), ("lp", 3000), circular=True) * gust(rng, n, 0.08, 0.3)
    y = norm_rms(waves) + norm_rms(bed) * G(-10) + norm_rms(low) * G(-14) + norm_rms(wind) * G(-20)
    return wet(y, rng, 1.0, -18.0, hf=0.4)


_VOWELS = [(730, 1090, 2440), (530, 1840, 2480), (270, 2290, 3010), (570, 840, 2410), (300, 870, 2240), (660, 1700, 2400)]


def _voice(rng, n):
    """One indistinct talker: glottal buzz + breath through hopping vowel formants, syllabic AM,
    phrases with pauses. Built circularly."""
    T = n / SR
    # syllable-rate formant tracks (circular smoothing keeps them periodic)
    tracks = np.zeros((3, n))
    t = 0.0
    while t < T:
        d = rng.uniform(0.08, 0.25)
        v = _VOWELS[int(rng.integers(0, len(_VOWELS)))]
        a, b = ns(t), min(ns(t + d), n)
        for i in range(3):
            tracks[i, a:b] = v[i] * rng.uniform(0.9, 1.1)
        t += d
    w = ns(0.03)
    for i in range(3):  # circular moving average (cumsum), keeps the tracks periodic
        ext = np.concatenate([tracks[i, -w:], tracks[i], tracks[i, :w]])
        c = np.concatenate([[0.0], np.cumsum(ext)])
        tracks[i] = ((c[w:] - c[:-w]) / w)[w // 2: w // 2 + n]
    f0 = rng.uniform(100, 230) * (1.0 + 0.08 * smooth_random(n, rng, 1.5))
    src = saw(f0, n, rng.random()) * 0.6 + noise(n, rng, "pink") * 0.5
    y = formant_filter(src, [(tracks[0], 6.0, 0.0), (tracks[1], 8.0, -5.0), (tracks[2], 9.0, -12.0)], circular=True, floor=0.01)
    syl = np.maximum(smooth_random(n, rng, 5.0), 0.0) ** 1.2
    phr = gust(rng, n, 0.25, 0.0, sharp=2.5)
    return eq(y * syl * phr, ("lp", 1800), ("hp", 150), circular=True)


def _hammer(rng):
    out = np.zeros(ns(4.5))
    t = 0.0
    for _ in range(int(rng.integers(4, 7))):
        k = modal([1100 * rng.uniform(0.98, 1.02), 2750, 4300, 5900], [0.35, 0.2, 0.12, 0.08], [1.0, 0.6, 0.4, 0.25], rng=rng)
        th = np.sin(TAU * 120.0 * tvec(ns(0.08))) * np.exp(-tvec(ns(0.08)) / 0.02)
        place(out, k * rng.uniform(0.7, 1.0), ns(t))
        place(out, th * 0.5, ns(t))
        t += rng.uniform(0.5, 0.75)
    return eq(out, ("lp", 3500), ("hp", 150))


def _cart(rng, n):
    d = rng.uniform(4.5, 6.0)
    m = ns(d)
    t = tvec(m)
    rot = rng.uniform(2.4, 3.2)
    rumble = eq(noise(m, rng, "brown"), ("lp", 300), ("hp", 50)) * (1.0 + 0.4 * np.sin(TAU * rot * t))
    clunks = np.zeros(m)
    for tt in np.arange(0.05, d, 1.0 / rot):
        k = modal([180, 420, 900], [0.04, 0.025, 0.012], [1.0, 0.6, 0.3], rng=rng)
        place(clunks, k * rng.uniform(0.5, 1.0), ns(tt + rng.uniform(-0.01, 0.01)))
    y = (norm_rms(rumble) + norm_rms(clunks) * G(-3)) * env([(0.0, 0.0), (1.2, 1.0), (d - 1.2, 1.0), (d, 0.0)], m, [1.0, 0.0, -1.0])
    p = np.linspace(-0.8, 0.8, m) * (1 if rng.random() < 0.5 else -1)
    return pan(eq(y, ("lp", 2500)), p)


@sound("Ambience", 30.0, loop=True, stereo=True, target=AMB, use="Towns and cities (Roa, Ars, Sharia, Millishion, Rapan...): distant murmur, hammer, carts")
def amb_town(rng, n):
    T = n / SR
    mur = np.zeros((2, n))
    for i in range(14):
        v = _voice(rng, n)
        mur += pan(norm_rms(v) * rng.uniform(0.5, 1.0), float(rng.uniform(-0.85, 0.85)))
    mur = wet(mur, rng, 1.2, -6.0, hf=0.4)
    ham = np.zeros((2, n))
    for tc in rng.uniform(0.0, T, 3):
        place(ham, pan(_hammer(rng), float(rng.uniform(-0.8, 0.8))), ns(tc), wrap=True)
    carts = np.zeros((2, n))
    for tc in rng.uniform(0.0, T, 2):
        place(carts, _cart(rng, n), ns(tc), wrap=True)
    events = wet(ev(ham, 4.0) + norm_rms(carts) * G(-7), rng, 1.4, -6.0, hf=0.4)
    bed = eq(st_noise(rng, n, "pink", 0.3), ("lp", 500), ("hp", 40), circular=True)
    birds = scatter(n, rng, 5, lambda r: bird_call(r, "chip"), 0.0, min_gap=3.0)
    return norm_rms(mur) + events + norm_rms(bed) * G(-10) + ev(birds, -6.0)
