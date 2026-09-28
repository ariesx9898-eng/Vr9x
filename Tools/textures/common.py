"""Paths and output helpers shared by the LA PLACE texture builders."""
import os
import zlib

import numpy as np

import texlib as T

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
AI_DIR = os.path.join(ROOT, "SourceArt", "AI", "Textures")
OUT_DIR = os.path.join(ROOT, "SourceArt", "Textures")
DOC_IMG = os.path.join(ROOT, "Docs", "Images")


def rng_for(name, salt=0):
    """Deterministic generator per material (independent of build order / process)."""
    return np.random.default_rng(zlib.crc32(name.encode()) + 7919 * salt)


def ai_path(name):
    return os.path.join(AI_DIR, name + ".png")


def load_ai(name, mode="RGB"):
    p = ai_path(name)
    if not os.path.exists(p):
        raise FileNotFoundError(f"missing AI source {p} (see Docs/LaPlace/Textures.md)")
    return T.load(p, mode)


def out_path(group, fname):
    d = os.path.join(OUT_DIR, group)
    os.makedirs(d, exist_ok=True)
    return os.path.join(d, fname)


def save_set(group, prefix, D, N, M, E=None):
    """Write T_<...>_D / _N / _M (/ _E) PNGs and return the relative file names."""
    files = {}
    for key, img in (("D", D), ("N", N), ("M", M), ("E", E)):
        if img is None:
            continue
        fn = f"{prefix}_{key}.png"
        T.save(out_path(group, fn), img)
        files[key] = f"{group}/{fn}"
    return files
