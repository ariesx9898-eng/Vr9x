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
from layers import (bell_hits, burst, click, crack, crackle, debris, drone, fpath, mix, moving_noise, rumble, shimmer,
                    space, sub_boom, thump, turbulence, whoosh)
from reg import L, LOOP_SMALL, M, MP, S, XL, sound
from sfx_earth import crunch

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
