"""Fire magic. Palette: turbulent roar (brown/pink noise with layered random AM, saturated), flame
flutter (8-20 Hz), granular crackle / wood pops, high hiss, low whumps, plus a thin tonal shimmer for
the mana-gathering gestures. Big events add a sub sweep, early reflections and outdoor slap echoes."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, db2lin, early_reflections, env, eq, noise, norm_peak, norm_rms, periodic_lfo,
                 perc, saturate, slap_echoes, smooth_random, transient_shape, tvec)
from layers import (burst, click, crack, crackle, debris, drone, fpath, mix, moving_noise, roar, rumble,
                    shimmer, space, sub_boom, thump, turbulence, whoosh)
from reg import LOOP_MED, LOOP_SMALL, MP, XL, L, M, S, sound

G = lambda d: float(db2lin(d))  # noqa: E731


def _swirl_fc(n, T, f_from, f_to, t_peak, rate_from, rate_to, depth, phase0=0.0):
    t = tvec(n)
    rate = rate_from + (rate_to - rate_from) * np.clip(t / T, 0, 1) ** 1.5
    ph = TAU * np.cumsum(rate) / SR + phase0
    base = f_from * (f_to / f_from) ** (np.clip(t / t_peak, 0, 1) ** 1.3)
    return base * (1.0 + depth * np.sin(ph))


@sound("Fire", 0.7, target=S, use="Fire spells wind-up (Fire_Fireball, Fire_FlameWave, Fire_Inferno, BanditMage_FireBolt): mana gathers into a swirling flame at the hand")
def fire_gather(rng, n):
    T = n / SR
    e_sw = env([(0.0, 0.0), (0.46, 1.0), (0.54, 0.45), (T - 0.03, 0.0)], n, [3.0, -2.0, -2.0])
    sw1 = moving_noise(rng, n, e_sw, "bp", _swirl_fc(n, T, 450, 2100, 0.5, 4, 14, 0.28), 2.2, "pink")
    sw2 = moving_noise(rng, n, e_sw, "bp", _swirl_fc(n, T, 900, 4200, 0.5, 4, 14, 0.28, np.pi), 3.0, "white", amp=G(-5))
    crk = crackle(rng, n, lambda t: 8 + 170 * np.clip(t / 0.5, 0, 1) ** 2, 0.0, 0.6,
                  amp_fn=lambda t: 0.3 + 0.7 * np.clip(t / 0.6, 0, 1), band=(1800, 9000), pop_prob=0.15)
    e_sh = env([(0.0, 0.0), (0.42, 1.0), (0.56, 0.3), (T - 0.03, 0.0)], n, [2.0, -2.0, -2.0])
    sh = shimmer(rng, n, e_sh, [587.3, 880.0, 1174.7, 1318.5], trem_rate=(8, 14), amp=G(-13),
                 glide=lambda t: 2.0 ** ((5 / 12) * np.clip(t / 0.6, 0, 1) ** 1.5))
    puff = burst(rng, n, 0.48, attack_ms=6, tau_ms=55, color="pink", filters=(("lp", 1800), ("hp", 120)), amp=G(-3), drive_db=6)
    th = thump(n, 0.48, 170, 70, 25, 50, amp=G(-9), drive_db=4)
    rr = roar(rng, n, env([(0.44, 0.0), (0.5, 1.0), (T - 0.04, 0.0)], n, [2.0, -3.5]), lp=1400, amp=G(-7))
    y = mix(sw1, sw2, norm_peak(crk) * G(-11), sh, puff, th, rr)
    return early_reflections(y, rng, level_db=-12)


@sound("Fire", 1.0, target=MP, use="Fire_Fireball / BanditMage_FireBolt / Barrage_FireBurst cast: ignition whump, whoosh and roar as the projectile leaves")
def fireball_launch(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=G(-9), hp=2500)
    th = thump(n, t0, 150, 55, 30, 110, amp=1.0, drive_db=8)
    ign = burst(rng, n, t0, 3, 70, "pink", (("lp", 900), ("hp", 60)), amp=G(-2), drive_db=6)
    wh = whoosh(rng, n, 0.01, 0.95, [(0.0, 2600), (0.12, 2000), (0.9, 420)], q=1.3, peak_at=0.13, amp=G(-3), flutter=0.3)
    e_r = env([(0.0, 0.0), (0.03, 1.0), (0.25, 0.55), (0.97, 0.0)], n, [-2.0, -1.5, -3.0])
    rr = roar(rng, n, e_r, lp=fpath([(0.0, 2600), (0.9, 650)]), amp=G(-2), drive_db=8)
    crk = crackle(rng, n, lambda t: 80 * np.exp(-t / 0.35) + 4, 0.01, 0.95, amp_fn=lambda t: np.exp(-t / 0.5), band=(1500, 8000))
    hs = moving_noise(rng, n, perc(n, 0.01, 0.01, 0.18) * turbulence(rng, n, ((14, 0.6),)), "hp", 4000, 0.7, "white", amp=G(-17))
    y = mix(ck, th, ign, wh, rr, norm_peak(crk) * G(-14), hs)
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, level_db=-10)


@sound("Fire", 2.0, loop=True, target=LOOP_MED, use="Fireball / fire bolt projectile in flight: loop attached to the projectile")
def fireball_travel_loop(rng, n):
    T = n / SR
    pulse = 1.0 + 0.12 * periodic_lfo(n, 4)  # gentle 2 Hz surge, whole cycles per loop
    rr = roar(rng, n, pulse, lp=1600, turb=((4, 0.3), (11, 0.3), (25, 0.2)), drive_db=6, circular=True, hp=70)
    fl = eq(noise(n, rng, "pink"), ("bp", 900, 0.8), circular=True)
    fl = norm_rms(fl) * turbulence(rng, n, ((12, 0.6), (30, 0.3))) * pulse
    crk = crackle(rng, n, 45.0, 0.0, T, band=(1500, 9000), pop_prob=0.2, wrap=True)
    hs = eq(noise(n, rng, "white"), ("hp", 5000), ("lp", 12000), circular=True) * turbulence(rng, n, ((20, 0.6),))
    return mix(rr, norm_peak(fl) * G(-3), norm_peak(crk) * G(-9), norm_peak(hs) * G(-18))


@sound("Fire", 2.5, target=L, max_gr=5.0, use="Fireball / fire bolt / fire burst impact: explosion boom with a crackling debris tail")
def fire_explosion(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=1.0, decay_ms=1.5, hp=1500)
    cr = crack(rng, n, t0, amp=G(-4), fc=2200, q=0.7, tau_ms=12, drive_db=10)
    sb = sub_boom(rng, n, t0, f0=90, f1=34, sweep_s=0.12, tau_s=0.35, amp=G(-8), drive_db=5)
    th = thump(n, t0, 170, 60, 20, 80, amp=G(-4), drive_db=8)
    body = moving_noise(rng, n, perc(n, t0, 0.002, 0.16), "lp", fpath([(0.0, 9000), (0.1, 4000), (0.6, 900)]), 0.8, "pink",
                        pre=(("hp", 90),))
    body = saturate(body, 9)
    e_r = env([(t0, 0.0), (0.03, 1.0), (0.25, 0.7), (1.6, 0.0)], n, [-2.0, -1.0, -2.5])
    rr = roar(rng, n, e_r, lp=fpath([(0.0, 2600), (1.5, 600)]), drive_db=8, amp=G(-2), hp=70)
    rum = rumble(rng, n, env([(t0, 0.0), (0.06, 1.0), (2.3, 0.0)], n, [-2.0, -3.0]), lp=140, amp=G(-11))
    crk = crackle(rng, n, lambda t: 110 * np.exp(-(t - 0.1) / 0.7) + 7, 0.08, 2.45,
                  amp_fn=lambda t: np.exp(-(t - 0.08) / 1.5), band=(1200, 8000), pop_prob=0.3)
    deb = debris(rng, n, 10, 0.15, 1.6, size=(0.0, 0.5), dist=lambda u: u ** 1.8)
    y = mix(ck, cr, sb, th, body * G(-1), rr, rum, norm_peak(crk) * G(-11), norm_peak(deb) * G(-17))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-9)
    y = slap_echoes(y, (0.12, 0.24, 0.39), (-15, -19, -24), lp=1800)
    return space(y, rng, rt60=1.4, wet_db=-17, hf_ratio=0.35)


@sound("Fire", 1.6, target=MP, use="Fire_FlameWave cast (also Wyrm_FireBreath): a rolling wall of fire surging forward")
def flamewave_cast(rng, n):
    T = n / SR
    t0 = 0.004
    th = thump(n, t0, 120, 50, 35, 120, amp=G(-2), drive_db=6)
    ign = burst(rng, n, t0, 4, 80, "pink", (("lp", 1200), ("hp", 70)), amp=G(-3), drive_db=6)
    t = tvec(n)
    rolls = 0.55 + 0.45 * (np.exp(-((t - 0.28) / 0.1) ** 2) + np.exp(-((t - 0.66) / 0.12) ** 2) + 0.8 * np.exp(-((t - 1.02) / 0.13) ** 2))
    e_r = env([(0.0, 0.0), (0.12, 1.0), (0.9, 0.8), (T, 0.0)], n, [-2.0, 0.0, -2.0]) * rolls
    rr = roar(rng, n, e_r, lp=fpath([(0.0, 1500), (0.3, 3800), (0.7, 2600), (1.05, 3200), (1.6, 700)]),
              turb=((2.5, 0.3), (8, 0.35), (20, 0.2)), drive_db=9, mid_db=-2)
    wh = whoosh(rng, n, 0.02, 1.5, [(0.0, 1600), (1.2, 380)], q=1.0, peak_at=0.25, amp=G(-6))
    crk = crackle(rng, n, lambda tt: 90 * np.exp(-tt / 0.6) + 8, 0.02, 1.55, amp_fn=lambda tt: np.interp(tt, t, e_r),
                  band=(1300, 8500), pop_prob=0.3)
    rum = rumble(rng, n, e_r, lp=120, amp=G(-7))
    y = mix(th, ign, rr, wh, norm_peak(crk) * G(-13), rum)
    y = early_reflections(y, rng, level_db=-10)
    return space(y, rng, rt60=1.0, wet_db=-18, hf_ratio=0.4)


@sound("Fire", 3.0, loop=True, target=LOOP_SMALL, use="Burning ground left by Fire_FlameWave / Fire_Inferno: looping fire bed")
def flame_burn_loop(rng, n):
    T = n / SR
    low = eq(noise(n, rng, "brown"), ("lp", 420), ("hp", 85), circular=True)
    low = norm_rms(low) * turbulence(rng, n, ((1.5, 0.35), (4.0, 0.25)))
    lick = eq(noise(n, rng, "pink"), ("bp", 700, 0.7), circular=True)
    lick = norm_rms(lick) * turbulence(rng, n, ((8.0, 0.65), (17.0, 0.35)))
    crk = crackle(rng, n, 26.0, 0.0, T, band=(1500, 9000), pop_prob=0.25, wrap=True)
    # a few bigger wood pops with a snappy tail
    pops = crackle(rng, n, 1.7, 0.0, T, band=(900, 4000), tick_ms=(1.5, 4.0), pop_prob=0.7, pop_band=(300, 900),
                   pop_ms=(3.0, 9.0), alpha=4.0, wrap=True)
    hs = eq(noise(n, rng, "white"), ("hp", 6000), ("lp", 12000), circular=True) * turbulence(rng, n, ((15, 0.6),))
    y = mix(norm_peak(low) * G(-9), norm_peak(lick) * G(-2), norm_peak(crk) * G(-3), norm_peak(pops) * G(-2), norm_peak(hs) * G(-17))
    return saturate(norm_peak(y), 3.0)


@sound("Fire", 1.5, target=M, use="Fire_Inferno charge: rising rumble + spinning magic-circle hum; start inferno_eruption as this ends")
def inferno_charge(rng, n):
    T = n / SR
    t = tvec(n)
    e_up = env([(0.0, 0.0), (1.3, 1.0), (T, 0.25)], n, [2.5, -1.5])
    rum = moving_noise(rng, n, e_up * turbulence(rng, n, ((3.0, 0.3), (7.0, 0.3))), "lp",
                       fpath([(0.0, 110), (1.4, 520)]), 0.9, "brown", order=4, pre=(("hp", 50),))
    gl = 2.0 ** ((2.0 / 12.0) * np.clip(t / T, 0, 1) ** 1.5)
    dr = drone(rng, n, [55.0 * gl, 82.41 * gl, 110.0 * gl], env([(0.0, 0.0), (0.3, 0.4), (1.3, 1.0), (T, 0.25)], n, [1.0, 1.0, -1.5]),
               detune_cents=9, lp=fpath([(0.0, 250), (1.4, 1900)]))
    trem = TAU * np.cumsum(6.0 + 12.0 * (t / T) ** 1.4) / SR
    ring = (0.6 * np.sin(TAU * 880.0 * gl * t) + 0.4 * np.sin(TAU * 1320.0 * gl * t) + 0.25 * np.sin(TAU * 1761.0 * gl * t))
    ring *= (0.55 + 0.45 * np.sin(trem)) * env([(0.0, 0.0), (0.4, 0.5), (1.3, 1.0), (T, 0.2)], n, [2.0, 1.0, -1.5])
    crk = crackle(rng, n, lambda tt: 5 + 150 * (tt / T) ** 2, 0.0, T, amp_fn=lambda tt: 0.3 + 0.7 * tt / T, band=(1500, 8500), pop_prob=0.25)
    rs = moving_noise(rng, n, env([(0.0, 0.0), (1.3, 1.0), (T, 0.15)], n, [3.0, -1.5]), "bp", fpath([(0.0, 400), (1.3, 4500)]), 1.2, "pink")
    y = mix(norm_peak(rum) * G(-3), dr * G(-3), norm_peak(ring) * G(-8), norm_peak(crk) * G(-9), rs * G(-3))
    return saturate(norm_peak(y), 3.0)


@sound("Fire", 3.5, target=XL, max_gr=5.0, use="Fire_Inferno impact: huge pillar of fire erupting from the ground")
def inferno_eruption(rng, n):
    T = n / SR
    t0 = 0.05
    pre = whoosh(rng, n, 0.0, 0.07, [(0.0, 500), (0.07, 2500)], q=1.0, peak_at=0.95, amp=G(-10), rise_curve=4.0)
    ck = click(rng, n, t0, amp=1.0, decay_ms=2.0, hp=1200)
    cr = crack(rng, n, t0, amp=G(-3), fc=1600, q=0.6, tau_ms=16, drive_db=12)
    sb = sub_boom(rng, n, t0, f0=70, f1=26, sweep_s=0.2, tau_s=0.9, amp=G(-0.5), drive_db=6)
    th = thump(n, t0, 150, 50, 25, 130, amp=G(-3), drive_db=8)
    blast = moving_noise(rng, n, perc(n, t0, 0.002, 0.14), "lp", fpath([(0.0, 10000), (0.15, 3500), (0.7, 700)]), 0.8, "pink")
    blast = saturate(blast, 10)
    e_p = env([(t0, 0.0), (t0 + 0.06, 1.0), (0.9, 0.95), (2.4, 0.55), (3.35, 0.0)], n, [-2.0, 0.0, -1.0, -2.0])
    pillar = roar(rng, n, e_p, lp=fpath([(0.0, 3000), (0.9, 2400), (2.5, 1100), (3.4, 500)]),
                  turb=((3.0, 0.3), (9.0, 0.35), (18.0, 0.25)), drive_db=11, mid_db=-1, air_db=-14)
    up = whoosh(rng, n, 0.08, 1.4, [(0.0, 300), (1.3, 3200)], q=1.4, peak_at=0.55, amp=G(-5), rise_curve=1.0)
    crk = crackle(rng, n, lambda tt: 160 * np.exp(-(tt - 0.1) / 1.0) + 10, 0.07, 3.4,
                  amp_fn=lambda tt: np.clip(np.interp(tt, tvec(n), e_p), 0.15, 1), band=(1200, 8500), pop_prob=0.35)
    rum = rumble(rng, n, env([(t0, 0.0), (0.2, 1.0), (3.4, 0.0)], n, [-2.0, -1.5]), lp=110, amp=G(-5))
    tone = drone(rng, n, [55.0, 82.41, 110.0], env([(t0, 0.0), (0.4, 1.0), (2.0, 0.6), (3.3, 0.0)], n, [-2.0, 0.0, -2.0]),
                 detune_cents=12, lp=700)
    deb = debris(rng, n, 16, 0.25, 2.6, size=(0.0, 0.6), dist=lambda u: u ** 1.5)
    y = mix(pre, ck, cr, sb * G(-3), th, blast, pillar, up, norm_peak(crk) * G(-10), rum * G(-3), tone * G(-15), norm_peak(deb) * G(-17))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-9)
    y = slap_echoes(y, (0.14, 0.29, 0.47), (-15, -19, -24), lp=1600)
    return space(y, rng, rt60=1.8, wet_db=-15, hf_ratio=0.35)
