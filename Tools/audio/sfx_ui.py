"""Menu / UI sounds (stereo, 2D). Warm and tactile: wooden/parchment clicks from small modal
resonators, paper crinkle (dense micro-grains) with soft whooshes for panels, FM chimes in D major
for confirmations, a rising magical swell for SPAWN. Quiet by design; no clipping stage."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, decorrelator, convolve, env, eq, fm_bell, modal, noise, norm_peak, ns, pan, perc,
                 place, pluck, saw, stereoize, sweep, to_stereo, tri, tvec)
from layers import burst, crackle, fpath, moving_noise, space, whoosh
from reg import sound

G = lambda d: float(db2lin(d))  # noqa: E731


def room(x, rng, wet_db=-20.0, rt=0.25):
    return space(to_stereo(x) if np.ndim(x) == 1 else x, rng, rt60=rt, wet_db=wet_db, stereo=True, hf_ratio=0.6,
                 predelay=0.004)


def wood_click(rng, n, t0, base=620.0, amp=1.0):
    y = np.zeros(n)
    k = modal([base, base * 2.39, base * 3.76, base * 6.29], [0.013, 0.008, 0.006, 0.004], [1.0, 0.55, 0.35, 0.15], rng=rng)
    place(y, k, ns(t0))
    return norm_peak(y) * amp


def paper_tick(rng, n, t0, amp=1.0, hp=3000.0):
    return burst(rng, n, t0, 0.2, 1.5, "white", (("hp", hp), ("lp", 12000)), amp=amp)


def crinkle(rng, n, rate_fn, t0, t1, band=(2000.0, 9000.0)):
    return crackle(rng, n, rate_fn, t0, t1, band=band, tick_ms=(0.1, 0.5), pop_prob=0.0, alpha=2.5)


@sound("UI", 0.08, stereo=True, target=-30.0, use="Menu item hover: very soft tick")
def ui_hover(rng, n):
    t = tvec(n)
    tick = burst(rng, n, 0.002, 0.2, 1.5, "white", (("bp", 5000, 1.2),), amp=1.0)
    tone = np.sin(TAU * 3200.0 * t) * perc(n, 0.002, 0.0015, 0.006)
    y = tick * G(-4) + norm_peak(tone) * G(-6)
    return stereoize(y, rng, 0.35)


@sound("UI", 0.12, stereo=True, target=-24.0, use="Button click: warm wooden / parchment click")
def ui_click(rng, n):
    t0 = 0.002
    wood = wood_click(rng, n, t0, 620.0)
    tick = paper_tick(rng, n, t0, 1.0)
    t = tvec(n)
    body = np.sin(TAU * np.cumsum(150.0 + 70.0 * np.exp(-t / 0.004)) / SR) * perc(n, t0, 0.0008, 0.012)
    y = wood + tick * G(-8) + norm_peak(body) * G(-10)
    return room(y, rng)


@sound("UI", 0.45, stereo=True, target=-22.0, use="Panel / menu open: parchment unfurls with a soft whoosh")
def ui_open(rng, n):
    T = n / SR
    cr = crinkle(rng, n, lambda x: 300 + 900 * np.exp(-((x - 0.15) / 0.1) ** 2), 0.0, 0.4)
    wh = whoosh(rng, n, 0.0, 0.42, [(0.0, 600), (0.3, 2400), (0.42, 2000)], q=1.5, peak_at=0.4, amp=1.0)
    y = norm_peak(cr) * G(-3) + wh * G(-2)
    p = np.clip(-0.5 + tvec(n) / 0.4, -0.5, 0.5)
    st = pan(y, p)
    st = st + stereoize(norm_peak(cr) * G(-10), rng, 0.8)
    return room(st, rng, wet_db=-18, rt=0.35)


@sound("UI", 0.42, stereo=True, target=-23.0, use="Panel / menu close: whoosh down, parchment folds, soft thud")
def ui_close(rng, n):
    cr = crinkle(rng, n, lambda x: 250 + 600 * np.exp(-((x - 0.1) / 0.08) ** 2), 0.0, 0.26)
    wh = whoosh(rng, n, 0.0, 0.3, [(0.0, 2200), (0.3, 500)], q=1.5, peak_at=0.35, amp=1.0)
    thud = burst(rng, n, 0.26, 1.0, 25, "pink", (("lp", 700), ("hp", 80)), amp=1.0)
    knock = wood_click(rng, n, 0.26, 300.0)
    y = norm_peak(cr) * G(-5) + wh * G(-3) + thud * G(-4) + knock * G(-6)
    p = np.clip(0.4 - tvec(n) / 0.3 * 0.8, -0.4, 0.4)
    return room(pan(y, p), rng, wet_db=-18, rt=0.3)


@sound("UI", 0.12, stereo=True, target=-25.0, use="Tab switch: light page flick")
def ui_tab(rng, n):
    fl = burst(rng, n, 0.002, 0.5, 8, "white", (("bp", 2800, 0.8),), amp=1.0)
    cr = crinkle(rng, n, lambda x: 800 * np.exp(-x / 0.04), 0.0, 0.08)
    tick = wood_click(rng, n, 0.01, 900.0)
    y = fl + norm_peak(cr) * G(-8) + tick * G(-10)
    return room(stereoize(y, rng, 0.4), rng)


def _chime(f, dur, ratio=3.0, index=1.4, tau=0.5):
    return fm_bell(f, dur, ratio=ratio, index=index, amp_tau=tau, index_tau=0.08, attack=0.002) + \
        0.35 * fm_bell(2.0 * f, dur, ratio=2.0, index=0.6, amp_tau=tau * 0.6, index_tau=0.05, attack=0.002)


@sound("UI", 0.9, stereo=True, target=-19.0, use="Confirm / accept: bright magical sparkle")
def ui_confirm(rng, n):
    T = n / SR
    st = np.zeros((2, n))
    for i, (tt, f) in enumerate(((0.0, 1174.66), (0.05, 1479.98), (0.1, 1760.0))):
        b = np.zeros(n)
        place(b, _chime(f, T - tt - 0.004, tau=0.28), ns(tt + 0.004))
        st += pan(b, -0.35 + 0.35 * i)
    for tt in np.sort(0.05 + 0.4 * rng.random(14) ** 1.3):
        f = float(rng.uniform(3500, 8000))
        k = np.zeros(n)
        place(k, modal([f], [rng.uniform(0.02, 0.06)], [1.0], rng=rng), ns(tt))
        st += pan(k * rng.uniform(0.1, 0.35), float(rng.uniform(-0.9, 0.9)))
    return space(st, rng, rt60=0.8, wet_db=-11, stereo=True, hf_ratio=0.7)


@sound("UI", 1.8, stereo=True, target=-16.0, use="SPAWN button: magical rising swell into a bright bloom")
def ui_spawn(rng, n):
    T = n / SR
    tc = 1.05
    t = tvec(n)
    riser = moving_noise(rng, n, env([(0.0, 0.0), (tc, 1.0), (tc + 0.02, 0.0)], n, [3.0, 0.0]), "bp",
                         fpath([(0.0, 300), (tc, 6000)]), 1.3, "pink")
    gl = 2.0 ** ((-2.0 / 12.0) * np.clip(1.0 - t / 0.95, 0, 1))
    pad = np.zeros((2, n))
    for i, f in enumerate((293.66, 440.0, 587.33, 659.26, 739.99)):
        for d, side in ((-6.0, 0), (6.0, 1)):
            pad[side] += saw(f * gl * 2.0 ** (d / 1200.0), n, rng.random())
    pad = sweep(pad, "lp", fpath([(0.0, 400), (tc, 5000), (T, 1500)]), 0.9)
    pad = norm_peak(pad * env([(0.0, 0.0), (tc, 1.0), (tc + 0.25, 0.5), (T - 0.12, 0.0)], n, [2.0, -1.0, -2.0]))
    arp = np.zeros((2, n))
    pent = [587.33, 659.26, 739.99, 880.0, 987.77, 1174.66, 1318.51, 1479.98, 1760.0, 1975.53, 2349.32, 2637.02, 2959.96, 3520.0]
    times = 0.42 + (tc - 0.44) * (np.linspace(0, 1, len(pent)) ** 0.75)
    for i, (tt, f) in enumerate(zip(times, pent)):
        b = np.zeros(n)
        place(b, pluck(f, min(0.9, T - tt), rng, t60=0.6, t60_hf=0.25, bright=7000, pick=0.2), ns(tt))
        arp += pan(b, -0.6 + 1.2 * (i % 2)) * (0.5 + 0.5 * i / len(pent))
    bloom = np.zeros((2, n))
    for f, p in ((1174.66, -0.3), (1760.0, 0.3), (2349.32, 0.0)):
        b = np.zeros(n)
        place(b, _chime(f, T - tc, ratio=3.0, index=1.2, tau=0.3), ns(tc))
        bloom += pan(b, p)
    sp = np.zeros((2, n))
    for tt in np.sort(tc + 0.35 * rng.random(24) ** 1.5):
        f = float(rng.uniform(3000, 9000))
        k = np.zeros(n)
        place(k, modal([f], [rng.uniform(0.03, 0.09)], [1.0], rng=rng), ns(tt))
        sp += pan(k * rng.uniform(0.2, 1.0), float(rng.uniform(-0.9, 0.9)))
    wh = np.sin(TAU * np.cumsum(40.0 + 40.0 * np.exp(-np.maximum(t - tc, 0) / 0.05)) / SR) * perc(n, tc, 0.004, 0.18)
    y = (to_stereo(riser) * G(-6) + pad * G(-5) + norm_peak(arp) * G(-7) + norm_peak(bloom) * G(-3) +
         norm_peak(sp) * G(-13) + to_stereo(norm_peak(wh)) * G(-12))
    return space(y, rng, rt60=1.0, wet_db=-10, stereo=True, hf_ratio=0.6)


@sound("UI", 0.35, stereo=True, target=-21.0, use="Error / not allowed: soft muted two-note negative cue")
def ui_error(rng, n):
    t = tvec(n)
    y = np.zeros(n)
    for t0, f, d in ((0.004, 293.66, 0.07), (0.1, 207.65, 0.14)):
        m = ns(d)
        tt = tvec(m)
        v = tri(f, m) + 0.3 * np.sin(TAU * 2.0 * f * tt)
        v *= env([(0.0, 0.0), (0.004, 1.0), (d * 0.6, 0.8), (d, 0.0)], m, [0.0, 0.0, -1.5])
        place(y, v, ns(t0))
    y = eq(y, ("lp", 1500), ("hp", 120))
    thunk = burst(rng, n, 0.004, 0.5, 20, "pink", (("lp", 500), ("hp", 80)), amp=1.0)
    y = norm_peak(y) + thunk * G(-8)
    return room(y, rng, wet_db=-20)


@sound("UI", 1.4, stereo=True, target=-19.0, use="World map location select: bell-like chime")
def map_select(rng, n):
    T = n / SR
    b = fm_bell(1174.66, T - 0.004, ratio=1.4, index=2.5, amp_tau=0.35, index_tau=0.2, attack=0.002)
    t = tvec(b.shape[0])
    b = b + 0.3 * np.sin(TAU * 2349.32 * t) * np.exp(-t / 0.22) + 0.35 * np.sin(TAU * 587.33 * t) * np.exp(-t / 0.4)
    y = np.zeros(n)
    place(y, norm_peak(b), ns(0.004))
    st = stereoize(y, rng, 0.3)
    return space(st, rng, rt60=1.1, wet_db=-10, stereo=True, hf_ratio=0.6)
