"""Earth magic. Palette: stone cracks (saturated band-limited bursts), low thumps and sub booms,
granular crunch (dense ticks + low pops), stone-on-stone grinding (stick-slip grains), modal rock
fragments that bounce (restitution model), dust hiss, rumble; mud = slow low bubbles with pops."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, bubble, db2lin, early_reflections, env, eq, modal, noise, norm_peak, norm_rms, ns,
                 periodic_lfo, perc, place, saturate, slap_echoes, smooth_random, sweep, transient_shape, tvec)
from layers import (boulder, bubbles, burst, buzz, click, crack, crackle, debris, fpath, glug, gravel, grind, groan, mix,
                    moving_noise, n_wave, place_reversed, pulse_train, rotor, rumble, space, sub_boom, thump, turbulence,
                    whoosh)
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


# ============================================================================================ ability overhaul
# Docs/Ability_Overhaul.md section 6. Rudeus's signature cannon is stone_compress (the hold) -> sonic_boom (the
# release); the rest are the Quagmire, Earth Wall and Earth Spikes stages.
def _settle(n, t_start, tau):
    """1 until t_start, then an exponential settle: a charge held past full charge ends cleanly."""
    return np.exp(-np.maximum(tvec(n) - t_start, 0.0) / tau)


def slab_ring(rng, n, t, base=200.0, tau=0.09, amp=1.0):
    """Body of a large stone slab struck hard: low, closely spaced and strongly damped modes."""
    ratios = np.array([1.0, 1.37, 2.02, 2.63, 3.41, 4.35, 5.6])
    freqs = base * ratios * (1.0 + 0.02 * rng.standard_normal(ratios.size))
    k = modal(freqs, tau / ratios ** 0.7, np.array([1.0, 0.85, 0.7, 0.55, 0.4, 0.3, 0.2]), rng=rng)
    y = np.zeros(n)
    place(y, k, ns(t))
    return norm_peak(y) * amp


def crack_run(rng, n, t0, t1, gap=(0.008, 0.024), fc=(1000.0, 3200.0), fade=0.6, dull=0.45, tau_ms=(3.0, 9.0), drive_db=9.0):
    """Cracks racing along the ground from t0 to t1: each later crack is further away (quieter, duller) and the
    spacing opens up a little as the fracture runs out."""
    out = np.zeros(n)
    tt = t0
    while tt < t1:
        d = (tt - t0) / max(t1 - t0, 1e-6)
        out += crack(rng, n, tt, amp=float((1.0 - fade * d) * rng.uniform(0.55, 1.0)),
                     fc=float(rng.uniform(*fc) * (1.0 - dull * d)), q=float(rng.uniform(0.8, 1.4)),
                     tau_ms=float(rng.uniform(*tau_ms)), drive_db=drive_db)
        tt += rng.uniform(*gap) * (0.8 + 0.6 * d)
    return out


@sound("Earth", 1.8, target=M, use="Rudeus_StoneCannon charge (CastSound; full charge at 1.6 s, faded by the game on release, so every moment of the build works as an ending): rock fragments grind together, compress, spin up into a rising whine and a tight high vibration")
def stone_compress(rng, n):
    T = n / SR
    t = tvec(n)
    tf = 1.6  # full charge (MaxChargeTime): the spin holds, then settles so an over-held charge ends cleanly
    settle = _settle(n, tf + 0.01, 0.035)
    # 1. fragments rip out of the ground and grind together as they converge above the palm
    e_gr = env([(0.0, 0.0), (0.03, 0.75), (0.4, 0.8), (0.95, 0.65), (1.35, 0.25), (tf, 0.0)], n, [-1.0, 0.0, -1.0, -1.0, -1.0])
    gr = grind(rng, n, e_gr, rate=(700.0, 3200.0), band=(280.0, 3000.0))
    rip = crunch(rng, n, 0.012, tau_fast=0.05, tau_slow=0.2, rate=2600.0, band=(600.0, 5000.0))
    peb = gravel(rng, n, lambda x: 40.0 + 420.0 * np.exp(-((x - 0.45) / 0.28) ** 2), 0.02, 1.05, size=(0.05, 0.4),
                 amp_fn=lambda x: 0.4 + 0.6 * np.clip(x / 0.5, 0, 1))
    rum = rumble(rng, n, env([(0.0, 0.0), (0.08, 0.8), (0.5, 1.0), (tf, 0.5), (T, 0.0)], n, [-1.0, 0.0, -1.0, -2.0]), lp=140, amp=1.0)
    # 2. compression: the mass creaks under load, then densifies into a smooth hiss as it fuses
    cre = groan(rng, n, env([(0.25, 0.0), (0.7, 1.0), (1.1, 0.6), (1.4, 0.0)], n, [1.0, -1.0, -1.0]),
                rate=(14.0, 75.0), t0=0.25, t1=1.4, body=(110.0, 170.0, 262.0, 395.0, 560.0))
    dense = moving_noise(rng, n, env([(0.35, 0.0), (0.95, 1.0), (1.3, 0.3), (tf, 0.0)], n, [1.5, -1.0, -1.0]), "bp",
                         fpath([(0.35, 1100.0), (1.2, 2600.0)]), 1.1, "pink")
    # 3. spin-up: the slug's three grooves chop the air, whups that accelerate and fuse into a whine near 1 kHz
    ts, te = 0.55, 1.52
    rot = np.where(t < ts, 2.5, 2.5 * (420.0 / 2.5) ** (np.clip((t - ts) / (te - ts), 0, 1) ** 0.85))
    e_sp = env([(0.45, 0.0), (0.75, 0.3), (te, 1.0)], n, [0.5, 0.6]) * settle
    spin = rotor(rng, n, rot, e_sp, flutes=3, sharp=5, tone_db=-1.0)
    # 4. at full spin the slug trembles: a tight high vibration and shimmering, distorted air
    e_vb = env([(0.95, 0.0), (1.5, 1.0)], n, [1.5]) * settle
    vib = buzz(rng, n, e_vb, fc=fpath([(0.95, 2600.0), (1.55, 3700.0)])(t), am_hz=118.0, am_depth=0.7)
    haze = moving_noise(rng, n, e_vb * pulse_train(rot * 3.0, n, 3), "hp", 6500.0, 0.7, "white")
    # pressure rings: a low throb that quickens with the spin
    thr = 5.0 + 9.0 * np.clip((t - 0.8) / 0.8, 0, 1)
    throb = np.sin(TAU * 58.0 * t) * (0.5 + 0.5 * np.sin(TAU * np.cumsum(thr) / SR)) * env([(0.8, 0.0), (1.5, 1.0)], n, [1.5]) * settle
    # loose dust pulled inward: reversed hisses converging on the slug
    dust = np.zeros(n)
    for te_ in (0.62, 0.84, 1.02, 1.18, 1.31, 1.42, 1.5):
        m = ns(rng.uniform(0.14, 0.26))
        puff = eq(noise(m, rng, "pink") * np.exp(-tvec(m) / rng.uniform(0.03, 0.06)), ("hp", 1800.0), ("lp", 9000.0))
        place_reversed(dust, norm_peak(puff) * (0.5 + 0.5 * te_ / tf), te_)
    y = mix(gr * G(-9), rip * G(-10), norm_peak(peb) * G(-13), rum * G(-15), cre * G(-10), dense * G(-13), spin,
            vib * G(-5), haze * G(-12), norm_peak(throb) * G(-18), norm_peak(dust) * G(-15))
    return early_reflections(y, rng, level_db=-12)


@sound("Earth", 1.0, target=L, use="Rudeus_StoneCannon release (RudeusCannon.Release, after stone_compress): a supersonic double crack, a pressure thump, and tearing air as the spinning slug leaves")
def sonic_boom(rng, n):
    t = tvec(n)
    t0 = 0.004
    D = 0.013  # N-wave length: the two shocks read as one fat 'k-RACK'
    nw = n_wave(n, t0, dur_ms=D * 1e3, rise_ms=0.05) + n_wave(n, t0 + 0.0045, dur_ms=D * 1e3, rise_ms=0.15, amp=0.45)
    nw = eq(nw, ("hp", 90.0))  # the thump layer owns the lows
    sh = crack(rng, n, t0, fc=4200.0, q=0.6, tau_ms=2.5, drive_db=10, hp=1500.0) + \
        crack(rng, n, t0 + D, amp=0.8, fc=3500.0, q=0.6, tau_ms=3.0, drive_db=10, hp=1500.0)
    # the palm's pressure cone: a thump and an air blast just behind the shock, so their peaks do not stack
    th = thump(n, t0 + 0.007, 115, 40, 35, 150, amp=1.0, drive_db=8, attack_ms=2.0)
    blast = burst(rng, n, t0 + 0.004, 4.0, 90.0, "brown", (("lp", 700.0), ("hp", 45.0)), amp=1.0, drive_db=6)
    body = saturate(moving_noise(rng, n, perc(n, t0, 0.001, 0.09), "lp", fpath([(0.0, 9000.0), (0.25, 1200.0)]), 0.8, "pink",
                                 pre=(("hp", 120.0),)), 9)
    # tearing air: a dense granular rip under a falling band-pass, receding with the slug
    e_tear = env([(t0, 0.0), (0.015, 1.0), (0.3, 0.55), (0.85, 0.0)], n, [-1.0, -0.8, -2.5])
    rip = crackle(rng, n, 3500.0, 0.0, 0.9, band=(600.0, 9000.0), tick_ms=(0.08, 0.5), pop_prob=0.0)
    tear = sweep(rip, "bp", fpath([(0.0, 5200.0), (0.12, 3200.0), (0.8, 650.0)]), 1.0) * e_tear
    wh = whoosh(rng, n, 0.006, 0.85, [(0.0, 3800.0), (0.2, 1800.0), (0.8, 380.0)], q=1.3, peak_at=0.06, rise_curve=-1.0,
                flutter=0.25, flutter_rate=30.0)
    # the slug's spin whine (stone_compress's end state), Doppler-dropped as it leaves
    fw = np.where(t < 0.035, 990.0 * (700.0 / 990.0) ** np.clip(t / 0.035, 0, 1),
                  700.0 * (560.0 / 700.0) ** np.clip((t - 0.035) / 0.6, 0, 1))
    lv = rotor(rng, n, fw / 3.0, env([(0.0, 0.0), (0.01, 1.0), (0.7, 0.0)], n, [0.0, -3.5]), flutes=3, sharp=5, tone_db=-2.0)
    y = mix(nw * G(-4), sh * G(-6), th * G(-10), blast * G(-4), saturate(body, 4) * G(-1), saturate(norm_peak(tear), 6),
            wh * G(-1), lv * G(-8))
    y = transient_shape(y, 2.0)
    y = early_reflections(y, rng, level_db=-9)
    y = slap_echoes(y, (0.13, 0.27, 0.41), (-14.0, -19.0, -24.0), lp=2200.0)
    return space(y, rng, rt60=1.1, wet_db=-19, hf_ratio=0.4)


@sound("Earth", 1.4, target=MP, use="Rudeus_Quagmire transformation (Quagmire.Zone, 0-1.2 s; quagmire_loop carries on): the ground cracks, water seeps and gurgles up through the cracks, then thick mud churns")
def quagmire_transform(rng, n):
    T = n / SR
    # 1. dry ground cracks (0-0.4 s)
    cr = np.zeros(n)
    for tc in 0.012 + 0.36 * np.sort(rng.random(13)) ** 1.25:
        cr += crack(rng, n, float(tc), amp=float(rng.uniform(0.45, 1.0) * (1.0 - 0.5 * tc / 0.4)),
                    fc=float(rng.uniform(900.0, 2800.0)), q=float(rng.uniform(0.7, 1.2)), tau_ms=float(rng.uniform(5.0, 14.0)),
                    drive_db=10)
    th = thump(n, 0.012, 130, 50, 25, 110, amp=1.0, drive_db=6)
    cru = crunch(rng, n, 0.015, tau_fast=0.08, tau_slow=0.25, rate=1800.0)
    rum = rumble(rng, n, env([(0.0, 0.0), (0.05, 1.0), (0.7, 0.6), (T, 0.0)], n, [-1.0, -1.0, -2.0]), lp=130, amp=1.0)
    # 2. water forced up through the cracks (0.3-0.9 s): a seeping hiss, rising bubbles, gurgles
    e_seep = env([(0.25, 0.0), (0.55, 1.0), (0.95, 0.45), (1.25, 0.0)], n, [1.5, -1.0, -2.0])
    seep = moving_noise(rng, n, e_seep * turbulence(rng, n, ((20.0, 0.5), (45.0, 0.3))), "bp",
                        fpath([(0.25, 2600.0), (0.9, 1300.0)]), 2.0, "pink")
    bub = bubbles(rng, n, lambda x: 20.0 + 300.0 * np.exp(-((x - 0.7) / 0.22) ** 2), 0.3, 1.2, f_lo=220.0, f_hi=1500.0,
                  rise=(0.2, 0.8), decay_mult=(1.0, 2.5))
    gl = mix(*[glug(rng, n, tg, f0=float(rng.uniform(200.0, 330.0)), count=int(rng.integers(3, 6)),
                    amp=float(rng.uniform(0.6, 1.0))) for tg in (0.38, 0.52, 0.66, 0.8, 0.93)])
    # 3. the soil liquefies (0.8-1.4 s): viscous blorps, a churning slosh, sucking
    mud = np.zeros(n)
    for tb in np.sort(rng.uniform(0.72, T - 0.2, 12)):
        place(mud, mud_bubble(rng, float(rng.uniform(70.0, 210.0)), float(rng.uniform(0.06, 0.15))) * rng.uniform(0.5, 1.0), ns(tb))
    e_mud = env([(0.7, 0.0), (1.0, 1.0), (T, 0.0)], n, [1.0, -1.5])
    churn = eq(noise(n, rng, "brown"), ("lp", 420.0), ("hp", 50.0))
    churn = norm_peak(norm_rms(churn) * turbulence(rng, n, ((2.5, 0.5), (5.0, 0.35))) * e_mud)
    suck = np.zeros(n)
    for ts_ in (0.92, 1.12):
        m = ns(0.28)
        s = moving_noise(rng, m, env([(0.0, 0.0), (0.07, 1.0), (0.28, 0.0)], m, [1.0, -2.0]), "bp",
                         fpath([(0.0, 900.0), (0.28, 260.0)]), 4.0, "pink")
        place(suck, s * rng.uniform(0.6, 1.0), ns(ts_))
    y = mix(norm_peak(cr) * G(-3), th * G(-10), cru * G(-6), rum * G(-12), seep * G(-8), norm_peak(bub) * G(-7), gl * G(-4),
            norm_peak(mud) * G(-2), churn * G(-8), norm_peak(suck) * G(-7))
    # dry -> saturated -> mud: the whole surface darkens as it liquefies
    y = sweep(y, "lp", fpath([(0.0, 10000.0), (0.4, 7000.0), (0.85, 4200.0), (T, 2800.0)]), 0.7)
    return early_reflections(y, rng, level_db=-11)


@sound("Earth", 0.45, target=S, use="Quagmire.Splash / Quagmire.Ripple: a foot or body plunging into deep mud (randomise pitch +-2 semitones)")
def mud_squelch(rng, n):
    t0 = 0.004
    slap = burst(rng, n, t0, 1.0, 16.0, "pink", (("lp", 1600.0), ("hp", 90.0)), amp=1.0)
    th = thump(n, t0, 120, 55, 20, 45, amp=1.0, drive_db=4)
    # displaced mud: a resonant 'schlorp' gliding down, with a viscous low blorp
    e_s = env([(0.01, 0.0), (0.04, 1.0), (0.2, 0.3), (0.28, 0.0)], n, [1.0, -1.5, -2.0])
    schl = moving_noise(rng, n, e_s * turbulence(rng, n, ((35.0, 0.4),)), "bp", fpath([(0.01, 850.0), (0.24, 240.0)]), 4.5, "pink")
    blorp = np.zeros(n)
    place(blorp, mud_bubble(rng, 115.0, 0.12), ns(0.03))
    # trapped air escapes around the leg
    pops = np.zeros(n)
    for tp, f in ((0.13, 520.0), (0.19, 380.0), (0.26, 700.0)):
        place(pops, bubble(f * rng.uniform(0.9, 1.1), rise=0.4, decay_mult=0.9, max_len=0.08) * rng.uniform(0.5, 1.0), ns(tp))
    # the mud closes around the ankle: a short suck and a wet 'tchk'
    e_k = env([(0.24, 0.0), (0.33, 1.0), (0.37, 0.0)], n, [2.0, -1.0])
    suck = moving_noise(rng, n, e_k, "bp", fpath([(0.24, 300.0), (0.37, 950.0)]), 5.0, "pink")
    tchk = burst(rng, n, 0.365, 0.3, 4.0, "pink", (("bp", 1300.0, 1.2),), amp=1.0)
    y = mix(slap * G(-3), th * G(-9), schl, norm_peak(blorp) * G(-4), norm_peak(pops) * G(-10), suck * G(-8), tchk * G(-12))
    y = eq(y, ("lp", 4500.0))  # mud is thick and dark
    return early_reflections(y, rng, level_db=-13)


@sound("Earth", 0.7, target=MP, use="EarthWall.Crack (a wall segment passes 66 % / 33 % health): the stone slab fractures under a hit")
def earth_wall_crack(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=1.0, hp=1500)
    cr = crack(rng, n, t0, fc=1500.0, q=0.7, tau_ms=12.0, drive_db=12)
    ring = slab_ring(rng, n, t0, base=190.0, tau=0.1)
    # the fracture runs through the slab: a quick chain of smaller cracks, slowing and getting duller
    run = np.zeros(n)
    tt, gap, a = t0 + 0.012, 0.009, 0.9
    for i in range(12):
        run += crack(rng, n, tt, amp=a, fc=float(2600.0 * 0.93 ** i * rng.uniform(0.85, 1.15)), q=1.0,
                     tau_ms=float(rng.uniform(3.0, 7.0)), drive_db=8)
        tt += gap * rng.uniform(0.7, 1.3)
        gap *= 1.22
        a *= 0.86
    th = thump(n, t0, 140, 62, 20, 80, amp=1.0, drive_db=8)
    cru = crunch(rng, n, t0 + 0.002, tau_fast=0.07, tau_slow=0.22, rate=3000.0)
    body = burst(rng, n, t0, 1.0, 110.0, "pink", (("hp", 180.0), ("lp", 2200.0)), amp=1.0, drive_db=10)
    chips = debris(rng, n, 9, 0.09, 0.55, size=(0.0, 0.45), dist=lambda u: u ** 1.4)
    dust = burst(rng, n, 0.03, 30.0, 220.0, "pink", (("hp", 2200.0), ("lp", 9000.0)), amp=1.0)
    y = mix(ck * G(-7), cr * G(-3), ring * G(-6), norm_peak(run) * G(-4), th * G(-14), cru * G(-2), body * G(-1),
            norm_peak(chips) * G(-11), dust * G(-19))
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, level_db=-10)


@sound("Earth", 1.6, target=L, use="EarthWall.Crumble (a wall segment breaks at 0 health or expires): a stone slab collapses into rubble")
def earth_wall_crumble(rng, n):
    T = n / SR
    t0 = 0.004
    # the slab gives: a failing crack and the base shearing with a low groan
    ck = click(rng, n, t0, amp=1.0, hp=1200)
    cr = crack(rng, n, t0, fc=1300.0, q=0.6, tau_ms=16.0, drive_db=12)
    gro = groan(rng, n, env([(0.0, 0.0), (0.02, 1.0), (0.25, 0.0)], n, [0.0, -1.5]), rate=(38.0, 20.0), t0=0.0, t1=0.28,
                body=(85.0, 132.0, 205.0, 310.0))
    th = thump(n, t0, 125, 55, 25, 90, amp=1.0, drive_db=8)
    # collapse: big chunks land between 0.12 and 0.8 s (the top of a 3-4 m slab falls for about 0.8 s)
    chunks = np.zeros(n)
    for tc in 0.2 + 0.55 * np.sort(rng.beta(2.2, 2.4, 12)):
        chunks += boulder(rng, n, float(tc), size=float(rng.uniform(0.55, 1.0)), amp=float(rng.uniform(0.55, 1.0)), thump_db=-12.0)
    pour = gravel(rng, n, lambda x: 3200.0 * np.exp(-((x - 0.45) / 0.2) ** 2) + 150.0 * np.exp(-(x - 0.1) / 0.6), 0.08, T - 0.1,
                  size=(0.0, 0.5), amp_fn=lambda x: np.clip((x - 0.08) / 0.25, 0, 1) * np.exp(-np.maximum(x - 0.55, 0) / 0.3))
    bounce = debris(rng, n, 36, 0.2, 1.2, size=(0.1, 0.8), dist=lambda u: u ** 1.3, bounces=(1, 3))
    # the heap settles: pebbles roll off, the dust cloud hisses, the ground rumbles out
    settle = debris(rng, n, 14, 0.8, T - 0.2, size=(0.0, 0.35), dist=lambda u: u ** 1.6, bounces=(1, 2))
    dust = moving_noise(rng, n, env([(0.1, 0.0), (0.5, 1.0), (T, 0.0)], n, [-1.0, -2.0]), "bp", 3200.0, 0.6, "pink")
    rum = rumble(rng, n, env([(t0, 0.0), (0.2, 0.7), (0.55, 1.0), (T - 0.05, 0.0)], n, [-1.0, 0.0, -2.0]), lp=140, amp=1.0)
    y = mix(ck * G(-11), cr * G(-7), gro * G(-8), th * G(-16), norm_peak(chunks) * G(-1), norm_peak(pour) * G(-3),
            norm_peak(bounce) * G(-6), norm_peak(settle) * G(-12), dust * G(-17), eq(rum, ("hp", 45.0)) * G(-17))
    y = transient_shape(y, 2.0)
    y = early_reflections(y, rng, level_db=-9)
    return space(y, rng, rt60=1.0, wet_db=-19, hf_ratio=0.4)


@sound("Earth", 1.6, target=XL, max_gr=5.0, use="EarthSpikes.Final: the enormous final spike (VR 300, 7 m tall) bursts up through the ground - deep and huge")
def spike_final(rng, n):
    T = n / SR
    tb = 0.035  # the burst; the ground bulges for a moment first (kept short so the burst stays on the visual)
    bulge = rumble(rng, n, env([(0.0, 0.0), (tb, 1.0), (tb + 0.02, 0.0)], n, [2.5, 0.0]), lp=180, amp=1.0)
    pre = crack_run(rng, n, 0.004, tb - 0.003, gap=(0.005, 0.01), fc=(1200.0, 3000.0), fade=-0.8, dull=0.0, drive_db=8)
    ck = click(rng, n, tb, amp=1.0, hp=1100, decay_ms=2.0)
    cr = crack(rng, n, tb, fc=1100.0, q=0.55, tau_ms=22.0, drive_db=13)
    cr2 = crack(rng, n, tb + 0.012, fc=2300.0, q=0.8, tau_ms=10.0, drive_db=10)
    sb = sub_boom(rng, n, tb, f0=62, f1=25, sweep_s=0.22, tau_s=0.75, amp=1.0, drive_db=6)
    th = thump(n, tb, 130, 42, 30, 160, amp=1.0, drive_db=8)
    blast = saturate(moving_noise(rng, n, perc(n, tb, 0.002, 0.16), "lp", fpath([(tb, 8000.0), (0.6, 500.0)]), 0.8, "pink",
                                  pre=(("hp", 80.0),)), 10)
    # the lance shoots up 7 m: stone scraping stone with a rising pitch, an upward rush of air, then it locks
    e_up = env([(tb, 0.0), (tb + 0.02, 1.0), (0.42, 0.6), (0.6, 0.0)], n, [-1.0, -1.0, -2.0])
    scrape = sweep(grind(rng, n, e_up, rate=(3500.0, 3500.0), band=(200.0, 4000.0)), "bp", fpath([(tb, 320.0), (0.5, 2400.0)]), 2.2)
    squeal = groan(rng, n, e_up, rate=(140.0, 420.0), t0=tb, t1=0.5, body=(310.0, 520.0, 790.0, 1130.0), tau=0.012, jitter=0.15)
    up = whoosh(rng, n, tb, 0.5, [(0.0, 300.0), (0.4, 2400.0)], q=1.2, peak_at=0.35, rise_curve=1.0)
    lock = slab_ring(rng, n, 0.47, base=92.0, tau=0.16) + thump(n, 0.47, 95, 40, 20, 90, amp=0.8, drive_db=6)
    cru = crunch(rng, n, tb + 0.003, tau_fast=0.12, tau_slow=0.45, rate=4200.0)
    # everything it tore up comes back down
    rain = debris(rng, n, 90, 0.35, T - 0.2, size=(0.0, 0.8), dist=lambda u: 1.0 - (1.0 - u) ** 2.0, bounces=(1, 3))
    chunks = mix(*[boulder(rng, n, float(tc), size=float(rng.uniform(0.6, 1.0)), amp=float(rng.uniform(0.5, 1.0)), thump_db=-12.0)
                   for tc in (0.58, 0.71, 0.86, 1.02)])
    rum = rumble(rng, n, env([(tb, 0.0), (0.15, 1.0), (T - 0.05, 0.0)], n, [-1.0, -1.8]), lp=100, amp=1.0)
    dust = moving_noise(rng, n, env([(tb, 0.0), (0.4, 1.0), (T, 0.0)], n, [-1.0, -2.0]), "bp", 3000.0, 0.6, "pink")
    y = mix(bulge * G(-12), norm_peak(pre) * G(-12), ck * G(-6), cr * G(-2), cr2 * G(-6), sb * G(-14), th * G(-12), blast * G(-2),
            saturate(norm_peak(scrape), 6), squeal * G(-8), up * G(-5), norm_peak(lock) * G(-8), cru * G(-5), norm_peak(rain) * G(-8),
            norm_peak(chunks) * G(-8), rum * G(-12), dust * G(-18))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-8)
    y = slap_echoes(y, (0.15, 0.31, 0.5), (-14.0, -18.0, -23.0), lp=1400.0)
    return space(y, rng, rt60=1.6, wet_db=-16, hf_ratio=0.35)


@sound("Earth", 0.8, target=M, use="Earth_EarthSpikes cast and ground-splitting strikes: cracks race away along the ground (before earth_spike / spike_final)")
def ground_crack_run(rng, n):
    T = n / SR
    tr = 0.62  # the fracture reaches the end of the line
    run = crack_run(rng, n, 0.004, tr, gap=(0.016, 0.034), fc=(900.0, 2800.0), fade=0.6, dull=0.45, tau_ms=(5.0, 12.0), drive_db=11)
    e_run = env([(0.0, 0.0), (0.02, 1.0), (tr, 0.35), (tr + 0.12, 0.0)], n, [-1.0, 0.5, -2.0])
    split = grind(rng, n, e_run, rate=(2500.0, 1500.0), band=(150.0, 1800.0))
    tearing = groan(rng, n, e_run, rate=(55.0, 30.0), t0=0.0, t1=tr + 0.1, body=(90.0, 140.0, 215.0, 330.0), tau=0.03)
    zipper = sweep(crackle(rng, n, 1800.0, 0.0, tr + 0.1, band=(900.0, 7000.0), tick_ms=(0.2, 1.2), pop_prob=0.2), "lp",
                   fpath([(0.0, 9000.0), (tr, 2500.0)]), 0.7) * e_run
    th = thump(n, 0.004, 120, 50, 20, 80, amp=1.0, drive_db=6)
    rum = rumble(rng, n, env([(0.0, 0.0), (0.05, 1.0), (tr + 0.05, 0.4), (T - 0.03, 0.0)], n, [-1.0, 0.0, -2.0]), lp=120, amp=1.0)
    pops = debris(rng, n, 16, 0.03, tr + 0.08, size=(0.0, 0.4), bounces=(1, 2))
    end = crack(rng, n, tr, amp=0.8, fc=1400.0, q=0.8, tau_ms=10.0, drive_db=10)
    y = mix(norm_peak(run) * G(-1), split * G(-5), tearing * G(-8), norm_peak(zipper) * G(-13), th * G(-9), rum * G(-10),
            norm_peak(pops) * G(-12), end * G(-8))
    # distance: the far end of the run is darker
    y = sweep(y, "lp", fpath([(0.0, 12000.0), (tr, 3500.0), (T, 2500.0)]), 0.7)
    y = transient_shape(y, 2.0)
    return early_reflections(y, rng, level_db=-10)
