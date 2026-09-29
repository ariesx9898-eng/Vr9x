"""Wind magic. Palette: resonant band-passed noise 'howls' (parallel formant-style resonances with
moving centres), broadband whooshes with Doppler-shaped sweeps, blade whistles (narrow resonances),
low air-pressure whoomps, debris flutter, and swirl motion from accelerating LFOs."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, early_reflections, env, eq, formant_filter, modal, noise, norm_peak, norm_rms,
                 ns, periodic_lfo, perc, place, saturate, smooth_random, sweep, transient_shape, tvec)
from layers import (burst, click, crackle, fpath, gravel, mix, moving_noise, place_reversed, space, thump, turbulence,
                    whoosh)
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


# ============================================================================================ ability overhaul
# Docs/Ability_Overhaul.md section 6 (Wind).
@sound("Wind", 0.6, target=MP, use="Wind_WindBlade release (WindBlade.Release) / Barrage_WindBlade: a giant crescent of compressed air cuts out and tears away")
def wind_slash(rng, n):
    t0 = 0.004
    snap = burst(rng, n, t0, 0.4, 6.0, "white", (("hp", 1800.0), ("lp", 12000.0)), amp=1.0)
    pres = burst(rng, n, t0, 3.0, 70.0, "brown", (("lp", 220.0), ("hp", 35.0)), amp=1.0)
    th = thump(n, t0, 95, 45, 25, 70, amp=1.0, drive_db=6, attack_ms=2.0)
    # the blade: a heavy whistle (narrow resonances well below wind_blade_swish's), Doppler-falling as it leaves
    fb = fpath([(0.0, 1500.0), (0.05, 1650.0), (0.5, 820.0)])
    e_b = env([(0.0, 0.0), (0.035, 1.0), (0.2, 0.45), (0.52, 0.0)], n, [-1.0, -1.0, -2.0])
    blade = formant_filter(noise(n, rng, "white"), [(fb, 24.0, 0.0), (lambda x: fb(x) * 1.52, 26.0, -5.0), (lambda x: fb(x) * 2.3, 20.0, -10.0)])
    blade = norm_peak(blade * e_b)
    # the air it cuts: a broadband tear falling in pitch, and the turbulent wake swirling behind it
    tear = whoosh(rng, n, 0.0, 0.55, [(0.0, 4200.0), (0.06, 3000.0), (0.5, 500.0)], q=1.0, peak_at=0.07, rise_curve=-1.0, color="white",
                  flutter=0.3, flutter_rate=25.0)
    wake = whoosh(rng, n, 0.08, 0.5, [(0.0, 1200.0), (0.45, 300.0)], q=1.4, peak_at=0.25, flutter=0.5, flutter_rate=11.0)
    y = mix(snap * G(-6), pres * G(-8), th * G(-13), blade * G(-1), tear, wake * G(-8))
    y = transient_shape(y, 2.0)
    return early_reflections(y, rng, level_db=-12)


@sound("Wind", 0.3, target=S, use="Wind_WindBurst formation (WindBurst.Formation, cast 0.12 s; wind_burst follows): air is sucked inward just before the blast")
def wind_compress(rng, n):
    T = n / SR
    tc = 0.115  # fully compressed at the Wind Burst cast time; the burst takes over here
    e_in = env([(0.0, 0.0), (tc, 1.0), (tc + 0.012, 0.25), (T - 0.02, 0.0)], n, [3.5, -1.0, -2.5])
    inhale = moving_noise(rng, n, e_in, "bp", fpath([(0.0, 450.0), (tc, 2600.0), (T, 1800.0)]), 1.6, "pink")
    # a hollow resonance pulled up in pitch, like air drawn through a narrowing gap
    hollow = formant_filter(noise(n, rng, "white"), [(fpath([(0.0, 320.0), (tc, 950.0), (T, 880.0)]), 9.0, 0.0)])
    hollow = norm_peak(hollow * env([(0.0, 0.0), (tc, 1.0), (T - 0.02, 0.0)], n, [3.0, -3.0]))
    low = np.zeros(n)
    m = ns(0.12)
    place_reversed(low, norm_peak(eq(noise(m, rng, "brown") * np.exp(-tvec(m) / 0.035), ("lp", 260.0), ("hp", 40.0))), tc)
    # the air locks at full compression: a small pressure tick
    tk = click(rng, n, tc, amp=1.0, hp=600.0, lp=4000.0, decay_ms=1.5)
    th = thump(n, tc, 140, 70, 10, 30, amp=1.0, drive_db=3)
    y = mix(inhale, hollow * G(-3), low * G(-7), tk * G(-12), th * G(-14))
    return early_reflections(y, rng, level_db=-13)


@sound("Wind", 1.2, target=MP, use="Wind_Tornado forming (Tornado.Formation, 0.4 s form time; tornado_loop carries on): a vortex spins up, touches down and tears upward")
def tornado_form(rng, n):
    t = tvec(n)
    tf = 0.42  # formed
    ph = swirl_phase(n, 1.5, 9.0, T=tf + 0.1, power=1.3)  # the swirl accelerates while it forms, then holds
    grow = env([(0.0, 0.0), (tf, 1.0), (0.9, 0.85), (1.12, 0.0)], n, [1.6, 0.0, -2.0])
    low = moving_noise(rng, n, grow * (1.0 + 0.45 * np.sin(ph)) * turbulence(rng, n, ((3.0, 0.25),)), "lp",
                       fpath([(0.0, 160.0), (tf, 650.0)]), 0.8, "brown", order=4, pre=(("hp", 35.0),))
    rise = lambda a, b: a * (b / a) ** (np.clip(t / (tf + 0.2), 0, 1) ** 1.1)  # noqa: E731
    hw = howl(rng, n, [rise(230, 700), rise(420, 1150), rise(680, 1800)], 10.0, 0.12, ph * 1.3, [0, -3, -6]) * grow
    # touchdown: the funnel meets the ground with a low suction whump
    whump = burst(rng, n, tf - 0.05, 12.0, 120.0, "brown", (("lp", 180.0), ("hp", 30.0)), amp=1.0)
    th = thump(n, tf - 0.04, 75, 36, 40, 120, amp=1.0, drive_db=4, attack_ms=8.0)
    # tearing upward: a rising rush, a ripping granular band climbing the column, a whistle gliding up
    e_up = env([(0.25, 0.0), (0.5, 1.0), (0.95, 0.5), (1.12, 0.0)], n, [1.0, -0.5, -2.0])
    up = whoosh(rng, n, 0.25, 0.85, [(0.0, 300.0), (0.7, 3500.0)], q=1.2, peak_at=0.45, rise_curve=1.0)
    rip = crackle(rng, n, 2600.0, 0.25, 1.12, band=(500.0, 8000.0), tick_ms=(0.08, 0.6), pop_prob=0.0)
    rip = sweep(rip, "bp", fpath([(0.25, 700.0), (1.0, 5000.0)]), 1.1) * e_up
    whis = formant_filter(noise(n, rng, "white"), [(fpath([(0.35, 480.0), (0.95, 1900.0)]), 18.0, 0.0)])
    whis = norm_peak(whis * env([(0.35, 0.0), (0.7, 1.0), (1.05, 0.0)], n, [1.5, -1.5]))
    # debris and dust torn off the ground and flung up the funnel
    deb = gravel(rng, n, lambda x: 20.0 + 380.0 * np.exp(-((x - 0.55) / 0.2) ** 2), 0.3, 1.05, size=(0.0, 0.3), dark=0.3)
    grit = crackle(rng, n, lambda x: 10.0 + 180.0 * np.clip((x - 0.3) / 0.4, 0, 1), 0.3, 1.05, band=(1500.0, 7000.0),
                   tick_ms=(0.2, 1.5), pop_prob=0.1)
    y = mix(low * G(-2), hw, whump * G(-6), th * G(-12), up * G(-3), norm_peak(rip) * G(-8), whis * G(-13),
            norm_peak(deb) * G(-13), norm_peak(grit) * G(-15))
    return saturate(norm_peak(y), 4.0)
