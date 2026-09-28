# QA: World (Fittoa / Buena Village)

Status as of this commit. **Nothing on the C++ side has been compiled.** This container has no Unreal Engine, so
every C++ item below is unverified until someone runs a UE 5.5 build.

## 1. Tests that were actually run here

### Placement validator: Python mirror of the C++ SAT, spacing and polyline maths
`python3 Tools/placement_validator_test.py` (exit code 0):

```
PASS sat_identical_boxes_overlap
PASS sat_separated_on_x
PASS sat_touching_edges_do_not_overlap
PASS sat_padding_creates_overlap
PASS sat_rotated_diamonds_separated_although_aabbs_overlap
PASS sat_rotated_diamonds_overlap_when_close
PASS sat_symmetric
PASS sat_corner_poke_detected
PASS sat_corner_miss_detected
PASS sat_matches_bruteforce_250_of_250
PASS circle_inside_rect_overlaps
PASS circle_far_no_overlap
PASS circle_near_rotated_corner
PASS circle_tangent_no_overlap
PASS circles_overlap
PASS circles_touching_no_overlap
PASS point_polyline_distance
PASS point_polyline_second_segment
PASS footprint_crossing_road_is_zero
PASS footprint_road_distance_400.0
PASS rotated_footprint_road_distance_358.58
PASS footprint_padding_reduces_distance
PASS spacing_ok_at_25m
PASS spacing_rejects_24m
PASS spacing_empty_ok
PASS deterministic_results

26/26 tests passed
```

The mirror follows `MTPlacementValidator.cpp` line for line: the same 4 SAT axes, the same `1e-3` touch epsilon, the
same corner order, and the same segment-vs-OBB distance. The C++ file is the source of truth. **If you change one file,
change the other.** The randomised test compares SAT against a brute-force sampling reference on 250 random pairs
(seed 1717). All 250 agree.

### Terrain generator
`python3 Tools/generate_fittoa_terrain.py` checks its own output:
- Heightmap is 2017x2017 16-bit. Heights run 25.2 m to 235.6 m. **0 clipped samples.**
- The PNG and the `.r16` round-trip to identical arrays (checked with numpy).
- The weight maps sum to exactly 255 on every pixel (min 255, max 255).
- `Content/Data/Locations.json` has all 13 required LocationIDs. Every Z is sampled from the final heightmap.
- The overview (`Docs/Images/Fittoa_Overview.png`) was checked by eye. The village is in the south-west third, the
  hill is to its north-west, the stream runs north to south east of the village with a ford where the Roa road
  crosses, the Roa road leaves at the north-east edge, the foothills are along the north, and the arena is flat in
  the south-east.

## 2. Tests to run in the editor (not run here)

| # | Test | Expected |
|---|---|---|
| 1 | Build the editor target | The World module compiles. See the compile risks in the handoff and in Docs/World_Fittoa.md. |
| 2 | Run `Content/Python/mt_world_setup.py` | L_Fittoa exists (World Partition). It contains the DayNight controller, the village generator at the square, the NavMeshBounds volume and the PlayerStart. |
| 3 | Import the landscape as described in `SourceArt/Terrain/Fittoa/README.md` | The PlayerStart sits on the ground. The Locations.json Z values match the terrain to within about 2 cm. |
| 4 | Generator > Generate twice with the same seed | Identical layout and identical report. ClearGenerated leaves no actor tagged `MTGenerated` behind. |
| 5 | Read the generator report | 10 to 14 houses at least 25 m apart, 3 barns, fields, fences, trees, props. Rejection reasons are listed. Every basic-shape placeholder is logged with `[BLOCKOUT]`. |
| 6 | `UnrealEditor-Cmd MushokuRPG -run=MTValidateWorld -map=/Game/Maps/L_Fittoa` | Exit code 0 and `Saved/Validation/L_Fittoa.json` with 0 errors |
| 7 | Place two cubes at the same transform, then run `mt_validate_world.py` | 1 DuplicateMesh error |
| 8 | Put a 1 cm plane on top of a floor cube | 1 CoplanarOverlap error |
| 9 | PIE, set the time with `SetTimeOfDayHours(5.9)` and then 18.4 | Warm light at dawn and dusk. `OnNightChanged` fires. Night never goes pure black (moon floor of 0.08 lux, sky light at 0.35 or above). There are no recapture hitches: at most one recapture per 15 in-game minutes. |

## 3. Known gaps
- Every mesh is a basic-shape BLOCKOUT until art fills the generator's mesh slots.
- No water surface mesh exists yet. The stream is only the carved channel plus the riverbed layer.
- The Greyrat house and the lone big tree are hero assets to place by hand. The generator keeps exclusion zones
  free for them (set by mt_world_setup.py).
