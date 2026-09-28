# Implementation plan

Scope rule: only the **Rudeus** and **Orsted** lineages until every system below passes QA (≥ 8.5 overall, no critical category < 8.0).

## Architecture (built)

```
Source/MushokuRPG/
  Core/        MTTypes (enums, damage/status structs), MTDataTypes (all data rows), MTDataRegistry
               (loads Content/Data/*.json), MTGameEvents (event bus), MTGameplayTags, MTGameMode + controller
  Character/   MTCharacterBase (lineage/race/element application, hits, dodge, perfect-defense hook),
               MTAttributeComponent (HP/MP/stamina/poise, status effects, combo scaling, block),
               MTPlayerCharacter (camera, runtime Enhanced Input, lock-on, interaction, Demon Eye)
  Animation/   MTAnimInstance (thread-safe locomotion/combat variables)
  Abilities/   MTAbility (phase machine: anticipation/charge -> action -> recovery),
               MTAbilityBehaviors (Projectile, Zone, Sequence, Dash, Counter, Buff, Structure, Melee),
               MTAbilityComponent (slots, cooldowns, input buffer, overrides, mastery reporting)
  Combat/      MTProjectile, MTZoneActor (decal-based ground spells), MTEarthWall, MTForesightGhost,
               MTTelegraphSubsystem (attack announcements + live spell registry), MTCombatStatics
  Progression/ MTProgressionSubsystem (level, mastery, magic rank, adventurer rank, spins, inventory),
               MTRollSubsystem (character/element/race rolls, pity, duplicate protection, history)
  Save/        MTSaveTypes (versioned payload), MTSaveGame, MTSaveSubsystem (autosave, migration)
  Quests/      MTQuestSubsystem, interactables, quest givers, pickups, triggers, puzzles, world events
  AI/          MTEnemyCharacter, MTEnemyAIController (utility AI), MTBossCharacter, MTTownNPC + schedules,
               MTEnemySpawner (validated spawns), MTTestArenaGameMode (Rudeus vs Orsted)
  World/       MTTimeOfDaySubsystem, MTDayNightController, MTPlacementValidator, MTVillageGenerator (Buena),
               MTWorldValidationLibrary + MTValidateWorld commandlet (z-fighting/overlap/floating scans)
  UI/          MTHUD (canvas HUD + all menus + roll screen, zero asset dependencies)
Content/Data/  Characters, Abilities, Elements, Races, Quests, Enemies, Items, Locations, RollConfigs (JSON)
Content/Python/ editor automation (Rudeus import + IK Rig/Retargeter, materials, world setup, validation)
Tools/         offline tools (GLB inspection and normalisation, terrain generation, data validation, roll simulation)
```

Adding a character later means adding one row to `Characters.json` plus its ability rows, a mesh, and an AnimBP. No code changes are needed unless the character needs a brand-new behaviour class.

## Phases and status

| Phase | Status | Remaining to reach the QA bar |
|---|---|---|
| 0 Audit | Done | — |
| 1 Rudeus import / rig / camera / input | Compiled on UE 5.8.3 and imported headless (`SK_Rudeus` 161.7 cm, 37 clips, sockets); runtime body/pose test passes | Play-In-Editor pass; deformation test on extreme poses |
| 2 Animation | 37 Rudeus + 40 Orsted clips authored on each skeleton, measured (`QA_Animation.md`, `QA_Orsted.md`) and imported; graph-free native playback | Watch blends in PIE; optional Mixamo clips; AnimDynamics / cloth on the coat chains |
| 3 Movement | Code done (walk/run/sprint/jump/dodge, root-motion dodges, stamina) | In-engine tuning |
| 4 Combat foundation | Code done (health/mana/stamina/poise, damage, cooldowns, buffer, lock-on, combo scaling, block, hit reactions) | In-engine tuning, hit-stop, montage content |
| 5 Rudeus abilities | Code and data done (Stone Cannon charge, Quagmire, Elemental Barrage, Demon Eye, Quagmire Magician) | Niagara assets, custom animations, SFX |
| 6 Orsted | Generated original model re-rigged onto the shared skeleton, 40 clips, imported (`SK_Orsted` 195 cm); Disturb Magic, Dragon Step (arrival strike now lands) and lock-on verified by runtime tests (`QA_Orsted.md`) | PIE feel pass, VFX, coat simulation, higher-budget mesh / texture |
| 7 Elements | 4 elements × 3 abilities in data, with behaviours implemented | VFX/SFX content |
| 8 Character roll | Done (pool = Rudeus + Orsted only, pity, duplicate protection, history, skippable) | UI polish |
| 9 Race framework | All 10 races in data, 3 rollable (Human, Migurd, Beast) | Race-specific VFX |
| 10 Fittoa | Terrain generated; Buena generator and validation done; setup script ready | Real meshes (currently blockout), foliage, landscape material, lighting pass |
| 11 Quests | Framework and a variety of quests done | Dialogue presentation polish, quest content in world |
| 12 AI | Utility enemy AI, boss phases, NPC schedules with LOD | Tuning, animation content |
| 13 UI | Canvas HUD and menus (functional) | UMG/Common UI visual polish pass |
| 14 Save | Versioned save with autosave | In-engine save/load round-trip test |
| 15 Polish | Not started | Everything above, in-engine |

## Next actions, in order

1. ~~Compile~~ and ~~run the editor setup~~: done on UE 5.8.3 (`Tools/mac/build_and_setup.sh` → `RESULT: OK`, 7/7 runtime tests).
2. Play-In-Editor with `MTSetCharacter Orsted` and with Rudeus: walk the animation checklist in `Docs/QA_Orsted.md` §C.
3. Import the Fittoa heightmap, press Generate on the Buena generator, re-run `MTValidateWorld`.
4. Niagara / audio assets for the spells (every `/Game/VFX` and `/Game/Audio` path in the data is a placeholder).
5. Regrade honestly with the same tables.
