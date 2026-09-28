# LA PLACE audio (procedural sound effects, ambience and music)

Every sound in `SourceArt/Audio/` is **synthesised from scratch** by `Tools/audio/synth_sfx.py`. The generator
uses Python 3.11 with numpy only: no recordings, samples, network or generative services. Seeds are derived
from the sound names, so a rebuild is byte-identical. There are 67 files in 10 categories, and the folder
name is the category. This document covers the output contract, the loudness table, how each category is
synthesised, how the results were verified, and how to rebuild.

## Rebuild

```sh
# from the project root; about 45-80 s on this Mac, uses all cores
/Users/wz/.venvs/mushoku-bpy311/bin/python Tools/audio/synth_sfx.py
# options
... synth_sfx.py --only Fire Water              # categories
... synth_sfx.py --only fire_explosion ui_click # single sounds
... synth_sfx.py --list                         # every sound with duration, loop flag, loudness target, use
... synth_sfx.py --jobs 4 --no-preview
```

Outputs:

| Path | Content |
|---|---|
| `SourceArt/Audio/<Category>/<name>.wav` | 16-bit PCM WAV, 44.1 kHz (git-ignored, regenerate) |
| `SourceArt/Audio/manifest.json` | every file: `file`, `name`, `category`, `duration` (s), `loop`, `channels`, `spatial` (3D/2D), `use`, peak / true peak, integrated and momentary-max loudness, loudness target |
| `SourceArt/Audio/_Preview/<Category>.png` | contact sheet per category: waveform (+ dB envelope), log-frequency spectrogram, band-energy bar, and for loops a seam panel (±25 ms waveform and ±1 s spectrogram around the joint) |
| `SourceArt/Audio/_Preview/analysis.json` | extended QA metrics per file (band balance, seam metrics, gain / clip / limiter amounts, pre-fade tail level, stereo correlation, SHA-1 of the PCM) |

Determinism check: two builds with different `--jobs` produce byte-identical WAVs for all 67 files.

## Output contract (Spec section 9)

* **Format:** WAV, 16-bit PCM, 44 100 Hz, with TPDF dither (±1 LSB, seeded).
* **Channels:**
  * **mono** for everything played in 3D: spell one-shots, projectile and area loops, hits.
  * **stereo** for UI, ambience, music and the two 2D player-feedback cues in `Combat/` (`cooldown_ready`, `level_up`).
  * `manifest.json` has `channels` and `spatial` for every file.
* **Peaks:** -1 dBFS is a *ceiling*. Every file's sample peak is at or below -1.0 dBFS and its true peak (4x
  oversampled) at or below -1.0 dBTP. Loud one-shots sit exactly at the ceiling. Quiet-by-design sounds (UI
  ticks, beds) sit lower, at their loudness target.
* **Loops** are exactly periodic by construction (see "Seamless loops"). Loop them from sample 0 to the end;
  no crossfade is needed.

## Loudness policy

Loudness is measured per ITU-R BS.1770-4 / EBU R128, validated against the EBU Tech 3341 test cases (see
Verification):

* **One-shots** are normalised on **momentary-max loudness** (the loudest 400 ms window).
* **Loops, beds and music** are normalised on **integrated loudness**.
* **Mono files** are measured as one channel, per BS.1770. Played to both speakers they read about 3 dB louder.

Gameplay categories share one tier system, so Fire, Water, Wind, Earth, Orsted, Rudeus and Combat stay
consistent with each other:

| Tier | LUFS | Used for |
|---|---:|---|
| XL | -11 M | eruptions, huge impacts, awakenings (`inferno_eruption`, `crater_impact`, `water_dragon_impact`, `shockwave_boom`, `dragon_god_awaken`) |
| L | -12 M | big impacts and summons (`fire_explosion`, `stone_cannon_launch`, `flood_crash`, `earth_wall_rise`, `aura_activate`, `awakening`...) |
| M+ | -13 M | casts and medium impacts (`fireball_launch`, `flamewave_cast`, `tornado_start`, `wind_burst`, `rock_impact`, `hit_heavy`...) |
| M | -14 M | smaller impacts and casts (`water_splash`, `earth_spike`, `dragon_step`, `disturb_magic`, `demon_eye`, `hit_magic`...) |
| S | -16 M | wind-ups and small casts (`*_gather`, `stone_form`, `cast_small`, `wind_blade_swish`, `hit_light`, `mana_empty`) |
| XS | -18 M | `dodge_whoosh` |
| Loops | -17 / -19 / -21 I | big (tornado, flood) / medium (projectiles, rumble) / small beds (burning ground, quagmire, aura hum) |
| UI | -30 to -16 M | hover -30, tab -25, click -24, close -23, open -22, error -21, confirm / map -19, spawn -16 |
| Ambience | -25 I | all seven beds |
| Music | -17 I / -14 M | title loop (integrated) / spawn sting (momentary max) |
| 2D feedback | -19 / -15 M | `cooldown_ready` / `level_up` |

**Mastering chain** (`synth_sfx.master`), in order:

1. 18 Hz high-pass for DC and subsonics. It is circular on loops, which also get exact mean removal.
2. A 0.4 ms fade-in, and a fade-out of 4% of the length (10-250 ms), on one-shots only.
3. Gain toward the target.
4. A transient **soft clipper**, 4x oversampled, on the noisy impact categories only. It shaves at most 3 dB off
   millisecond transients and is never used on UI, ambience, music or tonal chimes.
5. A **look-ahead true-peak limiter** with a -1 dBTP ceiling (circular on loops). It typically does about 2 dB of
   gain reduction and is capped at 3-5 dB.
6. The target is iterated until it is reached or the peak budget runs out.

**Peak-limited sounds.** Very short, transient-dominated hits (`palm_strike`, `hit_light`, `earth_spike`,
`wind_blade_impact`, and to a lesser degree a few impacts) cannot reach their tier within 400 ms without
flattening their attack. Their gain is therefore capped: they are effectively peak-normalised to -1 dBTP and
read 0.2-3.6 LU under the tier. Short sounds are perceived louder than a 400 ms measure suggests, so this is
also the perceptually safer choice.

### Loudness table (measured on the written files)

M = momentary-max LUFS target, I = integrated LUFS target. "Seam step" is the joint's sample step relative to
the file's 99.9th-percentile step, where at most 1.0 means an ordinary step.

| File | s | Ch | Loop | Target (LUFS) | Measured | Peak dBTP | Note |
|---|---:|:-:|:-:|---|---:|---:|---|
| `Fire/fire_gather.wav` | 0.70 | 1 |  | M -16 | -16.1 | -1.0 |  |
| `Fire/fireball_launch.wav` | 1.00 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Fire/fireball_travel_loop.wav` | 2.00 | 1 | yes | I -19 | -19.0 | -6.3 | seam step 0.07 |
| `Fire/fire_explosion.wav` | 2.50 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Fire/flamewave_cast.wav` | 1.60 | 1 |  | M -13 | -13.0 | -2.0 |  |
| `Fire/flame_burn_loop.wav` | 3.00 | 1 | yes | I -21 | -21.0 | -6.5 | seam step 0.01 |
| `Fire/inferno_charge.wav` | 1.50 | 1 |  | M -14 | -14.0 | -4.1 |  |
| `Fire/inferno_eruption.wav` | 3.50 | 1 |  | M -11 | -11.1 | -1.0 |  |
| `Water/water_gather.wav` | 0.70 | 1 |  | M -16 | -16.0 | -1.4 |  |
| `Water/water_bullet_launch.wav` | 0.60 | 1 |  | M -14 | -14.1 | -1.0 |  |
| `Water/water_splash.wav` | 1.00 | 1 |  | M -14 | -14.0 | -1.0 |  |
| `Water/water_dragon_rise.wav` | 2.20 | 1 |  | M -12 | -12.0 | -1.0 |  |
| `Water/water_dragon_impact.wav` | 2.80 | 1 |  | M -11 | -11.8 | -1.0 | peak-limited (-0.8 LU) |
| `Water/flood_wave_loop.wav` | 3.00 | 1 | yes | I -17 | -17.0 | -2.8 | seam step 0.22 |
| `Water/flood_crash.wav` | 2.20 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Wind/wind_gather.wav` | 0.60 | 1 |  | M -16 | -16.0 | -1.1 |  |
| `Wind/wind_blade_swish.wav` | 0.50 | 1 |  | M -16 | -16.0 | -1.0 |  |
| `Wind/wind_blade_impact.wav` | 0.60 | 1 |  | M -14 | -15.4 | -1.0 | peak-limited (-1.4 LU) |
| `Wind/tornado_start.wav` | 1.60 | 1 |  | M -13 | -13.0 | -2.2 |  |
| `Wind/tornado_loop.wav` | 3.00 | 1 | yes | I -17 | -17.0 | -4.2 | seam step 0.28 |
| `Wind/wind_burst.wav` | 1.30 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Earth/stone_form.wav` | 0.70 | 1 |  | M -16 | -16.0 | -1.2 |  |
| `Earth/stone_spin_loop.wav` | 1.00 | 1 | yes | I -19 | -19.0 | -8.1 | seam step 0.70 |
| `Earth/stone_cannon_launch.wav` | 0.90 | 1 |  | M -12 | -13.0 | -1.0 | peak-limited (-1.0 LU) |
| `Earth/rock_impact.wav` | 1.50 | 1 |  | M -13 | -13.1 | -1.0 |  |
| `Earth/crater_impact.wav` | 2.80 | 1 |  | M -11 | -11.6 | -1.0 | peak-limited (-0.6 LU) |
| `Earth/earth_wall_rise.wav` | 1.80 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Earth/earth_spike.wav` | 0.80 | 1 |  | M -14 | -15.8 | -1.0 | peak-limited (-1.8 LU) |
| `Earth/quagmire_loop.wav` | 3.00 | 1 | yes | I -21 | -21.0 | -8.2 | seam step 0.08 |
| `Earth/ground_rumble_loop.wav` | 2.00 | 1 | yes | I -19 | -19.0 | -5.4 | seam step 0.02 |
| `Orsted/palm_strike.wav` | 0.50 | 1 |  | M -13 | -16.6 | -1.0 | peak-limited (-3.6 LU) |
| `Orsted/shockwave_boom.wav` | 1.60 | 1 |  | M -11 | -11.0 | -1.0 |  |
| `Orsted/dragon_step.wav` | 0.70 | 1 |  | M -14 | -14.1 | -1.0 |  |
| `Orsted/ground_crack.wav` | 1.30 | 1 |  | M -12 | -13.0 | -1.0 | peak-limited (-1.0 LU) |
| `Orsted/disturb_magic.wav` | 0.90 | 1 |  | M -14 | -14.1 | -1.0 |  |
| `Orsted/aura_activate.wav` | 1.80 | 1 |  | M -12 | -12.0 | -1.0 |  |
| `Orsted/aura_hum_loop.wav` | 3.00 | 1 | yes | I -21 | -21.0 | -9.9 | seam step 0.05 |
| `Orsted/dragon_god_awaken.wav` | 3.00 | 1 |  | M -11 | -11.2 | -1.0 | peak-limited (-0.2 LU) |
| `Rudeus/demon_eye.wav` | 1.10 | 1 |  | M -14 | -14.0 | -5.3 |  |
| `Rudeus/awakening.wav` | 2.20 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Rudeus/cast_small.wav` | 0.40 | 1 |  | M -16 | -16.0 | -2.5 |  |
| `Combat/hit_light.wav` | 0.35 | 1 |  | M -16 | -18.5 | -1.0 | peak-limited (-2.5 LU) |
| `Combat/hit_heavy.wav` | 0.70 | 1 |  | M -13 | -13.9 | -1.0 | peak-limited (-0.9 LU) |
| `Combat/hit_magic.wav` | 0.60 | 1 |  | M -14 | -14.9 | -1.0 | peak-limited (-0.9 LU) |
| `Combat/dodge_whoosh.wav` | 0.45 | 1 |  | M -18 | -18.0 | -1.5 |  |
| `Combat/mana_empty.wav` | 0.60 | 1 |  | M -16 | -16.0 | -2.5 |  |
| `Combat/cooldown_ready.wav` | 1.00 | 2 |  | M -19 | -19.0 | -14.4 |  |
| `Combat/death_thud.wav` | 1.20 | 1 |  | M -14 | -14.9 | -1.0 | peak-limited (-0.9 LU) |
| `Combat/level_up.wav` | 2.50 | 2 |  | M -15 | -15.0 | -7.8 |  |
| `UI/ui_hover.wav` | 0.08 | 2 |  | M -30 | -30.0 | -9.3 |  |
| `UI/ui_click.wav` | 0.12 | 2 |  | M -24 | -24.0 | -3.0 |  |
| `UI/ui_open.wav` | 0.45 | 2 |  | M -22 | -22.0 | -7.2 |  |
| `UI/ui_close.wav` | 0.42 | 2 |  | M -23 | -23.0 | -5.3 |  |
| `UI/ui_tab.wav` | 0.12 | 2 |  | M -25 | -25.0 | -5.5 |  |
| `UI/ui_confirm.wav` | 0.90 | 2 |  | M -19 | -19.0 | -12.3 |  |
| `UI/ui_spawn.wav` | 1.80 | 2 |  | M -16 | -16.0 | -7.0 |  |
| `UI/ui_error.wav` | 0.35 | 2 |  | M -21 | -21.0 | -12.0 |  |
| `UI/map_select.wav` | 1.40 | 2 |  | M -19 | -19.0 | -12.4 |  |
| `Ambience/amb_plains_wind.wav` | 24.00 | 2 | yes | I -25 | -25.0 | -10.4 | seam step 0.02 |
| `Ambience/amb_snow_wind.wav` | 24.00 | 2 | yes | I -25 | -25.0 | -8.9 | seam step 0.34 |
| `Ambience/amb_desert_wind.wav` | 24.00 | 2 | yes | I -25 | -25.0 | -9.3 | seam step 0.17 |
| `Ambience/amb_forest.wav` | 28.00 | 2 | yes | I -25 | -25.0 | -7.2 | seam step 0.62 |
| `Ambience/amb_demon.wav` | 30.00 | 2 | yes | I -25 | -25.0 | -7.5 | seam step 0.08 |
| `Ambience/amb_ocean.wav` | 28.00 | 2 | yes | I -25 | -25.0 | -6.0 | seam step 0.03 |
| `Ambience/amb_town.wav` | 30.00 | 2 | yes | I -25 | -25.0 | -8.2 | seam step 0.03 |
| `Music/music_title_theme.wav` | 105.00 | 2 | yes | I -17 | -17.0 | -1.0 | seam step 0.05 |
| `Music/music_spawn_sting.wav` | 6.00 | 2 |  | M -14 | -14.0 | -4.4 |  |

`manifest.json` holds the authoritative numbers after any rebuild.

## How the sounds are made

### Toolkit (`Tools/audio/dsp.py`, `Tools/audio/layers.py`)

* **Static filters:**
  * RBJ biquads (low / high / band pass, notch, peaking, shelves) and Butterworth cascades (`('lp4', f)`).
  * Applied with their exact IIR frequency response via FFT. `circular=True` gives the steady-state response of
    a periodic signal, so loops stay seamless.
  * A time-domain biquad (`biquad_td`) is kept as the reference the FFT path is validated against.
* **Time-varying filters:**
  * A constant-overlap-add Hann STFT with zero-phase gain curves (`sweep`, `formant_filter`), used for whooshes,
    swirls, howls, vowel formants and the growl.
  * A per-sample TPT state-variable filter (`svf`) for resonant IIR gestures.
* **Sources:**
  * Noise in white, pink, brown, blue or violet (FFT-coloured, hence periodic).
  * Smooth periodic random control curves.
  * PolyBLEP saw and pulse, triangle, sine, 2-op FM and FM bells.
  * Modal resonators (wood, stone, glass, metal).
  * Van den Doel bubbles and droplet chirps.
  * Granular crackle: Poisson events with Pareto amplitudes convolved with tick and pop kernels.
  * Bouncing debris: modal rock fragments with a restitution model.
* **Strings:**
  * Karplus-Strong evaluated exactly in the frequency domain:
    `H(f) = 1 / (1 - g(f) e^{-j2πfP/SR})`, with a fractional period P and a decay time chosen per frequency.
  * Tuning is exact at any pitch, measured within 0.13 cents.
* **Processing:**
  * Oversampled tanh saturation and a soft clipper.
  * Wavefolding.
  * A look-ahead true-peak limiter and an RMS compressor, both with a circular mode.
  * A differential-envelope transient shaper.
* **Space:**
  * Synthetic room impulse responses: sparse early reflections plus a noise tail with frequency-dependent RT60.
  * FFT convolution (linear or circular), sparse early-reflection FIRs, and discrete "terrain slap" echoes.
* **Stereo:** equal-power pan (static or moving), M/S width, random-phase decorrelators, Haas.

### Impact anatomy (all hit / explosion sounds)

Each impact stacks four parts:

1. A **transient**: a click (impulse plus HP noise, 1-2 ms) and a saturated band-limited "crack".
2. A **low thump**: a sine with an exponential pitch drop, at 150-200 Hz falling to 45-60 Hz. Big hits add a
   long **sub boom**, a 60-95 Hz to 24-34 Hz sweep with a brown-noise rumble.
3. A **body**: material-specific noise (pink or white) through static or moving filters, often saturated.
4. A **tail**: crackle, debris, bubbles or dust, plus a rumble.

The space is then added: short early reflections, discrete slap echoes (0.1-0.5 s, low-passed) and a short
dark room for the big ones.

The band-balance bar on each contact sheet guards against mud. The key rule came out of the analysis: sine
thumps and sub booms sit **8-13 dB below** the noise bodies after peak normalisation. A sine carries about
9 dB more energy than noise at the same peak, and at equal peaks it swallowed the headroom without adding
loudness. Non-bass layers are high-passed so only the thump and sub own the bottom octave.

### Per category

* **Fire.**
  * Roar is brown or pink noise under stacked random amplitude modulation (flicker), with a flame band and an
    air band, oversampled saturation, and brightness that moves over time.
  * Crackle and wood pops come from the granular engine, with a 4-20 Hz flame flutter.
  * The gathers add a rising sine shimmer (D5-E6) with an accelerating swirl band-pass.
  * `inferno_charge` layers a rising A1-E2-A2 saw drone with an opening filter, a "magic circle" ring whose
    tremolo accelerates from 6 to 18 Hz, and a noise riser.
  * `inferno_eruption` puts a sub boom and blast under a saturated pillar roar, an upward whoosh, a crackle
    storm, debris, slaps and a room.
* **Water.**
  * Bubbles use the van den Doel model (resonant chirps whose pitch rises), droplets are fast upward chirps with
    a contact click, and "rush" is band-limited noise under 28-60 Hz turbulence.
  * Splashes combine a low-passed slap, a thump, a white-noise spray whose low-pass closes, and bubble plus
    droplet rain.
  * The water dragon is a jittered saw with a sub-octave and breath, run through moving vowel formants
    (roughly "ah" to "oh") and a 32 Hz "vocal fry" amplitude wobble, riding a rising torrent.
* **Wind.**
  * Howls are parallel resonant band-passes (Q 9-20) on noise, with wobbling, rising centre frequencies and an
    accelerating swirl modulation.
  * Swishes are Doppler-shaped band-pass sweeps plus a narrow blade whistle (Q around 28).
  * The impact adds a metallic "shing" (modal partials at 3.1-7.7 kHz).
  * `wind_burst` is a slow-attack air whoomp followed by a falling gust.
* **Earth.**
  * Stick-slip grinding comes from dense band-passed noise grains. Crunch is 2000-4000 grains/s, and cracks are
    saturated band bursts.
  * Rock fragments are 5-mode modal models that bounce with a restitution model.
  * `stone_spin_loop` is a 38 Hz rotation pulse (whole cycles in 1 s) with harmonic hum.
  * Mud is slow wobbling "blorps" with noisy bodies and dull pops, plus sucking formant sweeps, low-passed at
    4.5 kHz.
* **Orsted.** Dragon God: very dry martial hits, deep booms, and dissonance built on A-Bb-E-Eb.
  * The palm strike is almost dry (three reflections within 12 ms): an air snap, a mid slap and a kick-like thud.
  * The shockwave is a sub boom and pressure body with distant low-passed terrain slaps.
  * `dragon_step` is a 2500 events/s granular "rip" under a fast rise-and-fall band-pass.
  * `disturb_magic` combines several hundred glass shards (modal pings at 2.5-11 kHz), a sagging dissonant
    glass cluster with ring modulation, and an FM zap.
  * The aura hum has every partial on a 1/3 Hz grid, so the 3 s loop is exact.
  * `dragon_god_awaken` combines a sub drone, a dissonant saw cluster, a formant "choir" (ah), a noise wall and
    a climax boom.
* **Rudeus.** Bright and hopeful: quartal and D-major FM chimes (D6-G6-A6-D7), shimmer partials at 3-5 kHz, an
  airy swirl, and a saw chord gliding up an octave into a surge.
* **Combat.** Hits follow the impact anatomy.
  * `mana_empty` is a tone that rises, then sags with erratic vibrato over a sputtering crackle.
  * `cooldown_ready` and `level_up` are stereo FM chimes in D major (A5+E6, and a D5-F#5-A5-D6 arpeggio with a
    sparkle shower and pad).
* **UI.** Warm and tactile.
  * The click is a 4-mode wooden resonator (620 Hz base, 4-13 ms decays), a paper tick and a small body.
  * Panels use paper crinkle (dense 0.1-0.5 ms grains) with a soft whoosh and a moving pan.
  * Confirm and map sounds are FM chimes.
  * `ui_spawn` is a riser plus a detuned saw chord gliding up two semitones, an accelerating KS pentatonic
    arpeggio, and a bell bloom at 1.05 s that decays inside the 1.8 s file.
* **Ambience.** Seven stereo beds built entirely circularly.
  * **Plains:** pink breeze with a gust-driven low-pass, gust-squared grass rustle, and FM birds (trill,
    "tee-oo", chip, warble) set distant with a low-pass and reverb.
  * **Snow:** four panned howl voices (Q 14-20) whose pitch drifts with the gusts, plus icy hiss.
  * **Desert:** sharp gusts, 6000 grains/s sand hiss gated by gust strength, and panned sand swirls.
  * **Forest:** leaf rustle from turbulence plus granular leaves, drips and double drips, stick-slip tree
    creaks, soft birds and a forest reverb.
  * **Demon:** an A1 / D#2 / A2 saw drone with every frequency a multiple of 1/30 Hz, which gives a slow 30 s
    beat and a 22.8 Hz tritone roughness. On top: an eerie whistle, dusty gusts, three distant rumbles, low-passed
    cracks, a formant "breath" and a 3 s dark reverb.
  * **Ocean:** four waves per loop. Each has a swell, a 0.35 s break, a crash whose low-pass opens then closes,
    foam fizz and pebble drag, and is built from a centre layer plus per-side layers.
  * **Town:** 14 "talkers", each a glottal saw plus breath through vowel formants that hop per syllable, with
    syllabic and phrase envelopes, low-passed and reverberated into an indistinct murmur. Distant blacksmith
    hammer bouts, wooden carts (rolling rumble with wheel clunks, panned across) and a few sparrow chips sit on
    top.
* **Music.**
  * `music_title_theme` is an original air in **D dorian**, 6/8 at dotted-quarter = 64, 56 bars = 105 s.
  * Form: intro (Dm C Dm Am | Dm F C G), tune A (16 bars), tune B (8 bars, F G Am / F G C), tune A reprise with
    grace-note cuts and a lute counter-line, and an outro that ends on G so the dorian IV-i leads back into
    bar 1.
  * **Harp:** frequency-domain KS with pitch-dependent T60 (5 s low, 3 s high) and velocity-dependent
    brightness, playing rolling 6/8 arpeggios with a slight 6/8 lilt.
  * **Lute:** double-course KS (±2.5 cents), with strums in A and B and chord-tone counter notes in the reprise.
  * **Strings:** a detuned saw pad under a slowly sweeping 4th-order low-pass, with a bass voice.
  * **Flute:** phrase-built, with 30 ms legato portamento, delayed vibrato at 5.3 Hz up to ±16 cents, tonguing
    dips, swells on long notes, breath turbulence, breath noise and chiff.
  * **Percussion:** a bodhrán-like frame drum and a shaker, playing only in the body sections.
  * Everything is placed into a circular buffer (tails wrap past the end) and goes through a circular hall
    (RT60 2.3 s), bus compressor and limiter. The file starts 25 ms before the bar-1 downbeat.
  * `music_spawn_sting` (6 s) is a D-major harp glissando, a flute trill and run, a pad swell, a drum roll into a
    boom, and an FM bell and crash bloom at 1.1 s.

### Seamless loops

Every loop is built to be exactly periodic:

* Noise is FFT-coloured over exactly the loop length.
* Static filters are circular, and STFT filters use circular framing with a hop that divides the length.
* Modulators are periodic random curves or LFOs with whole cycles per loop.
* Tonal partials sit on a 1/L Hz grid, and gliding oscillators get their phase nudged to a whole number of
  cycles.
* Events (bubbles, birds, waves, notes) wrap past the end into the start.
* Reverb, compressor and limiter all run circularly.

The joint is therefore just another sample: there is no crossfade and no click.

## Verification

Everything below is objective measurement. **No sound was auditioned by a human** (see Limitations).

* **Measurement tools validated:**
  * K-weighted loudness matches EBU Tech 3341: a stereo 1 kHz tone at -23 dBFS measures -22.99, one at -33
    measures -32.99, and the gated -26/-20/-26 case measures -22.98 (expected -23.0).
  * True peak of an fs/4 sine at 45° measures +0.09 dBTP (expected 0) with sample peak -3.01.
* **DSP validated:**
  * Frequency-domain biquads match the time-domain reference to within 4e-6.
  * The STFT is an exact identity (1e-15).
  * Circular filtering equals tiled linear filtering.
  * KS tuning is within 0.13 cents from D3 to C6.
  * The limiter holds its ceiling.
* **Per file** (`_Preview/analysis.json`):
  * Sample peak ≤ -1.0 dBFS and true peak ≤ -1.0 dBTP.
  * DC below 1e-3; ≤ 4e-4 in practice.
  * Durations and channel counts as specified.
  * Loudness on target within 0.1 LU, or capped by the peak rule as listed.
  * The level just before the final fade, so truncated tails are caught. All one-shots are at -17 dB or less
    relative to peak, except the two intentional hand-offs `inferno_charge` and `tornado_start`.
  * Stereo L/R correlation is always positive (0.07-0.98); a mono downmix loses at most 3 dB.
* **Loops.** The seam metrics are:
  * sample step and curvature at the joint versus the file's 99.9th percentile (all at most 0.93);
  * a high-frequency click z-score (all at most 2.4, typical of interior points);
  * a timbre-jump rank against interior points.

  Rotating a loop and measuring interior "joints" gives the same spread as the real joint. For the music, the
  joint is ranked against the other 55 bar downbeats and comes 11th, which is ordinary; the biggest change is
  bar 9, where drums and flute enter.
* **Music:**
  * The pitch-class histogram puts 95.3% of energy on the seven D dorian notes (D 22%, A 17%, F 13%, C 12%,
    G 11%, E 10%, B 10%, chromatic notes about 1% each).
  * A pitch-tracked flute phrase plays the written notes within 4 cents, with vibrato only on the long notes.
* **Contact sheets** (`_Preview/*.png`) were reviewed per category. Visible problems were fixed:
  * a sub-dominated explosion, bottom-heavy Earth and Combat thumps, sparse ambience events that were
    RMS-normalised into jump-scares;
  * truncated tails, cartoon-like slide-whistle bubbles, a wooden click that rang too long, and fully
    decorrelated ocean waves;
  * the music outro dipping before the loop point.

  Two artefacts are display-only:
  * A regular row of low-frequency blobs in `amb_demon` is the spectrogram hop aliasing the drone's 22.8 Hz
    beat.
  * Faint broadband "blobs" at the very end of a few chimes are fade splatter about 70 dB down.
* **Determinism:** rebuilds are byte-identical, including with a different worker count.

## Unreal import notes

* **Looping:** enable looping for every `*_loop`, all `Ambience/*` and `Music/music_title_theme`, looping the
  whole file.
* **Spatialisation:** mono files are 3D. Give them attenuation; typical falloff is 1500-4000 cm for spells and
  8000+ cm for XL impacts. Stereo files are 2D.
* **Suggested Sound Classes:** SFX, UI, Ambience and Music, all at volume 1.0. The relative levels are baked
  into the files.
* **Hand-offs:**
  * Start `inferno_eruption` as `inferno_charge` ends.
  * Start `tornado_loop` about 0.3 s before `tornado_start` ends; the start sound has a short release for the
    overlap.
  * Projectile loops pair with their launch and impact one-shots (`fireball_*`, `stone_spin_loop` with
    `stone_cannon_launch` / `rock_impact`).
* **Variation:** for frequently repeated one-shots (hits, `earth_spike`, `palm_strike`, `cast_small`,
  `dodge_whoosh`, UI clicks), randomise pitch by ±1-2 semitones and volume by ±1.5 dB in a SoundCue or
  MetaSound. The files are single takes.
* **Music:** `music_title_theme` loops seamlessly from start to end.
* **Ability mapping:** the `use` field names the ability IDs from `Content/Data/Abilities.json` that each file
  fits (for example `Fire_Fireball`, `Water_WaterDragon`, `Orsted_DisturbMagic`, `Rudeus_DemonEye`). The ability
  data currently points at placeholder `/Game/Audio/...` cues, so wiring it up is the importer's job.

## Limitations

* **Unheard.** The sounds were designed, and checked only with measurements and contact sheets (loudness,
  spectra, seams, pitch). Treat an in-engine listening pass as required before shipping; the numbers can't judge
  taste.
* **Synthetic character.** Fire, water and wind are noise-plus-model approximations, and birds are FM
  caricatures. The town "voices" are an abstract formant murmur, deliberately unintelligible and not speech.
  Next to recorded libraries some layers will sound stylised. That suits the painterly art direction, but it is
  not photorealistic.
* **Peak-limited short hits.** Up to 3.6 LU under their tier (see the table). Adjust per Sound Class if they feel
  quiet next to the big spells.
* **The music is one 105 s loop.** It has no stems or intensity layers, so over long title-screen idles it
  will repeat. The flute and strings are synthesis models, not sampled instruments.
* **Statistical seam metrics.** For textured loops these indicators come from sampling, so a single number can
  look high by chance. The construction is exactly periodic, which is the actual guarantee.
* **Build environment.** Python 3.11 with numpy + Pillow only (no scipy). Some filters run in pure Python loops,
  but the full build still takes about 1 minute.

## Source layout (`Tools/audio/`)

| File | Role |
|---|---|
| `synth_sfx.py` | entry point: registry loading, parallel build, mastering, WAV writing, manifest, analysis, previews |
| `reg.py` | `@sound(...)` registry, categories, loudness tiers |
| `dsp.py` | DSP toolkit (filters, STFT, oscillators, KS, FM, modal, bubbles, reverb, dynamics, stereo) |
| `layers.py` | reusable sound-design layers (click, crack, thump, sub boom, burst, whoosh, roar, crackle, bubbles, droplets, debris, grind, shimmer, bells, drone, space) |
| `analysis.py` | BS.1770 loudness, true peak, band balance, seam metrics, chroma, contact-sheet rendering |
| `sfx_fire.py`, `sfx_water.py`, `sfx_wind.py`, `sfx_earth.py` | element spells |
| `sfx_orsted.py` | Orsted and Rudeus |
| `sfx_combat.py`, `sfx_ui.py` | combat feedback and menu sounds |
| `ambience.py`, `music.py` | region beds; title theme, spawn sting and the music preview sheet |
