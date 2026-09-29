# QA: Rudeus animation set (authored, not Mixamo)

Source: `Tools/anim/build_rudeus_anims.py` (clip definitions in `Tools/anim/clips.py` and `Tools/anim/clips_spells.py`). Output: `SourceArt/Characters/Rudeus/Rudeus_Animated.glb`: a skinned mesh with the 136-joint skeleton and **55 clips at 30 fps**, with the gameplay events of the casting clips in the sidecar `Rudeus_Animated.anim.json`. Every number below was measured by the tools in this repo. Nothing was estimated.

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
- **Ground clamp** (iterative, per frame) on every one-shot and stance loop: nothing but the planted feet ever goes below the floor. It only raises the body and is a no-op otherwise. Gait loops are exact IK by construction; Rise and Fall only play in the air.
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

## Casting clips of the ability overhaul (2026-09-29)

The 12 element spells used to borrow three generic clips (`CastBasic`, `CastTwoHand`, `CastGround`). Each now has its
own clip set with its own silhouette (`Docs/Animation_Pipeline.md` §0.1 has the table of keys, events, hands and
silhouettes), and Rudeus's Stone Cannon, Quagmire and Barrage were re-authored. Every clip runs anticipation → release
→ follow-through / recoil → settle, starts and ends on the combat stance, keeps both feet planted by IK unless a step is
lifted, and carries its `Release` (and Barrage's `Finale`) event at the §7 time, which equals the ability's `CastTime`.

How they read (contact sheets `Docs/Images/Animation/A_Rudeus_<Key>.png`, which now show the event frame):
- **Hands shape the magic.** New composer controls give forearm roll (palm up / down), finger spread and per-finger curl:
  the Fireball palm turns up and the fingers roll the sphere while the left hand shapes it from above; Stone Cannon's
  fingers claw and compress with a trembling wrist, the left hand bracing the forearm; Water Bullet is a two-finger
  point; Wind Blade a knife hand; Tornado an index stirring downward; Quagmire a flat, spread palm pressing the ground.
- **Big releases recoil.** Fireball, both Stone Cannons and the Barrage finale kick the arm up and rock the weight back
  after the release.
- **Full-body clips shift weight** (pelvis translation with IK-planted feet): Inferno rises onto the back foot with the
  raised fist, then drops into a 27 cm crouch on the front foot; Water Dragon rises from a crouch with the hands, onto
  the balls of the feet, then leans into the point; Flood rocks back with the front foot lifted, stomps and lunges.
- **Upper body over locomotion.** Gestures live in the arms, hands and spine (relative to the pelvis), so they still
  read when the native anim instance layers them over running; the pelvis and legs only matter for the full-body clips,
  which commit the caster.

Measured on every frame (build QA + `qa_animation_quality.py`); the clips marked full body commit the whole body:

| Clip | Length s | Events | Hand | Loop | Leg reach | IK err cm | Seam ° | Peak °/frame | Knee ° | Stretch | Hand–spine cm | Foot slide cm | Lowest vertex cm |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Cast_Fireball | 0.333 | Release 0.30 | R (L shapes) |  | 0.904 | 0.00 | 0.000 | 22.5 (arm\_L0\_1) | 50.7–57.2 | 4.25 | 26.7 | 0.0 | -0.0 |
| Cast_Fireball_Hold | 1.000 | – | R (L shapes) | loop | 0.888 | 0.00 | 0.000 | 1.7 (arm\_R0\_2) | 54.7–58.5 | 4.30 | 27.4 | 0.0 | -0.0 |
| Cast_Fireball_Release | 0.567 | Release 0.04 | R |  | 0.904 | 0.00 | 0.000 | 26.3 (arm\_R0\_2) | 50.7–57.7 | 5.00 | 20.4 | 0.0 | -0.0 |
| Cast_FlameWave | 1.000 | Release 0.42 | R |  | 0.904 | 0.00 | 0.000 | 25.6 (arm\_R0\_2) | 50.7–65.2 | 5.37 | 24.2 | 0.0 | -0.0 |
| Cast_Inferno (full body) | 1.600 | Release 0.90 | R |  | 0.973 | 0.00 | 0.000 | 29.7 (arm\_R0\_2) | 26.5–100.6 | 5.46 | 29.5 | 0.0 | -0.0 |
| Cast_WaterBullet | 0.600 | Release 0.22 | R |  | 0.905 | 0.00 | 0.000 | 27.3 (arm\_R0\_2) | 50.4–52.2 | 4.08 | 29.2 | 0.0 | -0.0 |
| Cast_WaterDragon (full body) | 1.500 | Release 0.75 | both, R points |  | 0.962 | 0.00 | 0.000 | 29.6 (arm\_L0\_2) | 31.6–68.9 | 4.91 | 26.9 | 1.4 | -0.0 |
| Cast_Flood (full body) | 1.500 | Release 0.70 | both |  | 0.904 | 0.00 | 0.000 | 24.7 (arm\_L0\_0) | 50.7–95.0 | 5.10 | 25.6 | 0.0 | -0.0 |
| Cast_StoneCannon | 0.367 | Release 0.35 | R |  | 0.904 | 0.00 | 0.000 | 23.1 (arm\_R0\_2) | 50.7–57.2 | 4.25 | 29.3 | 0.0 | -0.0 |
| Cast_StoneCannon_Hold | 1.000 | – | R | loop | 0.887 | 0.00 | 0.000 | 1.7 (arm\_R0\_2) | 55.0–58.1 | 4.29 | 29.2 | 0.0 | -0.0 |
| Cast_StoneCannon_Release | 0.500 | Release 0.04 | R |  | 0.904 | 0.00 | 0.000 | 16.5 (arm\_R0\_2) | 50.7–57.2 | 4.86 | 29.0 | 0.0 | -0.0 |
| Cast_EarthWall | 0.900 | Release 0.40 | R |  | 0.937 | 0.00 | 0.000 | 30.3 (arm\_R0\_2) | 40.9–83.8 | 5.20 | 25.9 | 0.0 | -0.0 |
| Cast_EarthSpikes | 0.900 | Release 0.40 | R |  | 0.923 | 0.00 | 0.000 | 27.3 (arm\_R0\_0) | 45.1–87.2 | 5.25 | 29.5 | 0.0 | -0.0 |
| Cast_WindBlade | 0.600 | Release 0.25 | R |  | 0.905 | 0.00 | 0.000 | 29.7 (arm\_R0\_1) | 50.2–52.3 | 4.86 | 28.0 | 0.0 | -0.0 |
| Cast_Tornado | 1.100 | Release 0.55 | R |  | 0.937 | 0.00 | 0.000 | 20.7 (arm\_R0\_2) | 40.9–57.0 | 4.37 | 29.3 | 0.0 | -0.0 |
| Cast_WindBurst | 0.600 | Release 0.12 | both |  | 0.923 | 0.00 | 0.000 | 28.1 (arm\_R0\_1) | 45.2–63.2 | 4.44 | 22.7 | 0.0 | -0.0 |
| StoneCannon_Charge | 0.367 | Release 0.35 | R (L braces) |  | 0.904 | 0.00 | 0.000 | 19.5 (arm\_L0\_1) | 50.7–63.4 | 5.24 | 19.9 | 0.0 | -0.0 |
| StoneCannon_Hold | 1.000 | – | R (L braces) | loop | 0.860 | 0.00 | 0.000 | 2.8 (arm\_R0\_2) | 61.4–64.3 | 5.24 | 25.3 | 0.0 | -0.0 |
| StoneCannon_Release | 0.700 | Release 0.04 | R |  | 0.904 | 0.00 | 0.000 | 25.3 (arm\_R0\_0) | 50.7–63.6 | 5.24 | 24.1 | 0.0 | -0.0 |
| Quagmire | 0.467 | Release 0.45 | L |  | 0.904 | 0.00 | 0.000 | 16.1 (arm\_R0\_0) | 50.7–96.6 | 5.64 | 29.5 | 0.0 | -0.0 |
| Quagmire_Hold | 1.000 | – | L | loop | 0.680 | 0.00 | 0.000 | 1.7 (arm\_L0\_1) | 94.3–97.8 | 5.68 | 34.9 | 0.0 | -0.0 |
| Quagmire_Release | 0.600 | Release 0.04 | L |  | 0.904 | 0.00 | 0.000 | 13.7 (arm\_R0\_0) | 50.7–100.5 | 5.79 | 29.5 | 0.0 | -0.0 |
| Barrage | 4.600 | Release 0.60, Finale 3.95 | both, R points |  | 0.910 | 0.00 | 0.000 | 29.7 (arm\_L0\_2) | 49.0–71.4 | 5.05 | 28.3 | 0.0 | -0.0 |

Worst values over these 23 clips: IK target error **0.00 cm**, planted slip **0.00 cm/s**, loop seams **0.000°**, fastest
bone **30.3°/frame** (Earth Wall's hand; the set's accepted maximum is 31.3), leg reach ≤ 0.973, planted-foot slide
**0.0 cm** except Water Dragon's 1.4 cm (the heels rise 8° onto the balls of the feet at the top of the spiral; the ball
stays pinned and the metric counts the ankle's pivot), lowest vertex **−0.0 cm** (never through the floor), no hand
through the torso (closest 19.9 cm), no knee hyperextension (min 26.5°), stretch ≤ 5.79. Forearm roll is split between
the forearm and the wrist, so the hand twists at most 52.8° relative to the forearm (Tornado).

**Loops:** `Cast_Fireball_Hold`, `Cast_StoneCannon_Hold`, `StoneCannon_Hold`, `Quagmire_Hold` (1.0 s, seam 0.000°). Each
starts exactly on its anticipation's last frame (every oscillation is zero at t = 0), stays within a few degrees of it
(peak 2.8°/frame), and the release clip starts on the same pose.

**How the snaps were removed.** The first pass had peaks up to 105°/frame: the IK design aid had picked anatomically
impossible wrists (roll 177°, wrist 120°, a hyperextended elbow) and flipped branch between keys. Anatomical limits,
position-first solving, a continuity cost, shorter reaches for the fast gestures, straight parameter-space transitions
where an arm returns home, and retimed keys (Water Bullet's snap is done by the fingers, Tornado's circle is smaller,
the Barrage directing gestures only move the arm) brought every clip under 31°/frame.

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
| A_Rudeus_Cast_Fireball_Hold | 1.000 | loop |  | 0.888 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_StoneCannon_Hold | 1.000 | loop |  | 0.887 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Hold | 1.000 | loop |  | 0.860 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Quagmire_Hold | 1.000 | loop |  | 0.680 | 0.00 | 0.00 | 0.000 |
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
| A_Rudeus_DemonEye | 0.800 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Awakening | 1.800 | one-shot |  | 0.951 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastTwoHand | 0.650 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastGround | 0.800 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_Fireball | 0.333 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_Fireball_Release | 0.567 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_FlameWave | 1.000 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_Inferno | 1.600 | one-shot |  | 0.973 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_WaterBullet | 0.600 | one-shot |  | 0.905 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_WaterDragon | 1.500 | one-shot |  | 0.962 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_Flood | 1.500 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_StoneCannon | 0.367 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_StoneCannon_Release | 0.500 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_EarthWall | 0.900 | one-shot |  | 0.937 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_EarthSpikes | 0.900 | one-shot |  | 0.923 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_WindBlade | 0.600 | one-shot |  | 0.905 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_Tornado | 1.100 | one-shot |  | 0.937 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Cast_WindBurst | 0.600 | one-shot |  | 0.923 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Charge | 0.367 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Release | 0.700 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Quagmire | 0.467 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Quagmire_Release | 0.600 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Barrage | 4.600 | one-shot |  | 0.910 | 0.00 | 0.00 | 0.000 |

Land and HardLand reach 1.0 by design (the legs straighten on touchdown; 0.06 cm is the IK clamp at full extension).

## Motion quality (every frame of the actually skinned mesh, `Tools/anim/qa_animation_quality.py`)

Columns:
- **Peak °/frame:** the largest single-frame rotation of any major body bone, i.e. the speed of the fastest limb at 30 fps. It is a snapping detector.
- **Knee °:** the flexion range; negative would mean hyperextension, and there is none.
- **Stretch:** the largest triangle-edge ratio against the bind pose. It is dominated by sub-centimetre edges at the hip crease and armpit, which is normal for linear-blend skinning.
- **Hand–spine:** the closest a wrist comes to the torso axis.
- **Ankle gap:** the closest the two ankles come.
- **Foot slide:** for non-locomotion clips, the largest travel of a foot while it stays flat on the ground.
- **Lowest vertex:** the lowest skinned vertex over every frame (new in this pass; negative = through the floor).

Full JSON: `Docs/QA_Animation_Metrics.json`.

| Clip | Peak °/frame | Bone | Knee ° | Stretch | Hand–spine cm | Ankle gap cm | Foot slide cm | Lowest vertex cm |
|---|---|---|---|---|---|---|---|---|
| Idle | 0.2 | head_C0_0 | 28.2–35.1 | 3.78 | 24.9 | 19.8 | 0.0 | -0.0 |
| CombatIdle | 0.5 | arm_R0_2 | 48.4–54.2 | 4.11 | 29.2 | 38.9 | 0.0 | -0.0 |
| Walk | 12.5 | leg_L0_1 | 19.9–61.4 | 4.24 | 24.5 | 19.5 | — | -0.0 |
| WalkBack | 11.8 | leg_R0_1 | 19.9–60.7 | 4.01 | 25.9 | 20.4 | — | -0.0 |
| StrafeLeft | 6.9 | leg_R0_1 | 20.4–64.2 | 4.04 | 27.0 | 13.2 | — | -0.0 |
| StrafeRight | 6.9 | leg_L0_1 | 20.4–64.2 | 4.04 | 27.0 | 13.2 | — | -0.0 |
| Run | 19.6 | leg_R0_1 | 19.9–109.9 | 5.68 | 23.1 | 25.5 | — | -0.0 |
| Sprint | 30.7 | leg_R0_2 | 19.9–123.0 | 6.66 | 22.7 | 29.7 | — | -0.0 |
| RunStrafeLeft | 18.1 | leg_R0_1 | 19.9–100.9 | 5.37 | 24.1 | 20.8 | — | -0.0 |
| RunStrafeRight | 17.8 | leg_R0_1 | 19.9–100.9 | 5.21 | 24.1 | 20.8 | — | -0.0 |
| RunBack | 20.3 | leg_R0_1 | 19.9–89.4 | 4.99 | 26.7 | 21.4 | — | -0.0 |
| Rise | 1.0 | leg_R0_0 | 37.0–65.0 | 3.34 | 42.8 | 18.2 | 0.0 | -2.7 |
| Fall | 1.4 | arm_R0_0 | 19.0–29.0 | 2.31 | 49.5 | 18.2 | 0.0 | 0.7 |
| JumpStart | 27.4 | arm_L0_2 | 29.7–69.5 | 4.06 | 25.5 | 19.8 | 0.8 | -0.0 |
| Land | 21.0 | leg_L0_0 | 3.6–74.5 | 3.78 | 25.8 | 19.8 | 0.0 | -0.0 |
| HardLand | 27.7 | arm_R0_2 | 3.6–109.8 | 4.99 | 25.8 | 19.8 | 0.0 | -0.0 |
| DodgeForward | 31.3 | arm_L0_0 | 31.7–73.0 | 3.95 | 25.2 | 19.8 | 3.2 | -0.2 |
| DodgeBack | 30.1 | arm_L0_1 | 26.3–53.9 | 4.17 | 25.8 | 17.1 | 0.9 | -0.2 |
| DodgeLeft | 19.5 | arm_R0_1 | 26.9–44.6 | 4.06 | 23.9 | 19.8 | 3.5 | -0.4 |
| DodgeRight | 19.5 | arm_L0_0 | 26.7–45.3 | 4.06 | 23.9 | 19.8 | 3.1 | -0.4 |
| HitFront | 27.3 | arm_L0_2 | 31.7–42.6 | 3.99 | 25.8 | 19.8 | 0.0 | -0.0 |
| HitBack | 18.1 | arm_L0_0 | 31.7–42.6 | 3.65 | 25.4 | 19.8 | 0.0 | -0.0 |
| HitLeft | 20.2 | arm_L0_2 | 31.7–42.6 | 3.80 | 19.5 | 19.8 | 0.0 | -0.0 |
| HitRight | 20.2 | arm_R0_0 | 31.7–42.6 | 3.80 | 19.4 | 19.8 | 0.0 | -0.0 |
| Stagger | 23.3 | arm_R0_1 | 31.7–72.3 | 3.80 | 25.8 | 19.8 | 0.9 | -0.0 |
| Knockdown | 21.1 | arm_R0_0 | 21.1–116.6 | 6.12 | 25.8 | 16.0 | 4.2 | -0.3 |
| Death | 11.8 | leg_L0_0 | 8.0–115.7 | 5.10 | 20.5 | 16.5 | 1.3 | -1.6 |
| CastBasic | 28.0 | arm_R0_0 | 50.2–52.1 | 4.69 | 29.5 | 38.9 | 0.0 | -0.0 |
| DemonEye | 18.2 | arm_L0_1 | 50.7–52.0 | 4.19 | 17.7 | 38.9 | 0.0 | -0.0 |
| Awakening | 19.8 | arm_L0_1 | 36.2–52.0 | 4.35 | 29.5 | 38.9 | 0.0 | -0.0 |
| CastTwoHand | 26.2 | arm_R0_2 | 50.7–60.4 | 4.70 | 29.5 | 38.9 | 0.0 | -0.0 |
| CastGround | 12.1 | arm_R0_2 | 50.7–94.5 | 5.60 | 26.9 | 38.9 | 0.0 | -0.0 |
| Cast_Fireball | 22.5 | arm_L0_1 | 50.7–57.2 | 4.25 | 26.7 | 38.9 | 0.0 | -0.0 |
| Cast_Fireball_Release | 26.3 | arm_R0_2 | 50.7–57.7 | 5.00 | 20.4 | 38.9 | 0.0 | -0.0 |
| Cast_FlameWave | 25.6 | arm_R0_2 | 50.7–65.2 | 5.37 | 24.2 | 38.9 | 0.0 | -0.0 |
| Cast_Inferno | 29.7 | arm_R0_2 | 26.5–100.6 | 5.46 | 29.5 | 38.9 | 0.0 | -0.0 |
| Cast_WaterBullet | 27.3 | arm_R0_2 | 50.4–52.2 | 4.08 | 29.2 | 38.9 | 0.0 | -0.0 |
| Cast_WaterDragon | 29.6 | arm_L0_2 | 31.6–68.9 | 4.91 | 26.9 | 38.9 | 1.4 | -0.0 |
| Cast_Flood | 24.7 | arm_L0_0 | 50.7–95.0 | 5.10 | 25.6 | 38.5 | 0.0 | -0.0 |
| Cast_StoneCannon | 23.1 | arm_R0_2 | 50.7–57.2 | 4.25 | 29.3 | 38.9 | 0.0 | -0.0 |
| Cast_StoneCannon_Release | 16.5 | arm_R0_2 | 50.7–57.2 | 4.86 | 29.0 | 38.9 | 0.0 | -0.0 |
| Cast_EarthWall | 30.3 | arm_R0_2 | 40.9–83.8 | 5.20 | 25.9 | 38.9 | 0.0 | -0.0 |
| Cast_EarthSpikes | 27.3 | arm_R0_0 | 45.1–87.2 | 5.25 | 29.5 | 38.9 | 0.0 | -0.0 |
| Cast_WindBlade | 29.7 | arm_R0_1 | 50.2–52.3 | 4.86 | 28.0 | 38.9 | 0.0 | -0.0 |
| Cast_Tornado | 20.7 | arm_R0_2 | 40.9–57.0 | 4.37 | 29.3 | 38.9 | 0.0 | -0.0 |
| Cast_WindBurst | 28.1 | arm_R0_1 | 45.2–63.2 | 4.44 | 22.7 | 38.9 | 0.0 | -0.0 |
| StoneCannon_Charge | 19.5 | arm_L0_1 | 50.7–63.4 | 5.24 | 19.9 | 38.9 | 0.0 | -0.0 |
| StoneCannon_Release | 25.3 | arm_R0_0 | 50.7–63.6 | 5.24 | 24.1 | 38.9 | 0.0 | -0.0 |
| Quagmire | 16.1 | arm_R0_0 | 50.7–96.6 | 5.64 | 29.5 | 38.9 | 0.0 | -0.0 |
| Quagmire_Release | 13.7 | arm_R0_0 | 50.7–100.5 | 5.79 | 29.5 | 38.9 | 0.0 | -0.0 |
| Barrage | 29.7 | arm_L0_2 | 49.0–71.4 | 5.05 | 28.3 | 38.9 | 0.0 | -0.0 |
| Cast_Fireball_Hold | 1.7 | arm_R0_2 | 54.7–58.5 | 4.30 | 27.4 | 38.9 | 0.0 | -0.0 |
| Cast_StoneCannon_Hold | 1.7 | arm_R0_2 | 55.0–58.1 | 4.29 | 29.2 | 38.9 | 0.0 | -0.0 |
| StoneCannon_Hold | 2.8 | arm_R0_2 | 61.4–64.3 | 5.24 | 25.3 | 38.9 | 0.0 | -0.0 |
| Quagmire_Hold | 1.7 | arm_L0_1 | 94.3–97.8 | 5.68 | 34.9 | 38.9 | 0.0 | -0.0 |

The per-frame lowest-vertex check is new. It finds two earlier clips that dip slightly on some frames: Death reaches
−1.6 cm while lying down (the ground clamp stops after six iterations) and the airborne Rise loop −2.7 cm. Neither clip
was changed in this pass.

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

The ability fires on the clip's `Release` notify (`UMTAnimNotify_Event`, added on import from the sidecar events), or
on `CastTime` if no notify arrives. For every §7 clip, Rudeus's `Release` equals the ability's `CastTime`:

| Ability | `Release` (= `CastTime`) | Motion at that frame |
|---|---|---|
| Basic cast (`CastBasic`, no event) | 0.12 s | strike extension at 0.14 s |
| Fireball / Stone Cannon (element) / signature Stone Cannon / Quagmire | 0.30 / 0.35 / 0.35 / 0.45 s (anticipation end) | the hold pose; chargeable, so they release on input release: release clips carry `Release` 0.04 and reach full extension by 0.10 s |
| Flame Wave, Inferno, Water Bullet, Water Dragon, Flood | 0.42, 0.90, 0.22, 0.75, 0.70 s | mid-sweep, fist at the bottom, finger snap, point, both palms at the push |
| Earth Wall, Earth Spikes, Wind Blade, Tornado, Wind Burst | 0.40, 0.40, 0.25, 0.55, 0.12 s | hand driving up, hand driven down, mid-slash, the fling up, arms starting to open |
| Elemental Barrage | `Release` 0.60 s, `Finale` 3.95 s | the point (first shot), then one directing gesture per shot every 0.2 s; the double push |

## Independent verification

`Tools/anim/verify_glb.py` re-parses the exported file with numpy, with no Blender involved, the way an engine importer would. On this build it reported **VERIFY: PASS**:
- 136 joints with the same names as the source rig;
- bind height 1.617 m with the feet at 0, facing +Z;
- 55 animations with exact durations;
- the lowest vertex of every floor-contact clip within 0.6 cm of the ground.

Rise is an air loop and reaches −2.7 cm in place; it only ever plays while airborne. The contact sheet is in `Docs/Images/Animation/Verify_ExportedGLB.png` (and `Verify_Rudeus_Casts.png` for Fireball release, Inferno, Water Dragon, Flood and Barrage), and per-clip sheets are in `Docs/Images/Animation/`.

## Orsted's clip set (his generated model)

Orsted's real model is in the repo (`SourceArt/Characters/Orsted/`), re-rigged onto the shared skeleton, with all 59
clips authored on his own proportions (40 imported into UE 5.8 on 2026-09-27; the 19 casting clips of this pass are not
imported yet). Everything about it (the model's problems, the re-rig
fixes, the full per-clip table, the before/after of every clip fix and the engine tests) is in **`Docs/QA_Orsted.md`**.
In short: IK error ≤ 0.01 cm, planted slip 0.00 cm/s, seams 0.000°, fastest limb 31.3°/frame (Dragon Step now 31.2,
was 31.9), planted-foot slide ≤ 2.5 cm except the side-dodge landing (4.4 cm), `verify_glb.py` PASS. The `A_Orsted_*` sheets now show him.

Two changes to the shared tools came out of that work, and neither changes Rudeus's output (re-authoring his 37 clips
reproduces the committed GLB to within 0.078° / 0.0001 mm):
- `Rig` derives the foot-roll pivots (ball, heel, toe tip) from each skeleton; Rudeus's equal the old constants.
- `qa_animation_quality.py` uses the rig's own ball height for its planted-foot test.

## Honest limits

- **Procedurally keyframed.** The poses and timing are deliberate, but there is no motion-capture nuance and no facial animation (the model has no blendshapes).
- **Coat is procedural.** It is driven procedurally, not cloth-simulated. In UE, enable a Physics Asset or AnimDynamics on the `skirt_*`, `hair_*` and `hood_*` chains for extra life.
- **Dodge landings slide slightly.** The feet land 3–3.5 cm from their planted spot relative to the root, because the dash's root motion is still carrying the body. In-engine foot IK (not implemented yet) would pin them.
- **No turn-in-place.** `TurnLeft90` / `TurnRight90` aren't authored yet; the native anim instance falls back gracefully.
- **The casting clips are not in Unreal yet.** The 18 new keys and the upgraded Stone Cannon, Quagmire and Barrage were
  verified offline only (per-frame QA, the independent GLB verifier, contact sheets); the notify import was exercised
  against a mock `unreal` module, not a real editor. Blends over locomotion, the `Release` notifies firing and the
  chargeable hand-offs (anticipation → hold → release) have to be watched in Play-In-Editor.
- **Imported into Unreal, not yet watched there.** On 2026-09-27 the project compiled on UE 5.8.3, `mt_setup_rudeus.py`
  imported all 37 clips (PASS), and the runtime tests confirmed the native anim instance plays them (no T-pose, feet on
  the floor; `Docs/QA_Orsted.md` §3). Watching the blends in Play-In-Editor is still to do.
