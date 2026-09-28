# LA PLACE world build

How the generated world becomes the playable map `/Game/Maps/L_LaPlace`. `Tools/mac/build_and_setup.sh` runs every
step. Each step can also run on its own, in this order:

| # | Step | Command | Output |
|---|---|---|---|
| 1 | Terrain, paint, regions, rivers, roads, sites | `$BPY Tools/world/generate_world.py` | `SourceArt/World/{Height.r16, Layers/, Regions.png, Density/, WorldMap.png}`, `Content/Data/{World,Locations}.json` |
| 2 | City layouts | `$BPY Tools/world/generate_cities.py` | `SourceArt/World/Scatter/Cities.{json,bin}`, `SourceArt/World/Cities/Street{Cobble,Dirt}.png` |
| 3 | Final paint + macro maps | `$BPY Tools/world/finalize_world.py` | `SourceArt/World/LayersFinal/` (12 layers incl. Cobble), `SourceArt/World/Macro/{RegionTint,GrassMix}.png`, `Content/Data/WorldRegions.png` |
| 4 | Nature scatter | `$BPY Tools/world/scatter_foliage.py` | `SourceArt/World/Scatter/Foliage.{json,bin}` (trees, bushes, rocks, cliffs; avoids cities and streets) |
| 5 | Textures | `$BPY Tools/textures/build_textures.py` | `SourceArt/Textures/{Terrain,Palette,Water}/` |
| 6 | Kit meshes + materials | `Tools/mac/run_editor_python.sh mt_setup_kit.py` | `/Game/LaPlace/Kit/**` (Nanite; trunk / complex collision), `MI_MT_*` |
| 7 | The map | `RENDER=1 MT_WORLD_FRESH=1 Tools/mac/run_editor_python.sh mt_build_world.py` | `/Game/Maps/L_LaPlace` |
| 8 | HLODs | `Tools/mac/build_hlods.sh` | instanced HLOD actors |
| 9 | Look at it | `Tools/mac/world_tour.sh` | `Saved/Screenshots/WorldTour/*.png` |

`$BPY` is `~/.venvs/mushoku-bpy311/bin/python`. Steps 1–5 are deterministic and git-ignored. Paid AI sources live in
`SourceArt/AI/` and are committed.

## What is in the map

- **World Partition map.** Runtime hash set with 512 m cells and a 1.6 km loading range. Streamed content includes the
  city buildings, walls and props, plus the nature tiles, each 1024 m square.
- **Landscape.** 6097 x 4573 vertices at 3 m, split into World Partition streaming proxies that are always loaded, so
  the terrain is visible to the horizon. 12 weight-blended paint layers.
  - Material `M_LaPlace_Landscape` (a generated Custom HLSL node). Layers are height-blended, and only layers with
    weight are sampled.
  - Colour: region tint from `RegionTint.png`, two-scale anti-tiling, and macro variation.
  - Surface: triplanar cliff rock tinted to the local ground, and a wet band along the shore.
  - Landscape grass: eight grass types (`/Game/LaPlace/World/Grass/LGT_*`) placed by the GPU around the camera. Their
    densities come from `GrassMix.png` and the paint layers.
- **Water.** Single Layer Water ocean at Z = 0 (120 km across), lake polygons at their water level (frozen lakes use
  ice), and river ribbons along their courses. Materials `M_LaPlace_{Ocean,Lake,River,Ice}`.
- **Sky.** Sun (moved by `AMTDayNightController`), sky atmosphere, real-time sky light, height fog, volumetric clouds,
  and an unbound post-process volume.
- **Regions.** At runtime `UMTRegionSubsystem` looks up the player's region in `WorldRegions.png` and drives:
  - per-region fog density and tint;
  - colour grading;
  - weather (`AMTWeatherActor`): snow, ash, embers, dust and haze, pollen, fireflies at night, and mist;
  - the region-name banner.

  Tuning lives in `Content/Data/RegionAtmosphere.json`.
- **HLOD.** Layers `HLOD_City` and `HLOD_Nature` are Instancing and always loaded. They are registered with the
  runtime hash set by `UMTWorldBuildLibrary::RegisterHLODLayers`.

## Editor helpers (C++)

`UMTWorldBuildLibrary` (`Source/MushokuRPG/Public/World/MTWorldBuildLibrary.h`) does the parts the Python API cannot:

- landscape import from a raw heightmap plus weightmaps, and World Partition streaming proxies;
- World Partition grid settings and HLOD registration;
- binary instance-set spawning into HISM actors;
- static meshes from triangles (water);
- trunk and complex collision, and single-rebuild material/Nanite configuration for kit meshes;
- material compile-error reporting.
