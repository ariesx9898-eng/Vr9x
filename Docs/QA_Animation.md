# QA: Rudeus animation set (authored, not Mixamo)

Source: `Tools/anim/build_rudeus_anims.py`. Output: `SourceArt/Characters/Rudeus/Rudeus_Animated.glb`: a skinned mesh with the 136-joint skeleton and **37 clips at 30 fps**. Every number below was measured by the tools in this repo. Nothing was estimated.

## Why these are authored rather than downloaded

Mixamo requires an interactive Adobe login, which this cloud session can't use. The brief says: *"If Mixamo does not have an appropriate animation: make the animation yourself."* So the whole set is authored directly on **Rudeus's own skeleton**, with no retargeting loss. Mixamo clips can still be added later through `mt_setup_rudeus.py`.

## How they are made

- **Semantic pose composer** (`pose_compose.py`).
  - Each pose is described by pelvis offset and rotation, spine bend and twist, arm lower/swing/reach, elbow and wrist, finger curl, and foot placement.
  - The composer converts that into armature-space rotation deltas, so the imported rig's odd bone axes never matter.
  - The clavicle follows the arm a little (it drops as the arm lowers, and lifts once the arm rises past 60°), so the shoulders sit naturally instead of staying shrugged at T-pose height.
- **Analytic two-bone leg IK** with knee pole vectors.
  - Feet stay planted on the ground.
  - Heel strike pivots on the heel, and toe-off on the ball, with the toes staying flat on the floor.
- **FK ↔ IK leg transitions in position space** (jumps, dodges, knockdown, death).
  - The ankle travels in a straight line from where the airborne (FK) pose puts it to its planted (IK) target.
  - The knee stays on the side the FK pose bends it towards.
  - The foot is never allowed through the floor.
  - Both ends of the blend match the pure FK and IK poses exactly (0.000° measured).
  - Keys that are fully planted hold the neighbouring airborne leg pose, so a blend never "straightens" the legs halfway.
- **Monotone cubic key interpolation** (Steffen). Motion flows through intermediate keys instead of stopping at each one. It eases into extremes (contacts, strike extensions, holds) and never overshoots a key.
- **Automatic pelvis lowering** whenever a stride would over-extend a leg, so no leg ever snaps straight.
- **Coat panels** (8 chains) follow the thighs, so legs never poke through the robe. Hair and coat trail at speed.
- **Ground clamp** (iterative, per frame) for clips where the body touches the floor.
- **Continuous quaternion signs** on export: no 360° flips for any interpolator.
- **Rig repair:** 214 eye vertices were 100% weighted to the root control bone and would have hung in the air at standing eye height during crouches, knockdowns and death. They are re-weighted to the head.

## Locked-on movement (new)

When Rudeus is locked on at running speed, the character faces the target while moving in any direction. Previously this played the forward Run clip while the body moved sideways, so the feet slid.

Three new loops fix it:
- **`RunStrafeLeft` / `RunStrafeRight`** (3.3 m/s): the hips and legs turn 60° toward the travel direction, while the chest and head keep facing the target. This is orientation warping, baked into the clip.
- **`RunBack`** (3.0 m/s): a backpedal.

`UMTNativeAnimInstance` blends 4-way by `Direction` on both gait bands:
- walking pace: Walk / StrafeRight / WalkBack / StrafeLeft
- running pace: Run (Sprint) / RunStrafeRight / RunBack / RunStrafeLeft

The walk-pace strafe was also re-spaced. The feet used to come within **5.4 cm** of each other, so the boots collided. The closest they now come is **13.2 cm**, with a 1.0 m/s reference speed.

## Build metrics (per clip, from the exporter)

Columns:
- **Leg reach:** 1.0 means a fully straight leg; 0.985 is the cap for locomotion.
- **IK target error:** how far the ankle misses its target, in cm, on fully planted frames.
- **Planted slip:** how far a planted foot's speed deviates from the ground speed, in cm/s.
- **Seam:** the largest joint-rotation difference between the last and first frame of a loop, in degrees.

| Clip | Duration s | Type | Ref speed cm/s | Leg reach | IK target error cm | Planted slip cm/s | Seam ° |
|---|---|---|---|---|---|---|---|
| A_Rudeus_Idle | 4.000 | loop |  | 0.970 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CombatIdle | 2.000 | loop |  | 0.912 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Walk | 0.933 | loop | 130.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_WalkBack | 0.933 | loop | 100.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StrafeLeft | 0.600 | loop | 100.0 | 0.984 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StrafeRight | 0.600 | loop | 100.0 | 0.984 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Run | 0.633 | loop | 360.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Sprint | 0.567 | loop | 580.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_RunStrafeLeft | 0.633 | loop | 330.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_RunStrafeRight | 0.633 | loop | 330.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_RunBack | 0.667 | loop | 300.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Rise | 0.600 | loop |  | 0.000 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Fall | 0.600 | loop |  | 0.000 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Hold | 1.000 | loop |  | 0.874 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_JumpStart | 0.260 | one-shot |  | 0.966 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Land | 0.400 | one-shot |  | 1.000 | 0.06 | 0.00 | 0.000 |
| A_Rudeus_HardLand | 0.800 | one-shot |  | 1.000 | 0.06 | 0.00 | 0.000 |
| A_Rudeus_DodgeForward | 0.400 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_DodgeBack | 0.400 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_DodgeLeft | 0.400 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_DodgeRight | 0.400 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_HitFront | 0.350 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_HitBack | 0.350 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_HitLeft | 0.350 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_HitRight | 0.350 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Stagger | 0.800 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Knockdown | 1.400 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Death | 1.600 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastBasic | 0.450 | one-shot |  | 0.905 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Charge | 0.350 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Release | 0.600 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Quagmire | 0.900 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Barrage | 1.450 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_DemonEye | 0.800 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Awakening | 1.800 | one-shot |  | 0.951 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastTwoHand | 0.650 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastGround | 0.800 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |

## Motion quality (every frame of the actually skinned mesh, `Tools/anim/qa_animation_quality.py`)

Columns:
- **Peak °/frame:** the largest single-frame rotation of any major body bone, i.e. the speed of the fastest limb at 30 fps. It is a snapping detector.
- **Knee °:** the flexion range; negative would mean hyperextension, and there is none.
- **Stretch:** the largest triangle-edge ratio against the bind pose. It is dominated by sub-centimetre edges at the hip crease and armpit, which is normal for linear-blend skinning.
- **Hand–spine:** the closest a wrist comes to the torso axis.
- **Ankle gap:** the closest the two ankles come.
- **Foot slide:** for non-locomotion clips, the largest travel of a foot while it stays flat on the ground.

Full JSON: `Docs/QA_Animation_Metrics.json`.

| Clip | Peak °/frame | Bone | Knee ° | Stretch | Hand–spine cm | Ankle gap cm | Foot slide cm |
|---|---|---|---|---|---|---|---|
| Idle | 0.2 | head_C0_0 | 28.2–35.1 | 3.78 | 24.9 | 19.8 | 0.0 |
| CombatIdle | 0.5 | arm_R0_2 | 48.4–54.2 | 4.11 | 29.2 | 38.9 | 0.0 |
| Walk | 12.5 | leg_L0_1 | 19.9–61.4 | 4.24 | 24.5 | 19.5 | — |
| WalkBack | 11.8 | leg_R0_1 | 19.9–60.7 | 4.01 | 25.9 | 20.4 | — |
| StrafeLeft | 6.9 | leg_R0_1 | 20.4–64.2 | 4.04 | 27.0 | 13.2 | — |
| StrafeRight | 6.9 | leg_L0_1 | 20.4–64.2 | 4.04 | 27.0 | 13.2 | — |
| Run | 19.6 | leg_R0_1 | 19.9–109.9 | 5.68 | 23.1 | 25.5 | — |
| Sprint | 30.7 | leg_R0_2 | 19.9–123.0 | 6.66 | 22.7 | 29.7 | — |
| RunStrafeLeft | 18.1 | leg_R0_1 | 19.9–100.9 | 5.37 | 24.1 | 20.8 | — |
| RunStrafeRight | 17.8 | leg_R0_1 | 19.9–100.9 | 5.21 | 24.1 | 20.8 | — |
| RunBack | 20.3 | leg_R0_1 | 19.9–89.4 | 4.99 | 26.7 | 21.4 | — |
| Rise | 1.0 | leg_R0_0 | 37.0–65.0 | 3.34 | 42.8 | 18.2 | 0.0 |
| Fall | 1.4 | arm_R0_0 | 19.0–29.0 | 2.31 | 49.5 | 18.2 | 0.0 |
| StoneCannon_Hold | 1.3 | arm_R0_2 | 58.1–61.7 | 4.45 | 30.7 | 38.9 | 0.0 |
| JumpStart | 27.4 | arm_L0_2 | 29.7–69.5 | 4.06 | 25.5 | 19.8 | 0.8 |
| Land | 21.0 | leg_L0_0 | 3.6–74.5 | 3.78 | 25.8 | 19.8 | 0.0 |
| HardLand | 27.7 | arm_R0_2 | 3.6–109.8 | 4.99 | 25.8 | 19.8 | 0.0 |
| DodgeForward | 31.3 | arm_L0_0 | 31.7–73.0 | 3.95 | 25.2 | 19.8 | 3.2 |
| DodgeBack | 30.1 | arm_L0_1 | 26.3–53.9 | 4.17 | 25.8 | 17.1 | 0.9 |
| DodgeLeft | 19.5 | arm_R0_1 | 26.9–44.6 | 4.06 | 23.9 | 19.8 | 3.5 |
| DodgeRight | 19.5 | arm_L0_0 | 26.7–45.3 | 4.06 | 23.9 | 19.8 | 3.1 |
| HitFront | 27.3 | arm_L0_2 | 31.7–42.6 | 3.99 | 25.8 | 19.8 | 0.0 |
| HitBack | 18.1 | arm_L0_0 | 31.7–42.6 | 3.65 | 25.4 | 19.8 | 0.0 |
| HitLeft | 20.2 | arm_L0_2 | 31.7–42.6 | 3.80 | 19.5 | 19.8 | 0.0 |
| HitRight | 20.2 | arm_R0_0 | 31.7–42.6 | 3.80 | 19.4 | 19.8 | 0.0 |
| Stagger | 23.3 | arm_R0_1 | 31.7–72.3 | 3.80 | 25.8 | 19.8 | 0.9 |
| Knockdown | 21.1 | arm_R0_0 | 21.1–116.6 | 6.12 | 25.8 | 16.0 | 4.2 |
| Death | 11.8 | leg_L0_0 | 8.0–115.7 | 5.10 | 20.5 | 16.5 | 1.3 |
| CastBasic | 28.0 | arm_R0_0 | 50.2–52.1 | 4.69 | 29.5 | 38.9 | 0.0 |
| StoneCannon_Charge | 7.3 | arm_L0_0 | 50.7–60.4 | 4.39 | 29.5 | 38.9 | 0.0 |
| StoneCannon_Release | 27.5 | arm_R0_2 | 50.7–60.4 | 4.70 | 29.5 | 38.9 | 0.0 |
| Quagmire | 10.9 | arm_L0_2 | 50.7–104.5 | 5.95 | 28.7 | 38.9 | 0.0 |
| Barrage | 28.0 | arm_R0_2 | 50.7–60.8 | 4.70 | 29.5 | 38.9 | 0.0 |
| DemonEye | 18.2 | arm_L0_1 | 50.7–52.0 | 4.19 | 17.7 | 38.9 | 0.0 |
| Awakening | 19.8 | arm_L0_1 | 36.2–52.0 | 4.35 | 29.5 | 38.9 | 0.0 |
| CastTwoHand | 26.2 | arm_R0_2 | 50.7–60.4 | 4.70 | 29.5 | 38.9 | 0.0 |
| CastGround | 12.1 | arm_R0_2 | 50.7–94.5 | 5.60 | 26.9 | 38.9 | 0.0 |

### What this pass fixed (measured before → after)

| Problem | Before | After | Fix |
|---|---|---|---|
| Quaternion sign flips (Run, Sprint, Death, Demon Eye) | 345–358° "pops" | none | continuous quaternion signs on export; the QA folds q/−q |
| Dodge start snap (trailing foot) | 52.6°/frame | 31.3°/frame on the fastest bone (the arm) | legs leave the ground over 3 frames; ankle angles soften the trailing foot |
| Hard landing snap | 45.5°/frame | 27.7°/frame | the impact is absorbed over 5 frames with an intermediate key |
| Casting strike snaps (two-hand, Barrage, basic cast) | 42.5 / 40.6 / 38.3°/frame | 26.2 / 28.0 / 28.0°/frame | softer wrist (−55°, pre-extended during the gather) and 3-frame strikes |
| Casting feet sliding between stance and charge pose | 2–4.5 cm | 0 | the charge pose keeps the combat stance's feet |
| Stagger: foot skated home | 22.5 cm | 0.9 cm | a real recovery step (lift, move, plant) |
| Knockdown: feet skated into the stance while getting up | 18.7 cm | 4.2 cm (while being knocked back) | legs fold in the air; feet plant where they finish |
| Awakening: feet skated into a neutral stance | 25.1 cm | 0 | the power-up keeps the combat stance's footing |
| Strafe: boots collide | 5.4 cm ankle gap | 13.2 cm | wider foot centres, shorter lateral stride |
| Sprint hip-crease stretching | 6.81 | 6.66 | slightly lower knee drive |
| First-frame jumps from missing key values | up to 3° | 0 | missing values fall back to the composer defaults, not 0 |

### Timing contract with the abilities

Projectiles spawn at the ability's `CastTime`, so each strike now reaches full extension at that moment:

| Ability | Spawn time | Strike extension |
|---|---|---|
| Basic cast | 0.12 s | 0.14 s |
| Two-hand casts | 0.25–0.30 s | 0.27 s |
| Elemental Barrage steps | 0.31 / 0.54 / 0.78 / 1.04 s | the same four times (the clip is now 1.45 s) |
| Stone Cannon | on release | in the first 2–3 frames of the release clip |

## Independent verification

`Tools/anim/verify_glb.py` re-parses the exported file with numpy, with no Blender involved, the way an engine importer would. On this build it reported **VERIFY: PASS**:
- 136 joints with the same names as the source rig;
- bind height 1.617 m with the feet at 0, facing +Z;
- 37 animations with exact durations;
- the lowest vertex of every floor-contact clip within 0.6 cm of the ground.

Rise is an air loop and reaches −2.7 cm in place; it only ever plays while airborne. The contact sheet is in `Docs/Images/Animation/Verify_ExportedGLB.png`, and per-clip sheets are in `Docs/Images/Animation/`.

## Honest limits

- **Procedurally keyframed.** The poses and timing are deliberate, but there is no motion-capture nuance and no facial animation (the model has no blendshapes).
- **Coat is procedural.** It is driven procedurally, not cloth-simulated. In UE, enable a Physics Asset or AnimDynamics on the `skirt_*`, `hair_*` and `hood_*` chains for extra life.
- **Dodge landings slide slightly.** The feet land 3–3.5 cm from their planted spot relative to the root, because the dash's root motion is still carrying the body. In-engine foot IK (not implemented yet) would pin them.
- **No turn-in-place.** `TurnLeft90` / `TurnRight90` aren't authored yet; the native anim instance falls back gracefully.
- **Not yet seen in Unreal.** Nothing has run in the engine; this needs the in-engine pass on your Mac.
