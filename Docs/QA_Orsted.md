# QA: Orsted (Phase 6) — model, rig, animation, engine integration

Date: 2026-09-27. Machine: Apple M3 Pro Mac, **Unreal Engine 5.8.3** (Epic Launcher build), Xcode 26.6, Blender `bpy` 5.0.1
on Python 3.11.8 (a venv made from the Python that ships with UE). Unlike every earlier QA pass, this one ran on a
machine **with Unreal Engine**: the project was compiled, the characters were imported by the editor, and runtime tests
ran in a real game world (headless, `-nullrhi`).

Results are split three ways:

- **A. Offline tools** (Blender / numpy, no engine): rig, weights, clip metrics, independent GLB verification.
- **B. Real engine, headless, on this Mac:** compile, editor import, world validation commandlet, automation tests.
- **C. Still needs a person in the editor:** anything that must be *seen* or *felt* (rendered play, camera, VFX).

Command that reproduces all of A: `PYTHON=<python with bpy> Tools/anim/make_orsted.sh SourceArt/Characters/Orsted/Orsted_Source.glb`.
Command that reproduces all of B: `Tools/mac/build_and_setup.sh` (final run: **`RESULT: OK`**, exit 0).

---

## 1. The model

| | |
|---|---|
| Source | Higgsfield generation `20688114-b544-40bb-a4d4-da56354a1f46` (`image_to_3d`, rigging on, 1.95 m, symmetry on), made from an original T-pose design sheet (`7c154109-…`) written from `Docs/Orsted_Model_Spec.md` (silver hair, golden eyes, off-white fur-collared greatcoat, charcoal undercoat, belt, slate trousers, knee boots, no weapon). Kept as `SourceArt/Characters/Orsted/Orsted_Source.glb`. |
| Mesh | 40,754 triangles, 55,802 vertices at 19,632 distinct positions (split at every UV seam: 9,104 islands), one 2048² colour atlas (saved as `Orsted_Atlas.png`), height 1.950 m, facing +Z. 0 duplicate / degenerate / zero-area faces; winding agrees with the stored normals (0 flipped of 40,754). |
| Its own rig | 24 Mixamo-style joints, no fingers, one 0.3 s clip. |

What the inspection found (all measured) and what the pipeline now does about it:

| Problem in the generated file | Measured | Fix (in `Tools/anim/rig_to_rudeus_skeleton.py`) |
|---|---|---|
| **A-pose**, although a T-pose was requested | arms 43.1° (L) / 41.2° (R) below horizontal | The arms are posed straight out with the model's own skin weights, then baked. The shared skeleton keeps Rudeus's T-pose rest orientations, so every authoring tool works unchanged. |
| **Knee joints outside the leg** (pulled forward by the coat) | 13.0 / 13.2 cm in front of the hip–ankle line | Knees refitted onto the line (plus the template's 1.2 cm bend bias). |
| **Asymmetric auto-rig** | right shin 6.4 mm shorter, right ankle 3.6 mm higher, right shoulder 12 mm lower; 1.7 cm worst mismatch | Landmarks mirror-averaged (every tool measures one leg and mirrors it). |
| **Self-lit material**: the colour atlas bound as full-strength emission, specular tint 2.0 | would glow in any engine | Emission removed, plain rough non-metallic surface. (UE also swaps in `MI_Orsted_Toon`.) |
| **UV-seam splits** | 177 seam positions whose copies got different bones → cracks in motion | Weights are computed once per welded position; copies deform identically. |
| **No fingers**, hands ~20 cm | — | Rudeus's 15 finger joints fitted and scaled to the hand (×1.028, same both sides). |

### Re-rig bugs found and fixed in the tool itself

| Bug | Symptom | Fix |
|---|---|---|
| Bone segments taken from the bone *tail* for any joint with more than one child. This rig's tails point sideways and the shin (foot + twist helper), forearm (hand + twist + sleeve), hand, chest, pelvis and head all have several children. | Shins weighted to the coat chains (lower legs smeared into a horizontal band when walking); forearms weighted to nothing near them. The earlier Rudeus stand-in test hid this under his robe. | Each deforming bone runs to its anatomical next joint; finger tips, toes, head and pelvis get explicit segments. |
| Coat cloth assigned to exactly one of the 8 panel chains | Panels tore apart at their borders | Linear blend between the two neighbouring chains around the body (the open front stays split). |
| Hard coat / trouser boundary | Slivers where a lifted thigh meets the coat | Majority vote cleans misclassified vertices; 6 smoothing passes below the waist, 4 at the shoulders, 1 everywhere; max 4 influences. |
| Coat chains sized for Rudeus's shorter robe | — | Chains re-spaced from the waist (1.169 m) to the measured hem (0.579 m). |
| Foot-roll pivots hard-coded to Rudeus's foot | Orsted's toes would dip ~1.5 cm into the floor at every toe-off | `Rig` derives ball / heel / toe pivots from each skeleton (Rudeus's values are unchanged). |

Measured effect (triangles stretched more than 2.5×, same poses):

| Pose | First working re-rig | + segment fix, panel blend | + seam weld, smoothing |
|---|---|---|---|
| Walk t=0 | 1,076 | 270 | 178 |
| Run t=0.1 | 1,354 | 326 | 206 |
| Dodge forward t=0.1 | 1,377 | 347 | 253 |

What remains is the intended smooth hand-over between coat and trousers, not tearing. Sheets:
`Docs/Images/Animation/Orsted_ReRig_QA.png`, `Docs/Images/Orsted_Model_Idle.png`.

---

## 2. Animation — 59 clips on his own proportions (A: offline)

Authored by `Tools/anim/build_rudeus_anims.py --character Orsted` from `Tools/anim/clips_orsted.py` (and, since the
ability overhaul, `Tools/anim/clips_spells.py`), then measured on the actually skinned mesh every frame
(`qa_animation_quality.py`) and re-parsed independently with numpy (`verify_glb.py`: **VERIFY: PASS**, 136 joints
matching the shared skeleton, 1.950 m, feet at 0, faces +Z, 59 clips with exact durations). 40 of them were imported
into UE 5.8 on 2026-09-27 (§3); the 19 clips added by the ability overhaul (§2.1) have not been imported yet. Full JSON: `Docs/QA_Animation_Metrics_Orsted.json`, per-clip contact sheets `Docs/Images/Animation/A_Orsted_*.png`.

Column meanings are those of `Docs/QA_Animation.md` (leg reach 1.0 = straight leg; slip = planted-foot speed error;
seam = loop end vs start; peak = fastest major-bone rotation per 30 fps frame, the snap detector; stretch = worst
edge-length ratio vs the bind pose; foot slide = travel of a flat, planted foot in one-shots; lowest vertex = the
lowest skinned vertex over every frame, measured per frame since the ability overhaul pass, so Death shows its true
−1.7 cm while lying down where the 6-sample verifier reported −1.0). Rise is an airborne-only loop.

| Clip | Duration s | Type | Ref speed cm/s | Leg reach | IK err cm | Planted slip cm/s | Seam ° | Peak °/frame | Knee ° | Stretch | Hand–spine cm | Ankle gap cm | Foot slide cm | Lowest vertex cm |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Idle | 4.000 | loop |  | 0.977 | 0.00 | 0.00 | 0.000 | 0.1 | 24.6–31.2 | 4.75 | 25.5 | 31.6 | 0.0 | 0.0 |
| CombatIdle | 2.000 | loop |  | 0.954 | 0.00 | 0.00 | 0.000 | 0.1 | 34.8–39.4 | 3.99 | 38.1 | 31.6 | 0.0 | 0.0 |
| Walk | 1.000 | loop | 145 | 0.985 | 0.00 | 0.00 | 0.000 | 11.9 | 19.9–54.9 | 4.79 | 28.1 | 18.5 | — | -0.4 |
| WalkBack | 1.000 | loop | 110 | 0.985 | 0.00 | 0.00 | 0.000 | 13.4 | 19.9–51.3 | 4.78 | 28.2 | 18.0 | — | -0.2 |
| StrafeLeft | 0.600 | loop | 115 | 0.985 | 0.00 | 0.00 | 0.000 | 6.4 | 19.9–51.8 | 4.69 | 29.6 | 15.7 | — | -0.2 |
| StrafeRight | 0.600 | loop | 115 | 0.985 | 0.00 | 0.00 | 0.000 | 6.4 | 19.9–51.8 | 4.69 | 29.6 | 15.7 | — | -0.2 |
| Run | 0.700 | loop | 420 | 0.985 | 0.00 | 0.00 | 0.000 | 17.5 | 19.9–95.3 | 5.55 | 29.0 | 25.5 | — | -0.4 |
| Sprint | 0.633 | loop | 660 | 0.985 | 0.00 | 0.00 | 0.000 | 28.1 | 19.9–109.6 | 6.82 | 28.5 | 31.2 | — | -0.6 |
| RunStrafeLeft | 0.700 | loop | 380 | 0.985 | 0.00 | 0.00 | 0.000 | 15.3 | 19.9–84.9 | 5.30 | 30.5 | 20.7 | — | -0.4 |
| RunStrafeRight | 0.700 | loop | 380 | 0.985 | 0.00 | 0.00 | 0.000 | 16.5 | 19.9–85.1 | 5.03 | 30.5 | 19.9 | — | -0.4 |
| RunBack | 0.700 | loop | 330 | 0.985 | 0.00 | 0.00 | 0.000 | 15.7 | 19.9–76.7 | 6.18 | 33.0 | 20.3 | — | -0.2 |
| CastBasic (palm strike) | 0.450 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 30.0 | 34.8–55.1 | 4.22 | 31.1 | 31.6 | 0.6 | 0.0 |
| DragonStep | 0.560 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 31.2 | 26.6–70.9 | 4.13 | 31.2 | 31.6 | 0.8 | -0.8 |
| Aura | 0.900 | one-shot |  | 0.976 | 0.00 | 0.00 | 0.000 | 8.2 | 25.4–39.4 | 4.19 | 32.8 | 31.6 | 0.0 | 0.0 |
| Awakening | 1.500 | one-shot |  | 0.977 | 0.00 | 0.00 | 0.000 | 7.3 | 24.6–39.4 | 4.77 | 28.4 | 31.6 | 0.0 | 0.0 |
| HitFront | 0.330 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 14.4 | 34.8–44.5 | 3.99 | 38.2 | 31.6 | 0.0 | 0.0 |
| HitBack | 0.330 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 10.4 | 34.8–44.5 | 4.10 | 32.8 | 31.6 | 0.0 | 0.0 |
| HitLeft | 0.330 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 11.0 | 34.8–44.5 | 3.99 | 36.7 | 31.6 | 0.0 | 0.0 |
| HitRight | 0.330 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 10.9 | 34.8–44.5 | 3.99 | 35.6 | 31.6 | 0.0 | 0.0 |
| Stagger | 0.800 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 12.2 | 34.8–60.4 | 3.99 | 38.1 | 31.6 | 1.5 | 0.0 |
| Knockdown | 1.400 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 22.3 | 34.8–116.7 | 6.27 | 26.6 | 31.6 | 1.4 | -0.8 |
| Cast_Fireball | 0.200 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 17.6 | 34.8–39.4 | 3.99 | 37.5 | 31.6 | 0.0 | 0.0 |
| Cast_Fireball_Release | 0.333 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 21.6 | 34.8–39.4 | 3.99 | 37.7 | 31.6 | 0.0 | 0.0 |
| Cast_FlameWave | 0.600 | one-shot |  | 0.955 | 0.00 | 0.00 | 0.000 | 25.9 | 34.6–39.4 | 3.99 | 35.8 | 31.6 | 0.0 | 0.0 |
| Cast_Inferno | 0.967 | one-shot |  | 0.965 | 0.00 | 0.00 | 0.000 | 28.7 | 30.4–49.2 | 3.99 | 37.1 | 31.6 | 0.0 | 0.0 |
| Cast_WaterBullet | 0.367 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 25.9 | 34.8–39.4 | 4.28 | 38.2 | 31.6 | 0.0 | 0.0 |
| Cast_WaterDragon | 0.900 | one-shot |  | 0.976 | 0.00 | 0.00 | 0.000 | 25.4 | 25.3–39.4 | 4.86 | 33.2 | 31.6 | 0.0 | 0.0 |
| Cast_Flood | 0.900 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 26.4 | 34.8–49.2 | 5.04 | 32.9 | 31.6 | 0.0 | 0.0 |
| Cast_StoneCannon | 0.233 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 20.5 | 34.8–39.4 | 3.99 | 38.2 | 31.6 | 0.0 | 0.0 |
| Cast_StoneCannon_Release | 0.300 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 26.1 | 34.8–39.4 | 4.25 | 38.1 | 31.6 | 0.0 | 0.0 |
| Cast_EarthWall | 0.533 | one-shot |  | 0.960 | 0.00 | 0.00 | 0.000 | 23.3 | 32.7–42.8 | 3.99 | 37.7 | 31.6 | 0.0 | 0.0 |
| Cast_EarthSpikes | 0.533 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 25.6 | 34.8–46.2 | 4.00 | 37.0 | 31.6 | 0.0 | 0.0 |
| Cast_WindBlade | 0.367 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 30.6 | 34.7–39.4 | 3.99 | 38.0 | 31.6 | 0.0 | 0.0 |
| Cast_Tornado | 0.667 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 22.9 | 34.8–39.4 | 3.99 | 38.1 | 31.6 | 0.0 | 0.0 |
| Cast_WindBurst | 0.367 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 23.3 | 34.8–42.9 | 4.24 | 35.2 | 31.6 | 0.0 | 0.0 |
| DisturbMagic | 0.867 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 15.6 | 34.8–39.4 | 3.99 | 38.2 | 31.6 | 0.0 | 0.0 |
| DragonCrush | 0.900 | one-shot |  | 0.954 | 0.00 | 0.00 | 0.000 | 25.0 | 34.8–75.0 | 5.13 | 33.5 | 31.6 | 0.6 | 0.0 |
| Cast_Fireball_Hold | 0.600 | loop |  | 0.954 | 0.00 | 0.00 | 0.000 | 0.7 | 34.8–39.4 | 3.99 | 37.7 | 31.6 | 0.0 | 0.0 |
| Cast_StoneCannon_Hold | 0.600 | loop |  | 0.954 | 0.00 | 0.00 | 0.000 | 0.5 | 34.8–39.4 | 3.99 | 38.2 | 31.6 | 0.0 | 0.0 |
| DodgeForward | 0.400 | one-shot |  | 0.981 | 0.00 | 0.00 | 0.000 | 31.3 | 22.3–72.7 | 4.91 | 31.8 | 31.6 | 1.1 | -0.1 |
| DodgeBack | 0.400 | one-shot |  | 0.981 | 0.00 | 0.00 | 0.000 | 30.1 | 3.6–52.7 | 4.74 | 32.4 | 31.5 | 1.4 | -0.4 |
| DodgeLeft | 0.400 | one-shot |  | 0.981 | 0.00 | 0.00 | 0.000 | 19.5 | 9.2–44.5 | 4.56 | 29.9 | 31.6 | 2.5 | -0.2 |
| DodgeRight | 0.400 | one-shot |  | 0.981 | 0.00 | 0.00 | 0.000 | 19.5 | 12.9–48.0 | 4.55 | 29.8 | 31.6 | 4.4 | -0.2 |
| Death | 1.600 | one-shot |  | 0.981 | 0.00 | 0.00 | 0.000 | 11.8 | 7.9–106.9 | 5.34 | 27.3 | 31.0 | 1.8 | -1.7 |
| StoneCannon_Charge | 0.367 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 19.5 | 48.2–60.5 | 4.05 | 27.2 | 31.6 | 0.0 | 0.0 |
| StoneCannon_Hold | 1.000 | loop |  | 0.878 | 0.00 | 0.00 | 0.000 | 2.8 | 57.3–61.2 | 4.08 | 34.0 | 31.6 | 0.0 | 0.0 |
| StoneCannon_Release | 0.700 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 25.3 | 48.2–60.5 | 4.79 | 31.6 | 31.6 | 0.0 | 0.0 |
| Quagmire | 0.467 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 16.1 | 48.2–89.3 | 4.95 | 36.8 | 31.6 | 0.0 | 0.0 |
| Quagmire_Hold | 1.000 | loop |  | 0.732 | 0.00 | 0.00 | 0.000 | 1.7 | 85.9–90.4 | 4.98 | 42.2 | 31.6 | 0.0 | 0.0 |
| Quagmire_Release | 0.600 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 13.7 | 48.2–92.8 | 5.02 | 36.8 | 31.6 | 0.0 | 0.0 |
| Barrage | 4.600 | one-shot |  | 0.918 | 0.00 | 0.00 | 0.000 | 29.7 | 46.7–67.6 | 5.04 | 36.1 | 31.6 | 0.0 | 0.0 |
| DemonEye | 0.800 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 18.2 | 48.2–51.2 | 3.70 | 20.3 | 31.6 | 0.0 | 0.0 |
| CastTwoHand | 0.650 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 26.2 | 48.2–58.0 | 4.22 | 36.8 | 31.6 | 0.0 | 0.0 |
| CastGround | 0.800 | one-shot |  | 0.913 | 0.00 | 0.00 | 0.000 | 12.1 | 48.2–87.4 | 4.93 | 32.0 | 31.6 | 0.0 | 0.0 |
| Rise | 0.600 | loop |  | — | 0.00 | 0.00 | 0.000 | 1.0 | 36.8–64.7 | 4.62 | 53.8 | 34.1 | 0.0 | -3.9 (air only) |
| Fall | 0.600 | loop |  | — | 0.00 | 0.00 | 0.000 | 1.4 | 18.8–28.8 | 3.34 | 62.8 | 34.2 | 0.0 | 0.5 (air only) |
| JumpStart | 0.260 | one-shot |  | 0.969 | 0.00 | 0.00 | 0.000 | 27.4 | 16.5–62.5 | 4.67 | 32.2 | 19.8 | 0.9 | -0.6 |
| Land | 0.400 | one-shot |  | 1.000 | 0.01 | 0.00 | 0.000 | 18.5 | 3.6–66.8 | 4.55 | 32.4 | 19.8 | 0.0 | -0.0 |
| HardLand | 0.800 | one-shot |  | 1.000 | 0.01 | 0.00 | 0.000 | 27.7 | 3.6–97.5 | 5.07 | 32.4 | 19.8 | 0.0 | -0.0 |

Summary: IK error ≤ 0.01 cm, planted slip 0.00 cm/s, loop seams 0.000°, no knee hyperextension (min 3.6°), no quaternion
flips, fastest limb 31.3°/frame (Dodge Forward; Dragon Step 31.2, was 31.9), no hand through the torso (closest
20.3 cm), boots never touch (closest 15.7 cm), planted-foot slide ≤ 2.5 cm except the side dodge landing (4.4 cm, the
dash root motion still carrying the body: same class as Rudeus's 3.1–3.5 cm), every floor-contact clip within 1.0 cm of
the floor except Death while lying down (−1.7 cm on a few frames, per-frame check; unchanged clip).

### What the clip pass fixed (first full run → final)

| Problem | Before | After | Fix |
|---|---|---|---|
| IK misses and planted-foot slip (asymmetric skeleton) | 0.39–0.86 cm, up to 5.06 cm/s | 0.00 cm, 0.00 cm/s | symmetric fitted skeleton |
| Palm strike wrist snap | 71.0°/frame | 30.0°/frame | wrist pre-opened in the coil, 3-frame strike |
| Palm strike half-step skated | 15.7 cm | 0.6 cm | the front foot lifts for the step and for the return |
| Disturb Magic hand snap | 57.9°/frame | 26.8°/frame | 3-frame raise |
| Disturb Magic cut its whiff recovery short | clip 0.55 s vs window + recovery 0.85 s | 0.85 s | readable hand reset over the whole recovery |
| Dragon Step feet still gliding after the dash stopped | glide keys to 0.30 s, plant 0.40 s (dash ends 0.26 s) | plant at 0.29 s with the arrival palm | retimed to the ability |
| Dragon Step elbow snap | 41.4°/frame | 31.9°/frame | softer arm trail |
| Awakening feet skated between stances | 20.7 cm | 0.0 cm | stays on the guard footing |
| Knockdown get-up dragged the feet (and the long coat fanned out lying on his back) | 23.3 cm | 1.4 cm | new Orsted knockdown: driven back a step, down on the rear knee, up again; he never lies on his back or rolls |
| Hit reactions from Rudeus's stance | 18–27°/frame | 10–14°/frame | Orsted's own minimal flinches from his guard |
| Idle ↔ combat stance crossfade moved planted feet | 8.5 / 10.4 cm | 0 | relaxed stance uses the guard's footing |
| Element / race casts blending in and out of his stance | Rudeus's combat footing (5–8 cm off his) | 0 | inherited casts remapped onto his guard footing |
| Shoulder cap stretch (idle) | 8.96 | 4.75 | shoulder weight smoothing |

### 2.1 Ability overhaul pass (2026-09-29): his casting clips

Orsted gets his own version of all 16 element keys, re-authored Disturb Magic and Dragon Step, and the new Dragon Crush
(`Docs/Animation_Pipeline.md` §0.1 has the per-key table). His personality (`Docs/Ability_Overhaul.md` §4): each clip is
55–65% as long as Rudeus's, one small gesture of one hand from his guard, calm and upright (a slight knee bend on the
heavy techniques only), and his `Release` notifies fire earlier (§7). The silhouettes stay those of the spell: the
Inferno hand still rises overhead and comes down, Flame Wave is still a sweep, Water Dragon still twirls up and points.

- **Disturb Magic** (`Release` 0.06): the right hand (the ability's cast socket) rises from the guard with no wind-up,
  the fingers flick open at 0.06 s while it is still rising, the ward hand holds at the chest through the 0.35 s window,
  then an unhurried return. It used to raise the left hand; the peak is now 15.6°/frame (was 26.8).
- **Dragon Step** (`Release` 0.08): a lower launch stance, a stronger blur lean with the arms and coat streaming back,
  and an arrival knee dip with the compact palm. Peak 31.2°/frame (was 31.9).
- **Dragon Crush** (new, `Release` 0.40 = impact): the right arm draws back to the hip while the hips coil and the weight
  goes back, then a 0.2 s drive into the palm with a front-foot step (lifted, planted before the impact); the palm keeps
  travelling a few centimetres past the impact frame so the strike does not decelerate into it, is held to 0.55 s, and
  he is back on his guard footing by 0.82 s (clip 0.9 s). Peak 25.0°/frame, planted-foot slide 0.6 cm.

His own casting clips (the last column is the length as a share of Rudeus's):

| Clip | Length s | Events | Hand | Loop | Leg reach | IK err cm | Seam ° | Peak °/frame | Knee ° | Stretch | Hand–spine cm | Foot slide cm | Lowest vertex cm | vs Rudeus |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Cast_Fireball | 0.200 | Release 0.19 | R |  | 0.954 | 0.00 | 0.000 | 17.6 (arm\_R0\_2) | 34.8–39.4 | 3.99 | 37.5 | 0.0 | 0.0 | 60% |
| Cast_Fireball_Hold | 0.600 | – | R | loop | 0.954 | 0.00 | 0.000 | 0.7 (arm\_R0\_2) | 34.8–39.4 | 3.99 | 37.7 | 0.0 | 0.0 | – |
| Cast_Fireball_Release | 0.333 | Release 0.04 | R |  | 0.954 | 0.00 | 0.000 | 21.6 (arm\_R0\_0) | 34.8–39.4 | 3.99 | 37.7 | 0.0 | 0.0 | 59% |
| Cast_FlameWave | 0.600 | Release 0.26 | R |  | 0.955 | 0.00 | 0.000 | 25.9 (arm\_R0\_2) | 34.6–39.4 | 3.99 | 35.8 | 0.0 | 0.0 | 60% |
| Cast_Inferno (full body) | 0.967 | Release 0.55 | R |  | 0.965 | 0.00 | 0.000 | 28.7 (arm\_R0\_2) | 30.4–49.2 | 3.99 | 37.1 | 0.0 | 0.0 | 60% |
| Cast_WaterBullet | 0.367 | Release 0.14 | R |  | 0.954 | 0.00 | 0.000 | 25.9 (arm\_R0\_2) | 34.8–39.4 | 4.28 | 38.2 | 0.0 | 0.0 | 61% |
| Cast_WaterDragon (full body) | 0.900 | Release 0.45 | both, R points |  | 0.976 | 0.00 | 0.000 | 25.4 (arm\_L0\_0) | 25.3–39.4 | 4.86 | 33.2 | 0.0 | 0.0 | 60% |
| Cast_Flood (full body) | 0.900 | Release 0.42 | both |  | 0.954 | 0.00 | 0.000 | 26.4 (arm\_R0\_0) | 34.8–49.2 | 5.04 | 32.9 | 0.0 | 0.0 | 60% |
| Cast_StoneCannon | 0.233 | Release 0.22 | R |  | 0.954 | 0.00 | 0.000 | 20.5 (arm\_R0\_2) | 34.8–39.4 | 3.99 | 38.2 | 0.0 | 0.0 | 64% |
| Cast_StoneCannon_Hold | 0.600 | – | R | loop | 0.954 | 0.00 | 0.000 | 0.5 (arm\_R0\_2) | 34.8–39.4 | 3.99 | 38.2 | 0.0 | 0.0 | – |
| Cast_StoneCannon_Release | 0.300 | Release 0.04 | R |  | 0.954 | 0.00 | 0.000 | 26.1 (arm\_R0\_2) | 34.8–39.4 | 4.25 | 38.1 | 0.0 | 0.0 | 60% |
| Cast_EarthWall | 0.533 | Release 0.25 | R |  | 0.960 | 0.00 | 0.000 | 23.3 (arm\_R0\_2) | 32.7–42.8 | 3.99 | 37.7 | 0.0 | 0.0 | 59% |
| Cast_EarthSpikes | 0.533 | Release 0.25 | R |  | 0.954 | 0.00 | 0.000 | 25.6 (arm\_R0\_1) | 34.8–46.2 | 4.00 | 37.0 | 0.0 | 0.0 | 59% |
| Cast_WindBlade | 0.367 | Release 0.15 | R |  | 0.954 | 0.00 | 0.000 | 30.6 (arm\_R0\_2) | 34.7–39.4 | 3.99 | 38.0 | 0.0 | 0.0 | 61% |
| Cast_Tornado | 0.667 | Release 0.34 | R |  | 0.954 | 0.00 | 0.000 | 22.9 (arm\_R0\_2) | 34.8–39.4 | 3.99 | 38.1 | 0.0 | 0.0 | 61% |
| Cast_WindBurst | 0.367 | Release 0.08 | both |  | 0.954 | 0.00 | 0.000 | 23.3 (arm\_L0\_2) | 34.8–42.9 | 4.24 | 35.2 | 0.0 | 0.0 | 61% |
| DisturbMagic | 0.867 | Release 0.06 | R |  | 0.954 | 0.00 | 0.000 | 15.6 (arm\_R0\_2) | 34.8–39.4 | 3.99 | 38.2 | 0.0 | 0.0 | – |
| DragonStep | 0.560 | Release 0.08 | R (arrival palm) |  | 0.954 | 0.00 | 0.000 | 31.2 (arm\_R0\_2) | 26.6–70.9 | 4.13 | 31.2 | 0.8 | -0.8 | – |
| DragonCrush (full body) | 0.900 | Release 0.40 | R |  | 0.954 | 0.00 | 0.000 | 25.0 (arm\_R0\_0) | 34.8–75.0 | 5.13 | 33.5 | 0.6 | 0.0 | – |

Rudeus's signature sets on his guard footing (so every key resolves on his skeleton; the same events as Rudeus's):

| Clip | Length s | Events | Hand | Loop | Leg reach | IK err cm | Seam ° | Peak °/frame | Knee ° | Stretch | Hand–spine cm | Foot slide cm | Lowest vertex cm |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| StoneCannon_Charge | 0.367 | Release 0.35 | R (L braces) |  | 0.913 | 0.00 | 0.000 | 19.5 (arm\_L0\_1) | 48.2–60.5 | 4.05 | 27.2 | 0.0 | 0.0 |
| StoneCannon_Hold | 1.000 | – | R (L braces) | loop | 0.878 | 0.00 | 0.000 | 2.8 (arm\_R0\_2) | 57.3–61.2 | 4.08 | 34.0 | 0.0 | 0.0 |
| StoneCannon_Release | 0.700 | Release 0.04 | R |  | 0.913 | 0.00 | 0.000 | 25.3 (arm\_R0\_0) | 48.2–60.5 | 4.79 | 31.6 | 0.0 | 0.0 |
| Quagmire | 0.467 | Release 0.45 | L |  | 0.913 | 0.00 | 0.000 | 16.1 (arm\_R0\_0) | 48.2–89.3 | 4.95 | 36.8 | 0.0 | 0.0 |
| Quagmire_Hold | 1.000 | – | L | loop | 0.732 | 0.00 | 0.000 | 1.7 (arm\_L0\_1) | 85.9–90.4 | 4.98 | 42.2 | 0.0 | 0.0 |
| Quagmire_Release | 0.600 | Release 0.04 | L |  | 0.913 | 0.00 | 0.000 | 13.7 (arm\_R0\_0) | 48.2–92.8 | 5.02 | 36.8 | 0.0 | 0.0 |
| Barrage | 4.600 | Release 0.60, Finale 3.95 | both, R points |  | 0.918 | 0.00 | 0.000 | 29.7 (arm\_L0\_2) | 46.7–67.6 | 5.04 | 36.1 | 0.0 | 0.0 |

Worst values over these 26 clips: IK error **0.00 cm**, planted slip **0.00 cm/s**, loop seams **0.000°**, fastest bone
**31.2°/frame** (Dragon Step), planted-foot slide ≤ **0.8 cm** (Dragon Step's arrival), lowest vertex **−0.8 cm** (Dragon
Step's glide), stretch ≤ 5.13, closest hand to the torso 27.2 cm. **Loops:** `Cast_Fireball_Hold`,
`Cast_StoneCannon_Hold` (0.6 s, almost still: a slow finger wave), `StoneCannon_Hold`, `Quagmire_Hold` (1.0 s); seams
0.000°.

---

## 3. Unreal Engine 5.8.3 on this Mac (B: real engine, headless)

### Build

First ever compile of the module. Three compile errors, all fixed; final build **0 errors, 0 warnings**:

1. UHT: `AMTCharacterBase::ReceiveHit` declared as a `UFUNCTION` clashes with `AActor::ReceiveHit` → renamed `ReceiveCombatHit`.
2. `MTPlacementValidator.cpp` and 3. `MTTestArenaGameMode.cpp`: loops that always exit on the first iteration (`-Werror,-Wunreachable-code-loop-increment`).

### Editor setup (`Tools/mac/build_and_setup.sh`, headless editor running `Content/Python`)

| Step | Final result |
|---|---|
| Data (`validate_data.py`) | ALL CHECKS PASSED |
| Materials | 4 / 4 created (zone decal was FAIL: the script used an expression class that does not exist) |
| Rudeus import | 46 passed, 0 failed: `SK_Rudeus` 161.7 cm, 37 clips, `MI_Rudeus_Toon`, 6 sockets |
| **Orsted import** | **49 passed, 0 failed: `SK_Orsted` 195.0 cm, 40 clips (loop flags, lengths vs the exporter sidecar), `MI_Orsted_Toon`, 6 sockets** (before the ability overhaul: the 59-clip GLB and its event notifies still have to be imported) |
| World (`L_Fittoa`, World Partition) | created: day/night controller, Buena generator, nav bounds, player start, HLOD layer (type now set), data layers |
| World validation (`MTValidateWorld` commandlet, every WP actor loaded) | 0 errors, 0 warnings |
| Automation tests | 7 / 7 passed |

### Automation tests (`Source/MushokuRPG/Private/Tests/MTCharacterIntegrationTests.cpp`, `Tools/mac/run_automation_tests.sh`)

A real game world (game instance + data registry, game mode, begun play, a floor) ticked at 60 Hz. Numbers are the
values each run records (`Saved/Automation/index.json`):

| Test | Measured |
|---|---|
| `MushokuRPG.Orsted.Data` | `SK_Orsted` 195.0 cm, 136 bones; all 40 AnimSet clips load and are on his skeleton; the 6 Orsted abilities play `A_Orsted_*` clips |
| `MushokuRPG.Orsted.BodyAndPose` | capsule 38 / 98, standing on the floor, feet joints 4.1 cm above the capsule bottom (his toe-joint height: planted), head joint 174.8 cm, idle hands 47.8 cm below the shoulder (animated, not a T-pose), native anim instance |
| `MushokuRPG.Orsted.LockOnFacing` | from 90° off: 0.00° after 1.0 s; target moved behind him: 0.01° after 0.8 s; locked-on sprint breaks the facing (orient to movement), 0.00° again 0.8 s after the sprint; strafing flag on the anim instance while locked |
| `MushokuRPG.Orsted.DragonStep` | 674 → 134 cm from the target, target health 900 → 882 (the row's 18 damage), 0.00° facing afterwards, no dodge tag left; a movement dash (Gale Step) past a bystander: 0 damage |
| `MushokuRPG.Orsted.DisturbMagic` | whiff: no Dragon God Knowledge, 4.73 s of the 6 s cooldown left; timed against a real Rudeus stone bullet: spell collapsed, 0 damage taken, Knowledge granted, cooldown refunded to 1.74 s |
| `MushokuRPG.Combat.PresentationFallbacks` | a stone bullet is visible (blockout cone) and Earth Fortress raised 3 visible walls while their authored meshes do not exist |
| `MushokuRPG.Rudeus.BodyAndPose` | regression: capsule 32 / 81, on the floor, feet joints 1.9 cm (planted), head joint 133.5 cm, idle hands 40.9 cm below the shoulder |

### Bugs the engine run exposed, all fixed

| # | Bug | Effect in the game | Fix |
|---|---|---|---|
| 1 | Attack dashes never applied their damage (`UMTAbility_Dash` had no strike) | Dragon Step's arrival palm, Awakened Dragon Step, Hunter's Dash, Wolf Lunge, Alpha Pounce and the boss's Wyrm Dive (90 damage) did nothing | shared `UMTAbility::StrikeHostilesInRadius` (melee and dash); movement dashes (no damage, no stagger) never strike |
| 2 | Locked-on sprint kept facing the target | sprint-speed strafes / backpedals at up to 1.9× the clips' authored speed (clamped at 1.5×): sliding feet | `AMTCharacterBase::IsStrafingLocked()`: a sprint runs where it is steered, the camera stays locked; the anim instance strafes exactly while it is true |
| 3 | AI retreat sprinted while still facing its target | the same backpedal slide on AI Orsted / Rudeus | `AMTEnemyCharacter::Tick` switches to orient-to-movement while sprinting away |
| 4 | Lock-on facing kept turning the body during dashes | Dragon Step / Gale Step skidded sideways mid-glide | facing held while the dodge / dash tag is set |
| 5 | Missing authored meshes made spells and walls invisible | every projectile (all name meshes that are not authored yet) was invisible; Earth Fortress was an invisible wall that still blocked | blockout look whenever the named asset does not load |
| 6 | Missing VFX / sound assets re-loaded on every cast | a load warning and a synchronous package search per effect per cast (hitches) | `MTCombat::LoadOptional`: checked once, skipped quietly after |
| 7 | `MTValidateWorld` crashed on exit | assertion in `UWorldPartition::BeginDestroy` (partition never uninitialised), then a subsystem ensure | partition uninitialised and the world destroyed properly |
| 8 | Zone decal material script used `MaterialExpressionRadialGradientExponential` (a material function, not an expression) | `M_MT_ZoneDecal` missing: ground spells and telegraph circles had no material | radial falloff built from basic expressions |
| 9 | Sockets could not be created from Python in 5.8 (`SocketName` / `BoneName` read-only) | all spells formed at the fallback chest point | `UMTEditorScriptingLibrary::AddOrUpdateSkeletalMeshSocket`; palm sockets 8 cm toward the middle finger (bone axes of this rig do not follow the limbs) |
| 10 | Re-running the import was not idempotent with the Interchange importer | the new mesh landed beside `SK_<C>`; `SK_<C>` kept the old mesh | the script's own assets are deleted and garbage-collected before the import |
| 11 | World validation step validated the empty "Untitled" editor world | the step proved nothing | the commandlet on `/Game/Maps/L_Fittoa` |
| 12 | The crash reporter of a crashed headless step kept the log pipe open | the whole pipeline hung | `-nocrashreports` on every headless editor call |
| 13 | HLOD layer type enum renamed in 5.8 (`HLODLayerType`) | HLOD layer left untyped | uses the 5.8 name, falls back to the old one |

The test harness itself needed two fixes to tick like a game (advance `GFrameCounter` per tick as the engine's
`FTestWorldWrapper` does; give controller-less characters their default movement mode). Those were harness bugs, not
game bugs.

### Rudeus + Orsted integration and regression

- **Rudeus's clips are unchanged.** Re-authoring all 37 with the updated shared tools reproduces the committed
  `Rudeus_Animated.glb` to within 0.078° / 0.0001 mm (float noise); his files were not touched.
- Both characters import side by side with separate skeletons; data validation, the body/pose test for each, and every
  ability test pass in the same run. Element spells cast by Orsted resolve to his own clips (`ResolveLineageAnim`).
- The gameplay fixes (1–6) apply to both characters and to enemies; the tests cover Orsted's side and Rudeus's body.

### Z-fighting / overlap / references

- Orsted's mesh: 0 duplicate, 0 degenerate, 0 zero-area triangles; consistent winding.
- Rudeus's uploaded mesh: 3 coincident face pairs, each under 0.6 cm² (two behind the head at 1.45 m, one of 0.02 cm² on
  the face), and 10% of triangles with winding opposite their normals (rendered correctly by the two-sided toon
  material). Left as uploaded (the brief forbids altering the model); the fix is deleting one face of each pair.
- `L_Fittoa` as generated: 0 validation errors or warnings (overlaps, duplicates, floating, coplanar checks). The
  landscape and the village are not generated yet (§C), so this covers the template and the placed gameplay actors.
- Ground spells are decals (no coplanar planes). Missing presentation assets no longer warn or leave invisible geometry.

---

## C. Still needs a person in the editor (not verifiable headless)

1. **Look at him rendered:** Play-In-Editor with `MTSetCharacter Orsted`. Check the toon material on the generated
   texture, the two-sided coat, shadows, and the silhouette at 30 m. The generated texture is softer than the spec's
   4K hand-painted target.
2. **Feel:** idle → walk → run → sprint → jump → land, lock-on strafing in 8 directions with a gamepad, every cast
   standing and moving (upper-body layer), dodge, hit, stagger, knockdown, death, Aura, Dragon God. The tests prove the
   logic and poses; blend smoothness has to be watched.
3. **Coat:** the 8 chains are driven procedurally; add a physics asset / AnimDynamics (or Chaos Cloth) on `skirt_*` for
   secondary motion. Deep lunges still pinch the coat's inner front edge a little.
4. **VFX / SFX:** none are authored (`/Game/VFX`, `/Game/Audio` paths in the data are placeholders); spells show the
   blockout look.
5. **World:** import the heightmap, press Generate on the Buena generator, re-run `MTValidateWorld`.
6. **AI arena, Rudeus vs Orsted:** needs a map with a NavMesh; the ability logic both sides use is covered by the tests.

---

## Grade (Phase 6 scope: Orsted), honest

| # | Category | Score | Why |
|---|---|---|---|
| 1 | Gameplay | 5.0 | Signature kit verified in a real engine world (Disturb Magic timing, Dragon Step strike, lock-on); never played by a person |
| 2 | Responsiveness | 3.5 | Input path unchanged and unmeasured with a controller |
| 3 | Animation | 6.0 | 40 clips measured clean and imported; procedural keys, procedural coat, not yet watched in-engine |
| 4 | Character accuracy | 6.5 | Canon cues present (silver hair, gold eyes, fur-collared white coat, 195 cm); generated mesh and texture below the spec's budget (40.7k tris vs 60k, no hair cards, no blendshapes, one LOD) |
| 5 | Combat | 5.5 | Six dash abilities now actually hit; counters and strikes verified; no hit-stop or tuning pass |
| 6 | VFX | 0.5 | No Niagara assets |
| 7 | Environment | 2.0 | Unchanged this phase (map validated, still blockout) |
| 8 | UI | 2.0 | Unchanged |
| 9 | AI | 2.5 | Retreat facing fixed; not run with navigation |
| 10 | Performance | 2.5 | Per-cast missing-asset loads removed; not profiled |
| 11 | Stability | 6.5 | Clean compile, idempotent setup, 7/7 runtime tests, exit crash fixed; no long play session yet |
| 12 | Lore / reference accuracy | 7.0 | Canon identity and kit (Disturb Magic, Saint Dragon Battle Aura) with CANON / ORIGINAL labels |

**Overall 4.1 / 10: FAIL** against the 8.5 bar (critical categories below 8.0: animation, gameplay, stability, VFX).

### Passes

- **PASS 1** (re-rig): score 1.5 → 2.8. Problems: A-pose, knees off the leg, shins weighted to the coat (segment bug),
  self-lit material. Changes: T-pose straightening, knee refit, anatomical segments, material clean-up. Testing:
  stretched-triangle counts 1,076–1,377 → 270–347 per pose, weight maps.
- **PASS 2** (weights): 2.8 → 3.2. Problems: seam cracks, panel tearing, asymmetric rig. Changes: welded weights,
  panel blend, smoothing, symmetric fit. Testing: 178–253 stretched triangles; IK error 0.00 cm, slip 0.00 cm/s.
- **PASS 3** (clips): 3.2 → 3.6. Problems: snaps up to 71°/frame, skates up to 23 cm, stance mismatches. Changes: the
  clip rewrites in §2. Testing: per-frame QA and the verifier (tables above).
- **PASS 4** (engine): 3.6 → 3.9. Problems: 3 compile errors, import / socket / material / validation script failures.
  Changes: §3 build fixes and bugs 7–13. Testing: `build_and_setup.sh` → `RESULT: OK`.
- **PASS 5** (runtime): 3.9 → 4.1. Problems: dashes never hit, sprint and dash facing, invisible spells and walls.
  Changes: bugs 1–6. Testing: 7 / 7 automation tests with the measurements above.

After five passes the phase is still below 8.5. What keeps it there is §C: nothing has been seen rendered or played by a
person, there are no VFX, and the coat has no simulation. The next pass has to happen in the editor.
