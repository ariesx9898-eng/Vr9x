# LA PLACE audio (procedural sound effects, ambience and music)

Every sound in `SourceArt/Audio/` is **synthesised from scratch** by `Tools/audio/synth_sfx.py`. The generator
uses Python 3.11 with numpy only: no recordings, samples, network or generative services. Seeds are derived from
the sound names, so a rebuild is byte-identical. There are 93 files in 10 categories, and the folder name is the
category: the original 67 plus 26 added for the ability overhaul (`Docs/Ability_Overhaul.md` section 6, see
"Ability overhaul sounds" below). This document covers the output contract, the loudness table, how each category
is synthesised, how the results were verified, and how to rebuild.

## Rebuild

```sh
# from the project root; about 1-2.5 minutes, uses all cores
/Users/wz/.venvs/mushoku-bpy311/bin/python Tools/audio/synth_sfx.py
# options
... synth_sfx.py --only Fire Water              # categories
... synth_sfx.py --only fire_explosion ui_click # single sounds
... synth_sfx.py --list                         # every sound with duration, loop flag, loudness target, use
... synth_sfx.py --jobs 4 --no-preview
```

`Tools/mac/build_and_setup.sh` only runs the generator when its marker file is missing. The marker is the newest sound,
`SourceArt/Audio/Rudeus/barrage_finale.wav`, so a machine that built the older sounds regenerates everything once. When
you add sounds, move the marker to one of them (or run `synth_sfx.py` by hand): otherwise the new WAVs do not exist and
the editor import skips their manifest entries.

Outputs:

| Path | Content |
|---|---|
| `SourceArt/Audio/<Category>/<name>.wav` | 16-bit PCM WAV, 44.1 kHz (git-ignored, regenerate) |
| `SourceArt/Audio/manifest.json` | every file: `file`, `name`, `category`, `duration` (s), `loop`, `channels`, `spatial` (3D/2D), `use`, peak / true peak, integrated and momentary-max loudness, loudness target |
| `SourceArt/Audio/_Preview/<Category>.png` | contact sheet per category: waveform (+ dB envelope), log-frequency spectrogram, band-energy bar, and for loops a seam panel (±25 ms waveform and ±1 s spectrogram around the joint) |
| `SourceArt/Audio/_Preview/analysis.json` | extended QA metrics per file (band balance, seam metrics, gain / clip / limiter amounts, pre-fade tail level, stereo correlation, SHA-1 of the PCM) |

Determinism check: two builds with different `--jobs` produce byte-identical WAVs for all 93 files. Adding the 26
overhaul sounds left the 67 earlier files byte-identical to the previous build (same SHA-1): the overhaul only
added new functions to the shared modules.

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
| XL | -11 M | eruptions, huge impacts, awakenings (`inferno_eruption`, `crater_impact`, `water_dragon_impact`, `shockwave_boom`, `dragon_god_awaken`, `spike_final`, `dragon_crush_impact`, `barrage_finale`) |
| L | -12 M | big impacts and summons (`fire_explosion`, `stone_cannon_launch`, `flood_crash`, `earth_wall_rise`, `aura_activate`, `awakening`, `sonic_boom`, `earth_wall_crumble`, `flamewave_roar`, `water_dragon_roar`...) |
| M+ | -13 M | casts and medium impacts (`fireball_launch`, `flamewave_cast`, `tornado_start`, `wind_burst`, `rock_impact`, `hit_heavy`, `quagmire_transform`, `earth_wall_crack`, `inferno_circle`, `water_lance`, `flood_gather`, `wind_slash`, `tornado_form`, `barrage_orbs`...) |
| M | -14 M | smaller impacts and casts (`water_splash`, `earth_spike`, `dragon_step`, `disturb_magic`, `demon_eye`, `hit_magic`, `stone_compress`, `fireball_charge`, `ground_crack_run`, `disturb_collapse`, `dragon_step_arrive`, `dragon_crush_charge`...) |
| S | -16 M | wind-ups and small casts (`*_gather`, `stone_form`, `cast_small`, `wind_blade_swish`, `hit_light`, `mana_empty`, `mud_squelch`, `wind_compress`, `seal`) |
| XS | -18 M | `dodge_whoosh`, `disturb_pulse` (almost silent by design), `inferno_pillar` (per pillar, see below) |
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
flattening their attack. Their gain is therefore capped: they are effectively peak-normalised to -1 dBTP and read
0.2-3.6 LU under the tier. Short sounds are perceived louder than a 400 ms measure suggests, so this is also the
perceptually safer choice. None of the 26 overhaul sounds needed this: all reach their tier within 0.1 LU.

**Charges and stacked sounds.**
* **Charge sounds** (`stone_compress`, `fireball_charge`) are the `CastSound`s of chargeable abilities. The game
  fades them out over 0.15 s on release, or 0.1 s on a cancel. Each is built as a steady crescendo, so a release
  at any moment cuts a build, never a gap. Its momentary maximum sits at full charge. It then settles within 35-40
  ms, so a charge held past full ends cleanly instead of being cut by the file fade. `dragon_crush_charge` (not
  chargeable) builds to the 0.40 s blow and settles in 18 ms.
* **Stacked sounds** take their tier as an event, not per copy. The Inferno plays `inferno_pillar` 30 times (3
  every 0.35 s, each about 1 s long). Simulated together at equal distance, they measure -11.5 to -10.8 LUFS
  momentary max at tier XS, so the whole eruption lands on XL like the single `inferno_eruption`. At tier M the
  same sequence reached -7.3 LUFS.

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
| `Fire/fireball_charge.wav` | 1.40 | 1 |  | M -14 | -14.0 | -1.9 | charge; full at 1.2 s, then settles |
| `Fire/flamewave_roar.wav` | 1.40 | 1 |  | M -12 | -12.0 | -1.0 |  |
| `Fire/inferno_circle.wav` | 1.20 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Fire/inferno_pillar.wav` | 1.10 | 1 |  | M -18 | -18.0 | -6.6 | per pillar; 30 per cast sum to XL |
| `Water/water_gather.wav` | 0.70 | 1 |  | M -16 | -16.0 | -1.4 |  |
| `Water/water_bullet_launch.wav` | 0.60 | 1 |  | M -14 | -14.1 | -1.0 |  |
| `Water/water_splash.wav` | 1.00 | 1 |  | M -14 | -14.0 | -1.0 |  |
| `Water/water_dragon_rise.wav` | 2.20 | 1 |  | M -12 | -12.0 | -1.0 |  |
| `Water/water_dragon_impact.wav` | 2.80 | 1 |  | M -11 | -11.8 | -1.0 | peak-limited (-0.8 LU) |
| `Water/flood_wave_loop.wav` | 3.00 | 1 | yes | I -17 | -17.0 | -2.8 | seam step 0.22 |
| `Water/flood_crash.wav` | 2.20 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Water/water_lance.wav` | 0.60 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Water/water_dragon_roar.wav` | 2.00 | 1 |  | M -12 | -12.0 | -1.0 |  |
| `Water/flood_gather.wav` | 1.00 | 1 |  | M -13 | -13.1 | -1.0 |  |
| `Wind/wind_gather.wav` | 0.60 | 1 |  | M -16 | -16.0 | -1.1 |  |
| `Wind/wind_blade_swish.wav` | 0.50 | 1 |  | M -16 | -16.0 | -1.0 |  |
| `Wind/wind_blade_impact.wav` | 0.60 | 1 |  | M -14 | -15.4 | -1.0 | peak-limited (-1.4 LU) |
| `Wind/tornado_start.wav` | 1.60 | 1 |  | M -13 | -13.0 | -2.2 |  |
| `Wind/tornado_loop.wav` | 3.00 | 1 | yes | I -17 | -17.0 | -4.2 | seam step 0.28 |
| `Wind/wind_burst.wav` | 1.30 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Wind/wind_slash.wav` | 0.60 | 1 |  | M -13 | -13.1 | -1.0 |  |
| `Wind/wind_compress.wav` | 0.30 | 1 |  | M -16 | -16.1 | -1.0 |  |
| `Wind/tornado_form.wav` | 1.20 | 1 |  | M -13 | -13.0 | -2.1 |  |
| `Earth/stone_form.wav` | 0.70 | 1 |  | M -16 | -16.0 | -1.2 |  |
| `Earth/stone_spin_loop.wav` | 1.00 | 1 | yes | I -19 | -19.0 | -8.1 | seam step 0.70 |
| `Earth/stone_cannon_launch.wav` | 0.90 | 1 |  | M -12 | -13.0 | -1.0 | peak-limited (-1.0 LU) |
| `Earth/rock_impact.wav` | 1.50 | 1 |  | M -13 | -13.1 | -1.0 |  |
| `Earth/crater_impact.wav` | 2.80 | 1 |  | M -11 | -11.6 | -1.0 | peak-limited (-0.6 LU) |
| `Earth/earth_wall_rise.wav` | 1.80 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Earth/earth_spike.wav` | 0.80 | 1 |  | M -14 | -15.8 | -1.0 | peak-limited (-1.8 LU) |
| `Earth/quagmire_loop.wav` | 3.00 | 1 | yes | I -21 | -21.0 | -8.2 | seam step 0.08 |
| `Earth/ground_rumble_loop.wav` | 2.00 | 1 | yes | I -19 | -19.0 | -5.4 | seam step 0.02 |
| `Earth/stone_compress.wav` | 1.80 | 1 |  | M -14 | -14.0 | -1.0 | charge; full at 1.6 s, then settles |
| `Earth/sonic_boom.wav` | 1.00 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Earth/quagmire_transform.wav` | 1.40 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Earth/mud_squelch.wav` | 0.45 | 1 |  | M -16 | -16.0 | -1.0 |  |
| `Earth/earth_wall_crack.wav` | 0.70 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Earth/earth_wall_crumble.wav` | 1.60 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Earth/spike_final.wav` | 1.60 | 1 |  | M -11 | -11.1 | -1.0 |  |
| `Earth/ground_crack_run.wav` | 0.80 | 1 |  | M -14 | -14.0 | -2.4 |  |
| `Orsted/palm_strike.wav` | 0.50 | 1 |  | M -13 | -16.6 | -1.0 | peak-limited (-3.6 LU) |
| `Orsted/shockwave_boom.wav` | 1.60 | 1 |  | M -11 | -11.0 | -1.0 |  |
| `Orsted/dragon_step.wav` | 0.70 | 1 |  | M -14 | -14.1 | -1.0 |  |
| `Orsted/ground_crack.wav` | 1.30 | 1 |  | M -12 | -13.0 | -1.0 | peak-limited (-1.0 LU) |
| `Orsted/disturb_magic.wav` | 0.90 | 1 |  | M -14 | -14.1 | -1.0 |  |
| `Orsted/aura_activate.wav` | 1.80 | 1 |  | M -12 | -12.0 | -1.0 |  |
| `Orsted/aura_hum_loop.wav` | 3.00 | 1 | yes | I -21 | -21.0 | -9.9 | seam step 0.05 |
| `Orsted/dragon_god_awaken.wav` | 3.00 | 1 |  | M -11 | -11.2 | -1.0 | peak-limited (-0.2 LU) |
| `Orsted/disturb_pulse.wav` | 0.50 | 1 |  | M -18 | -18.0 | -3.2 | almost silent by design |
| `Orsted/disturb_collapse.wav` | 0.90 | 1 |  | M -14 | -14.0 | -2.0 |  |
| `Orsted/seal.wav` | 0.80 | 1 |  | M -16 | -16.0 | -1.0 |  |
| `Orsted/dragon_step_arrive.wav` | 0.60 | 1 |  | M -14 | -14.0 | -1.0 |  |
| `Orsted/dragon_crush_charge.wav` | 0.50 | 1 |  | M -14 | -14.0 | -1.3 |  |
| `Orsted/dragon_crush_impact.wav` | 2.20 | 1 |  | M -11 | -11.1 | -1.0 |  |
| `Rudeus/demon_eye.wav` | 1.10 | 1 |  | M -14 | -14.0 | -5.3 |  |
| `Rudeus/awakening.wav` | 2.20 | 1 |  | M -12 | -12.1 | -1.0 |  |
| `Rudeus/cast_small.wav` | 0.40 | 1 |  | M -16 | -16.0 | -2.5 |  |
| `Rudeus/barrage_orbs.wav` | 0.80 | 1 |  | M -13 | -13.0 | -1.0 |  |
| `Rudeus/barrage_finale.wav` | 3.00 | 1 |  | M -11 | -11.0 | -1.0 |  |
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
* **Overhaul layers** (`layers.py`, added without touching the earlier layers):
  * `n_wave`: a supersonic N-wave (shock, linear fall through zero, second shock), built 8x oversampled with
    finite rise times and then band-limited exactly.
  * `rotor`: a spinning fluted object. Raised-cosine air pulses at the flute-pass rate are alias-free while they
    glide (`pulse_train` holds exactly k harmonics), and they fuse into a pitched whine as the spin accelerates.
  * `buzz`: a tight high vibration (an inharmonic cluster under fast, jittery amplitude modulation).
  * `groan`: stick-slip impulses exciting a low modal body. Creaks at slow rates, a pitched groan when fast.
  * `glug`: a run of big bubbles escaping through a narrow gap, each a little lower.
  * `gravel`: thousands of pebble ticks from a bank of modal rock kernels.
  * `boulder`: a big chunk landing (damped low modes, contact crunch, thump).
  * `comb_warp`: a flanger-style moving comb (warped air).
  * `place_reversed`: time-reversed puffs that are sucked in, for inward gestures.

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

### Ability overhaul sounds

The 26 sounds of `Docs/Ability_Overhaul.md` section 6. Each entry gives the tier, the ability field that plays it
in `Content/Data/Abilities.json`, and how it is made. Timings follow the ability's own numbers: cast times, charge
times, hit-stop and zone timelines.

**Earth**
* `stone_compress` (M; Rudeus Stone Cannon `CastSound`, hand-off to `sonic_boom`): fragments rip out of the ground
  and grind together (stick-slip grains, crunch, converging pebbles). The mass creaks under load (a `groan`
  speeding up from 14 to 75 Hz), then densifies into a smooth hiss. The slug's three grooves chop the air, and
  those whups accelerate from 7.5 Hz to a 1.26 kHz flute-pass whine (`rotor`). It ends in a tight 2.6-3.7 kHz
  vibration (`buzz`) with shimmering air and a quickening pressure throb, while reversed hisses pull dust inward.
  The 100 ms loudness climbs about 6.5 dB/s with no dip over 1.5 dB. Full spin holds to 1.6 s (`MaxChargeTime`),
  then settles in 35 ms.
* `sonic_boom` (L; `ReleaseSound`): a 13 ms N-wave with its ground reflection and shock cracks, the two shocks
  reading as one fat "k-RACK". The palm's pressure-cone thump and air blast land 4-7 ms behind it, so their peaks
  do not stack. Tearing air is a 3500 grains/s rip under a band-pass falling from 5.2 kHz to 650 Hz. The charge's
  spin whine is Doppler-dropped (990 Hz to 560 Hz) as the slug leaves.
* `quagmire_transform` (M+; Quagmire `AccentSound`, played when the zone appears; `quagmire_loop` carries on): it
  follows the zone's timeline.
  * 0-0.4 s: 13 dry ground cracks.
  * 0.3-0.95 s: water is forced up through them: a seeping hiss, rising bubbles, and `glug` runs.
  * From about 0.75 s: the soil liquefies: viscous mud blorps, a churning slosh, sucking formant sweeps.

  A moving low-pass darkens everything from 10 kHz to 2.8 kHz as the ground turns from dry to mud.
* `mud_squelch` (S; Quagmire `ImpactSound`, for splashes and ripples): a low-passed wet slap and a resonant
  "schlorp" gliding from 850 Hz to 240 Hz over a viscous blorp. Trapped air escapes as bubbles, then the mud
  closes round the ankle with a short suck and a wet tick. Everything sits under 4.5 kHz.
* `earth_wall_crack` (M+; Earth Wall crack stages, the `TravelSound` field): a hard crack over the slab's own body
  (seven damped modes from 190 Hz). A chain of 12 smaller cracks slows and dulls as the fracture runs through the
  stone, followed by crunch and a few chips.
* `earth_wall_crumble` (L; Earth Wall `AccentSound`): the slab gives with a failing crack and a shearing groan.
  The collapse is the loud part: 12 `boulder` impacts between 0.2 and 0.75 s over a gravel pour that peaks at 0.45
  s, pieces bouncing apart. Pebbles, dust and rumble then settle.
* `spike_final` (XL; Earth Spikes `AccentSound`, the final spike): a 35 ms ground bulge, kept short so the burst
  stays on the visual. The burst itself is a big low crack, sub boom, thump and saturated blast. The 7 m lance
  then scrapes upward: a grinding band climbs from 320 Hz to 2.4 kHz under a stick-slip squeal (140 to 420 Hz) and
  an upward rush. A deep stone lock marks full height at 0.47 s, followed by chunks and a debris rain, terrain
  slaps and a 1.6 s room.
* `ground_crack_run` (M; Earth Spikes `CastSound`): cracks race away every 15-50 ms, each further off (quieter,
  duller, spacing opening). Under them run a splitting grind and a low tearing groan, with stones popping out. A
  distance low-pass closes from 12 kHz to 2.5 kHz along the run.

**Fire**
* `fireball_charge` (M; Fireball `CastSound`, hand-off to `fireball_launch`): fire turns inward. Flame puffs
  played backwards converge on the palm ever closer together. An orbiting band speeds up (3 to 17 Hz) and climbs,
  the crackle tightens, and a blowtorch hiss takes over. A rising high-pass (60 to 420 Hz) thins the low roar: the
  sphere shrinks and heats from orange to white. It climbs about 8.9 dB/s, is full at 1.2 s, then settles.
* `flamewave_roar` (L; Flame Wave `AccentSound`): a soft-attack ignition whomp under three stacked roar voices,
  each with its own turbulence and brightness (a wall, not a jet), rolling at about 2.5 Hz. A broad whoosh falls
  from 2.8 kHz to 420 Hz and the roar darkens with distance as the front sweeps away. It pairs with
  `flamewave_cast` at the hand.
* `inferno_circle` (M+; Inferno `AccentSound`): three circles ignite in turn. Each is a run of nine burner puffs
  chasing round the ring, accelerating and rising, answered by a muted low FM "sigil" tone (D3, D#3, G#3). Under
  them a D/G# tritone drone and a formant "oom" swell up a minor third with a slow 2.2 Hz beat, and the fire rises
  with them.
* `inferno_pillar` (XS per pillar; Inferno `ImpactSound`): a soft-edged whoomp (6 ms attack, no click), a warm
  roar, an upward whoosh and sparse low crackle, with 4 dB less above 4 kHz. It measures centroid 427 Hz, 2.8 % of
  its energy at 2-6 kHz, and half-peak after 23 ms. `earth_spike`, for comparison: 2.1 kHz, 16 %, 4 ms. It is
  built to stay easy on the ear across 30 repeats; the tier is explained under Loudness policy.

**Water**
* `water_lance` (M+; Water Bullet `ReleaseSound`, `Barrage_WaterCannon` `TravelSound`): a hard hiss-crack onset (a
  sharp 5 kHz crack and a white burst). A pressurised jet holds for 0.17 s, then recedes: white noise falls from
  7.5 kHz to 3.2 kHz under 70-140 Hz turbulence, with a resonant whistle, a wet body band and a falling mass
  whoosh. Mist fizz and fine spray trail it.
* `water_dragon_roar` (L; Water Dragon `TravelSound`): a formant-filtered growl (a jittered saw, its sub-octave
  and breath, with a 38 Hz rattle) on a hunting contour: a lunge up to 118 Hz, a long guttural fall, then a second
  snarl as it turns. Gargling bubbles ride the voice over a torrent that surges with the serpent's roughly 3 Hz
  undulation.
* `flood_gather` (M+; Flood `CastSound`): water piles up. A rising torrent, a deepening surge and a hollow
  resonance swell upward (the wave's face), while splashes played backwards converge as incoming streams. It
  crests at 0.66 s, just before the 0.70 s release, and clears by 0.85 s for `flood_crash` and the wave.

**Wind**
* `wind_slash` (M+; Wind Blade `ReleaseSound`): an air snap and a low pressure push. The heavy blade whistle is
  three narrow resonances at 1.5-1.65 kHz, well below `wind_blade_swish`, Doppler-falling to 820 Hz as it leaves.
  A broadband tear falls from 4.2 kHz to 500 Hz, followed by a fluttering wake.
* `wind_compress` (S; Wind Burst `CastSound`): an inhale that accelerates to full compression at 0.115 s, the Wind
  Burst cast time. A band rises from 450 Hz to 2.6 kHz and a hollow resonance is pulled up from 320 Hz to 950 Hz,
  with a reversed low suck and a small pressure tick. It then dies away under `wind_burst`.
* `tornado_form` (M+; Tornado `AccentSound`; `tornado_loop` carries on): the swirl accelerates from 1.5 Hz to 9 Hz
  while the vortex forms (about 0.5 s), with rising howl bands and a low roar. The funnel touches down with a
  suction whump, then tears upward: a rush rising to 3.5 kHz, a ripping band climbing from 700 Hz to 5 kHz, a
  whistle gliding up, and debris flung up the column. It releases by 1.12 s.

**Orsted**
* `disturb_pulse` (XS; Disturb Magic `CastSound`): a thin chirp (3.2 kHz to 900 Hz) whose phase bends as it falls,
  a faint lower echo, and a pale 7.8 kHz glyph tick. A flanged ripple of warped air recedes (comb delay 0.3 to 4
  ms). It is high-passed at 250 Hz and almost silent by design.
* `disturb_collapse` (M; Disturb Magic `ImpactSound`, the `Collapse<Element>` phase): the construct crumbles like
  sugar glass, a dense cascade of small soft shards tumbling down in pitch. Its energy deflates: a chord sags 1.25
  octaves while its tremolo slows from 18 Hz to 4 Hz, over a falling "pfff" and a soft implosion.
* `seal` (S; Disturb Magic `AccentSound`, the `Seal` phase): a muted whirr and a ratchet of four clicks accelerate
  into the clamp, a dense damped double knock (the jaws meet, then seat) over a low thud. A low A/Bb hum and a
  muted glyph glow then fade. Everything is low-passed at 3.5 kHz.
* `dragon_step_arrive` (M; Dragon Step `AccentSound`): air rushes into the space in a short reversed puff, then
  the pressure lands. An air snap, a low-mid "fwump" of displaced air, a short thump, a pressure ring falling from
  2.6 kHz to 240 Hz and a dust hiss. It is dry (four reflections within 14 ms), with a trace of the A/E aura.
* `dragon_crush_charge` (M; Dragon Crush `CastSound`): the aura is squeezed. A pressure band narrows (Q 1.5 to 10)
  and climbs from 260 Hz to 950 Hz. Tension creaks speed up from 18 Hz to 95 Hz like a drawn cord, and a ratchet
  accelerates over a dark A/Bb/E cluster. It peaks with the blow at 0.4 s and settles in 18 ms.
* `dragon_crush_impact` (XL; Dragon Crush `ImpactSound`): two stages framing the 0.085 s hit-stop.
  * The blow: an air snap, a slap and a kick-like thud, with the sub boom already underneath.
  * As the freeze releases, the ground breaks: big splitting cracks, a deeper second hit, the saturated shockwave
    body and a falling pressure ring. Raised slabs grind back (a groan slowing from 45 Hz to 18 Hz), and chunks
    land over 0.4-1.2 s.

  A debris rain, a gravel trickle and a long rumble form the tail.

**Rudeus**
* `barrage_orbs` (M+; Elemental Barrage `CastSound`): the four formations ignite 0.1 s apart, each in its own
  voice, and Rudeus's chime marks each one (D5, F#5, A5, D6):
  * fire at 0.03 s: an ignition puff, roar and crackle;
  * water at 0.13 s: a splash, a glug, a swirling rush and bubbles;
  * earth at 0.23 s: a crack and a stone knock as fragments snap together;
  * wind at 0.33 s: a whistle flick up to 3 kHz and a circling howl.

  A mana shimmer then settles over the orbiting set.
* `barrage_finale` (XL; `Barrage_Finale` `ImpactSound`): one detonation (crack, sub boom, thump) shared by four
  element layers:
  * fire expands through the centre: blast body, rolling fireball roar, embers;
  * earth erupts outward: a crack run, crunch, chunks and a debris rain;
  * water spirals up: a splash, a whirling column whose brightness orbits upward, foam, then a rain of drops;
  * wind drives it all out: a falling gust, a pressure whump, a howl.

  A faint D-major bloom signs it as Rudeus's.

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
* **Determinism:** rebuilds are byte-identical, including with a different worker count (93 of 93 across
  `--jobs 4` and `--jobs 2`). The 67 earlier files kept their SHA-1 when the overhaul sounds were added.
* **Ability overhaul (26 files)**, all read back from the written WAVs:
  * **Contract:** every file is at its tier within 0.1 LU (-0.10 to 0.00), with none peak-capped. True peaks are
    at or below -1.03 dBTP, and pre-fade tails at -20.1 dB or less against the peak. DC is at most 3e-4.
    Durations, mono and 3D all match the spec.
  * **Charges:** the 100 ms loudness of `stone_compress` and `fireball_charge` rises steadily (6.5 and 8.9 dB/s,
    largest dip 1.5 dB), so the game's release fade always cuts a build, never a gap. A 0.35 s tap sits about 10
    dB under full charge.
  * **Stacking:** the full Inferno (30 pillars) measures -11.5 to -10.8 LUFS momentary max (see Loudness policy).
  * **Distinctness:** compared against every other sound using spectro-temporal fingerprints (1/3-octave dB
    spectrograms, resampled to 48 frames, with the library average removed). Each new sound's nearest existing
    neighbour correlates 0.39-0.84 (median 0.65). The 67 earlier sounds already reach 0.80 median (0.92 max) with
    their own nearest neighbours, so no new sound is closer to an existing one than the library's usual spread.
    The closest pairs are:
    * `dragon_crush_charge` / `dragon_god_awaken`, 0.84: both are rising Orsted builds, at 0.5 s against 3 s;
    * `wind_compress` / `dodge_whoosh`, 0.82.
  * **Contact sheets** were reviewed after every pass. The first drafts had problems that were fixed:
    * sub-heavy impacts that could not reach their tier under the peak ceiling (sub shares up to 80 %);
    * a charge whose crescendo only arrived in its last 0.3 s;
    * a fire charge that did not brighten;
    * blurred ignition runs (`inferno_circle`, `barrage_orbs`);
    * two tails at the -17 dB limit;
    * a 75 ms lead-in that would have put `spike_final`'s burst behind its visual.

## Unreal import notes

* **Looping:** enable looping for every `*_loop`, all `Ambience/*` and `Music/music_title_theme`, looping the
  whole file.
* **Spatialisation:** mono files are 3D. Give them attenuation; typical falloff is 1500-4000 cm for spells and
  8000+ cm for XL impacts. Stereo files are 2D.
* **Suggested Sound Classes:** SFX, UI, Ambience and Music, all at volume 1.0. The relative levels are baked
  into the files.
* **Import:** `setup_audio` in `Content/Python/mt_setup_laplace.py` reads `manifest.json` and imports every entry
  whose WAV exists as `/Game/LaPlace/Audio/<category>/<name>`. It sets looping from `loop` and gives `spatial: 3D`
  files the `ATT_Spell` attenuation, so new sounds need no importer change. The WAVs must exist first (see
  Rebuild).
* **Hand-offs:**
  * Start `inferno_eruption` as `inferno_charge` ends.
  * Charges fade on release (0.15 s): `stone_compress` into `sonic_boom`, `fireball_charge` into
    `fireball_launch`.
  * `dragon_crush_charge` peaks at 0.40 s, where `dragon_crush_impact` starts. `dragon_crush_impact`'s ground
    break sits 85 ms after the blow, on the hit-stop release.
  * `wind_compress` is fully compressed at 0.115 s (`wind_burst` follows).
  * `flood_gather` crests at 0.66 s, before the 0.70 s release.
  * `quagmire_transform` hands over to `quagmire_loop`, and `tornado_form` to `tornado_loop` (released by 1.12 s).
  * Start `tornado_loop` about 0.3 s before `tornado_start` ends; the start sound has a short release for the
    overlap.
  * Projectile loops pair with their launch and impact one-shots (`fireball_*`, `stone_spin_loop` with
    `stone_cannon_launch` / `rock_impact`).
* **Variation:** for frequently repeated one-shots (hits, `earth_spike`, `palm_strike`, `cast_small`,
  `dodge_whoosh`, `inferno_pillar`, `mud_squelch`, `earth_wall_crack`, `ground_crack_run`, `dragon_step_arrive`,
  `water_lance`, `wind_slash`, UI clicks), randomise pitch by ±1-2 semitones and volume by ±1.5 dB in a SoundCue
  or MetaSound. The files are single takes.
* **Stacking:** the files carry per-event levels, so copies that fire together add up.
  * `inferno_pillar` already accounts for its 30 copies.
  * `earth_wall_crumble` plays once per wall segment. At expiry the seven segments crumble within about 0.18 s: at
    full volume that measures about -1.6 LUFS momentary max, and with the rise's rule (the first segment at 1.0,
    the rest at 0.4) about -8.2. The game applies that rule to an expiring wall (`AMTEarthWall::Crumble(true)`);
    a segment broken by hits crumbles at full volume.
  * If the Elemental Barrage used the element launch sounds for its 15 shots, the stream would measure about -9
    LUFS.
* **Music:** `music_title_theme` loops seamlessly from start to end.
* **Ability mapping:** the `use` field names the ability IDs from `Content/Data/Abilities.json` that each file
  fits (for example `Fire_Fireball`, `Water_WaterDragon`, `Orsted_DisturbMagic`, `Rudeus_DemonEye`). The rows
  reference sounds as `/Game/LaPlace/Audio/<Category>/<name>.<name>`. All 61 sounds they reference, including the
  26 overhaul sounds and the loops the zones and projectiles carry, resolve to manifest entries. Other rows still point at placeholder `SC_*` cues.

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
* **Charges are one-shots.** The game holds a full charge for up to 1.5 s more before firing, which is 3.1 s for
  Rudeus's cannon. Past the file (`stone_compress` 1.8 s, `fireball_charge` 1.4 s) the build-up has settled, so at
  full charge the game fades in the projectile's travel loop at the hand (`stone_spin_loop`,
  `fireball_travel_loop`; `UMTAbility::StartHoldAudio`) and fades it out on release.
* **Timing choices to confirm in engine.** `dragon_crush_impact` puts the ground break 85 ms after the blow, on
  the hit-stop release, while the impact effect starts at the blow. That is inside the range where late audio is
  usually not noticed as out of sync (ITU-R BT.1359 puts detectability at about 125 ms late), but judge it in
  play.
* **The music is one 105 s loop.** It has no stems or intensity layers, so over long title-screen idles it
  will repeat. The flute and strings are synthesis models, not sampled instruments.
* **Statistical seam metrics.** For textured loops these indicators come from sampling, so a single number can
  look high by chance. The construction is exactly periodic, which is the actual guarantee.
* **Build environment.** Python 3.11 with numpy + Pillow only (no scipy). Some filters run in pure Python loops,
  but the full build still takes 1-2.5 minutes. Contact sheets use Menlo on macOS and DejaVu Sans Mono on Linux.

## Source layout (`Tools/audio/`)

| File | Role |
|---|---|
| `synth_sfx.py` | entry point: registry loading, parallel build, mastering, WAV writing, manifest, analysis, previews |
| `reg.py` | `@sound(...)` registry, categories, loudness tiers |
| `dsp.py` | DSP toolkit (filters, STFT, oscillators, KS, FM, modal, bubbles, reverb, dynamics, stereo) |
| `layers.py` | reusable sound-design layers (click, crack, thump, sub boom, burst, whoosh, roar, crackle, bubbles, droplets, debris, grind, shimmer, bells, drone, space; overhaul: N-wave, rotor, buzz, groan, glug, gravel, boulder, comb warp, reversed placement) |
| `analysis.py` | BS.1770 loudness, true peak, band balance, seam metrics, chroma, contact-sheet rendering |
| `sfx_fire.py`, `sfx_water.py`, `sfx_wind.py`, `sfx_earth.py` | element spells; each file ends with its ability-overhaul block |
| `sfx_orsted.py` | Orsted and Rudeus, each with an ability-overhaul block |
| `sfx_combat.py`, `sfx_ui.py` | combat feedback and menu sounds |
| `ambience.py`, `music.py` | region beds; title theme, spawn sting and the music preview sheet |
