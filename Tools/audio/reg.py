"""Sound registry: recipe modules register their generator functions with @sound(...)."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

SOUNDS: dict = {}

# Loudness tiers shared by every gameplay category (LUFS). One-shots: momentary-max (400 ms) loudness;
# loops: integrated loudness. Keeps Fire / Water / Wind / Earth / Orsted / Rudeus / Combat consistent.
XL, L, MP, M, S, XS = -11.0, -12.0, -13.0, -14.0, -16.0, -18.0
LOOP_BIG, LOOP_MED, LOOP_SMALL = -17.0, -19.0, -21.0

# Categories in build / manifest order.
CATEGORIES = ["Fire", "Water", "Wind", "Earth", "Orsted", "Rudeus", "Combat", "UI", "Ambience", "Music"]


@dataclass
class Spec:
    name: str
    category: str
    dur: float
    loop: bool
    stereo: bool
    spatial: str
    metric: str  # "M" = momentary-max LUFS target (one-shots), "I" = integrated LUFS (loops, beds, music)
    target: float
    max_gr: float  # max limiter gain reduction allowed while reaching the target (dB)
    clip_db: float  # max transient soft-clipping before the limiter (dB); 0 for tonal / UI / beds / music
    use: str
    fn: Callable = field(repr=False)


def sound(category: str, dur: float, *, target: float, use: str, loop: bool = False, stereo: bool = False,
          spatial: str | None = None, metric: str | None = None, max_gr: float = 5.0, clip_db: float | None = None):
    """Register fn(rng, n) -> np.ndarray ((n,) mono or (2, n) stereo) as a game sound."""

    def deco(fn):
        SOUNDS[fn.__name__] = Spec(
            name=fn.__name__, category=category, dur=dur, loop=loop, stereo=stereo,
            spatial=spatial or ("2D" if stereo else "3D"),
            metric=metric or ("I" if loop or category in ("Ambience", "Music") else "M"),
            target=target, max_gr=max_gr,
            clip_db=clip_db if clip_db is not None else (0.0 if category in ("UI", "Ambience", "Music") else 3.0),
            use=use, fn=fn)
        return fn

    return deco
