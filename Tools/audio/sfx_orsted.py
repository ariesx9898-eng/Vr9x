"""Orsted (the Dragon God) and Rudeus.

Orsted palette: very dry heavy martial hits (kick-like thud + air snap, almost no room), deep sub
booms with distant terrain slaps, air tearing (dense granular rip under a fast sweep), dark drones
built on A with a minor-second / tritone rub (A-Bb-E-Eb), and glassy dissonant shatters for
Disturb Magic. Rudeus palette: bright D-major / quartal FM chimes, airy swirls, rising surges."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, early_reflections, env, eq, fm, formant_filter, modal, noise, norm_peak, norm_rms,
                 ns, periodic_lfo, perc, place, poisson_times, saturate, saw, sine, slap_echoes, smooth_random,
                 transient_shape, tvec)
from layers import (bell_hits, boulder, bubbles, burst, click, comb_warp, crack, crackle, debris, drone, droplets, fpath, glug,
                    gravel, grind, groan, mix, moving_noise, place_reversed, roar, rumble, shimmer, space, sub_boom, thump,
                    turbulence, vpath, whoosh)
from reg import L, LOOP_SMALL, M, MP, S, XL, XS, sound
from sfx_earth import crack_run, crunch

G = lambda d: float(db2lin(d))  # noqa: E731


# ============================================================================================ Orsted
@sound("Orsted", 0.5, target=MP, use="Orsted_Basic (Dragon God Style: Palm Strike): dry, heavy martial-arts hit with an air snap")
def palm_strike(rng, n):
    t0 = 0.012
    push = whoosh(rng, n, 0.0, 0.03, [(0.0, 600), (0.03, 1500)], q=1.0, peak_at=0.9, amp=1.0, rise_curve=3.0)
    snap = burst(rng, n, t0, 0.3, 8, "white", (("hp", 2500), ("lp", 12000)), amp=1.0)
    snap2 = crack(rng, n, t0, fc=3500, q=0.8, tau_ms=5, drive_db=8)
    slap = burst(rng, n, t0, 0.5, 25, "pink", (("bp", 1200, 0.8),), amp=1.0, drive_db=4)
    thud = thump(n, t0, 180, 60, 20, 70, amp=1.0, drive_db=10)
    body = burst(rng, n, t0, 1.0, 75, "pink", (("lp", 1500), ("hp", 120)), amp=1.0, drive_db=6)
    low = burst(rng, n, t0, 2.0, 60, "brown", (("bp", 220, 1.0),), amp=1.0)
    air = whoosh(rng, n, t0, 0.3, [(0.0, 1800), (0.3, 450)], q=1.0, peak_at=0.07, amp=1.0, rise_curve=-1.0)
    y = mix(push * G(-16), snap * G(-3), snap2 * G(-5), slap * G(-3), thud * G(-7), body, low * G(-9), air * G(-5))
    y = transient_shape(y, 2.0)
    return early_reflections(y, rng, n_taps=3, t_range=(0.004, 0.012), level_db=-15)


@sound("Orsted", 1.6, target=XL, use="Orsted shockwave (Orsted_DragonCrush impact, awakened strikes): deep pressure boom radiating outward")
def shockwave_boom(rng, n):
    t0 = 0.004
    snap = click(rng, n, t0, amp=1.0, hp=400, lp=5000, decay_ms=2.0)
    sb = sub_boom(rng, n, t0, f0=60, f1=27, sweep_s=0.2, tau_s=0.6, amp=1.0, drive_db=8)
    whoomp = burst(rng, n, t0, 12, 180, "brown", (("lp", 180), ("hp", 30)), amp=1.0)
    body = burst(rng, n, t0, 6, 200, "pink", (("lp", 700), ("hp", 70)), amp=1.0, drive_db=8)
    ripple = whoosh(rng, n, 0.01, 0.9, [(0.0, 1500), (0.8, 200)], q=0.8, peak_at=0.1, amp=1.0, rise_curve=-1.0)
    rum = rumble(rng, n, env([(0.05, 0.0), (0.15, 1.0), (1.55, 0.0)], n, [-1.0, -2.0]), lp=100, rate=4.0, amp=1.0)
    y = mix(snap * G(-8), sb * G(-10), whoomp * G(-6), body, ripple * G(-9), rum * G(-10))
    y = transient_shape(y, 2.0)
    y = slap_echoes(y, (0.15, 0.31, 0.5), (-12, -16, -20), lp=800)
    return space(y, rng, rt60=2.0, wet_db=-14, hf_ratio=0.3)


@sound("Orsted", 0.7, target=M, use="Orsted_DragonStep / Orsted_DragonStep_Awakened: instantaneous dash that tears the air, arrival stomp")
def dragon_step(rng, n):
    T = n / SR
    e = env([(0.0, 0.0), (0.05, 1.0), (0.12, 0.6), (0.45, 0.0)], n, [2.0, -1.0, -2.5])
    src = crackle(rng, n, 2500.0, 0.0, 0.5, band=(500, 8000), tick_ms=(0.1, 0.6), pop_prob=0.0)
    from dsp import sweep
    tear = sweep(src, "bp", fpath([(0.0, 600), (0.06, 4000), (0.4, 1200)]), 1.2) * e
    disp = whoosh(rng, n, 0.0, 0.4, [(0.0, 3000), (0.35, 500)], q=2.0, peak_at=0.12, amp=1.0)
    cr = crack(rng, n, 0.03, fc=2200, tau_ms=6, drive_db=10)
    th = thump(n, 0.03, 120, 60, 20, 60, amp=1.0, drive_db=8)
    whump = burst(rng, n, 0.03, 4, 80, "brown", (("lp", 200), ("hp", 40)), amp=1.0)
    aura = drone(rng, n, [55.0, 58.27], env([(0.0, 0.0), (0.1, 1.0), (0.35, 0.0)], n, [1.0, -2.0]), lp=500)
    y = mix(norm_peak(tear), disp * G(-4), cr * G(-4), th * G(-9), whump * G(-7), aura * G(-16))
    y = transient_shape(y, 2.0)
    return early_reflections(y, rng, level_db=-12)


@sound("Orsted", 1.3, target=L, use="Orsted_DragonCrush and heavy downward blows: the ground splits, rubble falls")
def ground_crack(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=1.0, hp=1500)
    cr = crack(rng, n, t0, fc=1800, q=0.7, tau_ms=15, drive_db=12)
    casc = np.zeros(n)
    tt = t0
    a = 1.0
    for _ in range(16):
        tt += rng.uniform(0.005, 0.03)
        a *= rng.uniform(0.8, 0.97)
        casc += crack(rng, n, tt, amp=a, fc=float(rng.uniform(900, 3000)), tau_ms=float(rng.uniform(4, 10)), drive_db=8)
    th = thump(n, t0, 120, 50, 25, 100, amp=1.0, drive_db=8)
    body = burst(rng, n, t0, 2, 120, "pink", (("hp", 120), ("lp", 1500)), amp=1.0, drive_db=8)
    cru = crunch(rng, n, t0 + 0.002, tau_fast=0.08, tau_slow=0.3)
    rub = debris(rng, n, 30, 0.1, 1.2, size=(0.0, 0.7), dist=lambda u: u ** 1.3)
    dust = burst(rng, n, 0.05, 40, 450, "pink", (("hp", 2000),), amp=1.0)
    rum = rumble(rng, n, env([(t0, 0.0), (0.05, 1.0), (1.2, 0.0)], n, [-1.0, -2.0]), lp=100, amp=1.0)
    y = mix(ck * G(-5), cr * G(-2), norm_peak(casc) * G(-4), th * G(-11), body, cru * G(-2), norm_peak(rub) * G(-8),
            dust * G(-17), rum * G(-13))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-9)
    return slap_echoes(y, (0.12, 0.26), (-15, -20), lp=1800)


@sound("Orsted", 0.9, target=M, use="Orsted_DisturbMagic: an enemy spell is broken apart, glassy dissonant shatter")
def disturb_magic(rng, n):
    t0 = 0.004
    t = tvec(n)
    ck = click(rng, n, t0, amp=1.0, hp=4000)
    shards = np.zeros(n)
    times = poisson_times(lambda x: 2500.0 * np.exp(-(x - t0) / 0.04) + 200.0 * np.exp(-(x - t0) / 0.25), t0, 0.7, rng)
    amps = np.minimum(rng.pareto(2.0, times.size) + 1.0, 10.0) / 10.0
    for tt, a in zip(times, amps):
        f = float(np.exp(rng.uniform(np.log(2500), np.log(11000))))
        d = float(rng.uniform(0.004, 0.03))
        k = modal([f, f * rng.uniform(1.3, 1.6)], [d, d * 0.6], [1.0, 0.4], rng=rng)
        place(shards, k * a, ns(tt))
    sag = 2.0 ** ((-1.0 / 12.0) * np.clip((t - t0) / 0.5, 0, 1))
    wob = 1.0 + 0.012 * smooth_random(n, rng, 14.0) * np.clip((t - t0) / 0.3, 0, 1)
    glass = np.zeros(n)
    for f, d, a in ((1480.0, 0.45, 1.0), (1568.0, 0.4, 0.8), (2093.0, 0.35, 0.7), (2960.0, 0.3, 0.5), (3322.0, 0.25, 0.4)):
        glass += a * np.sin(TAU * np.cumsum(f * sag * wob) / SR + rng.random() * TAU) * np.exp(-np.maximum(t - t0, 0) / d)
    glass *= (t >= t0) * (0.6 + 0.4 * np.sin(TAU * 37.0 * t))  # ring-mod rub
    car = 300.0 + 2700.0 * np.exp(-np.maximum(t - t0, 0) / 0.07)
    zap = fm(car, car * 1.5, 5.0 * np.exp(-np.maximum(t - t0, 0) / 0.1), n) * perc(n, t0, 0.001, 0.09)
    th = thump(n, t0, 100, 60, 15, 60, amp=1.0, drive_db=6)
    y = mix(ck * G(-6), norm_peak(shards), norm_peak(glass) * G(-6), norm_peak(zap) * G(-9), th * G(-12))
    y = transient_shape(y, 2.0)
    return space(y, rng, rt60=0.9, wet_db=-12, hf_ratio=0.6)


@sound("Orsted", 1.8, target=L, use="Orsted_SaintDragonAura activation (also Race_Dragon_Aura): dark swell into a heavy bloom")
def aura_activate(rng, n):
    T = n / SR
    tp = 1.0
    riser = moving_noise(rng, n, env([(0.0, 0.0), (tp, 1.0), (tp + 0.05, 0.2), (T, 0.0)], n, [3.5, -2.0, -2.0]), "bp",
                         fpath([(0.0, 200), (tp, 2500)]), 1.0, "pink")
    e_d = env([(0.0, 0.0), (tp, 1.0), (1.25, 0.7), (T - 0.08, 0.0)], n, [2.5, -1.0, -2.5])
    dr = drone(rng, n, [55.0, 82.41, 110.0, 116.54], e_d, detune_cents=10, lp=fpath([(0.0, 200), (tp, 1600), (T, 500)]))
    sb = sub_boom(rng, n, tp, f0=55, f1=30, sweep_s=0.15, tau_s=0.4, amp=1.0, drive_db=6)
    whp = burst(rng, n, tp, 5, 120, "brown", (("lp", 200), ("hp", 35)), amp=1.0)
    bloom = burst(rng, n, tp, 3, 150, "pink", (("lp", 900), ("hp", 90)), amp=1.0, drive_db=6)
    e_s = env([(0.0, 0.0), (tp, 1.0), (T, 0.0)], n, [3.0, -2.5])
    sh = shimmer(rng, n, e_s, [880.0, 1244.5, 1874.4, 2428.8], trem_rate=(5, 9), amp=1.0)
    crk = crackle(rng, n, lambda x: 20 + 130 * np.exp(-((x - tp) / 0.3) ** 2), 0.2, T - 0.1, band=(3000, 8000), tick_ms=(0.1, 0.5),
                  pop_prob=0.0)
    y = mix(riser * G(-3), dr * G(-4), sb * G(-12), whp * G(-7), bloom, sh * G(-12), norm_peak(crk) * G(-15))
    return space(y, rng, rt60=1.2, wet_db=-12, hf_ratio=0.35)


@sound("Orsted", 3.0, loop=True, target=LOOP_SMALL, use="Orsted_SaintDragonAura sustained: low menacing hum loop while the aura is active")
def aura_hum_loop(rng, n):
    t = tvec(n)
    pulse = 1.0 + 0.2 * periodic_lfo(n, 2)  # 2 breaths per 3 s loop
    y = np.zeros(n)
    # every frequency is a multiple of 1/3 Hz -> whole cycles in the 3 s loop (seamless)
    for f0, gain in ((165 / 3.0, 1.0), (247 / 3.0, 0.45), (331 / 3.0, 0.35)):
        for h in range(1, 14):
            f = f0 * h
            a = gain / h / (1.0 + (f / 700.0) ** 2)
            y += a * np.sin(TAU * f * t + rng.random() * TAU)
    y = norm_peak(y * pulse)
    bed = eq(noise(n, rng, "brown"), ("lp", 300), ("hp", 35), circular=True)
    bed = norm_peak(norm_rms(bed) * pulse)
    air = eq(noise(n, rng, "pink"), ("hp", 600), ("lp", 2000), circular=True)
    air = norm_peak(norm_rms(air) * np.maximum(1.0 + 0.4 * smooth_random(n, rng, 1.0), 0.0))
    sh = np.zeros(n)
    for k in (2640, 2641, 3733, 3735):  # 880.0 / 880.33 / 1244.3 / 1245 Hz
        sh += np.sin(TAU * (k / 3.0) * t + rng.random() * TAU) * np.maximum(1.0 + 0.6 * smooth_random(n, rng, 0.7), 0.0)
    out = mix(y, bed * G(-10), air * G(-18), norm_peak(sh) * G(-20))
    return saturate(norm_peak(out), 3.0)


@sound("Orsted", 3.0, target=XL, use="Orsted_Awakening_DragonGod: deep drone and dissonant choir swell to a massive bloom")
def dragon_god_awaken(rng, n):
    T = n / SR
    tc = 2.25
    t = tvec(n)
    e_up = env([(0.0, 0.0), (tc, 1.0), (tc + 0.2, 0.6), (T - 0.1, 0.0)], n, [2.5, -1.0, -3.0])
    sub = saturate(norm_peak(np.sin(TAU * 27.5 * t) + 0.8 * np.sin(TAU * 55.0 * t + 0.3)), 8.0) * e_up
    clus = drone(rng, n, [55.0, 58.27, 82.41, 155.56], e_up, detune_cents=12, lp=fpath([(0.0, 150), (tc, 2500), (T, 700)]))
    src = saw(110.0, n, 0.1) + saw(164.81, n, 0.5) + saw(220.0, n, 0.8) + 0.2 * noise(n, rng, "pink")
    choir = formant_filter(src, [(700.0, 6.0, 0.0), (1150.0, 7.0, -4.0), (2600.0, 8.0, -9.0)], floor=0.02)
    choir = norm_peak(choir * env([(0.3, 0.0), (tc, 1.0), (T, 0.0)], n, [2.0, -2.0]) *
                      (1.0 + 0.1 * np.sin(TAU * 5.0 * t)))
    wall = moving_noise(rng, n, env([(0.0, 0.0), (tc, 1.0), (tc + 0.05, 0.3), (T, 0.0)], n, [3.0, -2.0, -2.0]), "bp",
                        fpath([(0.0, 150), (tc, 3000)]), 0.8, "pink")
    cym = moving_noise(rng, n, env([(0.5, 0.0), (tc, 1.0), (tc + 0.02, 0.0)], n, [4.0, 0.0]), "hp", 5000, 0.7, "white")
    ck = click(rng, n, tc, amp=1.0, hp=1200)
    sb = sub_boom(rng, n, tc, f0=50, f1=24, sweep_s=0.2, tau_s=0.5, amp=1.0, drive_db=8)
    blast = burst(rng, n, tc, 2, 110, "pink", (("lp", 3000), ("hp", 80)), amp=1.0, drive_db=8)
    th = thump(n, tc, 120, 45, 25, 120, amp=1.0, drive_db=8)
    rum = rumble(rng, n, env([(tc, 0.0), (tc + 0.1, 1.0), (T - 0.12, 0.0)], n, [-1.0, -2.5]), lp=100, amp=1.0)
    y = mix(sub * G(-12), clus * G(-4), choir * G(-8), wall * G(-3), cym * G(-14), ck * G(-6), sb * G(-12), blast,
            th * G(-9), rum * G(-10))
    y = slap_echoes(y, (0.16, 0.34), (-15, -20), lp=900)
    return space(y, rng, rt60=1.4, wet_db=-12, hf_ratio=0.3)


# ============================================================================================ Orsted: ability overhaul
# Docs/Ability_Overhaul.md section 6. Disturb Magic (pulse -> collapse -> seal) and Dragon Step -> Dragon Crush.
@sound("Orsted", 0.5, target=XS, use="Orsted_DisturbMagic cast and pulse (DisturbMagic.Cast / DisturbMagic.Pulse): a thin, fast, almost silent warped pulse")
def disturb_pulse(rng, n):
    t = tvec(n)
    t0 = 0.004
    u = np.maximum(t - t0, 0.0)
    # the flick: a thin chirp whose phase is bent as it drops (warped rather than zapped)
    f = 900.0 + 2300.0 * np.exp(-u / 0.022)
    ph = TAU * np.cumsum(f) / SR
    chirp = np.sin(ph + 1.4 * np.sin(0.5 * ph + 0.3) * np.exp(-u / 0.05)) * perc(n, t0, 0.0015, 0.045)
    # its faint reflection, lower and later (the air it passed through snapping back)
    f2 = 700.0 + 1500.0 * np.exp(-np.maximum(t - 0.07, 0.0) / 0.02)
    echo = np.sin(TAU * np.cumsum(f2) / SR) * perc(n, 0.07, 0.002, 0.035)
    # the pulse leaves: a faint ripple of warped air (flanged noise, the comb delay sweeping) receding
    e_r = env([(t0, 0.0), (0.02, 1.0), (0.4, 0.0)], n, [-1.0, -2.5])
    air = moving_noise(rng, n, e_r, "bp", fpath([(0.0, 4200.0), (0.35, 1300.0)]), 1.1, "pink")
    air = comb_warp(air, fpath([(0.0, 0.3), (0.2, 2.5), (0.4, 4.0)])(t), depth=0.9)
    # the glyph at its head: a pale tick; and the barest pressure dip
    glyph = np.zeros(n)
    place(glyph, modal([7800.0, 11700.0], [0.006, 0.004], [1.0, 0.4], rng=rng), ns(t0))
    dip = thump(n, t0, 90, 60, 10, 25, amp=1.0, drive_db=0)
    y = mix(norm_peak(chirp), norm_peak(echo) * G(-12), norm_peak(air) * G(-6), norm_peak(glyph) * G(-10), dip * G(-16))
    y = eq(y, ("hp", 250.0))
    return space(y, rng, rt60=0.6, wet_db=-16, hf_ratio=0.7)


@sound("Orsted", 0.9, target=M, use="DisturbMagic.Collapse<Element> (a forming spell or flying projectile is broken): the construct fails with a glassy crumble and its energy deflates")
def disturb_collapse(rng, n):
    t = tvec(n)
    t0 = 0.004
    u = np.clip((t - t0) / 0.7, 0, 1)
    tk = click(rng, n, t0, amp=1.0, hp=3500.0, decay_ms=0.5)
    cr = crack(rng, n, t0, fc=5200.0, q=1.0, tau_ms=2.5, drive_db=8, hp=2500.0)
    # a glassy crumble: a dense cascade of small soft shards tumbling down in pitch (sugar glass, not a shatter)
    crum = np.zeros(n)
    for tt in poisson_times(lambda x: 900.0 * np.exp(-(x - t0) / 0.12) + 120.0 * np.exp(-(x - t0) / 0.35), t0, 0.75, rng):
        v = (tt - t0) / 0.75
        f = float(np.exp(rng.uniform(np.log(1500.0), np.log(7000.0)))) * (1.0 - 0.35 * v)
        d = float(rng.uniform(0.003, 0.018))
        k = modal([f, f * rng.uniform(1.35, 1.7), f * rng.uniform(2.2, 2.8)], [d, d * 0.6, d * 0.4], [1.0, 0.5, 0.25], rng=rng)
        place(crum, k * rng.uniform(0.2, 1.0) * np.exp(-v * 1.5), ns(tt))
    grit = crackle(rng, n, lambda x: 1500.0 * np.exp(-(x - t0) / 0.15), t0, 0.6, band=(2500.0, 10000.0), tick_ms=(0.05, 0.3),
                   pop_prob=0.0)
    # deflating energy: the spell's chord sags more than an octave while its tremolo slows, over a falling 'pfff'
    sag = 2.0 ** (-1.25 * u ** 0.8)
    trem = 0.55 + 0.45 * np.sin(TAU * np.cumsum(18.0 - 14.0 * u) / SR)
    chord = np.zeros(n)
    for f, a in ((659.26, 1.0), (830.61, 0.8), (987.77, 0.7), (1318.51, 0.45)):
        chord += a * np.sin(TAU * np.cumsum(f * sag) / SR + rng.random() * TAU)
    chord *= trem * env([(t0, 0.0), (0.02, 1.0), (0.75, 0.0)], n, [0.0, -1.5])
    pff = moving_noise(rng, n, env([(t0, 0.0), (0.03, 1.0), (0.75, 0.0)], n, [-1.0, -1.5]), "bp", fpath([(0.0, 3200.0), (0.7, 280.0)]),
                       1.5, "pink")
    imp = thump(n, t0 + 0.01, 85, 40, 20, 70, amp=1.0, drive_db=3)  # the construct implodes
    y = mix(tk * G(-8), cr * G(-6), norm_peak(crum) * G(-1), norm_peak(grit) * G(-11), norm_peak(chord) * G(-7), pff * G(-6),
            imp * G(-12))
    return space(y, rng, rt60=0.8, wet_db=-14, hf_ratio=0.6)


@sound("Orsted", 0.8, target=S, use="DisturbMagic.Seal (the broken ability is sealed on that hand for 3 s): a muted magical lock clamping shut")
def seal(rng, n):
    t = tvec(n)
    tc = 0.13  # the clamp
    # the glyph closes: a short muted whirr and a ratchet of soft clicks accelerating into the clamp
    f = 260.0 * 3.0 ** np.clip(t / tc, 0, 1)
    whirr = np.sin(TAU * np.cumsum(f) / SR) * (0.6 + 0.4 * np.sin(TAU * np.cumsum(30.0 + 90.0 * np.clip(t / tc, 0, 1)) / SR))
    whirr *= env([(0.0, 0.0), (tc - 0.01, 1.0), (tc, 0.0)], n, [2.0, 0.0])
    ratchet = mix(*[click(rng, n, tc - d, amp=a, hp=600.0, lp=3000.0, decay_ms=1.2)
                    for d, a in ((0.1, 0.4), (0.062, 0.55), (0.036, 0.7), (0.018, 0.85))])
    # the clamp: a dense, damped double knock (jaws meeting, then seating) over a low thud
    knock = np.zeros(n)
    for dt, a, r in ((0.0, 1.0, 1.0), (0.017, 0.6, 1.06)):
        place(knock, a * modal(np.array([150.0, 236.0, 382.0, 540.0, 760.0]) * r, [0.05, 0.035, 0.022, 0.016, 0.011],
                               [1.0, 0.8, 0.6, 0.4, 0.25], rng=rng), ns(tc + dt))
    th = thump(n, tc, 120, 52, 18, 70, amp=1.0, drive_db=5)
    body = burst(rng, n, tc, 1.0, 30.0, "pink", (("lp", 1400.0), ("hp", 90.0)), amp=1.0)
    # sealed: a low dissonant hum (A2 against Bb2) and a muted glyph glow fading out
    e_h = env([(tc, 0.0), (tc + 0.02, 1.0), (0.72, 0.0)], n, [0.0, -2.0])
    hum = (np.sin(TAU * 110.0 * t) + 0.8 * np.sin(TAU * 116.54 * t + 0.4) + 0.3 * np.sin(TAU * 220.0 * t)) * e_h
    glow = eq(shimmer(rng, n, e_h, [880.0, 932.33, 1318.51], trem_rate=(4.0, 7.0), amp=1.0), ("lp", 2000.0))
    y = mix(norm_peak(whirr) * G(-12), norm_peak(ratchet) * G(-8), norm_peak(knock) * G(-1), th * G(-8), body * G(-5),
            norm_peak(hum) * G(-13), glow * G(-15))
    y = eq(y, ("lp", 3500.0))  # muted
    return early_reflections(y, rng, n_taps=4, t_range=(0.004, 0.02), level_db=-14)


@sound("Orsted", 0.6, target=M, use="Orsted_DragonStep arrival (DragonStep.Arrive, VR 220 pressure ring beside the target): a dry pressure thump")
def dragon_step_arrive(rng, n):
    t0 = 0.035
    # air rushes into the space he arrives in, then the pressure lands
    inrush = np.zeros(n)
    m = ns(0.05)
    place_reversed(inrush, norm_peak(eq(noise(m, rng, "pink") * np.exp(-tvec(m) / 0.012), ("bp", 1800.0, 0.8))), t0)
    snap = burst(rng, n, t0, 0.3, 5.0, "white", (("hp", 2200.0), ("lp", 11000.0)), amp=1.0)
    th = thump(n, t0, 110, 52, 22, 80, amp=1.0, drive_db=9, attack_ms=1.0)
    whump = burst(rng, n, t0, 3.0, 85.0, "brown", (("lp", 210.0), ("hp", 45.0)), amp=1.0)
    body = burst(rng, n, t0, 1.0, 60.0, "pink", (("lp", 1400.0), ("hp", 160.0)), amp=1.0, drive_db=8)
    fwump = burst(rng, n, t0, 2.0, 70.0, "pink", (("bp", 520.0, 1.1),), amp=1.0, drive_db=6)  # displaced air
    ring = whoosh(rng, n, t0, 0.5, [(0.0, 2600.0), (0.45, 240.0)], q=0.8, peak_at=0.1, rise_curve=-1.0, flutter=0.3, flutter_rate=20.0)
    dust = burst(rng, n, t0 + 0.01, 25.0, 260.0, "pink", (("hp", 2200.0), ("lp", 9000.0)), amp=1.0)
    aura = drone(rng, n, [55.0, 82.41], env([(t0, 0.0), (t0 + 0.02, 1.0), (0.45, 0.0)], n, [0.0, -2.5]), lp=420.0)
    y = mix(norm_peak(inrush) * G(-12), snap * G(-8), th * G(-12), whump * G(-10), body * G(-1), fwump * G(-3), ring,
            dust * G(-13), aura * G(-16))
    y = transient_shape(y, 1.0)
    return early_reflections(y, rng, n_taps=4, t_range=(0.004, 0.014), level_db=-14)


@sound("Orsted", 0.5, target=M, use="Orsted_DragonCrush wind-up (DragonCrush.Formation, 0.40 s; 0.14 s out of Dragon Step): the Dragon God aura compresses and tightens around the arm")
def dragon_crush_charge(rng, n):
    t = tvec(n)
    tp = 0.4  # the blow lands here (dragon_crush_impact takes over)
    settle = np.exp(-np.maximum(t - tp, 0.0) / 0.018)
    u = np.clip(t / tp, 0, 1)
    gl = 2.0 ** ((3.0 / 12.0) * u ** 1.5)
    e = env([(0.0, 0.0), (tp, 1.0)], n, [1.6]) * settle
    # a dense dark cluster (A / Bb / E) winding up: pitch creeping up, filter opening, a tension ratchet speeding up
    rat = 12.0 + 36.0 * u ** 1.4
    tension = 0.4 + 0.6 * (0.5 + 0.5 * np.sin(TAU * np.cumsum(rat) / SR)) ** 2
    dr = drone(rng, n, [55.0 * gl, 58.27 * gl, 82.41 * gl], e * tension, detune_cents=8, lp=fpath([(0.0, 220.0), (tp, 900.0)]))
    # the aura squeezed: a pressure band that narrows (Q 1.5 -> 10) and climbs as it is compressed
    from dsp import sweep
    squeeze = sweep(noise(n, rng, "pink"), "bp", fpath([(0.0, 260.0), (tp, 950.0)]), vpath([(0.0, 1.5), (tp, 10.0)]), order=4)
    squeeze = norm_peak(squeeze * e * tension)
    # tension creaks through it like a drawn cord (stick-slip speeding up)
    creak = groan(rng, n, e, rate=(18.0, 95.0), t0=0.0, t1=tp, body=(180.0, 290.0, 470.0, 690.0), tau=0.02, jitter=0.25)
    # a coil tightening: a narrow resonance climbing
    coil = formant_filter(noise(n, rng, "white"), [(fpath([(0.0, 420.0), (tp, 1700.0)]), 14.0, 0.0)])
    coil = norm_peak(coil * e)
    # air and aura drawn in toward the fist
    inward = moving_noise(rng, n, env([(0.0, 0.0), (tp, 1.0)], n, [3.0]) * settle, "bp", fpath([(0.0, 300.0), (tp, 2400.0)]), 1.2, "pink")
    sparks = crackle(rng, n, lambda x: 20.0 + 500.0 * np.clip(x / tp, 0, 1) ** 2, 0.0, tp + 0.02, band=(3000.0, 9000.0),
                     tick_ms=(0.05, 0.3), pop_prob=0.0)
    sub = np.sin(TAU * 41.0 * t) * e
    y = mix(dr * G(-5), squeeze, creak * G(-6), coil * G(-9), inward * G(-6), norm_peak(sparks) * G(-15), norm_peak(sub) * G(-14))
    return saturate(norm_peak(y), 5.0)


@sound("Orsted", 2.2, target=XL, max_gr=5.0, use="Orsted_DragonCrush impact (DragonCrush.Impact, VR 650; the 0.085 s hit-stop sits between the blow and the ground breaking): the heaviest close-range hit - bass impact, broken ground, a debris rumble tail")
def dragon_crush_impact(rng, n):
    T = n / SR
    t0 = 0.004
    tb = t0 + 0.085  # the ground breaks as the hit-stop releases
    # the blow: dry and heavy (air snap, slap, kick-like thud) with the sub already under it
    snap = burst(rng, n, t0, 0.3, 6.0, "white", (("hp", 2500.0), ("lp", 12000.0)), amp=1.0)
    slap = burst(rng, n, t0, 0.5, 22.0, "pink", (("bp", 1100.0, 0.8),), amp=1.0, drive_db=5)
    thud = thump(n, t0, 160, 55, 20, 80, amp=1.0, drive_db=10)
    sb = sub_boom(rng, n, t0, f0=58, f1=24, sweep_s=0.22, tau_s=0.55, amp=1.0, drive_db=8)
    # the ground breaks: a burst of big splitting cracks, a second deeper hit, the shockwave body
    casc = crack_run(rng, n, tb, tb + 0.16, gap=(0.004, 0.014), fc=(600.0, 2200.0), fade=0.5, dull=0.3, tau_ms=(6.0, 16.0),
                     drive_db=12)
    cr = crack(rng, n, tb, fc=1000.0, q=0.55, tau_ms=24.0, drive_db=13)
    th2 = thump(n, tb, 90, 30, 40, 200, amp=1.0, drive_db=8)
    blast = saturate(moving_noise(rng, n, perc(n, tb, 0.002, 0.28), "lp", fpath([(tb, 7000.0), (0.8, 450.0)]), 0.8, "pink",
                                  pre=(("hp", 70.0),)), 10)
    cru = crunch(rng, n, tb + 0.003, tau_fast=0.12, tau_slow=0.5, rate=5000.0)
    ring = whoosh(rng, n, tb, 0.9, [(0.0, 1500.0), (0.8, 150.0)], q=0.8, peak_at=0.08, rise_curve=-1.0)  # the shockwave ring
    # raised slabs grind and drop back; thrown chunks land; gravel and pebbles keep trickling
    gr = groan(rng, n, env([(tb, 0.0), (tb + 0.03, 1.0), (0.6, 0.0)], n, [0.0, -1.5]), rate=(45.0, 18.0), t0=tb, t1=0.6,
               body=(70.0, 110.0, 170.0, 260.0, 390.0))
    chunks = mix(*[boulder(rng, n, float(tc), size=float(rng.uniform(0.7, 1.0)), amp=float(rng.uniform(0.5, 1.0)), thump_db=-12.0)
                   for tc in (0.42, 0.55, 0.66, 0.83, 1.0, 1.21)])
    rain = debris(rng, n, 80, 0.3, T - 0.25, size=(0.0, 0.7), dist=lambda u: 1.0 - (1.0 - u) ** 1.8, bounces=(1, 3))
    trickle = gravel(rng, n, lambda x: 500.0 * np.exp(-(x - 0.4) / 0.6), 0.35, T - 0.15, size=(0.0, 0.3), dark=0.4)
    rum = rumble(rng, n, env([(tb, 0.0), (0.2, 1.0), (T - 0.05, 0.0)], n, [-1.0, -1.5]), lp=95, rate=3.0, amp=1.0)
    dust = moving_noise(rng, n, env([(tb, 0.0), (0.5, 1.0), (T, 0.0)], n, [-1.0, -2.0]), "bp", 2800.0, 0.6, "pink")
    y = mix(snap * G(-8), slap * G(-4), thud * G(-7), sb * G(-15), norm_peak(casc) * G(-3), cr * G(-2), th2 * G(-12), blast,
            cru * G(-4), ring * G(-7), gr * G(-10), norm_peak(chunks) * G(-8), norm_peak(rain) * G(-9), norm_peak(trickle) * G(-15),
            rum * G(-11), dust * G(-19))
    y = transient_shape(y, 2.5)
    y = early_reflections(y, rng, level_db=-9)
    y = slap_echoes(y, (0.15, 0.31, 0.5), (-13.0, -17.0, -22.0), lp=1000.0)
    return space(y, rng, rt60=1.8, wet_db=-16, hf_ratio=0.3)


# ============================================================================================ Rudeus
@sound("Rudeus", 1.1, target=M, use="Rudeus_DemonEye (Demon Eye of Foresight) activation: mystical shimmer")
def demon_eye(rng, n):
    T = n / SR
    t = tvec(n)
    swell = whoosh(rng, n, 0.0, 0.2, [(0.0, 300), (0.2, 1200)], q=1.5, peak_at=0.95, amp=1.0, rise_curve=3.0)
    bells = bell_hits(rng, n, [(0.12, 1174.66, 1.0), (0.19, 1567.98, 0.85), (0.26, 1760.0, 0.8), (0.33, 2349.32, 0.7)],
                      ratio=3.5, index=1.6, amp_tau=0.3, index_tau=0.12)
    e_s = env([(0.08, 0.0), (0.42, 1.0), (0.72, 0.35), (T - 0.08, 0.0)], n, [2.0, -1.0, -2.5])
    sh = shimmer(rng, n, e_s, [2960.0, 3520.0, 3950.0, 4435.0, 4700.0, 5274.0], trem_rate=(7, 11), amp=1.0)
    rate = 3.0 + 6.0 * np.clip(t / T, 0, 1)
    fc = 3500.0 * (1.0 + 0.35 * np.sin(TAU * np.cumsum(rate) / SR))
    swirl = moving_noise(rng, n, e_s, "bp", fc, 2.5, "pink")
    bloom = burst(rng, n, 0.2, 8, 60, "pink", (("lp", 1200), ("hp", 150)), amp=1.0)
    hint = np.sin(TAU * 73.42 * t) * env([(0.1, 0.0), (0.4, 1.0), (T, 0.0)], n, [1.0, -2.0])
    y = mix(swell * G(-8), bells, sh * G(-7), swirl * G(-11), bloom * G(-10), norm_peak(hint) * G(-16))
    return space(y, rng, rt60=0.9, wet_db=-9, hf_ratio=0.6)


@sound("Rudeus", 2.2, target=L, use="Rudeus_Awakening_QuagmireMagician: power surges up into a bright blast of mana")
def awakening(rng, n):
    T = n / SR
    ts = 1.1
    t = tvec(n)
    riser = moving_noise(rng, n, env([(0.0, 0.0), (ts, 1.0), (ts + 0.03, 0.0)], n, [3.0, 0.0]), "bp",
                         fpath([(0.0, 300), (ts, 5000)]), 1.2, "pink")
    gl = 2.0 ** (-np.clip(1.0 - t / ts, 0, 1) ** 1.5)  # -12 semitones -> 0 over the riser
    tone = np.zeros(n)
    for f in (146.83, 220.0, 293.66):
        tone += saw(f * gl, n, rng.random())
    from dsp import sweep
    tone = sweep(tone, "lp", fpath([(0.0, 400), (ts, 3000), (T, 1200)]), 0.9)
    tone = norm_peak(tone * env([(0.0, 0.0), (ts, 1.0), (ts + 0.4, 0.5), (T, 0.0)], n, [2.5, -1.0, -2.0]))
    ck = click(rng, n, ts, amp=1.0, hp=1500)
    cr = crack(rng, n, ts, fc=2500, tau_ms=8, drive_db=8)
    sb = sub_boom(rng, n, ts, f0=70, f1=30, sweep_s=0.15, tau_s=0.35, amp=1.0, drive_db=6)
    blast = burst(rng, n, ts, 2, 120, "pink", (("lp", 5000), ("hp", 100)), amp=1.0, drive_db=6)
    e_c = env([(ts, 0.0), (ts + 0.02, 1.0), (T, 0.0)], n, [0.0, -2.5])
    chord = shimmer(rng, n, e_c, [587.33, 739.99, 880.0, 1174.66, 1318.51], trem_rate=(6, 10), trem_depth=0.35, amp=1.0)
    crk = crackle(rng, n, lambda x: 120 * np.exp(-(x - ts) / 0.4), ts, T, band=(3000, 9000), tick_ms=(0.1, 0.5), pop_prob=0.0)
    air = moving_noise(rng, n, env([(ts, 0.0), (ts + 0.05, 1.0), (T, 0.0)], n, [0.0, -2.0]), "bp",
                       2000.0 * (1.0 + 0.4 * np.sin(TAU * 4.0 * t)), 1.5, "pink")
    y = mix(riser * G(-3), tone * G(-5), ck * G(-6), cr * G(-6), sb * G(-10), blast, chord * G(-6), norm_peak(crk) * G(-14),
            air * G(-12))
    return space(y, rng, rt60=1.5, wet_db=-12, hf_ratio=0.5)


@sound("Rudeus", 0.4, target=S, use="Rudeus quick casts / generic small spell cast (e.g. each Rudeus_ElementalBarrage shot)")
def cast_small(rng, n):
    wh = whoosh(rng, n, 0.0, 0.38, [(0.0, 800), (0.1, 3500), (0.38, 1500)], q=2.5, peak_at=0.27, amp=1.0)
    sp = np.zeros(n)
    for tt in np.sort(rng.uniform(0.03, 0.25, 9)):
        f = float(rng.uniform(2500, 6000))
        k = modal([f, f * 2.01], [rng.uniform(0.03, 0.08), 0.02], [1.0, 0.25], rng=rng)
        place(sp, k * rng.uniform(0.4, 1.0), ns(tt))
    th = thump(n, 0.004, 200, 100, 10, 30, amp=1.0, drive_db=3)
    t = tvec(n)
    chirp = np.sin(TAU * np.cumsum(900.0 * 2.0 ** np.clip(t / 0.08, 0, 1)) / SR) * perc(n, 0.0, 0.004, 0.05)
    y = mix(wh, norm_peak(sp) * G(-7), th * G(-14), norm_peak(chirp) * G(-11))
    return space(y, rng, rt60=0.7, wet_db=-14, hf_ratio=0.6)


# ============================================================================================ Rudeus: ability overhaul
# Docs/Ability_Overhaul.md section 6 (Elemental Barrage). Each element keeps its own physical voice, borrowed from its
# element module, and Rudeus's bright D-major chime ties the four together.
def _elements():
    from sfx_earth import stone_knock
    from sfx_fire import _swirl_fc
    from sfx_water import rush
    from sfx_wind import howl, swirl_phase
    return stone_knock, _swirl_fc, rush, howl, swirl_phase


@sound("Rudeus", 0.8, target=MP, use="Rudeus_ElementalBarrage formation (Barrage.Formation / Barrage.Orb*; the barrage starts at 0.6 s): fire, water, earth and wind ignite around Rudeus one after another")
def barrage_orbs(rng, n):
    stone_knock, _swirl_fc, rush, howl, swirl_phase = _elements()
    T = n / SR
    t = tvec(n)
    tf, tw, te, ta = 0.03, 0.13, 0.23, 0.33  # fire, water, earth, wind
    hold = lambda a: env([(a, 0.0), (a + 0.02, 1.0), (0.55, 0.45), (T - 0.03, 0.0)], n, [-1.0, -0.5, -2.0])  # noqa: E731
    # fire: an ignition puff, a small roar and crackle
    f_puff = burst(rng, n, tf, 3.0, 60.0, "pink", (("bp", 700.0, 0.9),), amp=1.0, drive_db=6)
    f_th = thump(n, tf, 130, 70, 15, 50, amp=1.0, drive_db=4)
    f_roar = roar(rng, n, hold(tf), lp=1800.0, drive_db=6, amp=1.0)
    f_crk = crackle(rng, n, lambda x: 20.0 + 220.0 * np.exp(-(x - tf) / 0.15), tf, T - 0.05, band=(1500.0, 8000.0), pop_prob=0.25)
    # water: a ring of water spins up: a swirling rush, a burst of bubbles, a few droplets
    w_sw = rush(rng, n, hold(tw), 400.0, 5000.0, lp=_swirl_fc(n, T, 1200, 2600, 0.5, 9, 14, 0.35, 1.0))
    w_bub = bubbles(rng, n, lambda x: 500.0 * np.exp(-(x - tw) / 0.08) + 30.0, tw, T - 0.1, f_lo=400.0, f_hi=2600.0, rise=(0.3, 1.0))
    w_drop = droplets(rng, n, lambda x: 60.0 * np.exp(-(x - tw) / 0.2), tw + 0.02, T - 0.1, f_lo=1800.0, f_hi=5000.0)
    w_slap = burst(rng, n, tw, 0.8, 14.0, "pink", (("lp", 3200.0), ("hp", 200.0)), amp=1.0)
    w_glug = glug(rng, n, tw + 0.005, f0=420.0, count=3, spacing=(0.012, 0.02))
    # earth: fragments snap together into a dense stone and start to orbit it
    e_cr = crack(rng, n, te, fc=2200.0, q=0.9, tau_ms=6.0, drive_db=10)
    e_kn = stone_knock(rng, n, te, 760.0)
    e_gr = grind(rng, n, hold(te), rate=(500.0, 500.0), band=(300.0, 2500.0))
    e_peb = debris(rng, n, 10, te - 0.08, te + 0.01, size=(0.0, 0.3), bounces=(1, 1), dist=lambda u: u ** 0.5)
    # wind: a streak ring whistles up and circles
    ph = swirl_phase(n, 6.0, 11.0)
    a_hw = howl(rng, n, [fpath([(ta, 1200.0), (ta + 0.12, 2300.0), (T, 2100.0)])(t), fpath([(ta, 1900.0), (ta + 0.12, 3500.0)])(t)],
                16.0, 0.06, ph, [0, -4]) * hold(ta)
    a_wh = whoosh(rng, n, ta - 0.05, 0.2, [(0.0, 700.0), (0.06, 2600.0), (0.2, 1600.0)], q=1.6, peak_at=0.25, amp=1.0)
    a_fwip = formant_filter(noise(n, rng, "white"), [(fpath([(ta - 0.01, 900.0), (ta + 0.05, 3000.0)]), 20.0, 0.0)])
    a_fwip = norm_peak(a_fwip * env([(ta - 0.015, 0.0), (ta + 0.01, 1.0), (ta + 0.09, 0.0)], n, [1.0, -2.0]))
    # Rudeus's chime answers each element (D5 F#5 A5 D6), with a mana shimmer as the set stabilises
    bells = bell_hits(rng, n, [(tf, 587.33, 0.8), (tw, 739.99, 0.8), (te, 880.0, 0.85), (ta, 1174.66, 1.0)], ratio=3.5, index=1.2,
                      amp_tau=0.25, index_tau=0.08)
    sh = shimmer(rng, n, env([(0.2, 0.0), (0.45, 1.0), (T - 0.03, 0.0)], n, [1.5, -2.0]), [2349.3, 2960.0, 3520.0, 4698.6],
                 trem_rate=(7.0, 11.0), amp=1.0)
    y = mix(f_puff * G(-3), f_th * G(-12), f_roar * G(-11), norm_peak(f_crk) * G(-15),
            w_slap * G(-4), w_glug * G(-6), w_sw * G(-9), norm_peak(w_bub) * G(-8), norm_peak(w_drop) * G(-15),
            e_cr * G(-4), e_kn * G(-5), e_gr * G(-12), norm_peak(e_peb) * G(-11),
            a_fwip * G(-4), a_hw * G(-9), a_wh * G(-6), bells * G(-8), sh * G(-18))
    return space(y, rng, rt60=0.9, wet_db=-15, hf_ratio=0.55)


@sound("Rudeus", 3.0, target=XL, max_gr=5.0, use="BarrageFinale.Impact (Rudeus_ElementalBarrage finale, VR 900): one huge multi-element blast - earth erupts outward, water spirals up, fire expands through the centre, wind drives it all out")
def barrage_finale(rng, n):
    stone_knock, _swirl_fc, rush, howl, swirl_phase = _elements()
    T = n / SR
    t = tvec(n)
    t0 = 0.01
    # the detonation shared by all four
    ck = click(rng, n, t0, amp=1.0, hp=1200.0, decay_ms=2.0)
    cr = crack(rng, n, t0, fc=1300.0, q=0.6, tau_ms=20.0, drive_db=12)
    sb = sub_boom(rng, n, t0, f0=68, f1=25, sweep_s=0.2, tau_s=0.6, amp=1.0, drive_db=6)
    th = thump(n, t0, 150, 48, 25, 140, amp=1.0, drive_db=8)
    # fire expands through the centre: the blast body and a roaring fireball that rolls outward
    blast = saturate(moving_noise(rng, n, perc(n, t0, 0.002, 0.2), "lp", fpath([(0.0, 11000.0), (0.15, 4000.0), (0.8, 700.0)]), 0.8,
                                  "pink", pre=(("hp", 80.0),)), 10)
    f_roar = roar(rng, n, env([(t0, 0.0), (0.05, 1.0), (0.5, 0.7), (1.9, 0.0)], n, [-2.0, -0.5, -2.0]),
                  lp=fpath([(0.0, 3200.0), (0.6, 1800.0), (1.8, 600.0)]), drive_db=10, mid_db=-2, amp=1.0)
    embers = crackle(rng, n, lambda x: 150.0 * np.exp(-(x - 0.1) / 0.8) + 8.0, 0.08, T - 0.1,
                     amp_fn=lambda x: np.exp(-(x - 0.08) / 1.2), band=(1200.0, 8000.0), pop_prob=0.3)
    # earth erupts outward: splitting ground, crunch, rocks thrown out and raining back
    e_run = crack_run(rng, n, t0, t0 + 0.12, gap=(0.004, 0.012), fc=(800.0, 2500.0), fade=0.4, dull=0.2, tau_ms=(5.0, 14.0), drive_db=11)
    e_cru = crunch(rng, n, t0 + 0.003, tau_fast=0.1, tau_slow=0.45, rate=4200.0)
    e_chunks = mix(*[boulder(rng, n, float(tc), size=float(rng.uniform(0.5, 0.9)), amp=float(rng.uniform(0.5, 1.0)), thump_db=-14.0)
                     for tc in (0.55, 0.72, 0.9, 1.15, 1.42)])
    e_rain = debris(rng, n, 110, 0.3, T - 0.35, size=(0.0, 0.7), dist=lambda u: 1.0 - (1.0 - u) ** 2.0, bounces=(1, 3))
    # water spirals up: a splash, then a column of water whirling upward, foam, and a rain of drops
    w_spl = saturate(moving_noise(rng, n, perc(n, t0 + 0.004, 0.004, 0.3), "lp", fpath([(0.0, 12000.0), (0.9, 1500.0)]), 0.8, "white",
                                  pre=(("hp", 250.0),)), 6)
    spiral = _swirl_fc(n, T, 450, 3600, 1.1, 5, 13, 0.3)
    w_col = rush(rng, n, env([(t0, 0.0), (0.12, 1.0), (0.9, 0.6), (1.6, 0.0)], n, [-1.0, -0.5, -2.0]), 300.0, 7000.0, lp=spiral * 1.6)
    w_bub = bubbles(rng, n, lambda x: 700.0 * np.exp(-(x - 0.05) / 0.4) + 20.0, 0.05, 1.8, f_lo=200.0, f_hi=2500.0, rise=(0.2, 1.0))
    w_rain = droplets(rng, n, lambda x: 150.0 * np.exp(-((x - 1.3) / 0.7) ** 2), 0.6, T - 0.15, f_lo=1500.0, f_hi=5500.0,
                      amp_fn=lambda x: np.exp(-np.maximum(x - 1.3, 0) / 1.0))
    # wind drives the whole blast outward: a massive falling gust and a pressure whump
    a_gust = whoosh(rng, n, t0, 1.7, [(0.0, 3200.0), (0.15, 2200.0), (1.5, 260.0)], q=0.8, peak_at=0.05, rise_curve=-1.0,
                    fall_curve=-2.0, flutter=0.4, flutter_rate=12.0)
    a_whump = burst(rng, n, t0, 6.0, 160.0, "brown", (("lp", 220.0), ("hp", 30.0)), amp=1.0)
    ph = swirl_phase(n, 9.0, 3.0)
    a_howl = howl(rng, n, [fpath([(0.0, 900.0), (1.5, 420.0)])(t), fpath([(0.0, 1500.0), (1.5, 700.0)])(t)], 9.0, 0.1, ph, [0, -4])
    a_howl = a_howl * env([(t0, 0.0), (0.1, 1.0), (1.6, 0.0)], n, [-1.0, -2.0])
    # Rudeus's signature: a bright D-major bloom inside the blast
    bloom = shimmer(rng, n, env([(t0, 0.0), (0.05, 1.0), (1.6, 0.0)], n, [0.0, -2.5]), [587.33, 880.0, 1174.66, 1479.98, 1760.0],
                    trem_rate=(6.0, 10.0), trem_depth=0.35, amp=1.0)
    rum = rumble(rng, n, env([(t0, 0.0), (0.15, 1.0), (T - 0.05, 0.0)], n, [-1.0, -1.6]), lp=100, amp=1.0)
    y = mix(ck * G(-6), cr * G(-3), sb * G(-14), th * G(-11), blast, f_roar * G(-2), norm_peak(embers) * G(-15),
            norm_peak(e_run) * G(-5), e_cru * G(-4), norm_peak(e_chunks) * G(-9), norm_peak(e_rain) * G(-10),
            w_spl * G(-4), w_col * G(-4), norm_peak(w_bub) * G(-10), norm_peak(w_rain) * G(-14),
            a_gust * G(-3), a_whump * G(-7), a_howl * G(-10), bloom * G(-15), rum * G(-12))
    y = transient_shape(y, 2.5)
    y = early_reflections(y, rng, level_db=-8)
    y = slap_echoes(y, (0.14, 0.3, 0.49), (-14.0, -18.0, -23.0), lp=1600.0)
    return space(y, rng, rt60=1.8, wet_db=-15, hf_ratio=0.35)
