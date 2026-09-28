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
| 1 Rudeus import / rig / camera / input | Code and scripts done, **not run in UE** | Compile; run `mt_create_materials.py` and `mt_setup_rudeus.py`; build the AnimBP; deformation test |
| 2 Animation | Pipeline scripted, **no clips yet** | Download Mixamo clips, retarget, fix the retarget pose, author custom Control Rig animations |
| 3 Movement | Code done (walk/run/sprint/jump/dodge, root-motion dodges, stamina) | In-engine tuning |
| 4 Combat foundation | Code done (health/mana/stamina/poise, damage, cooldowns, buffer, lock-on, combo scaling, block, hit reactions) | In-engine tuning, hit-stop, montage content |
| 5 Rudeus abilities | Code and data done (Stone Cannon charge, Quagmire, Elemental Barrage, Demon Eye, Quagmire Magician) | Niagara assets, custom animations, SFX |
| 6 Orsted | Gameplay done (Disturb Magic, Dragon Step, Saint Dragon Aura, Dragon God, passive); **no model** | Original model per `Orsted_Model_Spec.md`, rig, animations |
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

1. Open in UE 5.5 and compile. Fix any compile errors (none could be caught without the engine).
2. `py mt_create_materials.py`, then `py mt_setup_rudeus.py`. Check height 161.7 cm and the sockets.
3. Build `ABP_Rudeus` on `UMTAnimInstance` (see `Animation_Pipeline.md` §4). Download the Mixamo clips and re-run the setup script to batch-retarget.
4. `py mt_world_setup.py` to create L_Fittoa. Import the heightmap. Generate Buena. Run `mt_validate_world.py`.
5. Play. Walk the Phase 1 QA checklist. Regrade honestly.
