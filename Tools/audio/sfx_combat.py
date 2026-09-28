"""Generic combat feedback: hits, dodge, failed cast, cooldown and level-up cues.

Hits follow the impact anatomy (click + thump + mid slap/body + short tail). hit_magic adds an FM
zap and inharmonic sparkle. cooldown_ready and level_up are 2D player feedback (stereo) built from
FM chimes in D major so they sit with the title music."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, early_reflections, env, fm, fm_bell, modal, norm_peak, ns, pan, perc, place,
                 saw, smooth_random, sweep, transient_shape, tvec)
from layers import (burst, click, crack, crackle, debris, fpath, mix, moving_noise, space, thump, turbulence, whoosh)
from reg import M, MP, S, XS, sound
from sfx_earth import crunch

G = lambda d: float(db2lin(d))  # noqa: E731


@sound("Combat", 0.35, target=S, use="Light melee / weapon hit on a body")
def hit_light(rng, n):
    t0 = 0.003
    ck = click(rng, n, t0, amp=1.0, hp=2500, decay_ms=0.8)
    th = thump(n, t0, 190, 90, 12, 45, amp=1.0, drive_db=6)
    slap = burst(rng, n, t0, 0.5, 18, "pink", (("bp", 1500, 0.7),), amp=1.0, drive_db=4)
    body = burst(rng, n, t0, 1.0, 40, "pink", (("lp", 1200), ("hp", 150)), amp=1.0)
    tail = burst(rng, n, 0.01, 3, 60, "pink", (("lp", 600), ("hp", 80)), amp=1.0)
    y = mix(ck * G(-5), th * G(-8), slap, body * G(-3), tail * G(-12))
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, n_taps=4, t_range=(0.004, 0.02), level_db=-13)


@sound("Combat", 0.7, target=MP, use="Heavy melee hit / critical hit on a body")
def hit_heavy(rng, n):
    t0 = 0.003
    ck = click(rng, n, t0, amp=1.0, hp=2000)
    cr = crack(rng, n, t0, fc=1500, tau_ms=10, drive_db=10)
    th = thump(n, t0, 150, 55, 20, 80, amp=1.0, drive_db=8)
    body = burst(rng, n, t0, 1.0, 70, "pink", (("hp", 300), ("lp", 2500)), amp=1.0, drive_db=8)
    cru = crunch(rng, n, t0 + 0.002, tau_fast=0.03, tau_slow=0.1, rate=2000.0)
    whump = burst(rng, n, t0, 3, 80, "brown", (("lp", 250), ("hp", 40)), amp=1.0)
    y = mix(ck * G(-5), cr * G(-4), th * G(-9), body, cru * G(-5), whump * G(-11))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-10)
    return space(y, rng, rt60=0.5, wet_db=-18, hf_ratio=0.4)


@sound("Combat", 0.6, target=M, use="Spell / magic damage hit on a target")
def hit_magic(rng, n):
    t0 = 0.003
    t = tvec(n)
    ck = click(rng, n, t0, amp=1.0, hp=2500)
    th = thump(n, t0, 170, 70, 15, 60, amp=1.0, drive_db=6)
    body = burst(rng, n, t0, 1.0, 40, "pink", (("hp", 250), ("lp", 3000)), amp=1.0, drive_db=4)
    car = 600.0 + 600.0 * np.exp(-np.maximum(t - t0, 0) / 0.05)
    zap = fm(car, car * 1.5, 4.0 * np.exp(-np.maximum(t - t0, 0) / 0.05), n) * perc(n, t0, 0.001, 0.12)
    ring = np.zeros(n)
    place(ring, modal([2140, 2890, 3730, 4410, 5230, 6170], [0.18, 0.15, 0.12, 0.1, 0.08, 0.06],
                      [1.0, 0.8, 0.7, 0.5, 0.4, 0.3], rng=rng), ns(t0))
    sp = crackle(rng, n, lambda x: 120 * np.exp(-(x - 0.05) / 0.15), 0.03, 0.45, band=(3000, 9000), tick_ms=(0.1, 0.4), pop_prob=0.0)
    y = mix(ck * G(-6), th * G(-9), body * G(-2), norm_peak(zap) * G(-4), norm_peak(ring) * G(-7), norm_peak(sp) * G(-13))
    y = transient_shape(y, 2.0)
    return space(y, rng, rt60=0.8, wet_db=-14, hf_ratio=0.6)


@sound("Combat", 0.45, target=XS, use="Dodge roll / dash: body and cloth moving fast through the air")
def dodge_whoosh(rng, n):
    T = n / SR
    wh = whoosh(rng, n, 0.0, 0.43, [(0.0, 500), (0.16, 1600), (0.43, 700)], q=1.8, peak_at=0.38, amp=1.0)
    e = env([(0.0, 0.0), (0.16, 1.0), (0.43, 0.0)], n, [2.0, -2.5])
    cloth = moving_noise(rng, n, e * turbulence(rng, n, ((30.0, 0.6),)), "hp", 2000, 0.7, "pink")
    low = burst(rng, n, 0.05, 80, 90, "brown", (("lp", 250), ("hp", 50)), amp=1.0)
    y = mix(wh, cloth * G(-10), low * G(-12))
    return early_reflections(y, rng, level_db=-14)


@sound("Combat", 0.6, target=S, use="Not enough mana: the spell sputters and fizzles out")
def mana_empty(rng, n):
    t = tvec(n)
    f = np.where(t < 0.08, 500.0 * (900.0 / 500.0) ** (t / 0.08), 900.0 * (250.0 / 900.0) ** (np.clip((t - 0.08) / 0.37, 0, 1) ** 0.7))
    wob = 1.0 + 0.03 * smooth_random(n, rng, 12.0) * np.clip((t - 0.05) / 0.2, 0, 1)
    ph = TAU * np.cumsum(f * wob) / SR
    tone = (np.sin(ph) + 0.3 * np.sin(2 * ph) + 0.15 * np.sin(3 * ph)) * env([(0.0, 0.0), (0.01, 1.0), (0.08, 0.9), (0.45, 0.0)], n, [0.0, 0.0, -1.5])
    sput = crackle(rng, n, lambda x: 400.0 * np.exp(-x / 0.15), 0.02, 0.5, band=(1500, 7000), tick_ms=(0.1, 0.6), pop_prob=0.2)
    fizz = moving_noise(rng, n, perc(n, 0.02, 0.01, 0.15) * turbulence(rng, n, ((40.0, 0.8),)), "hp", 3000, 0.7, "white")
    pfft = burst(rng, n, 0.05, 3, 40, "pink", (("lp", 800), ("hp", 80)), amp=1.0)
    y = mix(norm_peak(tone) * G(-4), norm_peak(sput) * G(-6), fizz * G(-9), pfft * G(-6))
    return space(y, rng, rt60=0.5, wet_db=-16)


@sound("Combat", 1.0, stereo=True, target=-19.0, clip_db=0.0, use="Ability cooldown finished: soft bright chime (2D, player feedback)")
def cooldown_ready(rng, n):
    b1 = np.zeros(n)
    b2 = np.zeros(n)
    place(b1, fm_bell(880.0, 0.95, ratio=2.0, index=1.2, amp_tau=0.26, index_tau=0.08, attack=0.003), ns(0.004))
    place(b2, fm_bell(1318.51, 0.9, ratio=2.0, index=1.0, amp_tau=0.24, index_tau=0.08, attack=0.003), ns(0.074))
    st = pan(b1, -0.3) + pan(b2 * 0.8, 0.3)
    for tt in rng.uniform(0.02, 0.3, 4):
        f = float(rng.uniform(3000, 5000))
        k = np.zeros(n)
        place(k, modal([f], [0.05], [1.0], rng=rng), ns(tt))
        st = st + pan(k * 0.12, float(rng.uniform(-0.8, 0.8)))
    return space(st, rng, rt60=0.7, wet_db=-12, stereo=True, hf_ratio=0.7)


@sound("Combat", 1.2, target=M, use="Character death: body collapses to the ground")
def death_thud(rng, n):
    k1 = 0.004
    k2 = 0.2
    th1 = thump(n, k1, 120, 60, 15, 60, amp=1.0, drive_db=6)
    b1 = burst(rng, n, k1, 1, 30, "pink", (("lp", 700), ("hp", 60)), amp=1.0)
    th2 = thump(n, k2, 100, 45, 25, 110, amp=1.0, drive_db=6)
    b2 = burst(rng, n, k2, 2, 70, "pink", (("lp", 500), ("hp", 50)), amp=1.0, drive_db=4)
    slap = burst(rng, n, k2, 1, 25, "pink", (("bp", 900, 0.8),), amp=1.0)
    cloth = burst(rng, n, k2 - 0.01, 8, 60, "pink", (("bp", 3000, 0.8),), amp=1.0)
    dust = burst(rng, n, k2 + 0.02, 30, 250, "pink", (("lp", 2000), ("hp", 300)), amp=1.0)
    peb = debris(rng, n, 4, k2 + 0.03, 0.7, size=(0.0, 0.3))
    y = mix(th1 * G(-13), b1 * G(-6), th2 * G(-9), b2, slap * G(-5), cloth * G(-12), dust * G(-18), norm_peak(peb) * G(-18))
    y = transient_shape(y, 2.0)
    y = early_reflections(y, rng, level_db=-10)
    return space(y, rng, rt60=0.6, wet_db=-18)


@sound("Combat", 2.5, stereo=True, target=-15.0, clip_db=0.0, use="Level up: rising chime flourish (2D, player feedback)")
def level_up(rng, n):
    T = n / SR
    st = np.zeros((2, n))
    notes = [(0.0, 587.33), (0.06, 739.99), (0.12, 880.0), (0.18, 1174.66)]
    for i, (tt, f) in enumerate(notes):
        b = np.zeros(n)
        place(b, fm_bell(f, T - tt, ratio=2.0, index=1.5, amp_tau=0.5, index_tau=0.1) +
              0.4 * fm_bell(f * 2.0, T - tt, ratio=3.0, index=0.8, amp_tau=0.35, index_tau=0.06), ns(tt + 0.004))
        st += pan(b, -0.45 + 0.3 * i)
    for f, p in ((1479.98, -0.3), (1760.0, 0.3), (2349.32, 0.0)):
        b = np.zeros(n)
        place(b, fm_bell(f, T - 0.3, ratio=2.0, index=1.2, amp_tau=0.6, index_tau=0.12), ns(0.3))
        st += pan(b * 0.6, p)
    sp = np.zeros((2, n))
    for tt in np.sort(0.3 + 1.3 * rng.random(40) ** 1.6):
        f = float(rng.uniform(3000, 8000))
        k = np.zeros(n)
        place(k, modal([f, f * 2.0], [rng.uniform(0.04, 0.12), 0.03], [1.0, 0.3], rng=rng), ns(tt))
        sp += pan(k * rng.uniform(0.3, 1.0), float(rng.uniform(-0.9, 0.9)))
    e_p = env([(0.3, 0.0), (0.8, 1.0), (T - 0.15, 0.0)], n, [1.5, -2.5])
    pad = np.zeros(n)
    for f in (293.66, 440.0, 587.33, 739.99):
        pad += saw(f, n, rng.random()) + saw(f * 1.004, n, rng.random())
    from dsp import eq
    pad = norm_peak(eq(pad, ("lp4", 2000)) * e_p)
    wh = whoosh(rng, n, 0.0, 0.4, [(0.0, 400), (0.35, 4000)], q=1.2, peak_at=0.85, amp=1.0, rise_curve=2.0)
    y = norm_peak(st) + norm_peak(sp) * G(-10) + pan(pad, -0.2) * G(-14) + pan(pad, 0.2) * G(-14) + pan(wh, 0.0) * G(-12)
    return space(y, rng, rt60=1.5, wet_db=-8, stereo=True, hf_ratio=0.6)
