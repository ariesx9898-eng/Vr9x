# Phase 1 QA: Rudeus import, rig, camera, input, locomotion foundation

Scale: 0.0–10.0, where 8.5 means nearly AAA quality for this scope. Pass rule: overall ≥ 8.5 **and** no critical category below 8.0.

## Ground rule for this grade

This environment has **no Unreal Engine**. So:

- no C++ compile, no editor, no Play-In-Editor, no output log;
- nothing about in-engine behaviour could be observed.

Every category that depends on seeing the game run is graded on **evidence only**. Code that has never compiled or run is not a working feature, so those categories stay low. That is the honest result, not a pessimistic one.

## Evidence that *was* produced and tested here

| Test | Result |
|---|---|
| GLB parse + skin re-evaluation (`Tools/inspect_glb.py`) | 136-joint rig, T-pose, ≤4 normalized weights, 22.6k verts. Found a **10× scale error** (16.17 m). |
| Normalization (`Tools/prepare_rudeus_glb.py`) + re-verification | **161.7 cm**, feet at 0.0, bind pose and weights unchanged; ancestor scales flattened. **PASS** |
| Bind-pose render (`SourceArt/Characters/Rudeus/Rudeus_BindPose_Preview.png`) | Model matches the upload (face, colours, robe, proportions). **PASS** |
| Data validation (`Tools/validate_data.py`) | 62 abilities, 2 characters, 4×3 elements, 10 races, 16 quests: **ALL CHECKS PASSED** |
| Roll simulation (`Tools/sim_roll.py`, shipped config) | Orsted 6.1% per spin, guaranteed by 60, hard pity never exceeded: **PASS** |
| Placement validator mirror (`Tools/placement_validator_test.py`) | **26/26 PASS** |
| Terrain generation (`Tools/generate_fittoa_terrain.py`) | Heights 25–236 m, weight maps sum to 255 on every pixel: **PASS** |
| Static integrity check of the C++ module (89 files, ~23.7k lines, 102 reflected types) | Local includes resolve, `generated.h` correct and last, no duplicate types, no Blueprint-exposed weak pointers: **PASS** |
| Cross-module API check (HUD ↔ player ↔ progression ↔ save ↔ quests ↔ time of day) | Signatures match at every call site checked: **PASS** |
| Animation build metrics (`Tools/anim/build_rudeus_anims.py`, 37 clips) | 0.00 cm IK error on planted frames, 0.00 cm/s planted slip, 0.000° loop seams: **PASS** |
| Independent GLB check (`Tools/anim/verify_glb.py`, numpy skinning, no Blender) | 136 joints, 1.617 m, 37 clips with exact durations, floor-contact clips within 0.6 cm of the ground: **PASS** |
| Per-frame motion QA (`Tools/anim/qa_animation_quality.py`) | No hyperextension, no wrist twist, no quaternion flips, fastest limb 31.3°/frame (dash start), ankles ≥ 13.2 cm apart, planted-foot slide ≤ 4.2 cm (knockback / dodge landings): measured, see `Docs/QA_Animation.md` |
| Setup scripts against a mock `unreal` module | Rudeus 37/37 and Orsted 40/40 clips placed, second run idempotent, 0 FAIL lines |

## Grade: Phase 1 scope

| # | Category | Score | Why |
|---|---|---|---|
| 1 | Gameplay | 2.0 | Complete systems written, never played |
| 2 | Responsiveness | 1.5 | Input buffer, root-motion dodge, camera lag tuned on paper only |
| 3 | Animation | 3.5 | 37 clips authored on Rudeus's own skeleton and verified offline (metrics above), plus graph-free playback code. Never seen in Unreal, and the playback code has never compiled; procedural keys with no mocap nuance. |
| 4 | Character accuracy | 6.5 | Uses **your** model at the correct scale with original colours. Not yet seen in-engine; the lit toon material is untested. |
| 5 | Combat | 2.0 | Full framework in code, unplayed |
| 6 | VFX | 0.5 | No Niagara assets. Placeholders differ per element but are placeholders. |
| 7 | Environment | 2.0 | Real terrain data and a validated generator, but blockout meshes and nothing built in UE |
| 8 | UI | 2.0 | Functional canvas HUD and menus in code, unseen |
| 9 | AI | 1.5 | Utility AI, bosses and schedules in code, unplayed |
| 10 | Performance | 2.0 | Sensible choices (thread-safe anim, throttled scans, pooled FX, NPC LOD), unmeasured |
| 11 | Stability | 1.0 | Never compiled; first compile will surface errors |
| 12 | Lore / reference accuracy | 6.5 | Researched with CANON vs ORIGINAL labels, but only through search summaries (source pages were blocked) |

**Overall: 2.6 / 10. FAIL** (target 8.5; critical categories animation, stability and gameplay are far below 8.0).

## Improvement passes (five-pass rule)

### PASS 1
- **Score:** 1.9
- **Problems:** the model was 10× oversized; its material was unlit (would ignore the sun, shadows and day/night); FBX ancestor scales risked leaking into the root bone.
- **Changes:** `Tools/prepare_rudeus_glb.py` rescales translations, positions and inverse-bind matrices (a mathematically exact uniform scale), flattens the ancestors, and strips `KHR_materials_unlit`. The setup script assigns a lit toon material that keeps the painted atlas colours.
- **Testing:** re-skinned the output. Height 1.617 m, feet at 0, joint positions exactly ×0.1.
- **New score:** 2.1

### PASS 2
- **Score:** 2.1
- **Problems:** cross-module contract risk (four engineers writing in parallel); weak pointers exposed to Blueprint in shared structs (a UHT error); unreachable design gaps flagged in review (Second Wind/Immortal regen hooks, 90% stagger immunity from stacking, a roll sim not using the shipped config).
- **Changes:** API cross-check; fixed the UHT-unsafe fields (including in the world validation library); fixed the HUD marking Defend objectives done too early (it now asks the quest system); UseAbility objectives accept the same spell family (Earth_StoneCannon or an awakened variant counts for Rudeus_StoneCannon), so an Orsted build is not soft-locked out of story quests; added buff regen/restore and race regen hooks; capped stagger resistance at 75%; the roll sim now mirrors `GetEffectiveConfig` on `RollConfigs.json`.
- **Testing:** static include/type check PASS, data validator PASS, roll sim PASS.
- **New score:** 2.3

### PASS 3 (offline animation content)
- **Score:** 2.3
- **Problems:**
  - Mixamo needs an interactive login, so there were no clips at all.
  - The first authored set had:
    - 350° quaternion flips;
    - strike and dodge snaps up to 52.6°/frame;
    - feet skating up to 25 cm in stagger, knockdown and awakening;
    - strafe boots colliding (5.4 cm gap);
    - projectile spawns that were ahead of the casting gesture;
    - no clip for locked-on running (the forward run slid sideways).
- **Changes:**
  - 37 clips authored with the tools in `Tools/anim`: monotone-cubic keys, position-space FK/IK leg blending, re-timed strikes, real recovery steps, new RunStrafeLeft/Right/RunBack loops.
  - Native 4-way directional blending on both gait bands.
  - The Orsted pipeline switches his data to his own clips.
- **Testing:** builder metrics, the independent numpy GLB verifier and per-frame motion QA, all measured (table above and `Docs/QA_Animation.md`). Contact sheets were reviewed visually.
- **New score:** 2.6 (animation 0.5 → 3.5; every other category unchanged, because nothing has run in-engine).

### PASS 4 – PASS 5: blocked

The remaining weaknesses need Unreal Engine to fix and test: compile, in-engine animation and feel, VFX, and visual QA for z-fighting and flicker. Running more "passes" here would mean inventing scores, which the brief forbids.

## What remains below target, and exactly how to close it

1. **Compile** in UE 5.8 on the Mac: `Tools/mac/build_and_setup.sh --build-only`, and fix the errors. This is the stability gate, and nothing else counts until it's done. The riskiest engine APIs are listed in `Docs/Animation_Pipeline.md` §0.
2. Run `Tools/mac/build_and_setup.sh` (materials, Rudeus import with 37 clips and sockets, world). Every `FAIL` / `MANUAL` line is a task.
3. In Play-In-Editor, test these for snapping, foot sliding and blend pops:
   - idle → walk → run → sprint → jump → fall → land → run;
   - lock-on strafing at walk and run, in all 8 directions;
   - every cast while standing and while moving (the upper-body layer);
   - dodges, hits, stagger, knockdown and death.
4. Deformation check in-engine: shoulders, elbows, wrists, spine, neck, knees and ankles on extreme poses. Add a physics asset or AnimDynamics for the skirt and hair chains.
5. Orsted:
   - Get his generated GLB into the repo. The CloudFront host is blocked for this cloud session, so either allow it in the environment's network settings or upload the file.
   - Run `Tools/anim/make_orsted.sh <glb>`, then `Tools/mac/build_and_setup.sh`.
6. Regrade with the same table. Only then move to Phase 2.
