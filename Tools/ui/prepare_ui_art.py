"""Prepares the Higgsfield art in SourceArt/AI for import (Pillow + numpy, deterministic).

  SourceArt/AI/Icons/<AbilityId>.png      -> SourceArt/UI/Icons/T_Icon_<AbilityId>.png  (256 px)
  SourceArt/AI/Title/*.png                 -> SourceArt/UI/Art/T_<Name>.png              (1920 px wide)
  SourceArt/AI/Locations/<Id>.png          -> SourceArt/UI/Art/T_Location_<Id>.png       (1280 px wide)
  SourceArt/AI/UI/Ornament_*.png (on black)-> SourceArt/UI/Frame/T_<Name>.png            (black keyed to alpha)
  SourceArt/AI/UI/Parchment.png            -> SourceArt/UI/Frame/T_Parchment.png         (1600 px wide)
Run: ~/.venvs/mushoku-bpy311/bin/python Tools/ui/prepare_ui_art.py
"""
import glob
import os

import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
AI = os.path.join(ROOT, "SourceArt", "AI")
OUT = os.path.join(ROOT, "SourceArt", "UI")


def save(img, *parts):
    path = os.path.join(OUT, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(os.path.relpath(path, ROOT), img.size, img.mode)


def resize_width(img, width):
    if img.width == width:
        return img
    return img.resize((width, round(img.height * width / img.width)), Image.LANCZOS)


def key_black(img):
    """Gold-on-black art to straight alpha: alpha from brightness, colour un-premultiplied (no dark fringe)."""
    rgb = np.asarray(img.convert("RGB"), np.float32) / 255.0
    peak = rgb.max(axis=2)
    alpha = np.clip((peak - 0.04) / 0.22, 0.0, 1.0)
    colour = np.where(alpha[..., None] > 0.01, np.clip(rgb / np.maximum(alpha[..., None], 1e-3), 0, 1), 0)
    out = np.dstack([colour, alpha])
    # Trim to the drawn content plus a small margin.
    ys, xs = np.nonzero(alpha > 0.02)
    y0, y1 = max(0, ys.min() - 8), min(alpha.shape[0], ys.max() + 9)
    x0, x1 = max(0, xs.min() - 8), min(alpha.shape[1], xs.max() + 9)
    return Image.fromarray((out[y0:y1, x0:x1] * 255 + 0.5).astype(np.uint8), "RGBA")


def main():
    for path in sorted(glob.glob(os.path.join(AI, "Icons", "*.png"))):
        name = os.path.splitext(os.path.basename(path))[0]
        save(Image.open(path).convert("RGB").resize((256, 256), Image.LANCZOS), "Icons", f"T_Icon_{name}.png")
    for path in sorted(glob.glob(os.path.join(AI, "Title", "*.png"))):
        name = os.path.splitext(os.path.basename(path))[0]
        save(resize_width(Image.open(path).convert("RGB"), 1920), "Art", f"T_{name}.png")
    for path in sorted(glob.glob(os.path.join(AI, "Locations", "*.png"))):
        name = os.path.splitext(os.path.basename(path))[0]
        save(resize_width(Image.open(path).convert("RGB"), 1280), "Art", f"T_Location_{name}.png")
    for path in sorted(glob.glob(os.path.join(AI, "UI", "Ornament_*.png"))):
        name = os.path.splitext(os.path.basename(path))[0]
        save(key_black(Image.open(path)), "Frame", f"T_{name}.png")
    parchment = os.path.join(AI, "UI", "Parchment.png")
    if os.path.exists(parchment):
        save(resize_width(Image.open(parchment).convert("RGB"), 1600), "Frame", "T_Parchment.png")


if __name__ == "__main__":
    main()
