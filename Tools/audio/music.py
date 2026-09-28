"""Music: the title theme (seamless 105 s loop) and the spawn sting.

music_title_theme - an original Celtic / medieval air in D dorian, 6/8 at dotted-quarter = 64 BPM,
56 bars = exactly 105 s. Form: intro (8) - tune A (16) - tune B (8) - tune A reprise with ornaments
and a lute counter-line (16) - outro (8) that cadences (G -> Dm, the dorian IV-i) back into bar 1.
Instruments (all synthesised here):
  harp   Karplus-Strong string evaluated exactly in the frequency domain (dsp.pluck), pitch-dependent
         decay, soundboard EQ; rolling 6/8 arpeggios.
  lute   double-course KS plucks (two strings a few cents apart), darker, nasal body EQ; strums and a
         chord-tone counter-melody.
  pad    warm string pad: detuned PolyBLEP saws, 4th-order low-pass with a slow periodic sweep.
  flute  lead built phrase by phrase: harmonic stack whose brightness follows dynamics, legato
         portamento, delayed vibrato, tonguing dips, breath noise and onset chiff.
  drums  bodhran-like frame drum (pitched boom + skin + slap), finger taps and a soft shaker.
Everything is written into a circular buffer (tails wrap past the end into the start) and the hall
reverb, bus compressor and limiter are circular too, so the loop point is seamless.

music_spawn_sting - 6 s rising flourish in D major (harp glissando, flute run, pad swell, drum roll,
bell bloom)."""
from __future__ import annotations

import math

import numpy as np

from dsp import (SR, TAU, compress, db2lin, env, eq, fm_bell, make_ir, midi_hz, noise, norm_peak, note_midi, ns, pan,
                 perc, place, pluck, reverb, saw, smooth, smooth_random, sweep, tvec)
from reg import sound

G = lambda d: float(db2lin(d))  # noqa: E731

BPM = 64.0  # dotted quarter
EIGHTH = 60.0 / BPM / 3.0  # 0.3125 s
BAR = 6 * EIGHTH  # 1.875 s
BARS = 56
LOOP = BARS * BAR  # 105 s
PRE = 0.025  # the file starts 25 ms before the bar-1 downbeat so the loop joint sits in a quiet spot

# ------------------------------------------------------------------------------------------ harmony
CHORD_TONES = {  # root, third, fifth as MIDI in the octave used for arpeggios
    "Dm": (50, 53, 57), "C": (48, 52, 55), "Am": (45, 48, 52), "F": (53, 57, 60), "G": (55, 59, 62),
}
PAD_VOICING = {"Dm": (57, 62, 65), "C": (55, 60, 64), "Am": (57, 60, 64), "F": (57, 60, 65), "G": (55, 59, 62)}
BASS = {"Dm": 38, "C": 36, "Am": 45, "F": 41, "G": 43}

P_INTRO = ["Dm", "C", "Dm", "Am", "Dm", "F", "C", "G"]
P_A = ["Dm", "C", "Dm", "Am", "Dm", "F", "C", "G", "F", "C", "Dm", "G", "Dm", "C", "Am", "Dm"]
P_B = ["F", "G", "Am", "Am", "F", "G", "C", "C"]
P_OUT = ["Dm", "C", "Dm", "Am", "F", "C", "G", "G"]
CHORDS = P_INTRO + P_A + P_B + P_A + P_OUT  # 56 bars
SECTIONS = [("intro", 1, 8), ("A", 9, 24), ("B", 25, 32), ("A'", 33, 48), ("outro", 49, 56)]


def section_of(bar):
    for name, a, b in SECTIONS:
        if a <= bar <= b:
            return name
    return "intro"


# ------------------------------------------------------------------------------------------ melody
# (note, eighths); "r" = rest. One list per phrase (a breath between phrases).
TUNE_A = [
    [("A4", 2), ("D5", 1), ("D5", 2), ("E5", 1), ("F5", 2), ("E5", 1), ("D5", 2), ("C5", 1), ("D5", 3), ("A4", 2), ("G4", 1), ("A4", 5)],
    [("A4", 2), ("D5", 1), ("D5", 2), ("E5", 1), ("F5", 2), ("G5", 1), ("A5", 2), ("F5", 1), ("G5", 2), ("E5", 1), ("C5", 2), ("E5", 1), ("D5", 3), ("B4", 2)],
    [("A5", 2), ("A5", 1), ("C6", 2), ("A5", 1), ("G5", 2), ("E5", 1), ("C5", 2), ("D5", 1), ("F5", 2), ("E5", 1), ("D5", 2), ("A4", 1), ("B4", 2), ("C5", 1), ("D5", 2), ("B4", 1)],
    [("A4", 2), ("D5", 1), ("F5", 2), ("E5", 1), ("C5", 2), ("D5", 1), ("E5", 2), ("G5", 1), ("E5", 2), ("D5", 1), ("C5", 2), ("E5", 1), ("D5", 5)],
]
TUNE_B = [
    [("C6", 2), ("A5", 1), ("F5", 2), ("A5", 1), ("B5", 2), ("G5", 1), ("D5", 2), ("G5", 1), ("C6", 2), ("B5", 1), ("A5", 2), ("E5", 1), ("A5", 5)],
    [("F5", 2), ("A5", 1), ("C6", 2), ("A5", 1), ("B5", 2), ("D6", 1), ("B5", 2), ("G5", 1), ("E6", 2), ("D6", 1), ("C6", 2), ("G5", 1), ("E5", 3), ("C5", 2), ("B4", 1)],
]
# reprise: last phrase varied (reaches up to A5 then settles)
TUNE_A2_LAST = [("A4", 2), ("D5", 1), ("F5", 2), ("A5", 1), ("G5", 2), ("F5", 1), ("E5", 2), ("G5", 1), ("A5", 2), ("G5", 1), ("E5", 2), ("C5", 1), ("D5", 5)]
# bars where phrases start
PHRASES = ([(9 + 4 * i, TUNE_A[i], 1.0) for i in range(4)] + [(25, TUNE_B[0], 1.08), (29, TUNE_B[1], 1.12)] +
           [(33, TUNE_A[0], 1.0), (37, TUNE_A[1], 1.02), (41, TUNE_A[2], 1.05), (45, TUNE_A2_LAST, 1.05)] +
           [(49, TUNE_A[0], 0.72)])
ORNAMENT_BARS = {35, 36, 40, 44, 48}  # reprise: grace-note cuts on the long notes of these bars


def bar_time(bar, eighth=0.0, pre=PRE):
    return pre + (bar - 1) * BAR + eighth * EIGHTH


# ------------------------------------------------------------------------------------------ instruments
def harp_note(rng, midi, vel):
    f = float(midi_hz(midi))
    t60 = float(np.clip(4.6 * (220.0 / f) ** 0.4, 1.8, 6.0))
    return pluck(f, min(t60 * 1.05, 6.0), rng, t60=t60, t60_hf=0.6, hf_ref=4000.0, pick=0.12,
                 bright=4500.0 + 5000.0 * min(vel, 1.0)) * vel


def lute_note(rng, midi, vel, dur=2.4):
    f = float(midi_hz(midi))
    t60 = float(np.clip(2.2 * (220.0 / f) ** 0.3, 1.2, 3.0))
    a = pluck(f * 2 ** (2.5 / 1200), dur, rng, t60=t60, t60_hf=0.18, hf_ref=2500.0, pick=0.2, bright=3000.0)
    b = pluck(f * 2 ** (-2.5 / 1200), dur, rng, t60=t60, t60_hf=0.18, hf_ref=2500.0, pick=0.23, bright=2800.0)
    out = np.zeros(a.shape[0] + ns(0.005))
    out[: a.shape[0]] += a
    out[ns(0.005):] += 0.85 * b
    return out * vel * 0.6


def pad_chord(rng, chord, dur, vel):
    """One chord of the string pad (stereo): detuned saws per voice + a soft bass voice."""
    m = ns(dur)
    out = np.zeros((2, m))
    for midi in PAD_VOICING[chord]:
        f = float(midi_hz(midi))
        for side, dets in ((0, (-8.0, 3.0)), (1, (-3.0, 8.0))):
            for d in dets:
                out[side] += saw(f * 2 ** (d / 1200.0), m, rng.random())
    fb = float(midi_hz(BASS[chord]))
    bass = saw(fb, m, rng.random()) * 0.5 + np.sin(TAU * fb * tvec(m)) * 0.9
    out += np.stack([bass, bass]) * 1.3
    e = env([(0.0, 0.0), (0.5, 1.0), (dur - 1.0, 0.85), (dur, 0.0)], m, [1.5, 0.0, -2.5])
    return out * e * vel


def flute_phrase(rng, notes, vel):
    """Render one legato phrase. notes: [(midi, dur_s, cut)] where cut = grace-note ornament flag.
    Returns a mono array starting at the first note onset."""
    tot = sum(d for _, d, _ in notes)
    breath_gap = min(0.16, notes[-1][1] * 0.35)
    m = ns(tot + 0.35)
    t = tvec(m)
    logf = np.zeros(m)
    onsets = []
    pos = 0.0
    for i, (midi, d, cut) in enumerate(notes):
        a, b = ns(pos), ns(pos + d)
        logf[a:b] = math.log2(float(midi_hz(midi)))
        if cut and d > 0.4:  # grace note (a cut): upper neighbour for ~40 ms before the note
            g = ns(0.04)
            logf[a:a + g] = math.log2(float(midi_hz(midi + 2)))
        onsets.append((pos, d))
        pos += d
    logf[ns(pos):] = logf[ns(pos) - 1]
    w = ns(0.03)  # portamento between legato notes
    logf = smooth(logf, 30.0, passes=1) if w > 1 else logf
    # vibrato: delayed onset on longer notes, ~5.3 Hz with slow drift
    depth = np.zeros(m)
    for (p0, d) in onsets:
        if d < 0.45:
            continue
        a = ns(p0 + 0.18)
        b = ns(p0 + d)
        if b > a:
            ramp = np.clip((t[a:b] - (p0 + 0.18)) / 0.35, 0, 1)
            depth[a:b] = np.maximum(depth[a:b], 16.0 * ramp)
    depth = smooth(depth, 40.0, passes=1)
    rate = 5.3 + 0.25 * smooth_random(m, rng, 0.5)
    vib = np.sin(TAU * np.cumsum(rate) / SR)
    drift = 3.0 * smooth_random(m, rng, 0.7)
    f = 2.0 ** (logf + (depth * vib + drift) / 1200.0)
    # amplitude: phrase attack / release, tonguing dips, swells on long notes, phrase arc
    end = tot - breath_gap
    amp = env([(0.0, 0.0), (0.07, 1.0), (end, 1.0), (end + 0.12, 0.0)], m, [-1.0, 0.0, -1.5])
    for i, (p0, d) in enumerate(onsets):
        if i > 0:
            amp *= 1.0 - 0.3 * np.exp(-((t - p0) / 0.018) ** 2)
        if d > 0.5:
            u = np.clip((t - p0) / d, 0, 1)
            amp *= 1.0 + 0.12 * np.sin(np.pi * u) * ((t >= p0) & (t < p0 + d))
    amp *= 0.86 + 0.14 * np.sin(np.pi * np.clip(t / max(end, 1e-3), 0, 1))
    amp *= 1.0 + 0.035 * (depth / 16.0) * vib
    amp *= 1.0 + 0.025 * smooth_random(m, rng, 9.0)  # breath turbulence
    ph = TAU * np.cumsum(f) / SR
    lvl = amp * vel
    y = (np.sin(ph) + (0.2 + 0.12 * lvl) * np.sin(2 * ph + 0.3) + (0.06 + 0.07 * lvl) * np.sin(3 * ph + 1.1) +
         0.03 * lvl * np.sin(4 * ph + 2.0) + 0.012 * np.sin(5 * ph))
    y[f * 5 > 0.45 * SR] *= 1.0  # (partials stay far below Nyquist for this range)
    breath = eq(noise(m, rng, "pink"), ("hp", 900), ("lp", 8000)) * 0.065
    chiff = np.zeros(m)
    for (p0, d) in onsets:
        c = eq(noise(ns(0.06), rng, "white"), ("bp", 2800, 0.8)) * perc(ns(0.06), 0.0, 0.003, 0.018)
        place(chiff, c * 0.09, ns(p0))
    return (y + breath) * amp * vel + chiff * vel


def drum_boom(rng, vel):
    m = ns(0.7)
    t = tvec(m)
    f = 62.0 + 40.0 * np.exp(-t / 0.025)
    body = np.sin(TAU * np.cumsum(f) / SR) * perc(m, 0.0, 0.002, 0.22)
    skin = eq(noise(m, rng, "pink"), ("lp", 900), ("hp", 60)) * perc(m, 0.0, 0.001, 0.025)
    slap = eq(noise(m, rng, "white"), ("bp", 1800, 0.9)) * perc(m, 0.0, 0.0005, 0.006)
    return (norm_peak(body) + norm_peak(skin) * 0.4 + norm_peak(slap) * 0.18) * vel


def drum_tap(rng, vel):
    m = ns(0.3)
    t = tvec(m)
    tone = np.sin(TAU * np.cumsum(150.0 + 30.0 * np.exp(-t / 0.01)) / SR) * perc(m, 0.0, 0.001, 0.04)
    skin = eq(noise(m, rng, "white"), ("bp", 2500, 0.8)) * perc(m, 0.0, 0.0005, 0.012)
    return (norm_peak(tone) * 0.5 + norm_peak(skin) * 0.8) * vel


def shaker(rng, vel):
    m = ns(0.14)
    y = eq(noise(m, rng, "white"), ("hp", 5000), ("lp", 13000)) * env([(0.0, 0.0), (0.008, 1.0), (0.14, 0.0)], m, [0.0, -3.0])
    return norm_peak(y) * vel


LILT = [0.0, 0.009, 0.004, 0.0, 0.009, 0.004]  # 6/8 lilt: slightly late 2nd / 3rd eighths


# ------------------------------------------------------------------------------------------ the theme
@sound("Music", LOOP, loop=True, stereo=True, target=-17.0, max_gr=3.0,
       use="Title screen music: seamless 105 s loop, Celtic-medieval air in D dorian (harp, lute, strings, flute, frame drum)")
def music_title_theme(rng, n):
    buses = {k: np.zeros((2, n)) for k in ("harp", "lute", "pad", "flute", "drum")}
    jit = lambda s=0.006: float(rng.uniform(-s, s))  # noqa: E731

    for bar in range(1, BARS + 1):
        ch = CHORDS[bar - 1]
        sec = section_of(bar)
        r, th, fi = CHORD_TONES[ch]
        dyn = {"intro": 0.75, "A": 0.85, "B": 1.0, "A'": 0.92, "outro": 0.75}[sec]  # outro == intro: seamless joint
        # ---- harp arpeggios
        if sec in ("intro", "outro"):
            pat = [r, fi, r + 12, th + 12, fi + 12, th + 12]
        elif sec == "B":
            pat = [r, fi, th + 12, fi + 12, r + 24, fi + 12]
        else:
            pat = [r, fi, r + 12, th + 12, fi + 12, r + 12]
        for i, midi in enumerate(pat):
            v = dyn * (0.95 if i in (0, 3) else 0.72) * rng.uniform(0.9, 1.05)
            y = harp_note(rng, midi, v)
            place(buses["harp"], pan(y, -0.3 + 0.1 * (midi - 60) / 12.0), ns(bar_time(bar, i) + LILT[i] + jit()), wrap=True)
        # ---- lute
        if sec in ("intro", "outro"):
            y = lute_note(rng, BASS[ch] + 12, dyn * 0.9)
            place(buses["lute"], pan(y, 0.35), ns(bar_time(bar, 0) + jit()), wrap=True)
        elif sec in ("A", "B"):
            chord = [r + 12, th + 12, fi + 12] if sec == "A" else [r, fi, r + 12, th + 12]
            for beat in (0, 3):
                v = dyn * (0.75 if beat == 0 else 0.6)
                for k, midi in enumerate(chord):  # quick strum, low to high
                    y = lute_note(rng, midi, v * rng.uniform(0.85, 1.0), dur=1.6)
                    place(buses["lute"], pan(y, 0.35), ns(bar_time(bar, beat) + LILT[beat] + 0.018 * k + jit(0.003)), wrap=True)
        # ---- string pad (chord per bar, overlapping release)
        y = pad_chord(rng, ch, BAR + 1.2, dyn * (0.8 if sec in ("intro", "outro") else 1.0))
        place(buses["pad"], y, ns(bar_time(bar) - 0.05), wrap=True)
        # ---- frame drum + shaker
        if sec in ("A", "A'", "B") or (sec == "outro" and bar <= 52):
            if sec == "outro":
                hits = [(0, "boom", 0.8 - 0.12 * (bar - 49))]
            elif sec == "B":
                hits = [(0, "boom", 1.0), (3, "boom", 0.7), (1, "tap", 0.35), (2, "tap", 0.45), (4, "tap", 0.35), (5, "tap", 0.5)]
            else:
                hits = [(0, "boom", 0.95), (3, "boom", 0.55), (2, "tap", 0.3), (5, "tap", 0.4)]
                if bar in (24, 40, 48):  # fills
                    hits += [(3, "tap", 0.5), (4, "tap", 0.6), (5, "tap", 0.75)]
            for pos, kind, v in hits:
                y = (drum_boom if kind == "boom" else drum_tap)(rng, v * dyn * rng.uniform(0.9, 1.05))
                place(buses["drum"], pan(y, 0.05 if kind == "boom" else -0.1), ns(bar_time(bar, pos) + LILT[pos] + jit(0.004)), wrap=True)
            if bar >= 13 and sec != "outro":
                for pos in range(6):
                    v = (0.35 if pos in (0, 3) else 0.2) * dyn * rng.uniform(0.8, 1.1)
                    place(buses["drum"], pan(shaker(rng, v), 0.45), ns(bar_time(bar, pos) + LILT[pos] + jit(0.004)), wrap=True)

    # ---- flute phrases (+ lute counter-line in the reprise)
    for start_bar, phrase, pv in PHRASES:
        sec = section_of(start_bar)
        notes = []
        pos = 0.0
        for name, e8 in phrase:
            if name == "r":
                pos += e8
                continue
            b_here = start_bar + int((pos + 1e-6) // 6)
            cut = sec == "A'" and b_here in ORNAMENT_BARS and e8 >= 3
            notes.append((note_midi(name), e8 * EIGHTH, cut, pos))
            pos += e8
        y = flute_phrase(rng, [(mi, d, c) for mi, d, c, _ in notes], 0.9 * pv)
        place(buses["flute"], pan(y, 0.0), ns(bar_time(start_bar)), wrap=True)
        if sec == "A'":
            for mi, d, c, p8 in notes:
                if abs(p8 % 3) > 1e-6:
                    continue  # only notes that start on a dotted-quarter beat
                bar = start_bar + int((p8 + 1e-6) // 6)
                r, th, fi = CHORD_TONES[CHORDS[bar - 1]]
                tones = [x + 12 * o for x in (r, th, fi) for o in (-1, 0, 1, 2)]
                cands = [x for x in tones if 3 <= mi - x <= 9]
                if not cands:
                    continue
                y = lute_note(rng, max(cands), 0.55, dur=2.0)
                place(buses["lute"], pan(y, 0.35), ns(bar_time(start_bar) + p8 * EIGHTH + jit(0.004)), wrap=True)

    # ---- bus processing
    buses["harp"] = eq(buses["harp"], ("hp", 70), ("peak", 200, 1.0, 1.5), ("peak", 400, 1.0, -2.0), ("peak", 2500, 0.8, 2.0),
                       ("hs", 9000, 0.7, -2.0), circular=True)
    buses["lute"] = eq(buses["lute"], ("hp", 90), ("peak", 110, 1.2, 2.0), ("peak", 380, 1.0, -2.0), ("peak", 2800, 1.5, 2.5),
                       ("lp", 7000), circular=True)
    t = tvec(n)
    buses["pad"] = sweep(buses["pad"], "lp", 1600.0 + 600.0 * np.sin(TAU * 7.0 * t / (n / SR)), 0.8, order=4, circular=True)
    buses["pad"] = eq(buses["pad"], ("hp", 40), ("peak", 350, 1.0, -2.5), circular=True)
    buses["flute"] = eq(buses["flute"], ("hp", 200), ("peak", 900, 0.8, 1.5), ("peak", 3000, 1.0, 1.5), circular=True)
    buses["drum"] = eq(buses["drum"], ("hp", 40), circular=True)
    lv = {"harp": -4.0, "lute": -7.0, "pad": -11.0, "flute": -1.0, "drum": -9.0}
    send = {"harp": -9.0, "lute": -12.0, "pad": -8.0, "flute": -7.0, "drum": -16.0}
    ref = {k: np.sqrt(np.mean(v ** 2)) + 1e-12 for k, v in buses.items()}
    dry = sum(buses[k] / ref[k] * G(lv[k]) for k in buses)
    wet_in = sum(buses[k] / ref[k] * G(lv[k] + send[k]) for k in buses)
    ir = make_ir(2.3, rng, predelay=0.025, stereo=True, hf_ratio=0.45, er_level=0.35)
    mixb = dry + reverb(wet_in, ir, 0.0, dry=0.0, circular=True)
    return compress(mixb, threshold_db=-16.0, ratio=1.8, attack_ms=25.0, release_ms=250.0, circular=True)


# the loop joint is a bar-1 downbeat, so judge its timbre jump against the other downbeats
music_title_theme.seam_points = lambda n: [ns(bar_time(b)) for b in range(2, BARS + 1)]


# ------------------------------------------------------------------------------------------ spawn sting
@sound("Music", 6.0, stereo=True, target=-14.0, metric="M", max_gr=3.0, use="Spawn / enter-world sting: 6 s rising flourish into a bright D major bloom")
def music_spawn_sting(rng, n):
    T = n / SR
    tb = 1.1
    buses = {k: np.zeros((2, n)) for k in ("harp", "flute", "pad", "drum", "fx")}
    scale = ["D4", "E4", "F#4", "G4", "A4", "B4", "C#5", "D5", "E5", "F#5", "G5", "A5", "B5", "C#6", "D6"]
    times = 0.05 + 0.95 * (np.linspace(0, 1, len(scale)) ** 0.8)
    for i, (nm, tt) in enumerate(zip(scale, times)):
        y = harp_note(rng, note_midi(nm), 0.6 + 0.4 * i / len(scale))
        place(buses["harp"], pan(y, -0.5 + i / len(scale)), ns(tt))
    for nm in ("D3", "A3", "D4", "F#4", "A4"):  # final harp chord on the bloom
        place(buses["harp"], pan(harp_note(rng, note_midi(nm), 0.8), -0.2), ns(tb + 0.02))
    notes = [(note_midi("A5"), 0.08, False), (note_midi("B5"), 0.08, False)] * 3 + \
            [(note_midi("A5"), 0.1, False), (note_midi("C#6"), 0.12, False), (note_midi("D6"), 1.7, False)]
    y = flute_phrase(rng, notes, 0.95)
    place(buses["flute"], pan(y, 0.0), ns(tb - 0.66))
    pad = np.zeros((2, n))
    for midi in (note_midi("D3"), note_midi("A3"), note_midi("D4"), note_midi("F#4"), note_midi("A4")):
        f = float(midi_hz(midi))
        for side, dets in ((0, (-7.0, 2.0)), (1, (-2.0, 7.0))):
            for d in dets:
                pad[side] += saw(f * 2 ** (d / 1200.0), n, rng.random())
    pad = sweep(pad, "lp", env([(0.0, 400.0), (tb, 3500.0), (T, 1200.0)], n, [2.0, -1.0]), 0.8, order=4)
    buses["pad"] = pad * env([(0.2, 0.0), (tb, 1.0), (2.5, 0.7), (T - 0.3, 0.0)], n, [2.0, -1.0, -2.0])
    tt = 0.45
    gap = 0.11
    while tt < tb - 0.03:  # accelerating roll into the boom
        place(buses["drum"], pan(drum_tap(rng, 0.3 + 0.6 * (tt - 0.45) / 0.65), -0.05), ns(tt))
        tt += gap
        gap = max(gap * 0.86, 0.035)
    place(buses["drum"], pan(drum_boom(rng, 1.0), 0.0), ns(tb))
    t = tvec(n)
    riser = sweep(noise(n, rng, "white"), "hp", env([(0.0, 6000.0), (tb, 1500.0)], n, 1.0), 0.7) * \
        env([(0.0, 0.0), (tb, 1.0), (tb + 0.02, 0.0)], n, [3.0, 0.0])
    crash = eq(noise(n, rng, "white"), ("hp", 3000)) * perc(n, tb, 0.005, 1.1)
    bells = np.zeros((2, n))
    for nm, p in (("D6", -0.3), ("F#6", 0.3), ("A6", 0.0)):
        b = np.zeros(n)
        place(b, fm_bell(float(midi_hz(note_midi(nm))), T - tb, ratio=2.0, index=1.2, amp_tau=0.9, index_tau=0.12), ns(tb))
        bells += pan(b, p)
    buses["fx"] = np.stack([riser, riser]) * 0.5 + np.stack([crash, crash]) * 0.35 + bells * 1.0
    lv = {"harp": -5.0, "flute": -3.0, "pad": -12.0, "drum": -7.0, "fx": -12.0}
    ref = {k: np.sqrt(np.mean(v ** 2)) + 1e-12 for k, v in buses.items()}
    dry = sum(buses[k] / ref[k] * G(lv[k]) for k in buses)
    ir = make_ir(2.0, rng, predelay=0.02, stereo=True, hf_ratio=0.5, er_level=0.35)
    out = reverb(dry, ir, -8.0)
    return out * env([(0.0, 1.0), (T - 0.6, 1.0), (T, 0.0)], n, [0.0, -2.0])


# ------------------------------------------------------------------------------------------ preview
def preview_sheet(entries, path):
    """Music contact sheet: long spectrogram with section / bar markers, waveform, pitch-class
    histogram (D dorian notes highlighted) for the theme; a normal tile for the sting."""
    import os

    from PIL import Image, ImageDraw

    import analysis
    W = 1800
    blocks = []
    for e in entries:
        X = e["data"]
        st = e["stats"]
        dur = X.shape[-1] / SR
        if e["name"] == "music_title_theme":
            H = 30 + 90 + 320 + 200
            img = Image.new("RGB", (W, H), (12, 13, 18))
            d = ImageDraw.Draw(img)
            d.text((8, 4), "%s  %.2fs  LOOP  I %.1f LUFS  M-max %.1f  tp %.1f dBTP  seam step %.2f  hfz %.1f  timbre %.0f%%" % (
                e["name"], dur, st["lufs_i"], st["lufs_m_max"], st["true_peak_dbtp"], st["seam_step_ratio"], st["seam_hf_z"],
                st["seam_timbre_pct"]), fill=(240, 240, 240), font=analysis.font(15))
            img.paste(Image.fromarray(analysis.waveform_rgb(X, W, 90)), (0, 30))
            img.paste(Image.fromarray(analysis.spectrogram_rgb(X.mean(axis=0), W, 320, fmin=40.0, fmax=12000.0)), (0, 120))
            for name, a, b in SECTIONS:
                x0 = int((a - 1) * BAR / dur * W)
                d.line([(x0, 30), (x0, 440)], fill=(120, 220, 255))
                d.text((x0 + 4, 122), name, fill=(120, 220, 255), font=analysis.font(14))
            for bar in range(1, BARS + 1):
                x0 = int((bar - 1) * BAR / dur * W)
                d.line([(x0, 436), (x0, 440)], fill=(200, 200, 200))
            # pitch-class histogram
            pc = analysis.chroma(X)
            names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
            dorian = {"D", "E", "F", "G", "A", "B", "C"}
            y0 = 450
            d.text((8, y0), "pitch-class energy (60 Hz - 2 kHz); D dorian notes in gold, others grey", fill=(220, 220, 220), font=analysis.font(13))
            bw = 60
            for i, (nm, v) in enumerate(zip(names, pc)):
                x = 20 + i * (bw + 12)
                h = int(v / pc.max() * 150)
                col = (240, 190, 60) if nm in dorian else (120, 120, 130)
                d.rectangle([x, y0 + 175 - h, x + bw, y0 + 175], fill=col)
                d.text((x + 18, y0 + 178), nm, fill=(230, 230, 230), font=analysis.font(13))
                d.text((x + 8, y0 + 158 - h), "%.0f%%" % (100 * v), fill=(230, 230, 230), font=analysis.font(11))
            inside = sum(v for nm, v in zip(names, pc) if nm in dorian)
            d.text((900, y0 + 40), "in-mode energy: %.1f %%" % (100 * inside), fill=(240, 190, 60), font=analysis.font(18))
            blocks.append(img)
        else:
            blocks.append(analysis._tile(e, tile_w=W))
    Ht = sum(b.height for b in blocks) + 50 + 8 * len(blocks)
    sheet = Image.new("RGB", (W + 16, Ht), (6, 6, 9))
    ImageDraw.Draw(sheet).text((10, 8), "LA PLACE audio - Music", fill=(255, 255, 255), font=analysis.font(20, mono=False))
    y = 42
    for b in blocks:
        sheet.paste(b, (8, y))
        y += b.height + 8
    analysis.save_png(sheet, path)
    return path
