# Animation pipeline: authored clips, native playback, optional Mixamo and AnimBP

## 0. Runtime path: native playback, no Animation Blueprint (current)

> **Status: not compiled or run yet.** This was written in a container without Unreal Engine. Details are in `Tools/mac/README.md`.

The game animates with **no editor-authored animation assets** (no Animation Blueprint, no montage assets):

- **Clips.** `SourceArt/Characters/Rudeus/Rudeus_Animated.glb` contains the skinned mesh, the skeleton and 37 clips named `A_Rudeus_<Key>`. `Content/Python/mt_setup_rudeus.py` imports it and moves every clip to `/Game/Characters/Rudeus/Animations/A_Rudeus_<Key>`, logging PASS/FAIL per key. `Tools/mac/build_and_setup.sh` runs that script headless.
- **Data.** `Content/Data/AnimSets.json` has one row per character (`FMTAnimSetData`):
  - `Anims`: a `<Key>` → clip map.
  - Authored reference speeds (cm/s): `WalkSpeedRef` 130, `WalkBackSpeedRef` 100, `StrafeSpeedRef` 100, `RunSpeedRef` 360, `SprintSpeedRef` 580, `RunStrafeSpeedRef` 330, `RunBackSpeedRef` 300.
  - `UpperBodyRootBone`: default `spine_C0_1_jnt_061`.

  **Orsted** shares Rudeus's skeleton hierarchy.
  - **Until his model exists**, his row points at the Rudeus clips. His mesh must then use Rudeus's `USkeleton`, or be marked compatible with it, for them to play.
  - **Once his generated mesh exists**, `Tools/anim/make_orsted.sh <glb>` does the rest in one command:
    1. re-rigs the mesh onto the skeleton at 195 cm;
    2. authors his 40 clips on his own proportions, including his signature DisturbMagic, DragonStep and Aura;
    3. verifies the export;
    4. runs `Tools/anim/use_character_clips.py Orsted`, which points his AnimSet row, reference speeds and signature abilities at his own `A_Orsted_*` clips.

    `Tools/mac/build_and_setup.sh` then imports `Orsted_Animated.glb` through `Content/Python/mt_setup_orsted.py`. From then on he needs nothing of Rudeus's.
- **Anim instance.** `Characters.json` sets `AnimClass` = `/Script/MushokuRPG.MTNativeAnimInstance`. `AMTCharacterBase::ApplyCharacterLineage` also falls back to that class when `AnimClass` is unset or fails to load. `UMTNativeAnimInstance` (`Source/.../Animation/MTNativeAnimInstance.h`) replaces the anim graph with a custom `FAnimInstanceProxy`, the same pattern Epic uses for `UAnimSingleNodeInstance`:
  - **Ground:**
    - A single normalised phase is shared by every moving clip, so the feet never desync.
    - Play rate is ground speed divided by the blended reference speed, clamped to 0.6–1.5.
    - Weights follow `GaitValue`.
    - When locked on, a 4-way blend follows `Direction` on both gait bands. Walking pace uses Walk, StrafeRight, WalkBack and StrafeLeft. Running pace uses Run (or Sprint), RunStrafeRight, RunBack and RunStrafeLeft; in the strafe runs the hips turn toward the travel direction while the chest keeps facing the target.
    - Idle and CombatIdle crossfade over 0.3 s.
  - **Air:** JumpStart → Rise/Fall loops → Land or HardLand. Every state change crossfades over 0.12–0.25 s. The Death clip is held after the death montage ends.
  - **Montages:** everything code-driven plays in `DefaultSlot`, which the proxy evaluates like a slot node. That covers ability casts, dodges, hit reactions, stagger, knockdown and death. While casting and moving (or airborne), only `UpperBodyRootBone` and its children take the montage; the pelvis and legs keep locomotion.
  - **Missing clips** fall back to another clip and are logged once, for example Walk → Run, WalkBack → Walk reversed, Rise ↔ Fall. With no clips at all, the mesh shows the reference pose.
- **Playing clips from code.** `AMTCharacterBase::PlayAnimAsset` plays an authored `UAnimMontage` directly and wraps any plain sequence in a dynamic montage (`PlaySlotAnimationAsDynamicMontage`).
  - Data names clips by key (`A_Rudeus_<Key>`), and `ResolveLineageAnim` swaps in the caster's own `A_<C>_<Key>` when its AnimSet has that key. So an element spell cast by Orsted plays his version of the cast.
  - Two things feed it:
  - **Ability rows.** Each row has `Montage` / `ChargeLoopAnim` / `ReleaseAnim`. A chargeable ability plays its anticipation, then the hold loop, then the release clip.
  - **Character reactions.** The dodge, hit, stagger, knockdown and death properties are filled from the AnimSet in `ApplyCharacterLineage`. Values a designer set are never overwritten.

**Naming:** `A_<Character>_<Key>`. The keys are listed on `FMTAnimSetData` in `Core/MTDataTypes.h`. `TurnLeft90` and `TurnRight90` are optional and not authored yet; nothing uses them. Locomotion loops are in place and start with a left-foot contact at phase 0. One-shots should also be in place: code moves the capsule for dodges and dashes. `Tools/validate_data.py` checks the following offline, without Unreal:
- AnimSet keys
- the naming convention
- the reference speeds against the exporter sidecar `Rudeus_Animated.anim.json`
- that every ability animation path is a known clip

**Known limits of the native path:**
- Notifies authored on locomotion clips don't fire. Montage notifies do.
- There is no turn-in-place, lean, aim offset or foot IK. The variables for them are still computed.
- The pose trails gameplay state by one frame, because variables are copied in `PreUpdate`.

**Swapping in an Animation Blueprint later (no code change):**
1. Create an Animation Blueprint whose parent class is `UMTAnimInstance`. **Don't use `UMTNativeAnimInstance` as the parent**: it isn't Blueprintable, because its proxy bypasses the graph. The graph gets every variable `UMTAnimInstance` computes (`GaitValue`, `Direction`, `bIsStrafing`, `UpperBodyCastWeight`, `bIsJumping` / `bIsFalling` / `bHardLanding`, and so on). Section 4 below has a recommended graph.
2. The graph must contain a **`DefaultSlot`** slot node, because every code-driven animation plays there. Use a layered blend per bone from `spine_C0_1_jnt_061`, weighted by `UpperBodyCastWeight`, to reproduce the upper-body casting layer.
3. Set that character's `AnimClass` in `Content/Data/Characters.json` to `/Game/Characters/<C>/ABP_<C>.ABP_<C>_C`. If it ever fails to load, the game falls back to the native instance.


## 1. Optional: Mixamo clips to download (manual: needs your Adobe login)

> Not needed for the current clip set, which is authored on Rudeus's own skeleton (`Docs/QA_Animation.md`). Use this only to add extra Mixamo clips.

Settings for every clip: **FBX Binary, Without Skin, 30 fps, keyframe reduction none.** Turn "In Place" **off** for clips marked RM (root motion) and **on** for loops. Also download **Y Bot with skin in T-pose** once and import it as `/Game/Animation/Mixamo/SK_Mixamo_YBot`. Import every clip onto that skeleton into `/Game/Animation/Mixamo/Source`.

Pick clips by fit, not by the first search result. The target personality notes are the selection criteria.

| Use | Search term (Mixamo) | Character fit notes |
|---|---|---|
| Relaxed idle (Rudeus) | "Breathing Idle" | Light, slightly forward-leaning. Reject clips with arms crossed (they fight the robe sleeves). |
| Relaxed idle (Orsted) | "Standing Idle" / "Idle" (stoic variant) | Minimal sway. Orsted must look still. |
| Combat idle (Rudeus) | "Standing Magic Idle" / "Magic Idle" | Casting hand forward, weight on the back foot |
| Combat idle (Orsted) | "Fighting Idle" (unarmed, low motion) | Upright. Reject bouncy boxer idles. |
| Walk / back / strafe L/R | "Walking", "Walking Backwards", "Left Strafe Walking", "Right Strafe Walking" | Matching stride length avoids blend-space foot sliding |
| Run / sprint | "Running", "Fast Run" | Rudeus: light run. Orsted: "Standard Run" with less arm swing. |
| Jump start / rise / fall / land / hard land | "Jump" (split), "Falling Idle", "Falling To Landing", "Hard Landing" | Split "Jump" into start/loop/land in UE |
| Turn in place L/R | "Left Turn 90", "Right Turn 90" (RM) | Drives RootYawOffset unwinding |
| Casting | "Standing 1H Magic Attack 01", "Standing 2H Magic Attack 01/03", "Standing 2H Cast Spell 01" | Hand thrust for Stone Cannon release |
| Charge | "Magic Spell Charge" / "Standing 2H Magic Area Attack 01" (first half) | Loopable hold section |
| Defensive stance / block | "Standing Block Idle" | Orsted counters |
| Dodges L/R/back (RM) | "Standing Dodge Left/Right/Backward" | Directional dodge montages |
| Hit reactions | "Hit Reaction", "Head Hit", "Standing React Small From Left/Right/Front/Back" | Flinch set (additive-friendly) |
| Stagger / knockback recovery | "Stunned", "Knocked Down", "Getting Up" | Stagger and knockdown montages |
| Interactions | "Talking", "Picking Up", "Sitting Idle", "Stand To Sit", "Sit To Stand", "Reading", "Pointing" | NPC and player interactions |

## 2. Optional: retargeting (automated when a Mixamo Y Bot is present)

> `mt_setup_rudeus.py` skips this step unless `/Game/Animation/Mixamo/SK_Mixamo_YBot` exists. Every IK Rig / Retargeter call is guarded because the Python API changed in UE 5.6+. Anything it cannot do is logged as a `MANUAL` line.

`Content/Python/mt_setup_rudeus.py` builds:

- **IK_Rudeus**: retarget root = pelvis `spine_C0_0_jnt_05`. It has 22 chains: root, spine, neck, head, both clavicles, both arms (hand goals), both legs (foot goals), both toes, and 10 finger chains. Full-body IK solver goals are on the hands and feet.
- **IK_Mixamo**: the same chain names on the Mixamo skeleton, so exact auto-mapping is deterministic.
- **RTG_Mixamo_To_Rudeus**: both skeletons are T-pose, so the retarget pose needs only small corrections.
- A **batch retarget** of everything in `/Game/Animation/Mixamo/Source` to `/Game/Characters/Rudeus/Animations/*_Rudeus`.

Mandatory manual checks in the Retargeter editor. These are *not* done yet:

1. Shoulders. Rudeus's clavicle joints sit only 2.7 cm off-centre (Mixamo's sit wider). Check upper-arm roll so hands don't pass through the robe.
2. Knees. Both rigs have straight knees in the bind pose. Add a small pre-bend in the retarget pose if IK flips.
3. Pelvis height. Rudeus's pelvis is at 85.4 cm. Set the pelvis translation mode to "Globally Scaled" so feet stay planted.
4. Fingers. Rudeus's thumb joint axes differ from Mixamo's. Inspect the thumb chain in a grip pose.
5. Foot IK. Enable IK goals on the legs to remove sliding, then verify on walk, run and strafe.

## 3. Custom animations (Mixamo has no match)

> **Superseded:** the full set is now authored in Blender on Rudeus's skeleton (`Tools/anim/`, measured in `Docs/QA_Animation.md`). The table below is kept as the original design brief.

These must be authored with **Control Rig + Sequencer** on the Rudeus/Orsted rigs. Mixamo won't be substituted. Status for each: **NOT STARTED**, because it needs the editor.

| Animation | Beats: anticipation → action → impact → recovery |
|---|---|
| Rudeus mana gathering | Palms turn in, fingers spread → mana pulls between hands → brief hold → relax |
| Stone Cannon | Raise casting hand, plant feet (stable stance) → charge loop (sections `Charge` / `Release`) → sharp palm thrust → recoil and settle |
| Dual casting | Split-hand stance → alternating hand thrusts synced to the Barrage sequence steps |
| Quagmire | Crouch slightly and touch the palm down → ground pulse → rise |
| Large spell charge (Inferno Compression) | Two-hand compression gesture, visible 0.9 s startup |
| Demon Eye activation | Head tilt, hand near the left eye (canon: left eye), brief focus |
| Awakening: Quagmire Magician | Stop → mana gathers → cloth and hair wind response (AnimDynamics on the hair and skirt chains) → eye → aura → release |
| Orsted Disturb Magic | Minimal open-palm raise → finger flick timed to the window → hold |
| Orsted counters / Dragon Step | Launch frame → 0.18 s travel (motion-warped to `DragonStepTarget`) → plant and recover |
| Dragon Aura / Dragon God | Complete stillness → pressure (dust reacts) → aura reveal, camera pulls out (already in code) |

## 4. Optional: Animation Blueprint (to author in the editor on `UMTAnimInstance`)

The C++ base `UMTAnimInstance` computes every variable thread-safely: `GaitValue` (0 idle … 3 sprint, continuous), `Direction`, `LeanAmount`, `RootYawOffset` / `bTurningLeft` / `bTurningRight`, `AimYaw` / `AimPitch`, `bIsJumping` / `bIsFalling` / `bHardLanding` / `TimeSinceLanded`, `UpperBodyCastWeight`, `bInCombatStance`, `Stance`, `FootIKAlpha` and more.

Recommended graph:

- State machine **Locomotion**: Idle (relaxed ↔ combat via `bInCombatStance`, blended) → Moves (1D blend space on `GaitValue`, or a 2D strafe space on `Direction` when `bIsStrafing`) → JumpStart → Rising → Falling → Land (hard land if `bHardLanding`) → back to Moves or Idle. Blend times are 0.15–0.25 s, and inertialization is on for every transition.
- **Turn in place**: rotate the root bone by `RootYawOffset` and play turn animations when `bTurningLeft` or `bTurningRight` is set.
- **Upper-body layer**: a `Layered blend per bone` from `spine_C0_1_jnt_061` using `UpperBodyCastWeight`, with the `DefaultSlot` output as the blend pose. All code-driven animations play in `DefaultSlot`: casts, dodges, hit reactions and awakenings (`AMTCharacterBase::PlayAnimAsset`).
- **Aim offset** on `AimYaw` / `AimPitch` for cast direction.
- **Lean**: an additive lean pose scaled by `LeanAmount`.
- **Foot IK**: Leg IK or Control Rig foot placement, weighted by `FootIKAlpha`.
- **Secondary motion**: AnimDynamics or RigidBody on the skirt, hair, hood and bag chains.
