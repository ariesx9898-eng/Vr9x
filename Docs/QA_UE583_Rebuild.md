# UE 5.8.3 rebuild and verification (2026-09-28)

Project: `~/Downloads/MushokuRPG` (git branch `claude/modest-noether-7zw56h`), engine: Unreal Engine **5.8.3**
(`/Users/Shared/Epic Games/UE_5.8`, CL 58210709), Apple M3 Pro, Xcode 26.6. Every result below comes from the real engine
on this Mac unless it says otherwise. The only other copy on disk, `~/Downloads/Vr9x-claude-modest-noether-7zw56h`, is
an older zip extract (2026-09-27, no generated content) that Unreal has never opened.

Backup made first: an APFS clone of the whole project (no build caches) in
`~/Downloads/MushokuRPG_Backups/MushokuRPG_BACKUP_2026-09-28_before-UE583-rebuild`, plus git checkpoint `2c386a3`.

## What was broken, and the fix

| Problem | Cause | Fix |
|---|---|---|
| Two copies of the whole pipeline ran at once and overwrote each other's imports (kit, characters, maps) | Clicking the Desktop icon twice while the game module was missing started `build_and_setup.sh` twice | `Tools/mac/mt_lock.sh`: one content build at a time; `play.sh` waits and says so, and only recompiles when the code changed. Damaged content moved aside and rebuilt cleanly |
| HLOD build killed half way | A commandlet started from the `UnrealEditor` app opens a log window; closing it ends the run | Headless steps use `UnrealEditor-Cmd` (no window) |
| `L_Fittoa` player start 6.5 km outside its 2 km map; duplicate day-night controllers / village generators | `Locations.json` now holds LA PLACE world coordinates; one-per-map actors were streamed, so re-runs could not find them | The Fittoa setup keeps its own local Buena positions and makes those actors always loaded (a re-run reuses them) |
| Rings, beams, water dragon, spikes, Earth Wall drawn with the default material | Effect meshes imported with Nanite, which cannot draw their additive / translucent materials | Effect meshes import without Nanite |
| Dark square under the first ground impact of a session | The first draw of a decal material compiles its GPU pipeline; meanwhile the engine draws the whole decal box with the default material (confirmed: gone in slow motion) | Effect assets stay loaded; the ground-decal materials are drawn once, invisibly, when the player spawns |
| Disturb Magic invisible when nothing is disturbed | Its cast effect preset existed but was never spawned | The gold palm ring plays on every cast |
| Player could be dropped into empty space | Legacy fast travel teleported to raw coordinates; a save restored positions from another world | Fast travel / admin teleport only move onto ground this map has; out-of-world saved positions are ignored |
| `PresentationFallbacks` test failed | It predates the runtime travel effect that now draws projectiles | Test accepts the attached travel effect |
| UE 5.8 deprecation warnings (would break in the next release) | `Virtual_Accept`, HLOD layer spatial loading, `bCustomizedCollision` | Updated to the 5.8 APIs |

## Ability overhaul merged and made to work (later the same day)

A parallel cloud session pushed the LA PLACE ability overhaul (152 files: gameplay for the 18 abilities, layered effect
presets, a casting clip per ability, 26 new sounds, new tests), written without an engine. On this Mac it compiled first
time (0 errors, 0 warnings); its generators were re-run (VFX textures, sounds, VFX kit meshes; the building meshes came
out byte-identical), the effects, sounds and both characters re-imported (Rudeus 55 clips, Orsted 59). Its tests then
showed four problems, fixed here:

| Problem | Cause | Fix |
|---|---|---|
| Knockbacks died within ~2 m (Wind Burst threw bandits 2.2 m, designed ~8) | `BrakingFriction 6` (tuned for jumps) also brakes a falling body with no input or over walk speed | A hit's launch switches it off until the body stops falling |
| Water Dragon missed any prey within ~10 m | Its steering blended headings linearly; out of the circle it flipped and dived into the ground behind the caster | Yaw round level first, pitch onto the prey once it is ahead, turn rate grows near the prey |
| Loadout test: short cooldowns "never started", 13-15 "leaked" effects | Cooldowns checked after they had ended; idle pooled effects counted | Cooldown sampled during the cast; only effects still playing count |
| Tornado / Dragon Crush tests | The goblin died in the funnel (death disables movement); the throw was measured mid-flight | Goblin kept alive; measured after the flight |

## Verification (real engine)

| Check | Result |
|---|---|
| Clean C++ rebuild of `MushokuRPGEditor` | Succeeded, 0 errors, 0 warnings |
| `Tools/validate_data.py` | ALL CHECKS PASSED |
| Full pipeline (`build_and_setup.sh`): materials, UI/audio/VFX, Rudeus, Orsted, Fittoa, kit (172 meshes), LA PLACE world, HLODs | Every step ran. Every asset in the list taken before the rebuild is back (except two water meshes of the retired dev test map), plus the new LA PLACE world assets; `L_Fittoa` lost only its duplicate actors |
| LA PLACE world build | Landscape 6097 x 4573, ocean + 11 lakes + 14 rivers, 8 956 city and 85 864 foliage instances, 0 errors |
| HLODs (`build_hlods.sh`) | 517 / 517 built, 0 errors |
| Automation tests (`run_automation_tests.sh`) | 18 / 18 pass after the overhaul (9 / 9 before it) |
| `MushokuRPG.Abilities.RequiredSet` | All 17 must-keep abilities (Stone Cannon, Quagmire, Elemental Barrage, Disturb Magic, Dragon Step, Dragon Crush, Fireball, Flame Wave, Inferno, Water Bullet, Water Dragon, Flood, Earth Wall, Earth Spikes, Wind Blade, Tornado, Wind Burst) activate, show a runtime effect and damage / spawn their zone, wall or dash |
| Asset validation (`mt_validate_assets.py`) | 803 assets load, 0 failed; 2 112 packages (1 307 World Partition actors) with 0 missing hard references; both maps open |
| `CompileAllBlueprints` | 0 errors, 0 warnings (the project has no Blueprint assets of its own; engine / plugin Blueprints compiled) |
| `MTValidateWorld` on `L_Fittoa` | 0 errors, 0 warnings |
| Ability showcase (screenshots reviewed) | Before the overhaul 17 / 17; after it the overhaul's full showcase: 32 / 32 entries PASS (every ability, Orsted casting the element spells, both signature combos), no errors |
| UI tour on `L_LaPlace` (screenshots reviewed) | Title, map, edit, abilities, settings, HUD, pause, admin prompt and panel; the real 1+0 key press opens the admin popup; code accepted |
| World tour on `L_LaPlace` (19 screenshots reviewed) | Buena, Roa, Ars, Sharia, Rikarisu, Millishion, Rapan (view + on foot) and 5 aerials; 24-38 fps average at 1920x1080, one 2 s hitch on the first visit to Roa |
| Arena (`play.sh` default) on `L_LaPlace` | Arena Orsted spawns and fights at Buena |
| Unreal Editor opens `MushokuRPG.uproject` | Engine initialized, editor ready in 19 s, 0 errors (2 harmless warnings: audio sample-rate query, old window layout). With `L_LaPlace` open the editor grew to ~20 GB (every landscape edit layer is loaded) and swapped this 18 GB Mac to a halt, so the editor starts on `L_Fittoa`; the game runs `L_LaPlace` fine |

Automated game runs never write the player's save (`-MTNoSave`, or the save moved aside and restored byte for byte).

## Not done / known issues

- Road dressing (PCG) is not generated yet (`MT_WORLD_STAGES=pcg,save` + `Tools/mac/build_pcg.sh`, then HLODs again).
- 190 data references name art from the original design that was never made (item and race icons, race-ability sounds,
  meshes for wolves / goblins / bandits / the Red Wyrm). The game falls back for each, and none of those enemies is placed
  in a map. Listed by `mt_validate_assets.py`.
- First launch after a content rebuild compiles shaders ("Preparing shaders" on screen) and hitches once per new area.
- The live location preview on the world map card is hazy.

## Needs a person

Playing it: combat feel, camera, how the world and effects read at full speed, and whether the LA PLACE map should replace
Fittoa in every mode (the launcher and the game default now use `L_LaPlace`), and grading the overhaul's effects and animation timing
(`Docs/QA_Abilities.md` §3).
