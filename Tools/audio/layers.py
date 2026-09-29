"""Reusable sound-design layers built on dsp.py.

Every function returns a mono float array of length n (unless noted) so recipes can stack layers:
transient click + low thump + body + tail, tonal shimmer + moving noise, etc. Times are seconds from
the start of the file; `wrap=True` places events circularly (for seamless loops).
"""
from __future__ import annotations

import math

import numpy as np

from dsp import (SR, TAU, at, bubble, db2lin, droplet, env, eq, fade, make_ir, modal, noise, norm_peak,
                 norm_rms, ns, perc, place, poisson_times, reverb, saturate, sine, smooth_random, sweep,
                 tvec)


def fpath(points):
    """[(t, hz), ...] -> callable(t) interpolating log-frequency (holds the ends)."""
    ts = np.array([p[0] for p in points], dtype=np.float64)
    ls = np.log(np.array([p[1] for p in points], dtype=np.float64))
    return lambda t: np.exp(np.interp(t, ts, ls))


def vpath(points):
    """[(t, value), ...] -> callable(t) linear interpolation."""
    ts = np.array([p[0] for p in points], dtype=np.float64)
    vs = np.array([p[1] for p in points], dtype=np.float64)
    return lambda t: np.interp(t, ts, vs)


def mix(*layers):
    out = None
    for l in layers:
        if l is None:
            continue
        out = np.array(l, dtype=np.float64) if out is None else out + l
    return out


# --------------------------------------------------------------------------------------------- transients
def click(rng, n, t0=0.004, amp=1.0, decay_ms=1.2, hp=2000.0, lp=16000.0):
    """Broadband transient: impulse + a millisecond of high-passed noise."""
    L = ns(decay_ms * 1e-3 * 9) + 96
    t = tvec(L)
    b = rng.standard_normal(L) * np.exp(-t / (decay_ms * 1e-3))
    b[0] += 4.0
    b[1] -= 2.0
    b = eq(b, ("hp", hp), ("lp", lp))
    out = np.zeros(n)
    place(out, norm_peak(b) * amp, ns(t0))
    return out


def crack(rng, n, t0=0.004, amp=1.0, fc=1800.0, q=0.9, tau_ms=9.0, drive_db=8.0, hp=400.0):
    """Sharp splitting 'crack' (rock split, whip, bone snap): saturated band-limited noise burst."""
    L = ns(tau_ms * 1e-3 * 8) + 64
    t = tvec(L)
    b = rng.standard_normal(L) * np.exp(-t / (tau_ms * 1e-3)) * (1.0 - np.exp(-t / 0.00015))
    b = eq(b, ("bp", fc, q), ("hp", hp))
    b = saturate(norm_peak(b), drive_db)
    out = np.zeros(n)
    place(out, norm_peak(b) * amp, ns(t0))
    return out


def thump(n, t0=0.004, f0=140.0, f1=48.0, sweep_ms=45.0, tau_ms=140.0, amp=1.0, drive_db=6.0,
          attack_ms=0.7, hold_ms=0.0):
    """Kick-style low thump: sine with an exponential pitch drop, fast attack, exponential decay."""
    m = n - ns(t0)
    if m <= 0:
        return np.zeros(n)
    L = min(m, ns((hold_ms + tau_ms * 9.0) * 1e-3) + 64)
    t = tvec(L)
    f = f1 + (f0 - f1) * np.exp(-t / (sweep_ms * 1e-3))
    y = np.sin(TAU * np.cumsum(f) / SR)
    e = perc(L, 0.0, attack_ms * 1e-3, tau_ms * 1e-3, hold_ms * 1e-3)
    y = y * e
    if drive_db > 0:
        y = saturate(y, drive_db)
    out = np.zeros(n)
    place(out, norm_peak(y) * amp, ns(t0))
    return out


def sub_boom(rng, n, t0=0.004, f0=70.0, f1=28.0, sweep_s=0.25, tau_s=0.7, amp=1.0, drive_db=4.0,
             rumble_db=-10.0, attack_ms=2.0):
    """Big sub-bass boom: long sine sweep + low-passed noise rumble riding on it."""
    m = n - ns(t0)
    if m <= 0:
        return np.zeros(n)
    t = tvec(m)
    f = f1 + (f0 - f1) * np.exp(-t / sweep_s)
    e = perc(m, 0.0, attack_ms * 1e-3, tau_s)
    y = np.sin(TAU * np.cumsum(f) / SR) * e
    if drive_db > 0:
        y = saturate(y, drive_db)
    y = norm_peak(y)
    if rumble_db > -60:
        r = eq(noise(m, rng, "brown"), ("lp4", 110.0), ("hp", 22.0)) * perc(m, 0.0, 0.02, tau_s * 0.8)
        y = y + norm_peak(r) * float(db2lin(rumble_db))
    out = np.zeros(n)
    place(out, norm_peak(y) * amp, ns(t0))
    return out


# --------------------------------------------------------------------------------------------- noise bodies
def burst(rng, n, t0=0.004, attack_ms=1.0, tau_ms=60.0, color="pink", filters=(), amp=1.0, hold_ms=0.0,
          drive_db=0.0, power=1.0):
    """Enveloped noise burst through static filters."""
    m = n - ns(t0)
    if m <= 0:
        return np.zeros(n)
    L = min(m, ns((attack_ms + hold_ms + tau_ms * 8.0) * 1e-3) + 256)
    y = noise(L, rng, color) * perc(L, 0.0, attack_ms * 1e-3, tau_ms * 1e-3, hold_ms * 1e-3, power)
    if filters:
        y = eq(y, *filters)
    if drive_db > 0:
        y = saturate(norm_peak(y), drive_db)
    out = np.zeros(n)
    place(out, norm_peak(y) * amp, ns(t0))
    return out


def moving_noise(rng, n, e, kind="bp", fc=1000.0, q=1.0, color="pink", order=2, amp=1.0, frame=2048,
                 circular=False, pre=()):
    """Noise through a time-varying filter, multiplied by envelope e (array length n).
    fc / q may be scalars, arrays or callables of time. Peak-normalised to amp."""
    y = noise(n, rng, color)
    if pre:
        y = eq(y, *pre, circular=circular)
    y = sweep(y, kind, fc, q, order=order, frame=frame, circular=circular) * e
    return norm_peak(y) * amp if amp is not None else y


def whoosh(rng, n, t0, dur, fpoints, q=1.5, peak_at=0.4, color="pink", amp=1.0, rise_curve=2.5,
           fall_curve=-3.0, order=2, flutter=0.0, flutter_rate=18.0):
    """Air whoosh: band-passed noise sweeping along fpoints [(t_rel, hz)...] under a bell envelope."""
    t_rel = [(t0 + a, f) for a, f in fpoints]
    e = env([(t0, 0.0), (t0 + dur * peak_at, 1.0), (t0 + dur, 0.0)], n, [rise_curve, fall_curve])
    if flutter > 0:
        e = e * (1.0 + flutter * smooth_random(n, rng, flutter_rate))
        e = np.maximum(e, 0.0)
    return moving_noise(rng, n, e, "bp", fpath(t_rel), q, color, order, amp)


def turbulence(rng, n, rates=((3.0, 0.35), (9.0, 0.3), (22.0, 0.2))):
    """Multiplicative turbulence curve (>= 0): product of smoothed random modulators."""
    m = np.ones(n)
    for r, d in rates:
        m *= np.maximum(1.0 + d * smooth_random(n, rng, r), 0.05)
    return m


def roar(rng, n, e, lp=1500.0, color="brown", turb=((3.0, 0.35), (9.0, 0.3), (22.0, 0.2)), drive_db=6.0,
         hp=40.0, amp=1.0, circular=False, mid_band=(250.0, 2500.0), mid_db=-4.0, air_db=-18.0):
    """Fire / jet roar: low noise + mid flame band + a little air, each with its own turbulence,
    all following envelope e; lp may be a callable (moving brightness). Saturated for grit."""
    low = noise(n, rng, color)
    low = eq(low, ("hp", hp), circular=circular)
    low = sweep(low, "lp", lp, 0.8, order=2, circular=circular) if callable(lp) or np.ndim(lp) else eq(low, ("lp", lp), circular=circular)
    low = norm_rms(low) * turbulence(rng, n, turb)
    mid = eq(noise(n, rng, "pink"), ("hp", mid_band[0]), ("lp", mid_band[1]), circular=circular)
    mid = norm_rms(mid) * turbulence(rng, n, tuple((r * 1.6, d * 1.2) for r, d in turb)) * float(db2lin(mid_db))
    air = eq(noise(n, rng, "white"), ("hp", 4500.0), ("lp", 11000.0), circular=circular)
    air = norm_rms(air) * turbulence(rng, n, ((14.0, 0.6), (35.0, 0.4))) * float(db2lin(air_db))
    y = (low + mid + air) * e
    if drive_db > 0:
        y = saturate(norm_peak(y), drive_db)
    return norm_peak(y) * amp


def rumble(rng, n, e, lp=120.0, rate=2.5, depth=0.45, amp=1.0, circular=False):
    y = eq(noise(n, rng, "brown"), ("lp4", lp), ("hp", 24.0), circular=circular)
    y = norm_rms(y) * np.maximum(1.0 + depth * smooth_random(n, rng, rate), 0.05) * e
    return norm_peak(y) * amp


# --------------------------------------------------------------------------------------------- particles
def _tick_kernels(rng, count, band, dur_ms):
    ks = []
    for _ in range(count):
        tau = rng.uniform(*dur_ms) * 1e-3
        L = ns(tau * 7) + 32
        t = tvec(L)
        k = rng.standard_normal(L) * np.exp(-t / tau)
        k[0] += 2.0
        fc = math.exp(rng.uniform(math.log(band[0]), math.log(band[1])))
        k = eq(k, ("bp", fc, rng.uniform(0.7, 2.0)), ("hp", band[0] * 0.5))
        ks.append(norm_peak(k))
    return ks


def _pop_kernels(rng, count, band, tau_ms):
    ks = []
    for _ in range(count):
        f = math.exp(rng.uniform(math.log(band[0]), math.log(band[1])))
        tau = rng.uniform(*tau_ms) * 1e-3
        L = ns(tau * 7) + 32
        t = tvec(L)
        k = np.sin(TAU * f * t * (1.0 + 0.15 * np.exp(-t / tau))) * np.exp(-t / tau)
        k += 0.5 * rng.standard_normal(L) * np.exp(-t / (tau * 0.25))
        ks.append(norm_peak(eq(k, ("hp", 120.0))))
    return ks


def crackle(rng, n, rate, t0=0.0, t1=None, amp_fn=None, band=(1500.0, 9000.0), tick_ms=(0.15, 1.0),
            pop_prob=0.2, pop_band=(250.0, 1300.0), pop_ms=(1.5, 6.0), alpha=2.0, wrap=False, variants=8):
    """Granular crackle (fire, sparks, grit, foam). rate: events/s or callable(t)."""
    t1 = n / SR if t1 is None else t1
    ts = poisson_times(rate, t0, t1, rng)
    out = np.zeros(n)
    if ts.size == 0:
        return out
    amps = np.minimum(rng.pareto(alpha, ts.size) + 1.0, 12.0) / 12.0
    amps *= np.where(rng.random(ts.size) < 0.5, 1.0, -1.0)
    if amp_fn is not None:
        amps *= amp_fn(ts)
    is_pop = rng.random(ts.size) < pop_prob
    kernels = _tick_kernels(rng, variants, band, tick_ms)
    pops = _pop_kernels(rng, max(variants // 2, 1), pop_band, pop_ms) if pop_prob > 0 else []
    which = rng.integers(0, len(kernels), ts.size)
    whichp = rng.integers(0, max(len(pops), 1), ts.size)
    idx = np.round(ts * SR).astype(np.int64)
    for j, k in enumerate(kernels):
        sel = (~is_pop) & (which == j)
        _conv_events(out, idx[sel], amps[sel], k, wrap)
    for j, k in enumerate(pops):
        sel = is_pop & (whichp == j)
        _conv_events(out, idx[sel], amps[sel] * 1.3, k, wrap)
    return out


def _conv_events(out, idx, amps, kernel, wrap):
    if idx.size == 0:
        return
    n = out.shape[0]
    imp = np.zeros(n)
    if wrap:
        np.add.at(imp, np.mod(idx, n), amps)
    else:
        m = (idx >= 0) & (idx < n)
        np.add.at(imp, idx[m], amps[m])
    L = kernel.shape[0]
    if wrap:
        nfft = n
        kk = np.zeros(n)
        kk[:min(L, n)] = kernel[:min(L, n)]
    else:
        from dsp import good_size
        nfft = good_size(n + L)
        kk = kernel
    y = np.fft.irfft(np.fft.rfft(imp, nfft) * np.fft.rfft(kk, nfft), nfft)[:n]
    out += y


def bubbles(rng, n, rate, t0=0.0, t1=None, f_lo=300.0, f_hi=2500.0, rise=(0.1, 0.8), decay_mult=(1.0, 3.0),
            amp_fn=None, wrap=False, size_bias=1.0, max_len=0.35):
    """Stream of bubbles (liquid, foam, mud). Bigger (lower) bubbles are louder."""
    t1 = n / SR if t1 is None else t1
    ts = poisson_times(rate, t0, t1, rng)
    out = np.zeros(n)
    for t in ts:
        u = rng.random() ** size_bias
        f0 = f_lo * (f_hi / f_lo) ** u
        b = bubble(f0, rng.uniform(*rise), rng.uniform(*decay_mult), max_len)
        a = (f_lo / f0) ** 0.35 * rng.uniform(0.4, 1.0)
        if amp_fn is not None:
            a *= float(amp_fn(np.array([t]))[0])
        place(out, b * a, ns(t), wrap)
    return out


def droplets(rng, n, rate, t0=0.0, t1=None, f_lo=1200.0, f_hi=4500.0, amp_fn=None, wrap=False,
             tau=(0.006, 0.02), glide=(1.4, 2.6)):
    t1 = n / SR if t1 is None else t1
    ts = poisson_times(rate, t0, t1, rng)
    out = np.zeros(n)
    for t in ts:
        f0 = math.exp(rng.uniform(math.log(f_lo), math.log(f_hi)))
        d = droplet(f0, rng.uniform(*tau), rng.uniform(*glide), 0.25, rng)
        a = rng.uniform(0.3, 1.0)
        if amp_fn is not None:
            a *= float(amp_fn(np.array([t]))[0])
        place(out, d * a, ns(t), wrap)
    return out


def rock_piece(rng, size):
    """Modal model of one rock fragment. size 0 (pebble, bright) .. 1 (chunk, dull)."""
    base = 5200.0 * (650.0 / 5200.0) ** size
    ratios = np.array([1.0, rng.uniform(1.4, 1.75), rng.uniform(2.1, 2.7), rng.uniform(2.9, 3.7), rng.uniform(4.1, 5.2)])
    decay0 = rng.uniform(0.004, 0.011) * (1.0 + 1.6 * size)
    decays = decay0 / ratios ** 0.6
    amps = np.array([1.0, 0.7, 0.5, 0.35, 0.2]) * rng.uniform(0.5, 1.0, 5)
    y = modal(base * ratios, decays, amps, rng=rng)
    L = y.shape[0]
    k = min(L, ns(0.006))
    y[:k] += 1.1 * rng.standard_normal(k) * np.exp(-np.arange(k) / (k / 5.0))  # crunchy contact noise
    return norm_peak(eq(y, ("hp", 180.0)))


def debris(rng, n, count, t0, t1, dist=None, size=(0.0, 1.0), bounces=(1, 4), restitution=(0.35, 0.6),
           first_gap=(0.03, 0.12), amp=1.0, wrap=False, size_pow=1.8, dark=0.0):
    """Falling / bouncing fragments. dist: callable(u in 0..1) -> t in [t0, t1] (default uniform)."""
    out = np.zeros(n)
    for _ in range(int(count)):
        u = rng.random()
        t = t0 + (t1 - t0) * (dist(u) if dist else u)
        s = size[0] + (size[1] - size[0]) * rng.random() ** size_pow
        k = rock_piece(rng, s)
        if dark > 0:
            k = eq(k, ("lp", 6000.0 * (1.0 - 0.7 * dark)))
        a = amp * (0.25 + s) ** 1.1 * rng.uniform(0.45, 1.0)
        nb = int(rng.integers(bounces[0], bounces[1] + 1))
        gap = rng.uniform(*first_gap) * (0.6 + 0.8 * s)
        e = rng.uniform(*restitution)
        for _b in range(nb):
            place(out, k * a, ns(t), wrap)
            t += gap
            gap *= e
            a *= e * rng.uniform(0.75, 1.0)
    return out


def grind(rng, n, e, rate=(250.0, 700.0), band=(250.0, 2600.0), grain_ms=(2.0, 9.0), amp=1.0, wrap=False):
    """Stone-on-stone grinding: dense stick-slip grains of band-limited noise, shaped by e."""
    ts = poisson_times(lambda t: np.interp(t, [0, n / SR], [rate[0], rate[1]]), 0.0, n / SR, rng)
    out = np.zeros(n)
    ks = []
    for _ in range(6):
        tau = rng.uniform(*grain_ms) * 1e-3
        L = ns(tau * 5) + 16
        tt = tvec(L)
        k = rng.standard_normal(L) * np.exp(-tt / tau) * (1 - np.exp(-tt / 0.0008))
        fc = math.exp(rng.uniform(math.log(band[0] * 1.5), math.log(band[1] * 0.8)))
        ks.append(norm_peak(eq(k, ("bp", fc, 1.2))))
    idx = np.round(ts * SR).astype(np.int64)
    amps = rng.uniform(0.2, 1.0, ts.size) * np.where(rng.random(ts.size) < 0.5, 1, -1)
    which = rng.integers(0, len(ks), ts.size)
    for j, k in enumerate(ks):
        _conv_events(out, idx[which == j], amps[which == j], k, wrap)
    out = eq(out, ("hp", band[0]), ("lp", band[1]), circular=wrap)
    return norm_peak(out * e) * amp


# --------------------------------------------------------------------------------------------- tonal magic
def shimmer(rng, n, e, freqs, detune_cents=8.0, trem_rate=(5.0, 11.0), trem_depth=0.5, drift_cents=15.0,
            glide=None, amp=1.0, partial_decay=None):
    """Magical shimmer: a cluster of sine partials, each slowly drifting in pitch and twinkling
    (random tremolo). glide: optional callable(t) -> pitch ratio applied to all partials."""
    out = np.zeros(n)
    t = tvec(n)
    g = glide(t) if glide is not None else 1.0
    for i, f in enumerate(freqs):
        c = rng.uniform(-detune_cents, detune_cents) + drift_cents * smooth_random(n, rng, 0.8) * 0.5
        fr = f * 2.0 ** (c / 1200.0) * g
        tr = np.maximum(1.0 + trem_depth * smooth_random(n, rng, rng.uniform(*trem_rate)), 0.0)
        a = 1.0 / (1.0 + 0.35 * i)
        if partial_decay is not None:
            a = a * np.exp(-t / partial_decay[i])
        out += a * np.sin(TAU * np.cumsum(fr) / SR + rng.random() * TAU) * tr
    return norm_peak(out * e) * amp


def bell_hits(rng, n, notes, ratio=3.5, index=1.6, amp_tau=0.5, index_tau=0.12, amp=1.0, octave_mix=0.3):
    """Series of FM bell / chime strikes: notes = [(t, hz, gain), ...]."""
    from dsp import fm_bell
    out = np.zeros(n)
    for t, f, g in notes:
        L = min(n - ns(t), ns(amp_tau * 7))
        if L <= 0:
            continue
        b = fm_bell(f, L / SR, ratio, index, amp_tau, index_tau)
        b += octave_mix * fm_bell(f * 2.0, L / SR, ratio, index * 0.6, amp_tau * 0.6, index_tau)
        place(out, b * g, ns(t))
    return norm_peak(out) * amp


def drone(rng, n, freqs, e, detune_cents=6.0, voices=3, lp=900.0, amp=1.0, circular=False, saw_mix=1.0):
    """Warm detuned saw drone through a low-pass (lp may be a callable for an opening filter)."""
    from dsp import saw
    y = np.zeros(n)
    for f in freqs:
        for v in range(voices):
            c = 0.0 if voices == 1 else detune_cents * (2.0 * v / (voices - 1) - 1.0)
            y += saw(f * 2.0 ** (c / 1200.0), n, rng.random()) * saw_mix
            y += (1.0 - saw_mix) * sine(f * 2.0 ** (c / 1200.0), n, rng.random())
    if callable(lp) or np.ndim(lp):
        y = sweep(y, "lp", lp, 0.9, order=4, circular=circular)
    else:
        y = eq(y, ("lp4", lp), circular=circular)
    return norm_peak(y * e) * amp


def space(x, rng, rt60=1.2, wet_db=-14.0, hf_ratio=0.4, predelay=0.01, stereo=False, er_level=0.5,
          circular=False):
    ir = make_ir(rt60, rng, predelay=predelay, stereo=stereo, hf_ratio=hf_ratio, er_level=er_level)
    return reverb(x, ir, wet_db, circular=circular)


# --------------------------------------------------------------------------------------------- overhaul layers
# Added for the ability overhaul (Docs/Ability_Overhaul.md section 6). New functions only: the layers above are
# untouched, so every earlier sound still renders byte-identically.
def place_reversed(buf, x, t_end):
    """Add x time-reversed into buf so that it ends at t_end: a decaying puff becomes a swell that is 'sucked
    in' (inward gestures, charges, implosions)."""
    r = np.ascontiguousarray(np.asarray(x, dtype=np.float64)[::-1])
    place(buf, r, ns(t_end) - r.shape[-1])


def pulse_train(freq, n, sharp=4, phase0=0.0):
    """Raised-cosine pulses (0..1) on a (time-varying) frequency. (0.5 + 0.5 cos)^k holds exactly k harmonics,
    so the train stays alias-free while it glides."""
    from dsp import phase
    return (0.5 + 0.5 * np.cos(TAU * phase(freq, n, phase0))) ** int(sharp)


def rotor(rng, n, rot_hz, e, flutes=3, sharp=4, fc=None, q=1.3, tone_db=-5.0, jitter=0.004, amp=1.0,
          color="pink"):
    """Spinning object (fluted slug, vortex core): air 'whup' pulses at the flute-pass rate rot_hz * flutes
    (per-sample array) that fuse into a pitched whine as the spin accelerates. fc: band-pass centre of the air
    (defaults to following the pass rate)."""
    rot = np.broadcast_to(np.asarray(rot_hz, dtype=np.float64), (int(n),))
    fp = rot * flutes * (1.0 + jitter * smooth_random(n, rng, 30.0))
    pt = pulse_train(fp, n, sharp, rng.random())
    fcv = fc if fc is not None else np.clip(650.0 + 2.6 * fp, 650.0, 7000.0)
    air = norm_rms(moving_noise(rng, n, pt, "bp", fcv, q, color, amp=None))
    from dsp import smooth
    tone = pt - smooth(pt, 30.0)  # periodic part only
    w = np.clip((fp - 45.0) / 130.0, 0.0, 1.0)  # the pulses fuse into a pitch above roughly 45-175 Hz
    tone = norm_rms(tone * w) if np.any(w > 0) else np.zeros(int(n))
    return norm_peak((air + float(db2lin(tone_db)) * tone) * e) * amp


def buzz(rng, n, e, fc=3200.0, am_hz=95.0, am_depth=0.8, partials=(1.0, 1.48, 2.07), jitter=0.006, amp=1.0):
    """Tight high-frequency vibration (a rotor at its limit, a stressed crystal): a small inharmonic cluster
    under fast, jittery amplitude modulation. fc may be a per-sample array."""
    fcv = np.broadcast_to(np.asarray(fc, dtype=np.float64), (int(n),))
    y = np.zeros(int(n))
    for i, r in enumerate(partials):
        f = fcv * r * (1.0 + jitter * smooth_random(n, rng, 25.0))
        y += np.sin(TAU * np.cumsum(f) / SR + rng.random() * TAU) / (1.0 + i)
    amf = am_hz * (1.0 + 0.15 * smooth_random(n, rng, 8.0))
    am = 1.0 - am_depth * (0.5 + 0.5 * np.sin(TAU * np.cumsum(amf) / SR))
    am = am * np.maximum(1.0 + 0.3 * smooth_random(n, rng, 60.0), 0.0)
    return norm_peak(y * am * e) * amp


def groan(rng, n, e, rate=(20.0, 60.0), t0=0.0, t1=None, body=(95.0, 150.0, 235.0, 360.0, 520.0), tau=0.045,
          jitter=0.35, amp=1.0):
    """Material under load (stone, packed earth): stick-slip impulses whose rate glides rate[0] -> rate[1] over
    [t0, t1], each exciting a low modal body. Slow rates creak, fast ones groan with a pitch."""
    T = n / SR
    t1 = T if t1 is None else t1
    t = tvec(n)
    r = np.interp(t, [t0, t1], [rate[0], rate[1]]) * np.maximum(1.0 + jitter * smooth_random(n, rng, 6.0), 0.1)
    ph = np.cumsum(r) / SR
    k = np.nonzero(np.diff(np.floor(ph)) > 0)[0] + 1
    k = k[(t[k] >= t0) & (t[k] < t1)]
    out = np.zeros(n)
    if k.size == 0:
        return out
    amps = rng.uniform(0.3, 1.0, k.size) * np.where(rng.random(k.size) < 0.5, 1.0, -1.0)
    freqs = np.asarray(body, dtype=np.float64) * (1.0 + 0.03 * rng.standard_normal(len(body)))
    decays = tau / np.sqrt(np.arange(len(body)) + 1.0)
    ker = modal(freqs, decays, 1.0 / (1.0 + 0.4 * np.arange(len(body))), rng=rng)
    _conv_events(out, k, amps, norm_peak(ker), False)
    return norm_peak(out * e) * amp


def glug(rng, n, t, f0=260.0, count=4, spacing=(0.018, 0.045), fall=0.88, amp=1.0, body_db=-8.0):
    """Gurgle: a quick run of large bubbles escaping one after another through a narrow opening, each a little
    lower as the pocket empties, with a short noisy 'gulp' under each."""
    from dsp import bubble
    out = np.zeros(n)
    tt, f = t, f0
    for i in range(int(count)):
        b = bubble(f, rise=rng.uniform(0.2, 0.6), decay_mult=rng.uniform(0.7, 1.4), max_len=0.12)
        g = rng.uniform(0.6, 1.0) * 0.85 ** i
        place(out, b * g, ns(tt))
        L = ns(0.03)
        gulp = eq(rng.standard_normal(L) * np.exp(-tvec(L) / 0.008), ("bp", f * 1.8, 2.0))
        place(out, norm_peak(gulp) * g * float(db2lin(body_db)), ns(tt))
        tt += rng.uniform(*spacing)
        f *= fall * rng.uniform(0.95, 1.05)
    return norm_peak(out) * amp


def gravel(rng, n, rate, t0=0.0, t1=None, size=(0.0, 0.35), amp_fn=None, variants=10, dark=0.0, amp=1.0,
           alpha=2.2):
    """Gravel pour / rubble trickle: dense small modal rock ticks (a bank of rock_piece kernels) without the
    bounce model, so thousands of grains stay cheap. rate: events/s or callable(t)."""
    t1 = n / SR if t1 is None else t1
    ts = poisson_times(rate, t0, t1, rng)
    out = np.zeros(n)
    if ts.size == 0:
        return out
    bank = []
    for i in range(int(variants)):
        s = size[0] + (size[1] - size[0]) * (i + rng.random()) / variants
        k = rock_piece(rng, s)
        if dark > 0:
            k = eq(k, ("lp", 6000.0 * (1.0 - 0.7 * dark)))
        bank.append((k, (0.25 + s) ** 1.1))
    amps = np.minimum(rng.pareto(alpha, ts.size) + 1.0, 10.0) / 10.0
    if amp_fn is not None:
        amps = amps * amp_fn(ts)
    which = rng.integers(0, len(bank), ts.size)
    idx = np.round(ts * SR).astype(np.int64)
    for j, (k, a) in enumerate(bank):
        sel = which == j
        _conv_events(out, idx[sel], amps[sel] * a, k, False)
    return norm_peak(out) * amp


def boulder(rng, n, t, size=1.0, amp=1.0, thump_db=-6.0):
    """A big rock chunk landing: a low, heavily damped modal body (about 380 Hz for a rock, 120 Hz for a slab
    corner), crunchy contact noise and a short low thump."""
    base = 380.0 * (120.0 / 380.0) ** size
    ratios = np.array([1.0, rng.uniform(1.4, 1.7), rng.uniform(2.2, 2.7), rng.uniform(3.0, 3.8), rng.uniform(4.3, 5.4)])
    decays = rng.uniform(0.02, 0.04) * (1.0 + size) / ratios ** 0.7
    body = modal(base * ratios, decays, np.array([1.0, 0.8, 0.6, 0.4, 0.25]) * rng.uniform(0.6, 1.0, 5), rng=rng)
    k = min(body.shape[0], ns(0.012))
    body[:k] += 1.3 * rng.standard_normal(k) * np.exp(-np.arange(k) / (k / 4.0))
    body = norm_peak(eq(body, ("hp", 60.0)))
    out = np.zeros(n)
    place(out, body, ns(t))
    th = thump(n, t, 110.0 * (0.7 + 0.3 * (1.0 - size)), 55.0, 18.0, 55.0 + 40.0 * size, amp=float(db2lin(thump_db)),
               drive_db=6.0)
    return norm_peak(out + th) * amp


def comb_warp(x, delay_ms, depth=0.7):
    """Feed-forward comb with a moving delay (scalar or per-sample ms): the swept notches of a flanger, i.e. the
    'jet' / warped-air colour. Cubic interpolation keeps it smooth for any delay curve."""
    from dsp import hermite
    x = np.asarray(x, dtype=np.float64)
    n = x.shape[-1]
    d = np.broadcast_to(np.asarray(delay_ms, dtype=np.float64), (n,)) * 1e-3 * SR
    return x + depth * hermite(x, np.arange(n) - d)


def n_wave(n, t0=0.004, dur_ms=12.0, rise_ms=0.06, tail=0.9, amp=1.0, os=8):
    """Supersonic N-wave (the pressure signature of a sonic boom or a ballistic crack): a shock up to +1, a
    linear fall through zero to -tail, and a second shock back to rest. The two shocks are the double crack,
    the ramp carries the thump. Built 8x oversampled with finite rise times, then band-limited exactly."""
    from dsp import downsample
    D = dur_ms * 1e-3
    pre = 0.001
    L = ns(pre + D + 0.003) + 64
    m = L * os
    tt = np.arange(m) / (SR * os) - pre
    p = np.where((tt >= 0.0) & (tt < D), 1.0 - (1.0 + tail) * tt / D, 0.0)
    k = max(int(rise_ms * 1e-3 * SR * os), 2)
    ker = np.hanning(k + 2)[1:-1]
    p = np.convolve(p, ker / ker.sum(), mode="same")
    y = downsample(p, os, L)
    out = np.zeros(n)
    place(out, norm_peak(y) * amp, ns(t0) - ns(pre))
    return out
