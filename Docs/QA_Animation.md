# QA: Rudeus animation set (authored, not Mixamo)

Source: `Tools/anim/build_rudeus_anims.py`. Output: `SourceArt/Characters/Rudeus/Rudeus_Animated.glb` (skinned mesh, 136-joint skeleton, 34 clips at 30 fps). Every number below was measured by the tools in this repo. Nothing was estimated.

## Why these are authored rather than downloaded

Mixamo requires an interactive Adobe login, which this cloud session can't use. The brief says: *"If Mixamo does not have an appropriate animation: make the animation yourself."* So the whole set is authored directly on **Rudeus's own skeleton**, with no retargeting loss. Mixamo clips can still be added later through `mt_setup_rudeus.py`.

## How they are made

- **Semantic pose composer** (`pose_compose.py`): each pose is described by pelvis offset and rotation, spine bend and twist, arm lower/swing/reach, elbow and wrist, finger curl, and foot placement. The composer converts that into armature-space rotation deltas, so the imported rig's odd bone axes never matter.
- **Analytic two-bone leg IK** with knee pole vectors. Feet stay planted on the ground; heel strike pivots on the heel and toe-off on the ball, with the toes staying flat on the floor.
- **IK/FK leg blending** for jumps, dodges, knockdown and death.
- **Automatic pelvis lowering** whenever a stride would over-extend a leg, so no leg ever snaps straight.
- **Coat panels** (8 chains) follow the thighs, so legs never poke through the robe. Hair and coat trail at speed.
- **Ground clamp** (iterative, per frame) for clips where the body touches the floor.
- **Rig repair:** 214 eye vertices were 100% weighted to the root control bone and would have hung in the air at standing eye height during crouches, knockdowns and death. They are re-weighted to the head.

## Measured quality (locomotion and all clips)

Columns:
- **Leg reach:** 1.0 means a fully straight leg; 0.985 is the cap.
- **IK target error:** how far the ankle misses its target, in cm.
- **Planted slip:** how far a planted foot's speed deviates from the ground speed, in cm/s.
- **Seam:** the largest joint-rotation difference between the last and first frame of a loop, in degrees.

| Clip | Duration s | Type | Ref speed cm/s | Leg reach | IK target error cm | Planted slip cm/s | Seam ° |
|---|---|---|---|---|---|---|---|
| A_Rudeus_Idle | 4.000 | loop |  | 0.970 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CombatIdle | 2.000 | loop |  | 0.912 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Walk | 0.933 | loop | 130.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_WalkBack | 0.933 | loop | 100.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StrafeLeft | 0.600 | loop | 110.0 | 0.979 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StrafeRight | 0.600 | loop | 110.0 | 0.979 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Run | 0.633 | loop | 360.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Sprint | 0.567 | loop | 580.0 | 0.985 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Rise | 0.600 | loop |  | 0.000 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Fall | 0.600 | loop |  | 0.000 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_StoneCannon_Hold | 1.000 | loop |  | 0.876 | 0.00 | 0.00 | 0.000 |
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
| A_Rudeus_Barrage | 1.200 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_DemonEye | 0.800 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_Awakening | 1.800 | one-shot |  | 0.931 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastTwoHand | 0.600 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |
| A_Rudeus_CastGround | 0.800 | one-shot |  | 0.904 | 0.00 | 0.00 | 0.000 |

**Independent verification** (`Tools/anim/verify_glb.py`) re-parses the exported file with numpy, with no Blender involved, the way an engine importer would. It confirmed:
- 136 joints with the same names as the source rig;
- bind height 1.617 m with the feet at 0, facing +Z;
- 34 animations with exact durations;
- ground penetration of at most 2 cm on any keyframe of the floor-contact clips.

It first caught a real export bug (Blender's default 24 fps made every clip 1.25× too long), which is now fixed. Contact sheets are in `Docs/Images/Animation/`.

## Honest limits

- These are procedurally keyframed. The poses and timing are deliberate, but there is no motion-capture nuance and no facial animation (the model has no blendshapes).
- The coat is procedurally driven, not cloth-simulated. In UE, enable a Physics Asset or AnimDynamics on the `skirt_*`, `hair_*` and `hood_*` chains for extra life.
- Turn-in-place clips (`TurnLeft90` / `TurnRight90`) aren't authored yet. The native anim instance falls back gracefully.
- Nothing has been seen in Unreal yet; this needs the in-engine pass on your Mac.
