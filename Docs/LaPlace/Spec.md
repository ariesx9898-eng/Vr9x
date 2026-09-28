# LA PLACE: world, abilities and menu rebuild (shared contract)

LA PLACE is this project's new title: a Mushoku Tensei open world with the Rudeus and Orsted lineages. This file is
the contract between the tools that generate content (world generator, Blender kits, textures, audio) and the game
code that consumes it. **Any tool that writes files must follow the names, units and formats below exactly.**

Reference maps: `Docs/LaPlace/Reference/` (`Map_JP.webp` 1200x900, `Map_CN.webp` 1192x696, `Map_Central_EN.webp`).
The Japanese map is the primary layout reference; the Chinese map adds city positions; the English map adds detail on the
Central Continent. Everything must be **original work** (no tracing of copyrighted art into game assets; the maps are
used only for geography: where continents, ranges and cities are relative to each other).

## 1. Art direction

- Stylised-realistic, painterly fantasy (think a high-budget anime-adaptation game): clean readable shapes, rich but not
  neon colours, warm light. Every region must be recognisable without the HUD.
- Asura / Fittoa: green rolling fields, golden wheat, forest patches, rivers, stone bridges, windmills, timber-frame
  houses with red clay-tile roofs; warm, prosperous. Fittoa is rural and peaceful.
- Northern Territories / Ranoa: snow, frozen plains, evergreens, frozen lakes, dark slate roofs with snow caps.
- Red Wyrm Mountains: one enormous connected range, cliffs, deep valleys, passes, snowy summits, waterfalls.
- Demon Continent: barren dark-red soil, brown stone, jagged crags, craters, ravines, twisted trees, oversized purple
  plants, giant mushrooms; harsh light, dust.
- Millis: humid, green; the Great Forest (giant trees, moss, ferns, fog); lakes; white-stone Millishion with blue roofs.
- Begaritt: desert dunes, rocky plateaus, canyons, dry riverbeds, arches, oases, sandstone Rapan, giant labyrinth gates.
- Heaven Continent: an extremely high plateau with sheer cliffs, pale rock, snow and wind.

## 2. World coordinates and the landscape

| Item | Value |
|---|---|
| Landscape vertices | **6097 x 4573** (24 x 18 components of 254 quads; sections 127 quads, 2x2 per component) |
| Quad size | **300 cm** (3 m) |
| World size | 18 288 m (X, east) x 13 716 m (Y, south) |
| Landscape min corner (actor location) | X = -914 400 cm, Y = -685 800 cm |
| Map to world | `X_cm = (u - 0.5) * 1 828 800`, `Y_cm = (v - 0.5) * 1 371 600`, where u, v are 0..1 across the Japanese map (u to the right = east, v down = south) |
| Heightmap | 16-bit unsigned, little endian, row-major, rows = Y (north to south), columns = X (west to east) |
| Height encoding | `Z_cm = 72 400 + (h - 32 768) * 400 / 128` (landscape Z scale 400, landscape location Z 72 400) |
| Sea level | Z = 0 cm, which is h = 9 600 |
| Height range | -300 m (h = 0) to +1 744 m (h = 65 535) |

Nominal heights (metres above sea level): sea floor -30 near coasts to -200 in open sea; beaches 0-3; Asura/Fittoa
lowlands 10-80; Red Wyrm foothills 150-300, peaks 600-900 (snow above ~550); Northern plains 40-200, northern
mountains 400-700; Strife Zone 50-150; southern Central jungle 20-120, King Dragon range 400-700; Heaven plateau
750-950 with sheer cliffs down to the sea; Demon Continent 60-250 base, crags 300-650, crater rims +80 over their
surroundings with sunken floors; Millis lowlands 10-100, Great Forest 30-120, Blue Wyrm range 400-750; Begaritt desert
20-120, dunes +-20, plateaus 150-280 cut by canyons, a rocky ring 300-500.

## 3. Regions (IDs used by `Regions.png` and `World.json`)

| Id | Region | Continent | Ground / biome |
|---|---|---|---|
| 0 | Ocean | - | sea |
| 1 | Asura Kingdom | Central | temperate plains, farmland, forests, rivers |
| 2 | Fittoa Region | Central | rural fields, forest patches, rivers, small hills |
| 3 | Red Wyrm Mountains | Central | alpine range, cliffs, passes, snowy summits |
| 4 | Northern Territories | Central | snow, frozen plains, evergreens, frozen lakes |
| 5 | Strife Zone | Central | dry hills and plains, scarred farmland |
| 6 | Southern Central | Central | dense forest / jungle, King Dragon range |
| 7 | Heaven Continent | Heaven | high plateau, cliffs, snow |
| 8 | Demon Continent | Demon | barren badlands, crags, ravines, craters |
| 9 | Rikarisu Crater | Demon | crater bowl around the town |
| 10 | Great Forest | Millis | giant trees, moss, ferns, rivers |
| 11 | Millis Lowlands | Millis | green hills, lakes, farmland |
| 12 | Blue Wyrm Mountains | Millis | mountain range |
| 13 | Begaritt Desert | Begaritt | dunes, dry riverbeds, oases |
| 14 | Begaritt Badlands | Begaritt | plateaus, canyons, arches, labyrinth gates |
| 15 | Islands | - | small islands, beaches |

## 4. Sites (cities, villages, landmarks)

Approximate positions in (u, v) on the Japanese map; the world generator moves each onto suitable land in its region,
keeping the relative geography. Radii are the flattened footprint.

| Id | Name | Region | (u, v) | Radius | Style / contents |
|---|---|---|---|---|---|
| Ars | Ars, royal capital of Asura | 1 | (0.085, 0.31) | 450 m | AsuraCapital: huge walls, dense streets, noble district, markets, government buildings, the **Silver Palace** on a rise overlooking the city |
| Roa | Roa, seat of Fittoa | 2 | (0.098, 0.222) | 190 m | AsuraTown: defensive walls, stone buildings, market, larger streets, noble estate (Boreas manor) |
| Buena | Buena Village | 2 | (0.140, 0.255) | 130 m | RuralVillage: wooden houses, farms, fields, windmill, dirt paths; the Greyrat house on a gentle hill |
| Sharia | Sharia (Ranoa) | 4 | (0.100, 0.100) | 300 m | NorthernCity: stone/timber, magic shops, adventurer guild, workshops, the **Ranoa University of Magic** dominating the skyline |
| Rikarisu | Rikarisu | 9 | (0.890, 0.190) | 220 m | DemonTown inside a huge crater: rock structures, markets, adventurer area, dense streets |
| Wenport | Wenport | 8 | (0.915, 0.590) | 140 m | DemonPort |
| ZantPort | Zant Port | 11 | (0.825, 0.660) | 130 m | MillisPort |
| Millishion | Millishion | 11 | (0.770, 0.800) | 420 m | MillisCapital: huge walls, cathedral, towers, plazas, markets, clean ordered streets, beside a lake |
| WestPort | West Port | 11 | Millis west tip | 120 m | MillisPort |
| EastPort | East Port | 6 | Central south tip | 120 m | AsuraTown (port) |
| KingDragon | King Dragon Kingdom capital | 6 | (0.495, 0.830) | 220 m | AsuraTown |
| Rapan | Rapan, labyrinth city | 13 | (0.300, 0.925) | 280 m | DesertCity: sandstone, markets, adventurer guilds, inns, caravan yards |
| Labyrinth_1..5 | Labyrinth gates | 13/14 | spread over Begaritt | 40 m | Giant stone gates set into rock |
| SwordSanctuary | Sword Sanctuary | 4 | (0.025, 0.050) | 100 m | NorthernVillage |

Mountain ranges (polylines in u, v): Red Wyrm spine (0.11,0.14)-(0.15,0.20)-(0.19,0.28)-(0.20,0.36)-(0.21,0.45)-(0.22,0.55)
with the **Upper Jaw** pass near the north-west coast (~0.05,0.17) and the **Lower Jaw** pass (~0.215,0.58); the north-east
Red Wyrm range (0.25,0.23)-(0.30,0.235)-(0.36,0.26)-(0.39,0.29) with the Dragon-Roar peak (~0.27,0.23); King Dragon range
(0.29,0.58)-(0.32,0.65)-(0.35,0.72); Blue Wyrm range (0.80,0.77)-(0.85,0.80)-(0.90,0.80); Demon crags around Rikarisu and
in the continent's centre (see the reference); Begaritt rocky ring with a basin (~0.08-0.13, 0.74-0.80).

## 5. World generator outputs (`Tools/world/generate_world.py`)

Deterministic (fixed seed), numpy + Pillow only, runs in a few minutes. Large outputs are git-ignored and regenerated.

| File | Format |
|---|---|
| `SourceArt/World/Height.r16` | 6097 x 4573, uint16 LE (section 2 encoding) |
| `SourceArt/World/Layers/<Layer>.png` | 6097 x 4573, 8-bit grey, per paint layer; all layers sum to 255 per pixel |
| `SourceArt/World/Regions.png` | 1524 x 1143, 8-bit, region id per pixel (section 3) |
| `SourceArt/World/Density/<Kind>.png` | 1524 x 1143, 8-bit density 0..255 for `Trees`, `Bushes`, `Grass`, `Rocks`, `Flowers` (0 on roads, water, city footprints) |
| `Content/Data/World.json` | world constants, regions, sites (centre cm, ground Z, radius, yaw, style, districts, landmark placements), roads, rivers, lakes, bridges |
| `Content/Data/Locations.json` | spawn / fast-travel locations for the menu (schema below) |
| `SourceArt/World/WorldMap.png` | 4096 x 3072 painted parchment map of the generated world, **no text** (the UI draws labels) |
| `Docs/Images/LaPlace_World_Preview.png` | hill-shaded map with labels, for review |

Paint layers (names are the landscape layer names): `Grass`, `Farmland`, `ForestFloor`, `Moss`, `Snow`, `Sand`, `Desert`,
`DemonSoil`, `Rock`, `Road`, `Mud`.

`Locations.json` rows: `LocationID`, `DisplayName`, `Continent`, `Region`, `Biome`, `Difficulty` (1-5), `Description`
(one or two sentences, original wording), `WorldLocation {X,Y,Z}` (cm, on the ground), `SpawnYaw` (deg),
`bSpawnPoint`, `bFastTravel`, `DiscoveryRadius` (cm), `RequiredRank` ("F"), `MapUV {X,Y}` (0..1 on WorldMap.png, which covers exactly the world rectangle),
`PreviewCamera {Location {X,Y,Z}, Rotation {Pitch,Yaw,Roll}}` (a scenic view of the place, looking at it).
Spawn points: Buena, Roa, Ars, Sharia, Rikarisu, Millishion, Rapan (plus Wenport, ZantPort as non-spawn fast travel).

## 6. Material palette (mesh material slot names)

Meshes use these slot names verbatim; the game maps each to a material instance. UVs are box-projected at
**1 UV unit = 1 metre** unless noted, so tiling textures line up across assets.

Architecture: `MT_Plaster` (warm off-white lime plaster), `MT_PlasterTan` (tan adobe), `MT_Timber` (dark oak beams),
`MT_WoodPlanks`, `MT_Stone` (grey cut blocks), `MT_StoneWhite` (pale limestone/marble), `MT_Cobble`, `MT_Sandstone`,
`MT_DemonRock` (dark red-brown volcanic stone), `MT_RoofRed` (terracotta tiles), `MT_RoofBlue` (blue-grey slate),
`MT_RoofDark` (dark northern slate), `MT_RoofThatch`, `MT_RoofSilver` (silvery metal), `MT_RoofGreen` (oxidised copper),
`MT_Snow`, `MT_ClothRed`, `MT_ClothBlue`, `MT_ClothGreen`, `MT_ClothTan`, `MT_Hide`, `MT_Iron`, `MT_Gold`, `MT_Glass`,
`MT_Crystal` (glowing blue magic crystal), `MT_Bone`.

Nature: `MT_BarkOak`, `MT_BarkBirch`, `MT_BarkPine`, `MT_BarkDead`, `MT_BarkGiant`, `MT_BarkDemon`, `MT_LeavesOak`,
`MT_LeavesBirch`, `MT_NeedlesPine`, `MT_NeedlesSnow`, `MT_LeavesGiant`, `MT_LeavesDemon` (purple), `MT_LeavesPalm`,
`MT_Grass`, `MT_GrassDry`, `MT_Wheat`, `MT_Flowers`, `MT_Fern`, `MT_Reeds`, `MT_MushroomCap`, `MT_MushroomStem`,
`MT_Rock`, `MT_RockMossy`, `MT_RockSnow`, `MT_RockDesert`, `MT_RockDemon`, `MT_RockPale`.
Leaf / grass / flower / fern / reed materials are alpha-masked cards whose UVs cover the whole 0..1 texture.

VFX meshes use a single slot named `VFX`.

## 7. Mesh export conventions (Blender kits)

- One GLB per asset in `SourceArt/Kit/<Category>/<AssetName>.glb`; names `SM_<Group>_<Type>_<Variant>`
  (e.g. `SM_Asura_House_A`, `SM_Tree_Oak_B`, `SM_Landmark_SilverPalace`, `SM_VFX_Ring`).
- Metres, Blender Z-up (the glTF exporter converts), pivot at the base centre on the ground, building fronts / doors
  facing Blender -Y. Apply all transforms. Clean manifold-ish geometry, no duplicate or coincident faces (no z-fighting),
  no interior faces visible from outside, consistent outward normals.
- Budgets: houses 1-6k tris, landmarks up to 40k, trees 2-10k (card leaves), rocks 300-3k, grass/flower clumps < 400.
- A contact sheet per category in `SourceArt/Kit/<Category>/_Preview.png`, and `SourceArt/Kit/manifest.json` listing
  every asset: name, category, style, footprint (x, y metres), height, tris, material slots.

## 8. Textures (`SourceArt/Textures/`)

Generated by `Tools/textures/build_textures.py` (deterministic, ~40 s) from the committed AI sources in
`SourceArt/AI/Textures/` plus procedural code; the PNGs are git-ignored and rebuilt, `manifest.json` is committed.
Rebuild: `~/.venvs/mushoku-bpy311/bin/python Tools/textures/build_textures.py` (Python 3.11 + numpy + Pillow; `--only`,
`--group`, `--no-sheets` for partial runs). Details, sources and credits: `Docs/LaPlace/Textures.md`.

| Folder / file | Content | Size |
|---|---|---|
| `Terrain/T_Ground_<Layer>_{D,N,M}.png` | one set per paint layer of section 5 (`Grass` ... `Mud`) | 2048 |
| `Terrain/T_Macro_Noise.png` | linear; R, G, B = three independent smooth noise fields (~2, 4, 8 cycles per repeat), for anti-tiling at about 150 m, 40 m and 12 m | 1024 |
| `Palette/T_<Name>_{D,N,M}.png` | one set per section 6 slot, `<Name>` = slot name without `MT_` (e.g. `T_RoofRed_D.png`); `T_Crystal_E.png` is Crystal's emissive mask | 1024 |
| `Water/T_Ocean_N.png`, `Water/T_Foam_D.png` | wind-wave normal map; shoreline foam (RGB colour, A coverage) | 1024 |

Formats (every material):
- `_D`: albedo, sRGB. Alpha only on cards (`masked` in the manifest), where it is the opacity mask (clip at 0.5); colour
  is dilated into transparent texels.
- `_N`: tangent-space normal, **DirectX / Unreal convention**: R = -dh/du, G = -dh/dv with v pointing down the image
  (a dome has a bright right edge in R and a bright bottom edge in G). Import with sRGB off, as a normal map, no green flip.
- `_M`: linear (sRGB off): R = ambient occlusion, G = roughness, B = height 0..1.
- Every tiling texture is seamless; cards cover the whole 0..1 texture, V up (twig / stem / rib enters at the bottom).

`manifest.json` lists one entry per material: `name`, `group` (`Terrain` / `Palette` / `Water`), `files` {D, N, M, E?}
(paths relative to `SourceArt/Textures/`), `tile_m` (metres per repeat; with 1 UV = 1 m meshes scale UVs by
`1 / tile_m`; `null` for cards), `source` (`AI` + `ai_sources`, or `Procedural`), `metallic` (0 / 1),
`roughness_range`, `normal_strength`, `height_m` (relief the normal map was baked for), `mean_albedo` (linear RGB,
alpha-weighted for cards, for far-distance colour), and where relevant `masked` + `two_sided` + `opacity_clip` (cards),
`translucent` (Glass, Foam), `emissive_color` + `emissive_strength` (Crystal), `orientation` (roofs, wood, bark).
Terrain entries also carry `layer`. Roof textures follow the kit's roof UVs: rows along U, V up the slope (ridge at the
image top).

## 9. Audio (`SourceArt/Audio/`)

WAV, 16-bit PCM, 44.1 kHz; mono for 3D one-shots, stereo for UI, ambience and music; peaks at -1 dBFS; loops seamless.
`SourceArt/Audio/manifest.json` lists every file with its category, duration, loop flag and intended use.

## 10. Game-side names

- Imported assets live under `/Game/LaPlace/...` (`Kit`, `Textures`, `Materials`, `Audio`, `VFX`, `UI`). They are
  regenerated by `Tools/mac/build_and_setup.sh` and git-ignored, like the other editor content.
- Paid / non-deterministic sources (Higgsfield images and models) live in `SourceArt/AI/` and **are** committed.
- The world map is `/Game/Maps/L_LaPlace` (World Partition, landscape from section 2). `L_Fittoa` is retired.
