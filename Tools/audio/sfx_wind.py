"""Wind magic. Palette: resonant band-passed noise 'howls' (parallel formant-style resonances with
moving centres), broadband whooshes with Doppler-shaped sweeps, blade whistles (narrow resonances),
low air-pressure whoomps, debris flutter, and swirl motion from accelerating LFOs."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, early_reflections, env, eq, formant_filter, modal, noise, norm_peak, norm_rms,
                 ns, periodic_lfo, perc, place, saturate, smooth_random, transient_shape, tvec)
from layers import (burst, click, crackle, fpath, mix, moving_noise, space, thump, turbulence, whoosh)
from reg import LOOP_BIG, M, MP, S, sound

G = lambda d: float(db2lin(d))  # noqa: E731


def swirl_phase(n, r0, r1, T=None, power=1.5):
    t = tvec(n)
    T = T or n / SR
    return TAU * np.cumsum(r0 + (r1 - r0) * np.clip(t / T, 0, 1) ** power) / SR


def howl(rng, n, centres, q, wobble, phase, gains_db=None, color="white", circular=False):
    """Several resonant bands of noise; centres are arrays (per-sample Hz), each wobbling by
    +-wobble around its centre with the given phase curve (offset per band)."""
    bands = []
    for i, c in enumerate(centres):
        fc = c * (1.0 + wobble * np.sin(phase + i * 2.1))
        g = 0.0 if gains_db is None else gains_db[i]
        bands.append((fc, q, g))
    y = formant_filter(noise(n, rng, color), bands, frame=2048, circular=circular)
    return norm_peak(y)


@sound("Wind", 0.6, target=S, use="Wind spells wind-up (Wind_WindBlade, Wind_Tornado, Wind_WindBurst): air spirals around the hand")
def wind_gather(rng, n):
    T = n / SR
    t = tvec(n)
    rise = lambda a, b: a * (b / a) ** (np.clip(t / 0.52, 0, 1) ** 1.2)  # noqa: E731
    ph = swirl_phase(n, 6.0, 16.0)
    e = env([(0.0, 0.0), (0.44, 1.0), (0.5, 0.55), (T - 0.03, 0.0)], n, [2.5, -1.5, -2.0])
    hw = howl(rng, n, [rise(480, 1100), rise(780, 1750), rise(1300, 2700)], 12.0, 0.12, ph, [0, -3, -6]) * e
    air = moving_noise(rng, n, e * turbulence(rng, n, ((12, 0.3),)), "bp", rise(700, 2600), 0.8, "pink")
    sh = np.zeros(n)
    for f in (3150.0, 4200.0, 5000.0):
        sh += np.sin(TAU * f * t + rng.random() * TAU) * (0.6 + 0.4 * np.sin(ph * 1.3 + f))
    sh = norm_peak(sh * e)
    whuff = burst(rng, n, 0.46, 8, 40, "pink", (("lp", 700), ("hp", 60)), amp=G(-6))
    y = mix(hw, air * G(-4), sh * G(-20), whuff)
    return early_reflections(y, rng, level_db=-12)


@sound("Wind", 0.5, target=S, use="Wind_WindBlade / Barrage_WindBlade / Race_Elf_SpiritArrow cast: a crescent of air flies out with a whistle")
def wind_blade_swish(rng, n):
    t0 = 0.0
    sw = whoosh(rng, n, t0, 0.48, [(0.0, 900), (0.12, 3500), (0.45, 700)], q=3.0, peak_at=0.25, amp=1.0,
                rise_curve=2.0, fall_curve=-2.5, color="white")
    e = env([(0.0, 0.0), (0.12, 1.0), (0.48, 0.0)], n, [2.0, -2.5])
    fwh = fpath([(0.0, 2400), (0.12, 2650), (0.45, 1850)])
    wh = formant_filter(noise(n, rng, "white"), [(fwh, 28.0, 0.0), (lambda tt: fwh(tt) * 1.5, 30.0, -6.0)])
    wh = norm_peak(wh * e)
    body = burst(rng, n, 0.06, 40, 80, "brown", (("lp", 320), ("hp", 50)), amp=1.0)
    y = mix(sw, wh * G(-9), body * G(-11))
    return early_reflections(y, rng, level_db=-13)


@sound("Wind", 0.6, target=M, use="Wind_WindBlade impact: the blade slices a target then disperses as a gust")
def wind_blade_impact(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=G(-3), hp=3000)
    shing = np.zeros(n)
    place(shing, modal([3100, 4450, 5900, 7700], [0.12, 0.08, 0.06, 0.04], [1.0, 0.7, 0.5, 0.3], rng=rng), ns(t0))
    slash = burst(rng, n, t0, 0.5, 25, "white", (("hp", 2000),), amp=1.0)
    th = thump(n, t0, 150, 70, 20, 50, amp=G(-4), drive_db=6)
    gust = whoosh(rng, n, 0.02, 0.56, [(0.0, 2500), (0.5, 400)], q=1.2, peak_at=0.08, amp=1.0, rise_curve=-1.0,
                  flutter=0.35, flutter_rate=16.0)
    y = mix(ck, norm_peak(shing) * G(-4), slash * G(-2), th, gust * G(-1))
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, level_db=-10)


@sound("Wind", 1.6, target=MP, use="Wind_Tornado cast: winds spin up into a vortex; start tornado_loop about 0.3 s before this ends")
def tornado_start(rng, n):
    T = n / SR
    t = tvec(n)
    grow = env([(0.0, 0.0), (1.3, 1.0), (T, 0.3)], n, [2.0, -1.0])
    ph = swirl_phase(n, 1.5, 6.0)
    swirl = 1.0 + 0.4 * np.sin(ph)
    low = moving_noise(rng, n, grow * swirl * turbulence(rng, n, ((3.0, 0.25),)), "lp", fpath([(0.0, 200), (1.5, 650)]), 0.8,
                       "brown", order=4, pre=(("hp", 35),))
    rise = lambda a, b: a * (b / a) ** (np.clip(t / 1.45, 0, 1) ** 1.3)  # noqa: E731
    hw = howl(rng, n, [rise(250, 600), rise(450, 1000), rise(700, 1600), rise(1100, 2300)], 9.0, 0.1, ph * 1.3,
              [0, -2, -5, -8]) * grow * (1.0 + 0.3 * np.sin(ph + 1.0))
    wh = np.zeros(n)
    for tw in (0.45, 0.85, 1.15, 1.38):
        wh += whoosh(rng, n, tw - 0.12, 0.3, [(0.0, 900), (0.12, 2200), (0.3, 800)], q=1.5, peak_at=0.4, amp=1.0)
    deb = crackle(rng, n, lambda tt: 10 + 150 * (tt / T) ** 2, 0.0, T, band=(1500, 7000), tick_ms=(0.2, 1.5), pop_prob=0.1)
    y = mix(low * G(-3), hw, norm_peak(wh * grow) * G(-5), norm_peak(deb) * G(-14))
    return saturate(norm_peak(y), 4.0)


@sound("Wind", 3.0, loop=True, target=LOOP_BIG, use="Wind_Tornado sustained: looping swirling vortex attached to the tornado")
def tornado_loop(rng, n):
    T = n / SR
    t = tvec(n)
    spin = periodic_lfo(n, 12, "pulse", sharp=2.0)  # 4 Hz swirl, whole cycles per loop
    wob = TAU * 12 * t / T
    low = eq(noise(n, rng, "brown"), ("lp4", 520), ("hp", 35), circular=True)
    low = norm_peak(norm_rms(low) * (1.0 + 0.35 * spin) * turbulence(rng, n, ((2.0, 0.2), (6.0, 0.15))))
    cen = [np.full(n, f) * (1.0 + 0.05 * smooth_random(n, rng, 0.8)) for f in (600.0, 1000.0, 1600.0, 2300.0)]
    hw = howl(rng, n, cen, 9.0, 0.1, wob, [0, -2, -5, -8], circular=True) * (1.0 + 0.3 * periodic_lfo(n, 12, phase=0.25))
    mid = eq(noise(n, rng, "pink"), ("hp", 800), ("lp", 4000), circular=True)
    mid = norm_peak(norm_rms(mid) * (1.0 + 0.5 * periodic_lfo(n, 12, "pulse", phase=0.5, sharp=3.0)) * turbulence(rng, n, ((9.0, 0.3),)))
    deb = crackle(rng, n, 60.0, 0.0, T, band=(1500, 7000), tick_ms=(0.2, 1.5), pop_prob=0.1, wrap=True)
    y = mix(low, hw * G(-3), mid * G(-7), norm_peak(deb) * G(-17))
    return saturate(norm_peak(y), 3.0)


@sound("Wind", 1.3, target=MP, use="Wind_WindBurst (also Wind_GaleStep): a heavy whoomp of air pressure then a spreading gust")
def wind_burst(rng, n):
    t0 = 0.004
    th = thump(n, t0, 90, 40, 40, 120, amp=1.0, drive_db=6, attack_ms=3.0)
    whp = burst(rng, n, t0, 8, 90, "brown", (("lp", 250), ("hp", 30)), amp=G(-1))
    pop = click(rng, n, t0, amp=G(-8), hp=300, lp=3000, decay_ms=2.0)
    gust = whoosh(rng, n, 0.02, 1.25, [(0.0, 1800), (1.1, 350)], q=0.9, peak_at=0.06, amp=1.0, rise_curve=-1.0,
                  fall_curve=-2.0, flutter=0.4, flutter_rate=10.0)
    hs = burst(rng, n, 0.01, 10, 200, "white", (("hp", 3000),), amp=G(-14))
    y = mix(th, whp, pop, gust * G(-1), hs)
    y = transient_shape(y, 2.0)
    y = early_reflections(y, rng, level_db=-10)
    return space(y, rng, rt60=0.9, wet_db=-18)
