"""Water magic. Palette: bubbles (van den Doel resonant chirps), droplet plinks, 'rush' (band-limited
noise under fast 20-60 Hz turbulence), splash bursts with a low slap, foam fizz (tiny dense grains),
pressurised hiss, glassy cool shimmer (E major colour) and, for the water dragon, a formant-filtered
growl riding on the rushing water."""
from __future__ import annotations

import numpy as np

from dsp import (SR, TAU, bubble, db2lin, early_reflections, env, eq, noise, norm_peak, norm_rms, ns,
                 periodic_lfo, perc, place, saturate, saw, slap_echoes, smooth_random, sweep, transient_shape,
                 tvec, formant_filter)
from layers import (bubbles, burst, click, crackle, droplets, fpath, mix, moving_noise, rumble, shimmer, space,
                    sub_boom, thump, turbulence, whoosh)
from reg import L, LOOP_BIG, M, S, XL, sound

G = lambda d: float(db2lin(d))  # noqa: E731


def rush(rng, n, e, lo=350.0, hi=5000.0, lp=None, circular=False, fast=((28.0, 0.45), (60.0, 0.25)), color="pink"):
    """Rushing / churning water: band-limited noise under fast turbulence; lp may move (callable)."""
    y = eq(noise(n, rng, color), ("hp", lo), ("lp", hi), circular=circular)
    if lp is not None:
        y = sweep(y, "lp", lp, 0.8, circular=circular)
    y = norm_rms(y) * turbulence(rng, n, fast)
    return norm_peak(y * e)


def _swirl(n, T, f_from, f_to, t_peak, r0, r1, depth, ph0=0.0):
    t = tvec(n)
    rate = r0 + (r1 - r0) * np.clip(t / T, 0, 1) ** 1.5
    base = f_from * (f_to / f_from) ** (np.clip(t / t_peak, 0, 1) ** 1.3)
    return base * (1.0 + depth * np.sin(TAU * np.cumsum(rate) / SR + ph0))


@sound("Water", 0.7, target=S, use="Water spells wind-up (Water_WaterBullet, Water_WaterDragon, Water_Flood): water gathers and swirls at the hand")
def water_gather(rng, n):
    T = n / SR
    e_sw = env([(0.0, 0.0), (0.44, 1.0), (0.52, 0.5), (T - 0.03, 0.0)], n, [2.5, -2.0, -2.0])
    sw = moving_noise(rng, n, e_sw * turbulence(rng, n, ((25, 0.4),)), "bp", _swirl(n, T, 500, 1500, 0.55, 5, 12, 0.3), 3.0, "pink")
    sw2 = moving_noise(rng, n, e_sw, "bp", _swirl(n, T, 1300, 3600, 0.55, 5, 12, 0.3, np.pi), 4.0, "white", amp=G(-8))
    bub = bubbles(rng, n, lambda t: 40 + 380 * np.clip(t / 0.5, 0, 1) ** 2, 0.0, 0.55, f_lo=500, f_hi=2600,
                  rise=(0.2, 0.6), decay_mult=(1.5, 3), amp_fn=lambda t: 0.3 + 0.7 * np.clip(t / 0.5, 0, 1))
    e_sh = env([(0.0, 0.0), (0.42, 1.0), (0.56, 0.3), (T - 0.03, 0.0)], n, [2.0, -2.0, -2.0])
    sh = shimmer(rng, n, e_sh, [659.3, 987.8, 1318.5, 1480.0], trem_rate=(6, 10), amp=G(-12),
                 glide=lambda t: 2.0 ** ((3 / 12) * np.clip(t / 0.6, 0, 1)))
    gl = np.zeros(n)
    place(gl, bubble(220.0, rise=0.5, decay_mult=1.5), ns(0.47))
    spl = burst(rng, n, 0.48, 2, 35, "white", (("bp", 2500, 0.8),), amp=G(-10))
    y = mix(sw, sw2, norm_peak(bub) * G(-4), sh, norm_peak(gl) * G(-3), spl)
    return early_reflections(y, rng, level_db=-12)


@sound("Water", 0.6, target=M, use="Water_WaterBullet / Barrage_WaterCannon cast: pressurised jet of water fired from the hand")
def water_bullet_launch(rng, n):
    t0 = 0.004
    t = tvec(n)
    pssh = moving_noise(rng, n, perc(n, t0, 0.002, 0.16), "bp", fpath([(0.0, 5500), (0.4, 1300)]), 1.3, "white")
    jet = moving_noise(rng, n, env([(0.0, 0.0), (0.02, 1.0), (0.3, 0.3), (0.58, 0.0)], n, [-2.0, -1.5, -2.0]),
                       "bp", fpath([(0.0, 3200), (0.55, 900)]), 2.5, "pink")
    f = 140.0 + 510.0 * np.exp(-np.maximum(t - t0, 0) / 0.03)
    thwip = np.sin(TAU * np.cumsum(f) / SR) * perc(n, t0, 0.001, 0.06)
    th = thump(n, t0, 150, 70, 20, 50, amp=G(-6), drive_db=4)
    drops = droplets(rng, n, lambda tt: 140 * np.exp(-tt / 0.1), 0.02, 0.45, f_lo=2000, f_hi=6000)
    bub = bubbles(rng, n, lambda tt: 200 * np.exp(-tt / 0.08), 0.0, 0.3, f_lo=700, f_hi=3000, rise=(0.4, 1.2))
    wh = whoosh(rng, n, 0.015, 0.55, [(0.0, 2400), (0.5, 600)], q=1.6, peak_at=0.15, amp=G(-5))
    y = mix(pssh, jet * G(-3), norm_peak(thwip) * G(-5), th * G(-2), norm_peak(drops) * G(-11), norm_peak(bub) * G(-10), wh * G(1))
    y = transient_shape(y, 3.0)
    return early_reflections(y, rng, level_db=-11)


@sound("Water", 1.0, target=M, use="Water_WaterBullet impact: splash with bubbles and falling droplets")
def water_splash(rng, n):
    t0 = 0.004
    ck = click(rng, n, t0, amp=G(-6), hp=3000)
    slap = burst(rng, n, t0, 0.8, 22, "pink", (("lp", 3500), ("hp", 150)), amp=1.0)
    th = thump(n, t0, 160, 70, 15, 55, amp=G(-5), drive_db=4)
    body = moving_noise(rng, n, perc(n, t0 + 0.002, 0.004, 0.24), "lp", fpath([(0.0, 9000), (0.5, 2200)]), 0.8, "white",
                        pre=(("hp", 300),))
    wash = rush(rng, n, perc(n, 0.01, 0.02, 0.3), 300, 4500)
    bub = bubbles(rng, n, lambda t: 500 * np.exp(-(t - 0.01) / 0.12), 0.005, 0.5, f_lo=300, f_hi=3000, rise=(0.2, 1.0),
                  decay_mult=(1.5, 4))
    drops = droplets(rng, n, lambda t: 60 * np.exp(-(t - 0.15) / 0.3), 0.12, 0.95, f_lo=1400, f_hi=4800)
    y = mix(ck * G(-2), slap * G(-1), th * G(-2), body, wash * G(-4), norm_peak(bub) * G(-5), norm_peak(drops) * G(-11))
    y = transient_shape(y, 2.0)
    return early_reflections(y, rng, level_db=-10)


@sound("Water", 2.2, target=L, use="Water_WaterDragon summon: a torrent rises and takes shape with a low draconic roar")
def water_dragon_rise(rng, n):
    T = n / SR
    t = tvec(n)
    e_w = env([(0.0, 0.0), (1.4, 1.0), (1.7, 0.8), (T - 0.05, 0.0)], n, [2.0, 0.0, -3.0])
    water = rush(rng, n, e_w, 300, 7000, lp=fpath([(0.0, 1400), (1.4, 6500), (T, 2500)]))
    up = whoosh(rng, n, 0.2, 1.5, [(0.0, 300), (1.4, 2600)], q=1.5, peak_at=0.75, amp=G(-5), rise_curve=1.5)
    # draconic growl: jittered saw + sub-octave + breath noise through moving vowel formants
    f0 = (70.0 + 25.0 * np.clip((t - 0.45) / 1.2, 0, 1)) * (1.0 + 0.04 * smooth_random(n, rng, 8.0))
    src = saw(f0, n, 0.1) + 0.5 * saw(f0 * 0.5, n, 0.4) + 0.35 * noise(n, rng, "pink")
    src = src * (1.0 + 0.45 * smooth_random(n, rng, 32.0))  # vocal-fry growl
    vow = formant_filter(src, [(fpath([(0.4, 520), (1.5, 700), (T, 600)]), 4.0, 0.0),
                               (fpath([(0.4, 1000), (1.5, 1250), (T, 1100)]), 5.0, -3.0),
                               (2500.0, 6.0, -8.0)], floor=0.04)
    e_g = env([(0.45, 0.0), (1.3, 1.0), (1.65, 0.85), (T - 0.08, 0.0)], n, [1.5, 0.0, -3.0])
    growl = saturate(norm_peak(vow * e_g), 10.0)
    sub = np.sin(TAU * 45.0 * t) * env([(0.3, 0.0), (1.4, 1.0), (T, 0.0)], n, [1.5, -2.0])
    rum = rumble(rng, n, e_w, lp=120, amp=1.0)
    foam = bubbles(rng, n, lambda tt: 30 + 400 * np.clip(tt / 1.4, 0, 1) * np.clip((T - 0.1 - tt) / 0.5, 0, 1), 0.1, T - 0.15,
                   f_lo=400, f_hi=3000, rise=(0.3, 1.0))
    spray = droplets(rng, n, lambda tt: 90 * np.exp(-((tt - 1.5) / 0.3) ** 2), 1.0, T - 0.2, f_lo=1800, f_hi=5500)
    y = mix(water, up, growl * G(-3), norm_peak(sub) * G(-15), rum * G(-13), norm_peak(foam) * G(-11), norm_peak(spray) * G(-13))
    y = early_reflections(y, rng, level_db=-10)
    return space(y, rng, rt60=1.0, wet_db=-15, hf_ratio=0.45)


@sound("Water", 2.8, target=XL, max_gr=5.0, use="Water_WaterDragon impact: the dragon crashes down, huge splash + shockwave")
def water_dragon_impact(rng, n):
    T = n / SR
    t0 = 0.004
    ck = click(rng, n, t0, amp=G(-4), hp=2500)
    slap = burst(rng, n, t0, 0.8, 30, "pink", (("lp", 3000), ("hp", 120)), amp=1.0, drive_db=6)
    sb = sub_boom(rng, n, t0, f0=75, f1=26, sweep_s=0.18, tau_s=0.7, amp=G(-7), drive_db=5)
    whoomp = burst(rng, n, t0, 10, 150, "brown", (("lp", 220), ("hp", 30)), amp=G(-3))
    splash = moving_noise(rng, n, perc(n, t0 + 0.003, 0.003, 0.35), "lp", fpath([(0.0, 12000), (1.0, 1500)]), 0.8, "white",
                          pre=(("hp", 200),))
    splash = saturate(splash, 6)
    wash = rush(rng, n, perc(n, 0.02, 0.05, 0.8), 250, 3000)
    bub = bubbles(rng, n, lambda tt: 900 * np.exp(-tt / 0.35) + 20, 0.03, 1.9, f_lo=150, f_hi=2500, rise=(0.2, 1.0))
    rain = droplets(rng, n, lambda tt: 140 * np.exp(-(tt - 0.3) / 0.6) + 5, 0.3, T - 0.15, f_lo=1500, f_hi=5500,
                    amp_fn=lambda tt: np.exp(-(tt - 0.3) / 1.2))
    rum = rumble(rng, n, env([(t0, 0.0), (0.1, 1.0), (T - 0.1, 0.0)], n, [-2.0, -2.5]), lp=110, amp=1.0)
    y = mix(ck * G(-2), slap, sb * G(-5), whoomp * G(-6), splash, wash * G(-1), norm_peak(bub) * G(-7), norm_peak(rain) * G(-12), rum * G(-13))
    y = transient_shape(y, 3.0)
    y = early_reflections(y, rng, level_db=-9)
    y = slap_echoes(y, (0.13, 0.27, 0.43), (-15, -19, -24), lp=1800)
    return space(y, rng, rt60=2.0, wet_db=-15, hf_ratio=0.4)


@sound("Water", 3.0, loop=True, target=LOOP_BIG, use="Water_Flood wave travelling (also Race_Sea_TideRush): looping rushing water mass")
def flood_wave_loop(rng, n):
    T = n / SR
    surge = 1.0 + 0.3 * periodic_lfo(n, 1) + 0.12 * periodic_lfo(n, 3, phase=0.3)
    bed = rush(rng, n, surge, 250, 5500, circular=True)
    low = eq(noise(n, rng, "brown"), ("lp", 280), ("hp", 40), circular=True)
    low = norm_peak(norm_rms(low) * turbulence(rng, n, ((1.5, 0.3), (5.0, 0.2))) * surge)
    hiss = eq(noise(n, rng, "white"), ("hp", 3000), ("lp", 11000), circular=True)
    hiss = norm_peak(norm_rms(hiss) * turbulence(rng, n, ((30.0, 0.5),)) * surge)
    bub = bubbles(rng, n, 140.0, 0.0, T, f_lo=250, f_hi=2000, rise=(0.2, 1.0), wrap=True)
    crests = np.zeros(n)
    for tc in rng.uniform(0.0, T, 6):
        b = noise(ns(0.25), rng, "white") * perc(ns(0.25), 0.0, 0.01, 0.06)
        b = eq(b, ("bp", float(rng.uniform(1500, 4000)), 0.9))
        place(crests, norm_peak(b) * rng.uniform(0.5, 1.0), ns(tc), wrap=True)
    y = mix(bed, low * G(-3), hiss * G(-12), norm_peak(bub) * G(-9), norm_peak(crests) * G(-9))
    return saturate(norm_peak(y), 2.0)


@sound("Water", 2.2, target=L, use="Water_Flood crash: a wall of water slams into the ground")
def flood_crash(rng, n):
    T = n / SR
    tc = 0.3
    build = rush(rng, n, env([(0.0, 0.0), (tc, 1.0), (tc + 0.05, 0.0)], n, [3.0, -2.0]), 300, 7000,
                 lp=fpath([(0.0, 1000), (tc, 6000)]))
    slap = burst(rng, n, tc, 1.0, 35, "pink", (("lp", 3200), ("hp", 100)), amp=1.0, drive_db=6)
    th = thump(n, tc, 110, 45, 30, 120, amp=G(-8), drive_db=6)
    blast = moving_noise(rng, n, perc(n, tc, 0.004, 0.3), "lp", fpath([(tc, 11000), (tc + 0.8, 2000)]), 0.8, "white",
                         pre=(("hp", 180),))
    blast = saturate(blast, 6)
    wash = rush(rng, n, perc(n, tc + 0.02, 0.06, 0.7), 200, 6000, lp=fpath([(tc, 6000), (T, 800)]))
    fizz = crackle(rng, n, lambda t: 900 * np.exp(-(t - tc) / 0.6), tc, T, band=(3000, 11000), tick_ms=(0.08, 0.35),
                   pop_prob=0.0, amp_fn=lambda t: np.exp(-(t - tc) / 0.9))
    bub = bubbles(rng, n, lambda t: 600 * np.exp(-(t - tc) / 0.4), tc, T - 0.1, f_lo=200, f_hi=2500)
    drops = droplets(rng, n, lambda t: 80 * np.exp(-(t - tc - 0.3) / 0.5), tc + 0.2, T - 0.1, f_lo=1500, f_hi=5000)
    rum = rumble(rng, n, env([(tc, 0.0), (tc + 0.1, 1.0), (T, 0.0)], n, [-2.0, -2.0]), lp=120, amp=1.0)
    y = mix(build * G(-5), slap, th, blast, wash * G(-1), norm_peak(fizz) * G(-10), norm_peak(bub) * G(-9),
            norm_peak(drops) * G(-13), rum * G(-13))
    y = transient_shape(y, 2.5)
    y = early_reflections(y, rng, level_db=-9)
    return space(y, rng, rt60=1.5, wet_db=-16, hf_ratio=0.4)
