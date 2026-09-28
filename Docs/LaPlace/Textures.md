# LA PLACE textures

Every texture of Spec section 8 (terrain layers, mesh palette, alpha cards, water, macro noise) is built by one
deterministic script from committed AI source images plus procedural code. The PNG outputs are git-ignored; the
manifest and the review sheets are committed.

## Rebuild

```sh
PY=~/.venvs/mushoku-bpy311/bin/python             # Python 3.11 with numpy + Pillow (no scipy / OpenCV needed)
$PY Tools/textures/build_textures.py              # everything: 67 materials, 196 PNGs, manifest, contact sheets (~40 s on 12 cores)
$PY Tools/textures/build_textures.py --group Terrain          # Terrain, Palette, Cards or Extras
$PY Tools/textures/build_textures.py --only Grass,RoofRed     # subset; the manifest keeps the other entries
$PY Tools/textures/build_textures.py --no-sheets --jobs 4
$PY Tools/textures/build_textures.py --selftest               # normal-map convention test only
```

The output is bit-identical from run to run and for any `--jobs` value (checked by hashing all 196 PNGs of two
builds). Every run first runs the normal-map self-test: a synthetic dome must get a bright right edge in R, a bright
bottom edge in G, and read as a bump when lit from the top of the image in Unreal's (DirectX, +Y down) convention. The
same map was also rendered in Blender (Cycles, green flipped to OpenGL) as a cross-check, and read as a bump there too.

| Output | Content |
|---|---|
| `SourceArt/Textures/Terrain/T_Ground_<Layer>_{D,N,M}.png` | 11 landscape layers, 2048 px |
| `SourceArt/Textures/Terrain/T_Macro_Noise.png` | R / G / B smooth noise at ~2 / 4 / 8 cycles per repeat, linear |
| `SourceArt/Textures/Palette/T_<Name>_{D,N,M}.png` | 40 tiling materials + 13 alpha cards, 1024 px; `T_Crystal_E.png` |
| `SourceArt/Textures/Water/T_Ocean_N.png`, `T_Foam_D.png` | wave normals; foam colour + alpha |
| `SourceArt/Textures/manifest.json` | one entry per material (fields in Spec section 8) |
| `Docs/Images/LaPlace_Textures_{Terrain,Palette,Cards}.jpg` | review sheets |

Code: `build_textures.py` (driver, manifest), `terrain.py`, `palette.py`, `cards.py`, `procedural.py`
(Iron, Gold, Glass, Crystal), `extras.py` (macro noise, ocean, foam), `surface.py` (height, normal, AO, roughness),
`texlib.py` (image maths), `sheets.py`, `common.py`.

## Sources and credits

All AI images are Higgsfield `gpt_image_2_5` generations (1:1, default low quality). Raw files live in
`SourceArt/AI/Textures/` (committed, 59 files, 134 MB); their job ids are in `SourceArt/AI/manifest.json`.

**Credits: 15.25 of the 25-credit budget** (balance 78.45 before, 63.20 after): 57 images at 1k (0.25 each) and two at
2k (0.50 each, the cliff-rock candidates). Three generations are not used by the build and are kept only as
alternates: `Terrain_Rock_A` (1k strata, too small-scale for an 8 m tile), `Terrain_Rock_B` (2k, read as stacked
masonry when tiled) and `Card_LeavesOak_A` (first card test). They can be deleted if repository size matters; nothing
references them.

Procedural (no AI): Iron, Gold, Glass, Crystal (+ emissive), T_Macro_Noise, T_Ocean_N, T_Foam_D. Derived from a
shared AI source: the four Cloth colours (one neutral woven linen, dyed in linear light) and palette Snow (terrain snow
source, rebuilt at mesh scale).

| Material | Tile (m) | Source | Notes |
|---|---:|---|---|
| Ground_Grass | 4 | Terrain_Grass_A | meadow grass, clover, flower specks |
| Ground_Farmland | 4 | Terrain_Farmland_A | 12 furrow rows per tile (33 cm), rows along U, ridge relief in the normal |
| Ground_ForestFloor | 4 | Terrain_ForestFloor_A | leaf litter, twigs, needles, moss |
| Ground_Moss | 4 | Terrain_Moss_A | cushion moss, small stones |
| Ground_Snow | 6 | Terrain_Snow_A | wind ripples rebuilt from the painted shading (no baked light direction), blue tint in hollows |
| Ground_Sand | 4 | Terrain_Sand_A | fine beach sand, shell bits |
| Ground_Desert | 8 | Terrain_Desert_A | golden dune sand, wind ripples (as Snow) |
| Ground_DemonSoil | 6 | Terrain_DemonSoil_A | red-brown cracked soil, ash, black grit |
| Ground_Rock | 8 | Terrain_Rock_C (2k) | fractured cliff rock with ledges |
| Ground_Road | 4 | Terrain_Road_A | packed earth and gravel, no ruts |
| Ground_Mud | 4 | Terrain_Mud_A | puddles: flat water level in height, flat normal, roughness ~0.06 |
| Plaster, PlasterTan | 2 | Palette_Plaster_A, _PlasterTan_A | |
| Timber | 1 | Palette_Timber_A | grain along V |
| WoodPlanks | 2 | Palette_WoodPlanks_A | 8 boards per tile along U, wrap on a board gap |
| Stone / StoneWhite / Sandstone | 2 | Palette_Stone_A / _StoneWhite_A / _Sandstone_A | 7 / 4 / 5 courses, wrap on a joint |
| Cobble | 1.5 | Palette_Cobble_A | |
| DemonRock | 2 | Palette_DemonRock_A | |
| RoofRed | 2 | Palette_RoofRed_A | terracotta barrel tiles, 6 rows x 10 barrels (33 cm rows, the kit's courses are 34 cm) |
| RoofBlue / RoofDark | 2 | Palette_RoofBlue_A / _RoofDark_A | staggered slates, even course count across the wrap |
| RoofThatch | 2 | Palette_RoofThatch_A | 5 courses, butt ends down |
| RoofSilver | 2 | Palette_RoofSilver_A | fish-scale metal shingles, metallic |
| RoofGreen | 2 | Palette_RoofGreen_A | verdigris copper, standing seams along V |
| Snow | 2 | Terrain_Snow_A | |
| ClothRed / Blue / Green / Tan | 1 | Palette_Cloth_A | one woven source, four dyes |
| Hide, Bone | 1 | Palette_Hide_A, _Bone_A | |
| Iron, Gold | 1 | procedural | hammered / brushed metal, metallic |
| Glass | 1 | procedural | wavy old glass, bubbles, grime; `translucent`, `opacity_hint` 0.35 |
| Crystal | 1 | procedural | faceted, glowing veins; `T_Crystal_E` mask, `emissive_color` (linear) x `emissive_strength` 2 |
| BarkOak, BarkBirch, BarkPine, BarkDead, BarkGiant, BarkDemon | 1 | Palette_Bark*_A | grain along V, seamless in U and V |
| MushroomCap / MushroomStem | 2 / 1 | Palette_MushroomCap_A / _Stem_A | |
| Rock, RockMossy, RockSnow, RockDesert, RockDemon, RockPale | 2 | Palette_Rock*_A | RockDemon is glossy obsidian (roughness 0.3-0.45) |
| Leaves Oak / Birch / Giant / Demon, Needles Pine / Snow | card | Card_*_A (Oak: _B) | clusters, twig or stem from the bottom centre |
| LeavesPalm, Fern | card | Card_LeavesPalm_A, Card_Fern_A | one frond, base at the bottom, rib up the centre |
| Grass, GrassDry, Wheat, Flowers, Reeds | card | Card_*_A | tufts rooted along the bottom edge |
| Ocean / Foam | 12 / 6 | procedural | suggested repeats |

The manifest has the exact values (`tile_m`, `roughness_range`, `height_m`, `mean_albedo`, card `coverage`, ...).

## How the textures are made

- **Clean-up of every AI image**: the Moisan periodic + smooth decomposition removes border mismatch; a spectral
  notch removes the model's faint 8 / 16 px decoder grid (visible as a checker in dark areas otherwise); lighting
  gradients are divided out in linear light.
- **Terrain** (`terrain.py`): each 1024 px source covers half a tile at its native texel density (512 px/m on 4 m
  tiles). The 2048 tile is synthesised by toroidal image quilting (Efros-Freeman min-error cuts, with wrap-around
  constraints on the last row and column) from the source and its rotations / mirrors (mirrors only for layers whose
  painted relief has a direction), so it is seamless by construction with no mirror seams and no visible 2 x 2
  repeat. Farmland quilting keeps the furrow phase. A final pass removes blotches above ~1/8 tile. Rock uses one 2k
  source covering the full 8 m tile. Snow and Desert relief is recovered from the painted ripple shading (linearised
  shape-from-shading), and the albedo rebuilt without it, so the sun can come from any direction.
- **Palette** (`palette.py`): seams are closed by min-cut self-overlap whose cut ends where it starts (no smoothed line
  at the wrap). Coursed materials (planks, masonry, slate, thatch) are first cropped to a whole number of courses
  between two detected joint lines, so the vertical wrap falls on a joint; tile rows (barrel tiles, slates, scales)
  use lattice-aware overlaps a whole number of tiles apart. Tile-scale blotches are removed with a Fourier notch
  below ~2.5 cycles per tile, keeping block-to-block variation. Colours are graded toward the Spec palette colours.
- **Surface maps** (`surface.py`): height from band-passed luma plus joints / cracks as grooves (black / white
  top-hat), normal strength set per material by a target mean surface tilt, AO from height cavities at several radii,
  roughness from a material base modulated by height (crests smoother) and albedo.
- **Cards** (`cards.py`): generated on a transparent background; the alpha is thresholded to a crisp mask with a
  ~1 px anti-aliased edge (no semi-transparent haze, specks removed), the plant is re-framed to the kit's card layout
  (clusters fill the card with the twig at the bottom centre; fronds are resampled for their strips' 2.4-2.7:1 length;
  tufts use the clumps' height / width ratio), and colour is push-pull dilated into every transparent texel so mips
  never get dark or white fringes (see the 1/8 alpha-tested mip column of the cards sheet). Normals give each leaf a
  slight dome.

## Import notes for Unreal

- `_D`: sRGB on (Default / BC1, BC3 for cards and Foam). `_N`: Normalmap compression, sRGB off, **no** green flip.
  `_M`, `_E`, `T_Macro_Noise`: sRGB off (Masks / linear).
- Cards: masked, two-sided, opacity mask = `_D` alpha, clip 0.5.
- Metals (RoofSilver, Iron, Gold): metallic 1; `_D` is the specular colour.
- Scale mesh UVs by `1 / tile_m` (1 UV = 1 m on the kit); landscape layers by their `tile_m` in world units.

## Known limitations

- Terrain layers repeat every 4-8 m by design; the macro noise is meant to break that up at distance. Rock (8 m) has
  the most recognisable features (a few large ledges).
- Cloth is one weave dyed four ways; a few thick slub threads can line up when a large surface shows many repeats.
- RockMossy's source had small purple flowers; they are recoloured toward pale lichen but a few lilac-grey patches
  remain (they read as lichen on stone).
- Not checked in Unreal from this tool (the engine is not run here); the normal convention is verified numerically
  and in Blender.
