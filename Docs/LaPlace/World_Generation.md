# LA PLACE world generator

`Tools/world/generate_world.py` builds the whole LA PLACE landscape (Spec.md sections 2-5) from hand-authored
geography plus deterministic procedural passes. It uses only Python 3.11, numpy and Pillow (no scipy, no network)
and writes every output listed in Spec section 5.

## Running it

```sh
PY=~/.venvs/mushoku-bpy311/bin/python
$PY Tools/world/generate_world.py                 # full run: all world data + WorldMap + preview + log
$PY Tools/world/check_world.py                    # validation (reads outputs only), exit code 1 on failure
$PY Tools/world/generate_world.py --maps-only     # redraw WorldMap.png + preview, writes no world data
$PY Tools/world/generate_world.py --preview-only  # redraw Docs/Images/LaPlace_World_Preview.png only
```

- **Deterministic.** The seed is `wg_config.SEED = 20260928`. A redraw run recomputes everything in memory and
  confirms that `Height.r16` on disk matches the recomputed terrain byte for byte.
- **Run time for the final data:** 124 s wall, 125 s CPU, peak RSS 3.67 GB (MacBook M3 Pro). This was measured
  while other jobs kept the machine at a load average of about 30, so an idle machine should be faster. Stage times:
  design 23 s, erosion 30 s, hydrology 5 s, sites 2 s, roads 5 s, full-resolution terrain 10 s, layers and
  densities 23 s, JSON 1 s, maps 25 s.
- `SourceArt/World/generation_log.json` holds the timings, per-region statistics, output sizes, regeneration
  commands and the latest validation summary. `check_world.py` also writes `SourceArt/World/validation_report.json`.

## Modules (`Tools/world/`)

| File | Role |
|---|---|
| `wg_config.py` | Grid sizes, landscape encoding, region table, layer and density names, output paths |
| `wg_geo.py` | Hand-authored geography in (u, v): continent and island outlines, region partitions, range crest lines (height and width per vertex), passes and pass corridors, craters, caldera, ring, buttes, river guides, lakes, sites, road stops |
| `wg_noise.py` | Vectorised gradient noise: fBm, ridged multifractal, billow, domain warp. Every octave is rotated and offset |
| `wg_grid.py` | Vertex-aligned grids (3 m / 9 m / 18 m), overshoot-free cubic upsampling, blurs, jump-flood distance transforms, polyline fields, labelling, rasterisation |
| `wg_terrain.py` | Designed 9 m heightfield: land, regions, base relief, ranges, Heaven, Demon, Begaritt, lakes, sea floor |
| `wg_erosion.py` | Priority-flood routing, uplift and stream-power landscape evolution, talus relaxation, slope cap |
| `wg_hydro.py` | Pass corridors, river valleys, lake levels, river tracing, water surfaces, channel carving, dry beds |
| `wg_sites.py` | Site placement, footprint planes, landmark mounds, districts, landmarks |
| `wg_roads.py` | A* road network, smoothing with a minimum turning radius, grade-limited profiles, bridges |
| `wg_fullres.py` | 3 m assembly: upsampling, detail, dunes, rivers, lakes, towns, roads, uint16 encoding |
| `wg_paint.py` | Paint layers (sum 255), density maps, `Regions.png` |
| `wg_maps.py` | Parchment `WorldMap.png` (no text) and the labelled review preview |
| `wg_export.py` | `World.json`, `Locations.json`, lake polygons, preview cameras |
| `check_world.py` | Validation |

## Method

1. **Geography, hand-traced.** Each continent is a coarse polygon measured by hand against the Japanese reference
   layout: Central has 99 vertices, Heaven 24, Demon 64, Millis 51 and Begaritt 47, plus 47 island outlines.
   Only positions are taken from the reference. The polygons are rasterised on an 18 m grid, converted to a signed
   distance field, domain-warped (150 m) and given 7-octave fractal edge noise (±75 m on continents, ±22 m on
   islands) on the 9 m grid. Specks and pockets are then removed. Near the coast every profile uses the continuous
   fractal distance field, so the waterline is smooth at any resolution. Each connected landmass gets exactly one
   continent (majority vote).
2. **Regions.** Partition polygons are evaluated at noise-warped coordinates. The Red Wyrm and Blue Wyrm bodies,
   including their foothills, come from the range masks, and the Rikarisu bowl comes from the crater radius.
   Per-region parameters (base level, hill amplitude, coast ramp, erodibility, repose angle) are blended across a
   300 m blur.
3. **Designed relief.**
   - Lowlands are warped billow hills with creased side valleys, broad uplands and a 480 m mid-scale term.
   - Beaches follow the coast ramp; cliff coasts are noise-selected.
   - Ranges use a crest-distance profile (with a per-vertex width factor) × ridged multifractal, plus foothills.
   - Heaven is a plateau at about 835 m with rounded knolls. Its sheer edge has three riser bands whose positions
     and heights vary with noise; the ledges wander relative to the coast, couloirs cut back into the rim, and
     scree lies at the foot.
   - The Demon Continent has ridged badlands, winding ravines up to 48 m deep, jagged crags, nine craters (rims
     broken by radial gullies, Rikarisu with a flat 350 m floor and a road notch) and a breached caldera.
   - Begaritt has warped three-tier mesas, canyons, the irregular gullied rocky ring (300-500 m) around a 95 m
     basin, and three lobed buttes with cap rock, ledges and talus aprons.
   - The sea floor is a shelf to -30 m near coasts and -200 m in the open sea, dropping steeper at Heaven.
4. **Landscape evolution** (18 m grid, 80 steps, Central and Millis ranges only). Tectonic uplift regrows each range
   while an implicit stream-power law erodes it: Braun and Willett 2013, K = 0.2, m = 0.5, n = 1, with
   priority-flood routing and steepest-descent receivers on the filled surface. Talus relaxation at a 39° repose
   angle then gives dendritic valleys with sharp ridges.
   - The relief is rescaled toward the designed envelope (gain 0.6-1.25), and an exact slope cap keeps flanks at or
     below 50° at 18 m. The cap is a Dijkstra-style threshold-hillslope surface.
   - The result is resampled through a gentle domain warp to break D8 grid alignment.
   - A final slope cap on the 9 m grid limits ranges to 55°, other land to 64° and Heaven to 80°.
   - The Demon Continent and Begaritt are left as designed: the simulation smoothed their crags and mesas.
   - Particle (droplet) erosion was tried and dropped because it left streaky textures at this scale.
5. **Hydrology** (9 m grid).
   - Graded pass corridors cut broad cols: the Upper Jaw at 120 m, the Lower Jaw at 120 m (8.5% max), and the ring
     gate canyon.
   - Meandering river valleys are carved along guide lines, with floors below the lowest ground within 170 m that
     never rise downstream.
   - Designed lake basins get natural rims, and each outflow valley starts exactly at its lake's level.
   - Priority-flood routing then gives the drainage area. Lake levels are the basin spill levels. Rivers are traced
     downstream from their guide sources and extended upstream along the largest tributary.
   - Water surfaces run 0.8 m below the floodplain, never rise downstream, and reach 0 at the sea or the lake level.
     Width grows with √(drainage area), and channels are carved with banks at least 0.25 m above the water.
   - Begaritt dry riverbeds are the main stems of the largest desert basins.
6. **Sites.** Each site is searched for within its radius of the spec target: inland sites need dry land in their
   region, away from water, with low roughness; ports straddle the shoreline; fixed sites (Rikarisu, Millishion)
   have terrain designed around them; labyrinth gates sit at the foot of a rock face ≥ 25 m high on an open apron.
   Each footprint is a plane (tilt ≤ 2%) inside 0.62 R that blends to the terrain by 1.45 R + 40 m along an organic
   outline. The Silver Palace rise (+18 m) sits at the edge of Ars (0.93 R) and the Greyrat house hill (+5 m) just
   outside Buena, both outside the flat 0.62 R core.
7. **Roads.**
   - A* runs on the 18 m grid with 16 move directions. Costs cover grade (hard limit 20%), side slope seen at 9 m,
     altitude and wetness; river entry costs 260 m; existing roads cost 45%.
   - Roads pass through waypoints: Upper Jaw, Lower Jaw, Strife south, Kikka plain, King's coast, tail, Great
     Forest heart and Rapan harbour.
   - Paths are smoothed with an 18 m tether and a minimum turning radius of 14 m, then resampled every 6 m.
   - Profiles are grade-limited to 12%. They follow towns (plane inside 0.62 R), rise over bridges (deck at water
     + 2.2 m + 6% of the river width), and on stretches shared with an earlier road keep that road's exact level.
   - At 3 m each road is flattened with a level cross-section, a 0.12 m crown and embankments scaled to the height
     difference. Bridge spans leave the channel untouched.
8. **3 m assembly.**
   - The 9 m terrain is upsampled with an overshoot-free (clamped) Catmull-Rom filter; plain Catmull-Rom rang by up
     to ±22 m at cliff feet.
   - Detail noise is added, scaled by slope and biome and faded near water, towns and sea level.
   - Begaritt dunes are added: transverse, 185 m wavelength, wind toward the WSW, gentle windward faces and steep
     slip faces, a 90 m oblique set, 720 m draa and ripples.
   - Rivers, lakes, town footprints and roads are applied last, and the result is encoded as uint16.
9. **Paint and foliage.**
   - Layer weights start from region affinity with warped boundaries.
   - Modifiers: Rock on slopes over about 30°; the snow line (550 m in the Red Wyrm, 600 m in the King Dragon and
     Blue Wyrm, the whole north), stronger on north-facing slopes and none on cliffs; beaches; riverbanks
     (Mud/Sand); lake shores; noise-shaped forest patches; patchwork fields (a rotated, jittered-grid Voronoi
     around the farming towns of Asura, Fittoa and Millis); oases; ravine dust; the road mask.
   - Weights are sharpened (power 2.2) and quantised so every vertex sums to exactly 255.
   - Densities are sampled at the 12 m pixel centres and zeroed on roads, water, town footprints (+8 m) and cliffs
     steeper than 45°.
10. **WorldMap.png** is rendered in 256-row bands:
    - paper grain, stains, foxing and fold creases;
    - a depth-tinted sea with coastal ripple lines, engraved hatching in open water and wave marks;
    - soft watercolour biome washes over a warm-light, cool-shadow hillshade;
    - inked coast and lake shores, rivers, dotted roads and dune strokes;
    - forest glyphs (conifer, deciduous, purple twisted flora), cliff hachures around Heaven, and multi-peak
      mountain glyphs at real summits and ridges;
    - a compass rose and a frame.

    It has no text. Pixel (x, y) covers u = x / 4096, v = y / 3072, the same mapping as `MapUV`.

## Outputs

| File | Size | Content |
|---|---|---|
| `SourceArt/World/Height.r16` | 55.8 MB | 6097 × 4573 uint16 LE. Z_cm = 72400 + (h − 32768) · 400/128, so h = 9600 is Z = 0 |
| `SourceArt/World/Layers/<Layer>.png` | 11 files, 0.09-1.9 MB each | 6097 × 4573 8-bit; Grass, Farmland, ForestFloor, Moss, Snow, Sand, Desert, DemonSoil, Rock, Road, Mud; each vertex sums to 255 |
| `SourceArt/World/Regions.png` | 20 KB | 1524 × 1143 region ids 0-15. Pixel p covers vertices 4p..4p+3 |
| `SourceArt/World/Density/<Kind>.png` | 5 files, 57-293 KB each | 1524 × 1143 densities 0-255: Trees, Bushes, Grass, Rocks, Flowers |
| `SourceArt/World/WorldMap.png` | 9.5 MB | 4096 × 3072 parchment map, no text |
| `Docs/Images/LaPlace_World_Preview.png` | 4.5 MB | 3048 × 2286 hillshaded review map with labels |
| `Content/Data/World.json` | 0.87 MB | See the World.json section below |
| `Content/Data/Locations.json` | 21 KB | 30 rows in the new schema: 18 sites and 12 legacy Fittoa quest markers |
| `SourceArt/World/generation_log.json`, `validation_report.json` | small | Run and validation logs |

### World.json

- **Top level:** `Landscape` constants and `MapToWorld`, then `Regions`, `Sites`, `Roads`, `Rivers`, `Lakes`,
  `Bridges`, `DryRiverbeds`, `Passes` and `MountainRanges`. Positions are UE cm and Z is the height above sea
  level.
- **Regions:** id, key, name, continent, biome, area in km², and height min / median / max.
- **Sites:**
  - `Center`, `GroundZ`, `RadiusCm`, `Yaw` (toward the main road), `TiltPercent`, `MapUV`, `MovedFromSpecM`;
  - `Districts`: rings with inner and outer radius in cm;
  - `Landmarks`: name, location, yaw, radius, rise;
  - `RoadEntry`.
- **Roads:** 14 roads, 48.4 km in total, one point every 6 m with ground Z, plus width, class, stops and max
  grade.
- **Rivers:** 14 rivers, 20.6 km in total. Each point has X, Y, water-surface Z, `WidthCm` and `DepthCm`, plus
  `Mouth` (Sea or Lake:<id>) and `FromLake`.
- **Lakes:** 11 lakes, each a polygon with `WaterZ`, `Frozen` and `Kind` (Lake, CraterLake or Oasis).
- **Bridges:** 6 bridges with start, end, `DeckZ`, `WaterZ`, length and width.

### Locations.json

- Every row has all the fields from Spec section 5. `WorldLocation` sits on the ground.
- Spawn points (`bSpawnPoint`) are Buena, Roa, Ars, Sharia, Rikarisu, Millishion and Rapan. Wenport and Zant Port
  are fast travel only. The other sites are discoverable, non-travel rows.
- A spawn point sits on the main road at 0.45 R from the site centre, inside the flat core, facing the centre.
- `PreviewCamera` is about 2.4 R away and raised until its line of sight clears the terrain.
- The 12 legacy quest ids (`Buena_Village` … `Fittoa_Wyrm_Foothills`) are kept, re-placed around the new Buena
  and Roa, so `Quests.json` still resolves.

## Validation results (final data, `check_world.py`)

**0 failures, 0 warnings.**

- **Heights:** raw h 3089-37803, Z −203.5 m to 881.3 m, no clipping. h = 9600 decodes to Z = 0.
- **Land:** 37.9% of the landscape.
  - Central 43.96 km², Demon 22.94, Begaritt 12.49, Millis 10.99, Heaven 2.55, islands 1.91.
- **Heights per region (m, min / median / max):**
  - The minimums are slightly negative because 12 m region pixels straddle the waterline.
  - Asura −2.7 / 24.4 / 119.5; Fittoa −2.4 / 37.8 / 130.2; Red Wyrm −0.5 / 238.5 / 775.5;
    North −2.4 / 65.6 / 481.7.
  - Strife −2.5 / 84.1 / 441.3; Southern Central −3.1 / 38.0 / 546.3; Heaven −23 / 823.9 / 881.2;
    Demon −2.9 / 116.4 / 657.2; Rikarisu Crater 117.2 / 140.6 / 343.2.
  - Great Forest −2.4 / 38.3 / 104.2; Millis Lowlands −2.8 / 20.4 / 47.9; Blue Wyrm −0.6 / 131.6 / 565.0.
  - Begaritt Desert −4.7 / 39.4 / 218.1; Begaritt Badlands −4.8 / 132.8 / 488.1; Islands −0.9 / 2.1 / 141.2.
- **Slopes on land:**

  | Slope | 0-2° | 2-5° | 5-10° | 10-15° | 15-20° | 20-30° | 30-45° | 45-60° | 60-90° |
  |---|---|---|---|---|---|---|---|---|---|
  | Share of land | 7.0% | 14.9% | 20.7% | 12.6% | 7.8% | 8.8% | 7.9% | 15.5% | 4.8% |

- **Spikes:** 27 isolated spikes or pits over 2 m, all single vertices on the Heaven cliff risers. 1080 vertices
  sit on sharp cliff edges with more than 6 m of curvature.
- **Paint:** all 27,881,581 vertices sum to exactly 255. Land coverage: Rock 28.0%, DemonSoil 17.2%, Grass 16.3%,
  ForestFloor 12.8%, Desert 7.8%, Sand 6.3%, Snow 6.1%, Farmland 3.0%, Mud 1.5%, Moss 0.6%, Road 0.4%.
- **Sites:** all 18 are on land and in their region, with tilt ≤ 2.01%, plane residual ≤ 0.34 m and p99 slope
  ≤ 4.2% inside 0.6 R. Ports are judged on their dry part.
- **Spawns:** all 7 are on dry, flat ground, with relief 0.12-0.38 m within 9 m, none in a lake and at least 96 m
  from any river.
- **Roads:** max grade 12.1% (p99 ≤ 12.0%). Road Z is within 0.71 m of the final ground, excluding bridge spans
  (the 0.71 m is where the Ring Gate track meets the gate apron).
  Minimum turning radius is 14 m.
- **Rivers:** all 14 have water surfaces that never rise downstream; 12 end at the sea at Z = 0 and 2 at their lake
  level. 6 bridges.
- **Densities:** all five are zero on roads, water and town footprints.
- **Locations.json:** all required fields are present. MapUV is within 0..1, WorldLocation Z is within 0.3 m of the
  ground, and every preview camera is above the ground.

## Sites and spawns (UE cm)

| Site | Center X | Center Y | Ground Z | Radius | Spawn / fast-travel location (X, Y, Z), yaw |
|---|---|---|---|---|---|
| Ars | -758952 | -260604 | 2325 | 45000 | spawn (-750506, -279188, 2596), 114° |
| Roa | -745011 | -381638 | 4403 | 19000 | spawn (-749202, -374251, 4369), −60° |
| Buena | -654183 | -331260 | 5853 | 13000 | spawn (-658077, -335828, 5838), 50° |
| Sharia | -672026 | -543640 | 8288 | 30000 | spawn (-685519, -541800, 8485), −8° |
| Rikarisu | 704088 | -404622 | 11830 | 22000 | spawn (703801, -394802, 11840), −88° |
| Millishion | 470002 | 407365 | 3312 | 42000 | spawn (451213, 406800, 3324), 2° |
| Rapan | -365093 | 578930 | 4623 | 28000 | spawn (-356069, 570269, 4572), 136° |
| Wenport | 785619 | 96444 | 270 | 14000 | fast travel (790781, 92415, 278), 142° |
| Zant Port | 642769 | 235981 | 263 | 13000 | fast travel (642600, 241794, 268), −88° |
| West Port | 124358 | 524694 | 502 | 12000 | none |
| East Port | 55378 | 477945 | 220 | 12000 | none |
| King Dragon | -7811 | 466754 | 3007 | 22000 | none |
| Sword Sanctuary | -866536 | -558898 | 3610 | 10000 | none |
| Labyrinth 1 (ring) | -706500 | 390600 | 9409 | 4000 | none |
| Labyrinth 2 (mesa) | -754200 | 251100 | 9079 | 4000 | none |
| Labyrinth 3 (canyon) | -712800 | 271800 | 6632 | 4000 | none |
| Labyrinth 4 (butte) | -449100 | 536400 | 2758 | 4000 | none |
| Labyrinth 5 (south butte) | -707400 | 544500 | 4677 | 4000 | none |

## Deviations from the Spec and the references

**Geography**
- **Heaven Continent:** a separate cliff-walled plateau across a ~350 m strait from the Northern Territories. The
  Japanese map shows a ~190 m neck; the spec asks for cliffs down to the sea. The strip is also about 120 m wider
  than on the reference.
- **Coast margins:** a sea margin is kept inside the landscape. The southern Begaritt coast is pulled ~200 m north
  and the Demon east coast ~250 m west.
- **King Dragon:** the Central tail is widened ~150 m around the King Dragon capital so the 220 m city fits.

**Red Wyrm range**
- One connected range: a north-west arm runs from the spine start to the west coast, with the Upper Jaw col at
  (0.056, 0.165).
- The spine ends in a coastal spur with the Lower Jaw col at (0.2165, 0.578).
- The north-east branch joins the spine at (0.16, 0.217).
- Both cols are graded corridors (saddle 120 m, ≤ 8.5%).
- Peak heights: max 776 m, median 239 m. The spec says 600-900 m; the 50°/55° slope caps limit the heights.

**Sites** (the spec lets sites move onto suitable land)
- **Sharia:** target (0.138, 0.100), because at u = 0.10 the north is only ~0.9 km wide between the coast and the
  range.
- **Rikarisu:** its crater is centred at (0.885, 0.205) so the rim stays off the north coast.
- **Millishion:** at (0.757, 0.797), with its lake to the south, clear of the Blue Wyrm's west end.
- **Coast and gates:** Zant Port, Wenport and the Sword Sanctuary were moved onto the coast or land; the spec
  points are offshore. Labyrinth gates moved 234-678 m to real rock faces.
- **Strife road route:** the Lower Jaw road goes around the north end of the King Dragon range and down the
  east-coast plain.

**Regions and Locations.json**
- **Strife Zone:** median 84 m, within spec. Its maximum of 441 m is where it meets Red Wyrm foothills.
- **Locations.json:** the 12 legacy quest locations are kept for `Quests.json` / `validate_data.py`.
  `FMTLocationData` must gain the new fields (`Continent`, `Biome`, `Difficulty`, `Description`, `SpawnYaw`,
  `bSpawnPoint`, `MapUV`, `PreviewCamera`) before `Tools/validate_data.py` stops flagging unknown keys.

## Known issues and limitations

- **Below-sea-level pockets.** 22 isolated pockets not connected to the sea add up to 0.055 km².
  - The six largest hold 99% of that area. They are the lowest few hundred metres of the Ars, Fittoa, Strife and
    Kings rivers, the Holy Lake Outflow and the Frost Outflow: channel beds 2.4-3.5 m below sea level behind a
    sub-metre sill at the mouth. They lie inside river channels that carry water in `World.json`, so no dry land
    floods.
  - The rest are 16 hollows at the Heaven cliff foot, each ≤ 216 m² and ≤ 0.47 m deep.
- **Steep terrain.** 20% of land is steeper than 45° and 4.8% steeper than 60°. This comes from the Heaven cliffs
  (by design), mesas, crags and the ranges: 600-900 m peaks fit into 1.5-2.5 km wide belts. Range flanks are capped
  at 55° at 9 m, and at 3 m rock detail adds local steepness. Such faces will need triplanar rock materials.
- **Crater Road.** One ~20 m deep road cut through a badland ridge on the Demon Continent.
- **Lowland drainage.** Lowland hills are designed (billow and fBm with creases), not simulated: the stream-power
  simulation produced grid-aligned channels on gentle slopes. Only the ranges carry simulated dendritic networks.
  Rivers follow authored guide lines.
- **Rivers are short** (0.5-2.6 km) because the continents are only a few kilometres across.
- **WorldMap.png** has no settlement icons or text by design; the UI draws them.
- **Labyrinth gates** are aprons with a yaw facing away from the rock face. The gate mesh is expected to be set into
  that face.
