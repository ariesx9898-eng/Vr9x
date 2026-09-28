"""Earth magic. Palette: stone cracks (saturated band-limited bursts), low thumps and sub booms,
granular crunch (dense ticks + low pops), stone-on-stone grinding (stick-slip grains), modal rock
fragments that bounce (restitution model), dust hiss, rumble; mud = slow low bubbles with pops."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, early_reflections, env, eq, modal, noise, norm_peak, norm_rms, ns, periodic_lfo,
                 perc, place, saturate, slap_echoes, smooth_random, transient_shape, tvec)
from layers import (burst, click, crack, crackle, debris, fpath, grind, mix, moving_noise, rumble, space, sub_boom,
                    thump, turbulence, whoosh)
from reg import L, LOOP_MED, LOOP_SMALL, M, MP, S, XL, sound

G = lambda d: float(db2lin(d))  # noqa: E731


def crunch(rng, n, t0, tau_fast=0.06, tau_slow=0.25, rate=3000.0, amp=1.0, band=(800.0, 6000.0)):
    """Rock crunch: very dense granular ticks and low pops decaying after t0."""
    y = crackle(rng, n, lambda t: rate * np.exp(-(t - t0) / tau_fast) + 0.13 * rate * np.exp(-(t - t0) / tau_slow),
                t0, min(n / SR, t0 + 6 * tau_slow), band=band, tick_ms=(0.3, 2.5), pop_prob=0.3, pop_band=(300.0, 1200.0),
                amp_fn=lambda t: np.exp(-(t - t0) / (tau_slow * 1.5)))
    return norm_peak(y) * amp


def stone_knock(rng, n, t0, base=900.0, amp=1.0):
    y = np.zeros(n)
    k = modal([base, base * 1.61, base * 2.56, base * 3.78], [0.05, 0.035, 0.02, 0.012], [1.0, 0.6, 0.45, 0.25], rng=rng)
    place(y, k, ns(t0))
    return norm_peak(y) * amp


@sound("Earth", 0.7, target=S, use="Earth spells wind-up (Rudeus_Basic stone bullet, Rudeus_StoneCannon, Earth_StoneCannon, Earth_EarthWall): stones grind and form")
def stone_form(rng, n):
    T = n / SR
    e = env([(0.0, 0.0), (0.45, 1.0), (0.5, 0.35), (T - 0.04, 0.0)], n, [2.0, -2.0, -2.5])
    gr = grind(rng, n, e, rate=(200.0, 650.0), band=(250.0, 2600.0))
    clicks = debris(rng, n, 26, 0.0, 0.48, size=(0.0, 0.3), bounces=(1, 2), dist=lambda u: u ** 0.6)
    rum = rumble(rng, n, e, lp=150, amp=1.0)
    knock = stone_knock(rng, n, 0.5, 850.0)
    th = thump(n, 0.5, 180, 90, 15, 45, amp=1.0, drive_db=4)
    ck = click(rng, n, 0.5, amp=1.0, hp=1500)
    after = debris(rng, n, 5, 0.51, 0.57, size=(0.0, 0.3), bounces=(1, 2))
    y = mix(gr * G(-3), norm_peak(clicks) * G(-8), rum * G(-10), knock * G(-4), th * G(-6), ck * G(-8), norm_peak(after) * G(-12))
    return early_reflections(y, rng, level_db=-11)


@sound("Earth", 1.0, loop=True, target=LOOP_MED, use="Stone bullet / stone cannon projectile in flight: spinning stone whirr loop")
def stone_spin_loop(rng, n):
    T = n / SR
    t = tvec(n)
    spin = periodic_lfo(n, 38, "pulse", sharp=2.0)  # 38 Hz rotation, whole cycles in 1 s
    wh = eq(noise(n, rng, "pink"), ("hp", 400), ("lp", 1500), circular=True)
    wh = norm_peak(norm_rms(wh) * (1.0 + 0.7 * spin))
    hum = np.sin(TAU * 76.0 * t) + 0.5 * np.sin(TAU * 38.0 * t + 0.7) + 0.2 * np.sin(TAU * 114.0 * t + 1.3)
    hum = norm_peak(hum * (1.0 + 0.15 * periodic_lfo(n, 3)))
    air = eq(noise(n, rng, "pink"), ("bp", 1800, 0.8), circular=True)
    air = norm_peak(norm_rms(air) * (1.0 + 0.3 * periodic_lfo(n, 3, phase=0.2)) * turbulence(rng, n, ((18.0, 0.3),)))
    grit = crackle(rng, n, 30.0, 0.0, T, band=(1500, 6000), tick_ms=(0.2, 1.0), pop_prob=0.0, wrap=True)
    y = mix(wh, hum * G(-9), air * G(-6), norm_peak(grit) * G(-16))
    return saturate(norm_peak(y), 2.0)


@sound("Earth", 0.9, target=L, use="Rudeus_StoneCannon / Earth_StoneCannon / Barrage_Stone cast: a boulder is fired, gunshot-like crack and boom")
def stone_cannon_launch(rng, n):
    t0 = 0.004
    t = tvec(n)
    ck = click(rng, n, t0, amp=1.0, hp=1500)
    cr = crack(rng, n, t0, amp=G(-2), fc=1800, q=0.8, tau_ms=10, drive_db=10)
    th = thump(n, t0, 120, 45, 25, 110, amp=G(-9), drive_db=8)
    boom = burst(rng, n, t0, 2, 90, "brown", (("lp", 500), ("hp", 70)), amp=G(-4))
    blast = moving_noise(rng, n, perc(n, t0, 0.001, 0.12), "lp", fpath([(0.0, 7000), (0.35, 900)]), 0.8, "pink",
                         pre=(("hp", 100),))
    body = burst(rng, n, t0, 2, 110, "pink", (("hp", 180), ("lp", 1600)), amp=1.0, drive_db=6)
    blast = saturate(blast, 9)
    fly = whoosh(rng, n, 0.03, 0.6, [(0.0, 1800), (0.6, 500)], q=2.0, peak_at=0.12, amp=1.0)
    fly = fly * (1.0 + 0.6 * np.sin(TAU * 35.0 * t))
    peb = debris(rng, n, 6, 0.05, 0.5, size=(0.0, 0.4))
    y = mix(ck * G(-5), cr * G(-3), th, boom, blast, body * G(-2), norm_peak(fly) * G(-6), norm_peak(peb) * G(-12))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-8)
    return slap_echoes(y, (0.12, 0.25), (-14, -19), lp=2000)


@sound("Earth", 1.5, target=MP, use="Stone bullet / stone cannon impact (also Goblin_RockThrow): crunch + debris")
def rock_impact(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=1.0, hp=1800)
    cr = crack(rng, n, t0, amp=G(-3), fc=2000, q=0.9, tau_ms=8, drive_db=10)
    th = thump(n, t0, 140, 55, 20, 90, amp=G(-8), drive_db=8)
    cru = crunch(rng, n, t0 + 0.002, tau_fast=0.1, tau_slow=0.4)
    body = burst(rng, n, t0, 1, 120, "pink", (("hp", 150), ("lp", 1800)), amp=1.0, drive_db=8)
    deb = debris(rng, n, 22, 0.08, 1.3, size=(0.0, 0.8), dist=lambda u: u ** 1.5)
    dust = burst(rng, n, 0.02, 30, 400, "pink", (("hp", 2000), ("lp", 9000)), amp=1.0)
    y = mix(ck * G(-5), cr * G(-3), th, cru, body, norm_peak(deb) * G(-8), dust * G(-16))
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, level_db=-9)


@sound("Earth", 2.8, target=XL, max_gr=5.0, use="Rudeus_StoneCannon_Awakened impact and other huge earth hits (Race_Ogre_GroundBreaker): big boom, crater, debris rain")
def crater_impact(rng, n):
    T = n / SR
    t0 = 0.004
    ck = click(rng, n, t0, amp=1.0, hp=1200, decay_ms=2.0)
    cr = crack(rng, n, t0, amp=G(-2), fc=1500, q=0.6, tau_ms=18, drive_db=12)
    sb = sub_boom(rng, n, t0, f0=65, f1=28, sweep_s=0.25, tau_s=0.8, amp=G(-6), drive_db=6)
    th = thump(n, t0, 140, 50, 25, 150, amp=G(-3), drive_db=8)
    blast = moving_noise(rng, n, perc(n, t0, 0.002, 0.35), "lp", fpath([(0.0, 10000), (0.8, 600)]), 0.8, "pink",
                         pre=(("hp", 90),))
    body = burst(rng, n, t0, 3, 200, "pink", (("hp", 120), ("lp", 1500)), amp=1.0, drive_db=8)
    blast = saturate(blast, 10)
    cru = crunch(rng, n, t0 + 0.003, tau_fast=0.12, tau_slow=0.5, rate=4000.0)
    rum = rumble(rng, n, env([(t0, 0.0), (0.1, 1.0), (T - 0.05, 0.0)], n, [-2.0, -2.0]), lp=120, amp=1.0)
    rain = debris(rng, n, 150, 0.3, T - 0.45, size=(0.0, 0.7), dist=lambda u: 1.0 - (1.0 - u) ** 2.2, bounces=(1, 3))
    dust = moving_noise(rng, n, env([(0.05, 0.0), (0.4, 1.0), (T, 0.0)], n, [-2.0, -2.0]), "bp", 3500, 0.6, "pink")
    y = mix(ck * G(-5), cr * G(-3), sb * G(-13), th * G(-9), blast, body * G(-1), cru * G(-1), rum * G(-11),
            norm_peak(rain) * G(-7), dust * G(-17))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-8)
    y = slap_echoes(y, (0.14, 0.3, 0.49), (-14, -18, -23), lp=1600)
    return space(y, rng, rt60=1.8, wet_db=-15, hf_ratio=0.35)


@sound("Earth", 1.8, target=L, use="Earth_EarthWall / earth fortress: a wall of rock grinds up out of the ground and locks in place")
def earth_wall_rise(rng, n):
    T = n / SR
    t = tvec(n)
    tl = 1.35
    grow = env([(0.0, 0.0), (tl, 1.0), (tl + 0.1, 0.35), (T, 0.0)], n, [1.5, -2.0, -2.0])
    gr = grind(rng, n, grow, rate=(150.0, 700.0), band=(150.0, 2200.0))
    low = moving_noise(rng, n, grow * turbulence(rng, n, ((4.0, 0.3), (12.0, 0.25))), "lp", fpath([(0.0, 150), (tl, 900)]),
                       0.8, "brown", order=4, pre=(("hp", 30),))
    tone = np.sin(TAU * np.cumsum(38.0 + 17.0 * np.clip(t / tl, 0, 1)) / SR) * grow
    crumble = debris(rng, n, 40, 0.2, tl + 0.05, size=(0.0, 0.5), dist=lambda u: u ** 0.7)
    th = thump(n, tl, 110, 45, 30, 120, amp=1.0, drive_db=8)
    cr = crack(rng, n, tl, amp=G(-3), fc=1400, tau_ms=12)
    cru = crunch(rng, n, tl + 0.002, tau_fast=0.05, tau_slow=0.2, rate=2500.0)
    settle = debris(rng, n, 14, tl + 0.05, T - 0.25, size=(0.0, 0.4), dist=lambda u: u ** 1.8)
    dust = burst(rng, n, tl, 40, 350, "pink", (("hp", 1800),), amp=1.0)
    y = mix(gr, low * G(-5), norm_peak(tone) * G(-15), norm_peak(crumble) * G(-8), th * G(-7), cr, cru * G(-2),
            norm_peak(settle) * G(-11), dust * G(-18))
    y = early_reflections(y, rng, level_db=-9)
    return space(y, rng, rt60=1.2, wet_db=-18, hf_ratio=0.4)


@sound("Earth", 0.8, target=M, use="Earth_EarthSpikes: a stone spike bursts from the ground (play per spike, randomise pitch +-2 semitones)")
def earth_spike(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=1.0, hp=2500, decay_ms=0.8)
    cr = crack(rng, n, t0, amp=G(-1), fc=2600, q=1.0, tau_ms=7, drive_db=12)
    zip_ = whoosh(rng, n, 0.0, 0.09, [(0.0, 400), (0.09, 2500)], q=1.5, peak_at=0.8, amp=1.0, rise_curve=1.0)
    cru = crunch(rng, n, t0 + 0.002, tau_fast=0.07, tau_slow=0.25, rate=2500.0)
    body = burst(rng, n, t0, 1, 80, "pink", (("hp", 200), ("lp", 2200)), amp=1.0, drive_db=8)
    th = thump(n, t0, 160, 60, 18, 70, amp=G(-8), drive_db=8)
    deb = debris(rng, n, 10, 0.05, 0.7, size=(0.0, 0.6), dist=lambda u: u ** 1.4)
    ring = stone_knock(rng, n, t0, 420.0)
    y = mix(ck * G(-4), cr * G(-2), zip_ * G(-7), cru, body * G(-1), th, norm_peak(deb) * G(-9), ring * G(-9))
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, level_db=-10)


def mud_bubble(rng, f0, dur):
    """Thick mud bubble: a slow, slightly wobbling 'blorp' with a noisy viscous body, then a dull
    wet pop when it bursts."""
    m = ns(dur)
    t = tvec(m)
    f = f0 * (1.0 + 0.5 * (t / dur) ** 1.5) * (1.0 + 0.03 * np.sin(TAU * rng.uniform(18, 30) * t))
    e = env([(0.0, 0.0), (0.015, 1.0), (dur * 0.8, 0.7), (dur, 0.0)], m, [0.0, 0.0, -2.0])
    tone = np.sin(TAU * np.cumsum(f) / SR)
    gloop = norm_peak(eq(rng.standard_normal(m), ("bp", f0 * 2.5, 2.0))) * 0.4
    y = (tone + gloop) * e
    k = ns(0.008)
    pop = rng.standard_normal(k) * np.exp(-np.arange(k) / 60.0)
    pop = eq(pop, ("bp", float(rng.uniform(500, 1400)), 1.0), ("lp", 2500))
    out = np.zeros(m + k)
    out[:m] += y
    out[m - 16: m - 16 + k] += norm_peak(pop) * 0.8
    return out


@sound("Earth", 3.0, loop=True, target=LOOP_SMALL, use="Rudeus_Quagmire / Rudeus_Quagmire_Awakened field: looping bubbling, sucking mud")
def quagmire_loop(rng, n):
    T = n / SR
    bub = np.zeros(n)
    for tb in np.sort(rng.uniform(0.0, T, 22)):
        b = mud_bubble(rng, float(rng.uniform(80, 340)), float(rng.uniform(0.04, 0.13)))
        place(bub, b * rng.uniform(0.4, 1.0), ns(tb), wrap=True)
    sq = eq(noise(n, rng, "brown"), ("lp", 600), ("hp", 60), circular=True)
    sq = norm_peak(norm_rms(sq) * np.maximum(1.0 + 0.6 * smooth_random(n, rng, 3.0), 0.0))
    suck = np.zeros(n)
    for ts in rng.uniform(0.0, T, 4):
        m = ns(0.3)
        s = moving_noise(rng, m, env([(0.0, 0.0), (0.08, 1.0), (0.3, 0.0)], m, [1.0, -2.0]), "bp",
                         fpath([(0.0, 900), (0.3, 280)]), 4.0, "pink")
        place(suck, s * rng.uniform(0.5, 1.0), ns(ts), wrap=True)
    bed = eq(noise(n, rng, "brown"), ("lp", 150), ("hp", 30), circular=True)
    wet = crackle(rng, n, 16.0, 0.0, T, band=(700, 3000), tick_ms=(1.0, 4.0), pop_prob=0.5, pop_band=(300, 900),
                  pop_ms=(3.0, 8.0), wrap=True)
    y = mix(norm_peak(bub), sq * G(-9), norm_peak(suck) * G(-5), norm_peak(bed) * G(-14), norm_peak(wet) * G(-7))
    y = eq(y, ("lp", 4500), circular=True)  # mud is thick and dark
    return saturate(norm_peak(y), 2.0)


@sound("Earth", 2.0, loop=True, target=LOOP_MED, use="Sustained tremor under big earth magic (Quagmire_Awakened, Awakening_QuagmireMagician, earth wall rising)")
def ground_rumble_loop(rng, n):
    T = n / SR
    t = tvec(n)
    deep = eq(noise(n, rng, "brown"), ("lp4", 90), ("hp", 25), circular=True)
    deep = norm_peak(norm_rms(deep) * np.maximum(1.0 + 0.4 * smooth_random(n, rng, 3.0), 0.05))
    mid = eq(noise(n, rng, "pink"), ("hp", 80), ("lp", 300), circular=True)
    mid = norm_peak(norm_rms(mid) * np.maximum(1.0 + 0.5 * smooth_random(n, rng, 6.0), 0.05))
    peb = debris(rng, n, 7, 0.0, T, size=(0.1, 0.5), bounces=(1, 3), wrap=True, dark=0.7)
    pulses = np.zeros(n)
    for tp in (0.3, 1.25):
        m = ns(0.6)
        tt = tvec(m)
        p = np.sin(TAU * 45.0 * tt) * env([(0.0, 0.0), (0.08, 1.0), (0.6, 0.0)], m, [1.0, -3.0])
        place(pulses, p, ns(tp), wrap=True)
    y = mix(deep, mid * G(-1), norm_peak(peb) * G(-14), norm_peak(pulses) * G(-7))
    return saturate(norm_peak(y), 4.0)
