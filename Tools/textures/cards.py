"""Alpha-masked foliage cards: SourceArt/Textures/Palette/T_<Name>_{D,N,M}.png, 1024 px, D alpha = mask.

Layouts match the nature kit (Tools/kit/nature_textures.py stand-ins, nature_trees.py / nature_plants.py UVs):
- cluster  (LeavesOak, LeavesBirch, LeavesGiant, LeavesDemon, NeedlesPine, NeedlesSnow): square cards; a twig or
  stem enters at the bottom centre and the foliage fills the card (V up). Conifer boughs are strip cards with the
  branch along V (bottom = trunk side), so the needle sprays keep their stem on the vertical centre line.
- frond    (LeavesPalm, Fern): strip cards whose V runs from the frond base (bottom) to its tip (top) with u across;
  the frond is resampled so that its rib runs up the centre and spans the full height, pre-squashed for the
  strips' typical length / width ratio so leaflets keep their natural angle on the mesh.
- tuft     (Grass, GrassDry, Wheat, Flowers, Reeds): blades rooted along the bottom edge, V up; the source region
  is chosen with the cards' typical height / width ratio (fan_clump / blade sizes in nature_plants.py).

Alpha: the AI cutout alpha is thresholded to a crisp mask with a ~1 px anti-aliased edge (no semi-transparent
haze), specks are removed, and RGB is push-pull dilated into every transparent texel so mip maps and bilinear
filtering never pull black or white fringes into the leaves.
"""
import numpy as np

import texlib as T
import surface as SF
from common import load_ai, rng_for, save_set

SIZE = 1024

# aspect = typical card height / width on the meshes (1 = square cluster cards)
CARDS = {
    "LeavesOak": dict(src="Card_LeavesOak_B", layout="cluster", target=(78, 112, 46), grade=0.6, rough=0.62,
                      note="leaf-cluster card, twig from the bottom centre (oak crowns, bushes)"),
    "LeavesBirch": dict(src="Card_LeavesBirch_A", layout="cluster", target=(118, 150, 62), grade=0.6, rough=0.6,
                        note="airy drooping twig spray, small leaves (birch crowns, bush B)"),
    "NeedlesPine": dict(src="Card_NeedlesPine_A", layout="cluster", target=(46, 80, 50), grade=0.6, rough=0.72,
                        note="bough spray, stem up the centre line (conifer bough strips, leader, pads)"),
    "NeedlesSnow": dict(src="Card_NeedlesSnow_A", layout="cluster", target=(176, 196, 196), grade=0.4, rough=0.68,
                        note="snow-laden bough spray (snow pines, snowy bush)"),
    "LeavesGiant": dict(src="Card_LeavesGiant_A", layout="cluster", target=(56, 100, 46), grade=0.6, rough=0.52,
                        note="broad-leaf cluster (Great Forest giants, jungle trees; 3-4.5 m cards)"),
    "LeavesDemon": dict(src="Card_LeavesDemon_A", layout="cluster", target=(118, 58, 138), grade=0.5, rough=0.5,
                        note="purple spray with a central stem; also stretched along frond strips and hung "
                             "upside down as tassels"),
    "LeavesPalm": dict(src="Card_LeavesPalm_A", layout="frond", aspect=2.7, target=(92, 128, 50), grade=0.6,
                       rough=0.6, note="single frond, rib up the centre, base at the bottom (palm frond strips)"),
    "Grass": dict(src="Card_Grass_A", layout="tuft", aspect=1.25, target=(86, 128, 48), grade=0.6, rough=0.72,
                  note="blades rooted along the bottom edge (grass / flower clumps)"),
    "GrassDry": dict(src="Card_GrassDry_A", layout="tuft", aspect=1.25, target=(176, 152, 96), grade=0.6,
                     rough=0.85, note="dry blades rooted along the bottom edge (dry tufts, desert scrub, palm skirt)"),
    "Wheat": dict(src="Card_Wheat_A", layout="tuft", aspect=1.6, target=(208, 172, 92), grade=0.5, rough=0.78,
                  note="ripe stalks with ears near the top, rooted along the bottom edge (2 x 2 m wheat patches)"),
    "Flowers": dict(src="Card_Flowers_A", layout="tuft", aspect=1.15, target=None, grade=0.0, rough=0.7,
                    note="mixed wildflowers on stems rooted along the bottom edge (flower clumps, bush C)"),
    "Fern": dict(src="Card_Fern_A", layout="frond", aspect=2.4, target=(70, 118, 50), grade=0.6, rough=0.7,
                 note="single frond, stem up the centre, base at the bottom (fern frond strips)"),
    "Reeds": dict(src="Card_Reeds_A", layout="tuft", aspect=2.8, target=(118, 132, 70), grade=0.5, rough=0.72,
                  note="tall blades and cattails rooted along the bottom edge (reed clumps, 1.8-2.4 m cards)"),
}
NAMES = list(CARDS)


def clean_alpha(a):
    """Crisp mask with a thin anti-aliased edge: threshold the cutout alpha, drop specks and pin holes."""
    a = np.clip((a - 0.35) / 0.3, 0.0, 1.0)
    a = a * a * (3 - 2 * a)
    m = (a > 0.5).astype(np.float32)
    opened = SF.minmax_filter(SF.minmax_filter(m, 1, "min", False), 1, "max", False)    # kills 1-2 px specks
    closed = SF.minmax_filter(SF.minmax_filter(opened, 1, "max", False), 1, "min", False)  # fills pin holes
    a = a * SF.minmax_filter(opened, 1, "max", False)          # keep the AA edge of surviving shapes only
    a = np.where((closed > 0.5) & (m < 0.5), 1.0, a)
    return np.clip(a, 0.0, 1.0).astype(np.float32)


def _bbox(a, thr=0.5):
    ys, xs = np.nonzero(a > thr)
    return int(ys.min()), int(ys.max()) + 1, int(xs.min()), int(xs.max()) + 1


def _place(rgba, box, out_w, out_h, dst):
    """Resample rgba[box] to (out_w, out_h) and paste it into dst (H, W, 4) at the given top-left corner."""
    y0, y1, x0, x1 = box
    crop = rgba[y0:y1, x0:x1]
    res = T.resize(crop, (out_w, out_h))
    oy, ox = dst[1], dst[0]
    canvas = np.zeros((SIZE, SIZE, 4), np.float32)
    ys = slice(max(0, oy), min(SIZE, oy + out_h))
    xs = slice(max(0, ox), min(SIZE, ox + out_w))
    canvas[ys, xs] = res[ys.start - oy:ys.stop - oy, xs.start - ox:xs.stop - ox]
    return canvas


def layout(rgba, kind, aspect=1.0, margin=12):
    a = rgba[..., 3]
    y0, y1, x0, x1 = _bbox(a)
    h, w = y1 - y0, x1 - x0
    if kind == "cluster":
        # uniform scale so the foliage fills the card; bottom of the twig on the bottom edge, centred on x
        s = min((SIZE - margin) / h, (SIZE - 2 * margin) / w)
        ow, oh = int(round(w * s)), int(round(h * s))
        return _place(rgba, (y0, y1, x0, x1), ow, oh, ((SIZE - ow) // 2, SIZE - oh))
    if kind == "frond":
        # frond region pre-squashed for strips `aspect` times longer than wide: fill the height, rib centred
        cx = 0.5 * (x0 + x1)
        need_w = h / aspect
        half = max(w / 2.0, need_w / 2.0) if w < need_w else w / 2.0
        xa, xb = int(max(0, np.floor(cx - half))), int(min(rgba.shape[1], np.ceil(cx + half)))
        return _place(rgba, (y0, y1, xa, xb), SIZE - 2 * margin, SIZE - margin, (margin, margin))
    # tuft: region with the cards' height / width ratio, bottom edge = roots, top edge = tallest tips
    cx = 0.5 * (x0 + x1)
    need_w = h / aspect
    if w > need_w:
        xa, xb = int(round(cx - need_w / 2)), int(round(cx + need_w / 2))
    else:
        xa, xb = x0, x1
    return _place(rgba, (y0, y1, xa, xb), SIZE, SIZE - margin, (0, margin))


def card_maps(D, alpha, rough, rng):
    """Leaf-level normal / AO / roughness: leaves domed toward their interior (from the mask), veins and
    overlaps from the albedo detail; flat outside the mask."""
    y = T.luma(D)
    inner = T.blur(alpha, 3.0) * alpha
    detail = (y - T.blur(y, 6.0)) * alpha
    h = 0.7 * inner / max(float(inner.max()), 1e-6) + 1.2 * detail
    h01 = np.clip((h - h[alpha > 0.5].min()) / max(float(np.ptp(h[alpha > 0.5])), 1e-6), 0.0, 1.0) * (alpha > 0.5)
    texel = 1.0 / SIZE
    amp = SF.tilt_amplitude(h01, texel, 14.0)
    N = T.normal_from_height(T.blur(h01, 0.8) * amp, texel)
    flat = np.array([0.5, 0.5, 1.0], np.float32)
    w = (T.blur(alpha, 1.0) > 0.02).astype(np.float32)[..., None]
    N = N * w + flat * (1 - w)
    dens = T.blur(alpha, 18.0)
    ao = np.clip(1.0 - 0.45 * np.clip((dens - 0.35) / 0.65, 0.0, 1.0) - 0.25 * np.clip(-detail * 6, 0.0, 1.0),
                 0.35, 1.0)
    ao = ao * alpha + (1 - alpha)
    g = np.clip(rough + 0.08 * T.fbm_periodic(alpha.shape, rng, 12, 3), 0.3, 0.95)
    M = np.stack([ao, g, h01], -1).astype(np.float32)
    return N.astype(np.float32), M


def build(name, log=print):
    C = CARDS[name]
    rng = rng_for("Card_" + name)
    src = load_ai(C["src"], "RGBA")
    rgb = T.degrid(src[..., :3])
    a = clean_alpha(src[..., 3])
    rgba = np.concatenate([np.clip(rgb, 0, 1), a[..., None]], -1)
    rgba = layout(rgba, C["layout"], C.get("aspect", 1.0))
    a = clean_alpha(rgba[..., 3])
    rgb = rgba[..., :3]
    # colour: unpremultiplied colour of trusted texels, graded toward the palette colour (alpha-weighted)
    trusted = (a > 0.6).astype(np.float32)
    if C.get("target") is not None and C.get("grade", 0) > 0:
        lin = T.srgb_to_lin(rgb)
        cur = (lin * trusted[..., None]).reshape(-1, 3).sum(0) / max(float(trusted.sum()), 1.0)
        tgt = T.srgb_to_lin(np.asarray(C["target"], np.float32) / 255.0)
        gain = (tgt / np.maximum(cur, 1e-5)) ** C["grade"]
        rgb = T.lin_to_srgb(lin * gain)
    rgb = T.dilate_colour(rgb, trusted)
    D = np.concatenate([rgb, a[..., None]], -1)
    N, M = card_maps(rgb, a, C["rough"], rng)
    files = save_set("Palette", f"T_{name}", D, N, M)
    cover = float((a > 0.5).mean())
    fringe = float(((a > 0.02) & (a < 0.98)).mean())
    entry = dict(name=name, group="Palette", kind="Card", files=files, tile_m=None, size=SIZE, source="AI",
                 ai_sources=[f"SourceArt/AI/Textures/{C['src']}.png"], metallic=0,
                 roughness_range=[round(float(np.percentile(M[..., 1][a > 0.5], 1)), 3),
                                  round(float(np.percentile(M[..., 1][a > 0.5], 99)), 3)],
                 normal_strength=1.0, masked=True, two_sided=True, opacity_clip=0.5, layout=C["layout"],
                 card_aspect=C.get("aspect", 1.0), coverage=round(cover, 3), soft_edge_fraction=round(fringe, 4),
                 note=C["note"], mean_albedo=SF.mean_albedo_linear(rgb, a))
    log(f"Card {name}: coverage {cover:.2f}, soft edge {fringe:.4f}")
    return entry, dict(D=D, N=N, M=M)
