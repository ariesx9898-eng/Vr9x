"""Review contact sheets (JPEG, quality 85) in Docs/Images/:
- LaPlace_Textures_Terrain.jpg: every terrain layer tiled 2 x 2 with its normal map beside it, plus the water
  textures and the macro-noise channels.
- LaPlace_Textures_Palette.jpg: every tiling palette material tiled 2 x 2, with its normal map.
- LaPlace_Textures_Cards.jpg: every alpha card over mid grey, with its normal map and a 1/8 alpha-tested mip.
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from common import OUT_DIR, DOC_IMG

BG = (46, 48, 52)
FG = (235, 235, 235)
SUB = (170, 175, 182)


def _font(size):
    for p in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc",
              "/Library/Fonts/Arial.ttf"):
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except OSError:
                pass
    return ImageFont.load_default()


def _open(rel, mode="RGB"):
    return Image.open(os.path.join(OUT_DIR, rel)).convert(mode)


def _tiled(im, n, cell):
    w, h = im.size
    big = Image.new(im.mode, (w * n, h * n))
    for i in range(n):
        for j in range(n):
            big.paste(im, (w * i, h * j))
    return big.resize((cell, cell), Image.LANCZOS)


def _over(im_rgba, bg=(128, 128, 128)):
    base = Image.new("RGBA", im_rgba.size, bg + (255,))
    base.alpha_composite(im_rgba)
    return base.convert("RGB")


def _save(sheet, name, max_bytes=2_500_000):
    path = os.path.join(DOC_IMG, name)
    os.makedirs(DOC_IMG, exist_ok=True)
    q = 85
    sheet.save(path, quality=q, optimize=True, progressive=True)
    # keep the review sheets light enough for the repo: shrink (never below 60 % size) if above the budget
    scale = 1.0
    while os.path.getsize(path) > max_bytes and scale > 0.6:
        scale -= 0.05
        sheet.resize((int(sheet.width * scale), int(sheet.height * scale)), Image.LANCZOS).save(
            path, quality=q, optimize=True, progressive=True)
    return path


def _grid(items, cols, cell_w, cell_h, label_h, title, pad=10):
    rows = (len(items) + cols - 1) // cols
    W = cols * cell_w + (cols + 1) * pad
    H = 64 + rows * (cell_h + label_h + pad) + pad
    sheet = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(sheet)
    d.text((pad, 14), title, fill=FG, font=_font(28))
    f1, f2 = _font(17), _font(14)
    for k, (img, name, sub) in enumerate(items):
        x = pad + (k % cols) * (cell_w + pad)
        y = 64 + (k // cols) * (cell_h + label_h + pad)
        d.text((x, y + 2), name, fill=FG, font=f1)
        d.text((x, y + 22), sub, fill=SUB, font=f2)
        sheet.paste(img, (x, y + label_h))
    return sheet


def terrain_sheet(entries):
    cell = 440
    items = []
    for e in entries:
        if e["group"] != "Terrain" or "files" not in e or "N" not in e["files"]:
            continue
        D = _tiled(_open(e["files"]["D"]), 2, cell)
        Nfull = _open(e["files"]["N"])
        N = Nfull.resize((cell, cell), Image.LANCZOS)
        inset = cell * 4 // 10                       # 1:1 crop of the normal map, so its fine relief is visible
        crop = Nfull.crop((0, 0, inset, inset))
        N.paste(crop, (cell - inset, cell - inset))
        ImageDraw.Draw(N).rectangle([cell - inset, cell - inset, cell - 1, cell - 1], outline=(40, 40, 40))
        pair = Image.new("RGB", (2 * cell + 6, cell), BG)
        pair.paste(D, (0, 0))
        pair.paste(N, (cell + 6, 0))
        items.append((pair, f"T_Ground_{e['layer']}",
                      f"{e['tile_m']:g} m tile: albedo 2 x 2 | normal, 1 repeat with a 1:1 inset"))
    for e in entries:
        if e["group"] == "Water" and e["name"] == "Ocean":
            N = _tiled(_open(e["files"]["N"]), 2, cell)
            pair = Image.new("RGB", (2 * cell + 6, cell), BG)
            pair.paste(N, (0, 0))
            items.append((pair, "Water/T_Ocean_N", f"{e['tile_m']:g} m suggested repeat, 2 x 2"))
        if e["group"] == "Water" and e["name"] == "Foam":
            F = _open(e["files"]["D"], "RGBA")
            F2 = Image.new("RGBA", (F.width * 2, F.height * 2))
            for i in range(2):
                for j in range(2):
                    F2.paste(F, (F.width * i, F.height * j))
            over = _over(F2, (26, 70, 92)).resize((cell, cell), Image.LANCZOS)
            alpha = F2.getchannel("A").convert("RGB").resize((cell, cell), Image.LANCZOS)
            pair = Image.new("RGB", (2 * cell + 6, cell), BG)
            pair.paste(over, (0, 0))
            pair.paste(alpha, (cell + 6, 0))
            items.append((pair, "Water/T_Foam_D", "2 x 2 over sea blue | alpha"))
        if e["name"] == "Macro_Noise":
            M = _open(e["files"]["D"])
            chans = [M.getchannel(c).convert("RGB").resize((cell * 2 // 3, cell * 2 // 3), Image.LANCZOS)
                     for c in "RGB"]
            pair = Image.new("RGB", (2 * cell + 6, cell), BG)
            for i, c in enumerate(chans):
                pair.paste(c, (i * (cell * 2 // 3 + 3), (cell - cell * 2 // 3) // 2))
            items.append((pair, "Terrain/T_Macro_Noise", "R | G | B channels (1 repeat)"))
    sheet = _grid(items, 2, 2 * cell + 6, cell, 44, "LA PLACE terrain layers (2 x 2 tiled) and water / macro")
    return _save(sheet, "LaPlace_Textures_Terrain.jpg")


def palette_sheet(entries):
    cell = 230
    items = []
    for e in entries:
        if e["group"] != "Palette" or e.get("kind") == "Card" or "files" not in e:
            continue
        D = _tiled(_open(e["files"]["D"]), 2, cell)
        N = _open(e["files"]["N"]).resize((cell, cell), Image.LANCZOS)
        pair = Image.new("RGB", (2 * cell + 4, cell), BG)
        pair.paste(D, (0, 0))
        pair.paste(N, (cell + 4, 0))
        if "E" in e["files"]:
            E = _open(e["files"]["E"]).resize((cell // 3, cell // 3), Image.LANCZOS)
            pair.paste(E, (2 * cell + 4 - cell // 3, cell - cell // 3))
        tag = f"{e['tile_m']:g} m, {e['source'].lower()}"
        if e.get("metallic"):
            tag += ", metal"
        items.append((pair, e["name"], tag))
    sheet = _grid(items, 4, 2 * cell + 4, cell, 42, "LA PLACE mesh palette (albedo 2 x 2 | normal)")
    return _save(sheet, "LaPlace_Textures_Palette.jpg")


def cards_sheet(entries):
    cell = 300
    items = []
    for e in entries:
        if e.get("kind") != "Card" or "files" not in e:
            continue
        D = _open(e["files"]["D"], "RGBA")
        over = _over(D).resize((cell, cell), Image.LANCZOS)
        N = _open(e["files"]["N"]).resize((cell, cell), Image.LANCZOS)
        # 1/8 mip, alpha-tested at 0.5, shown enlarged: checks for fringes at distance
        mip = D.resize((D.width // 8, D.height // 8), Image.BOX)
        a = np.asarray(mip)[..., 3] > 127
        rgb = np.asarray(mip)[..., :3].copy()
        rgb[~a] = 128
        mip = Image.fromarray(rgb).resize((cell, cell), Image.NEAREST)
        trio = Image.new("RGB", (3 * cell + 8, cell), BG)
        trio.paste(over, (0, 0))
        trio.paste(N, (cell + 4, 0))
        trio.paste(mip, (2 * cell + 8, 0))
        items.append((trio, e["name"], f"{e['layout']} card (over grey | normal | 1/8 mip alpha-tested)"))
    sheet = _grid(items, 2, 3 * cell + 8, cell, 42, "LA PLACE alpha cards")
    return _save(sheet, "LaPlace_Textures_Cards.jpg")


def build_all(manifest):
    entries = manifest["entries"]
    return [terrain_sheet(entries), palette_sheet(entries), cards_sheet(entries)]
