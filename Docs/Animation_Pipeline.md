# Animation pipeline: Mixamo, retargeting, custom animations, AnimBP

## 1. Mixamo clips to download (manual: needs your Adobe login)

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

## 2. Retargeting (automated)

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

## 4. Animation Blueprint (to author in the editor on `UMTAnimInstance`)

The C++ base `UMTAnimInstance` computes every variable thread-safely: `GaitValue` (0 idle … 3 sprint, continuous), `Direction`, `LeanAmount`, `RootYawOffset` / `bTurningLeft` / `bTurningRight`, `AimYaw` / `AimPitch`, `bIsJumping` / `bIsFalling` / `bHardLanding` / `TimeSinceLanded`, `UpperBodyCastWeight`, `bInCombatStance`, `Stance`, `FootIKAlpha` and more.

Recommended graph:

- State machine **Locomotion**: Idle (relaxed ↔ combat via `bInCombatStance`, blended) → Moves (1D blend space on `GaitValue`, or a 2D strafe space on `Direction` when `bIsStrafing`) → JumpStart → Rising → Falling → Land (hard land if `bHardLanding`) → back to Moves or Idle. Blend times are 0.15–0.25 s, and inertialization is on for every transition.
- **Turn in place**: rotate the root bone by `RootYawOffset` and play turn animations when `bTurningLeft` or `bTurningRight` is set.
- **Upper-body layer**: a `Layered blend per bone` from `spine_C0_1_jnt_061` using `UpperBodyCastWeight` (montage slot `UpperBody`). A `DefaultSlot` holds full-body montages (dodges, awakenings).
- **Aim offset** on `AimYaw` / `AimPitch` for cast direction.
- **Lean**: an additive lean pose scaled by `LeanAmount`.
- **Foot IK**: Leg IK or Control Rig foot placement, weighted by `FootIKAlpha`.
- **Secondary motion**: AnimDynamics or RigidBody on the skirt, hair, hood and bag chains.
