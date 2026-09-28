#!/usr/bin/env python3
"""synth_sfx.py - procedural sound effects, ambience and music for LA PLACE.

Every sound is synthesised from scratch (numpy only, deterministic seeds derived from the sound
name), mastered, and written as 16-bit PCM WAV at 44.1 kHz:

  SourceArt/Audio/<Category>/<name>.wav     mono for 3D one-shots, stereo for UI / ambience / music
  SourceArt/Audio/manifest.json             file, category, duration, loop flag, intended use + metrics
  SourceArt/Audio/_Preview/<Category>.png   waveform + spectrogram contact sheet per category
  SourceArt/Audio/_Preview/analysis.json    extended QA metrics (band balance, seams, gain reduction)

Mastering per file: 18 Hz DC/subsonic high-pass (circular for loops) -> short fades on one-shots ->
loudness normalisation to the sound's target (momentary-max LUFS for one-shots, integrated LUFS for
loops, beds and music) -> look-ahead true-peak limiter at -1 dBTP (circular for loops, gain
reduction capped per sound so transients are not flattened) -> TPDF-dithered 16-bit PCM.

Usage (from the project root):
  /Users/wz/.venvs/mushoku-bpy311/bin/python Tools/audio/synth_sfx.py              # everything
  ... Tools/audio/synth_sfx.py --only Fire Water                                   # categories
  ... Tools/audio/synth_sfx.py --only fire_explosion ui_click                      # single sounds
  ... Tools/audio/synth_sfx.py --jobs 8 --no-preview
See Docs/LaPlace/Audio.md for the sound design notes and loudness table.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys
import time
import wave
from concurrent.futures import ProcessPoolExecutor

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import analysis  # noqa: E402
import dsp  # noqa: E402
from dsp import SR, db2lin, eq, fade, fit, limiter, make_rng, ns  # noqa: E402
from reg import CATEGORIES, SOUNDS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "SourceArt", "Audio")
PREVIEW = os.path.join(OUT, "_Preview")
CEILING_DB = -1.0  # spec: peaks at -1 dBFS (true-peak ceiling)
GENERATOR_VERSION = 1


RECIPE_MODULES = ["sfx_fire", "sfx_water", "sfx_wind", "sfx_earth", "sfx_orsted", "sfx_combat", "sfx_ui",
                  "ambience", "music"]


def load_recipes():
    import importlib
    for m in RECIPE_MODULES:
        if os.path.exists(os.path.join(HERE, m + ".py")):
            importlib.import_module(m)


def _measure(x, spec) -> float:
    L = analysis.loudness(x)
    return L["lufs_i"] if spec.metric == "I" else L["lufs_m_max"]


def master(x, spec):
    """Returns (float signal, info dict)."""
    n = ns(spec.dur)
    x = np.asarray(x, dtype=np.float64)
    want_ch = 2 if spec.stereo else 1
    have_ch = x.shape[0] if x.ndim == 2 else 1
    if have_ch != want_ch:
        raise ValueError("%s: recipe returned %d channels, expected %d" % (spec.name, have_ch, want_ch))
    if x.shape[-1] != n:
        raise ValueError("%s: recipe returned %d samples, expected %d" % (spec.name, x.shape[-1], n))
    if not np.all(np.isfinite(x)):
        raise ValueError("%s: non-finite samples" % spec.name)
    x = eq(x, ("hp", 18.0), circular=spec.loop)
    if spec.loop:
        x = x - x.mean(axis=-1, keepdims=True)
    else:
        x = fade(x, 0.0004, min(max(0.04 * spec.dur, 0.01), 0.25))
    ceiling = CEILING_DB - 0.03  # leaves room for dither / inter-sample estimate error

    def chain(g):
        z = x * db2lin(g)
        clip_red = 0.0
        if spec.clip_db > 0:
            pk0 = float(np.max(np.abs(z)))
            # clip transients down to ceiling + 2 dB (untouched below ceiling + 0.5 dB); the limiter then
            # removes the last ~2 dB, so it only ducks what follows a transient by a small amount
            z = dsp.soft_clip(z, ceiling + 2.0, knee_db=1.5)
            clip_red = float(dsp.lin2db(pk0 / max(float(np.max(np.abs(z))), 1e-12)))
        z, gr_ = limiter(z, ceiling, circular=spec.loop)
        return z, gr_, clip_red

    gain = spec.target - _measure(x, spec)
    y, gr, cr = chain(gain)
    capped = False
    for _ in range(10):
        m = _measure(y, spec)
        err = spec.target - m
        excess = max(gr - spec.max_gr, cr - spec.clip_db)
        if excess > 0.05:  # too much peak reduction: back the gain off instead of squashing
            gain -= min(excess, 6.0)
            capped = True
        elif abs(err) > 0.1 and not (capped and err > 0):
            gain += err
        else:
            break
        y, gr, cr = chain(gain)
    y = np.clip(y, -db2lin(CEILING_DB), db2lin(CEILING_DB))
    return y, {"gain_db": round(float(gain), 2), "limiter_gr_db": round(float(gr), 2), "clip_db": round(float(cr), 2),
               "target_capped": capped}


def to_pcm16(y, rng):
    Y = y if y.ndim == 2 else y[None, :]
    d = rng.random(Y.shape) - rng.random(Y.shape)  # TPDF dither, +-1 LSB
    q = np.round(Y * 32767.0 + d)
    return np.clip(q, -32768, 32767).astype(np.int16)


def write_wav(path, pcm):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(pcm.shape[0])
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(np.ascontiguousarray(pcm.T).tobytes())


def read_wav(path):
    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        assert w.getsampwidth() == 2 and w.getframerate() == SR
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
    return data.reshape(-1, ch).T.astype(np.float64) / 32768.0


def rel_path(spec):
    return "%s/%s.wav" % (spec.category, spec.name)


def build_one(name):
    load_recipes()
    spec = SOUNDS[name]
    t0 = time.time()
    rng = make_rng("laplace-audio", spec.category, name)
    x = spec.fn(rng, ns(spec.dur))
    y, info = master(x, spec)
    pcm = to_pcm16(y, make_rng("dither", name))
    path = os.path.join(OUT, rel_path(spec))
    write_wav(path, pcm)
    info["render_s"] = round(time.time() - t0, 2)
    info["sha1"] = hashlib.sha1(pcm.tobytes()).hexdigest()
    return name, info


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", nargs="*", help="categories and/or sound names to build")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument("--no-preview", action="store_true", help="skip contact sheets")
    ap.add_argument("--list", action="store_true", help="list registered sounds and exit")
    args = ap.parse_args(argv)
    load_recipes()
    specs = [s for c in CATEGORIES for s in SOUNDS.values() if s.category == c]
    if args.list:
        for s in specs:
            print("%-8s %-24s %6.2fs %s %s %s%.1f  %s" % (s.category, s.name, s.dur, "loop" if s.loop else "    ",
                                                        "st" if s.stereo else "mo", s.metric, s.target, s.use))
        return 0
    if args.only:
        want = set(args.only)
        specs = [s for s in specs if s.category in want or s.name in want]
        if not specs:
            print("nothing matches --only %s" % " ".join(args.only))
            return 1
    t0 = time.time()
    order = sorted(specs, key=lambda s: -s.dur * (2 if s.stereo else 1))
    infos = {}
    if args.jobs > 1 and len(order) > 1:
        with ProcessPoolExecutor(max_workers=args.jobs) as ex:
            for name, info in ex.map(build_one, [s.name for s in order]):
                infos[name] = info
                print("  built %-24s %5.1fs  gain %+6.1f dB  clip %3.1f  GR %4.1f dB%s" % (
                    name, info["render_s"], info["gain_db"], info["clip_db"], info["limiter_gr_db"],
                    "  (target capped)" if info["target_capped"] else ""), flush=True)
    else:
        for s in order:
            name, info = build_one(s.name)
            infos[name] = info
            print("  built %-24s %5.1fs  gain %+6.1f dB  GR %4.1f dB" % (name, info["render_s"], info["gain_db"], info["limiter_gr_db"]), flush=True)
    print("rendered %d sounds in %.1fs" % (len(specs), time.time() - t0))

    # ---- analysis of the files actually written (read back from disk) + manifest
    man_path = os.path.join(OUT, "manifest.json")
    ana_path = os.path.join(PREVIEW, "analysis.json")
    manifest = json.load(open(man_path)) if os.path.exists(man_path) else {}
    ana = json.load(open(ana_path)) if os.path.exists(ana_path) else {}
    rows = {r["file"]: r for r in manifest.get("files", [])}
    datas = {}
    for s in specs:
        path = os.path.join(OUT, rel_path(s))
        X = read_wav(path)
        datas[s.name] = X
        sp = getattr(s.fn, "seam_points", None)
        st = analysis.analyse(X, loop=s.loop, seam_points=sp(X.shape[-1]) if sp else None)
        info = infos.get(s.name, {})
        ana[s.name] = dict(st, target={"metric": s.metric, "lufs": s.target}, **info)
        rows[rel_path(s)] = {
            "file": rel_path(s),
            "name": s.name,
            "category": s.category,
            "duration": round(X.shape[-1] / SR, 4),
            "loop": s.loop,
            "channels": X.shape[0],
            "spatial": s.spatial,
            "use": s.use,
            "peak_dbfs": st["peak_dbfs"],
            "true_peak_dbtp": st["true_peak_dbtp"],
            "lufs_integrated": st["lufs_i"],
            "lufs_momentary_max": st["lufs_m_max"],
            "loudness_target": {"metric": "integrated" if s.metric == "I" else "momentary_max", "lufs": s.target},
        }
    all_specs = [sp for c in CATEGORIES for sp in SOUNDS.values() if sp.category == c]
    order_idx = {rel_path(sp): i for i, sp in enumerate(all_specs)}
    manifest = {
        "generator": "Tools/audio/synth_sfx.py (procedural, numpy only, deterministic)",
        "generator_version": GENERATOR_VERSION,
        "format": {"container": "WAV", "encoding": "PCM", "bit_depth": 16, "sample_rate": SR,
                   "channels": "mono for 3D one-shots and 3D loops, stereo for UI, 2D feedback, ambience and music"},
        "peak_ceiling_dbtp": CEILING_DB,
        "loudness_note": "One-shots are normalised on momentary-max loudness (400 ms, BS.1770), loops / beds / "
                         "music on integrated loudness. Mono files are measured as one channel.",
        "files": sorted(rows.values(), key=lambda r: order_idx.get(r["file"], 1 << 30)),
    }
    with open(man_path, "w") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")
    os.makedirs(PREVIEW, exist_ok=True)
    with open(ana_path, "w") as f:
        json.dump(dict(sorted(ana.items())), f, indent=1)
        f.write("\n")
    print("wrote %s (%d files)" % (os.path.relpath(man_path, ROOT), len(manifest["files"])))

    if not args.no_preview:
        cats = [c for c in CATEGORIES if any(s.category == c for s in specs)]
        for c in cats:
            ents = []
            for s in all_specs:
                if s.category != c:
                    continue
                X = datas.get(s.name)
                if X is None:
                    p = os.path.join(OUT, rel_path(s))
                    if not os.path.exists(p):
                        continue
                    X = read_wav(p)
                st = ana.get(s.name) or analysis.analyse(X, loop=s.loop)
                ents.append({"name": s.name, "data": X, "stats": st, "loop": s.loop,
                             "target": ("I" if s.metric == "I" else "M", s.target)})
            if c == "Music":
                import music
                music.preview_sheet(ents, os.path.join(PREVIEW, "Music.png"))
            else:
                analysis.contact_sheet(ents, os.path.join(PREVIEW, "%s.png" % c), "LA PLACE audio - %s" % c,
                                       cols=1 if c == "Ambience" else 2)
            print("preview %s" % os.path.relpath(os.path.join(PREVIEW, c + ".png"), ROOT))
    print("done in %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
