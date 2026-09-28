# Phase 0: Project audit

Date: 2026-09-28. Auditor: lead developer (Claude Code).

## 1. What existed before this work

| Item | Finding |
|---|---|
| Git repository `ariesx9898-eng/Vr9x` | **Empty.** No commits, branches, releases or files. |
| Unreal project (`.uproject`, `Source/`, `Content/`, `Config/`) | **None existed.** Everything in this repo was created in this session. |
| Uploaded Rudeus model | Found at the session upload path as `rudeus_greyrat_-_mushoku_tensei.glb` (1.79 MB). Copied to `SourceArt/Characters/Rudeus/Rudeus_Greyrat.glb`. |
| Other assets, maps, plugins, Blueprints | None. |
| Build environment | Linux cloud container **without Unreal Engine**. C++ cannot be compiled and the editor cannot be run here (see §4). |

## 2. Rudeus model inspection

`Tools/inspect_glb.py` and `Tools/render_glb_preview.py` parse the GLB and re-skin it. Nothing was eyeballed from a thumbnail.

| Property | Value | Assessment |
|---|---|---|
| Format | glTF 2.0 binary, Sketchfab exporter 17.1 | OK |
| Licence | CC-BY-4.0, author pamogss (see CREDITS.md) | Attribution required. Done. |
| Meshes | 1 mesh, 1 primitive | Single draw call, game-ready |
| Vertices / triangles | 22,624 / 28,040 | Light, suitable for gameplay and LOD generation |
| Materials | 1 (`material_atlas_77913_1`), **unlit**, double-sided | Unlit would ignore sun and shadows. Converted to a lit toon master. Double-sided is kept for thin cloth. |
| Textures | 1 × 1024² RGB atlas (PNG) | Colours are preserved as-is. No normal or roughness maps. |
| Morph targets | None | No facial blendshapes. Expressions need texture swaps or new morphs later. |
| **Skeleton** | **Rigged. 136 joints**, root `_rootJoint`, pelvis `spine_C0_0_jnt_05` | Full humanoid, 5 fingers × 3 joints per hand, plus secondary chains |
| Secondary chains | 8 coat-skirt chains (5 joints each), hood, bag, belt, 8 hair chains, sleeves, eyes, tears, L/R item attach points | Good candidates for Physics Asset or AnimDynamics secondary motion |
| Skin weights | ≤4 influences per vertex, all sums = 1.0 ± 1e-7 | Clean |
| Unweighted joints | 34 (end joints, eyes, tears, items, bag tips) | Normal. They don't affect deformation. |
| Bind pose | **T-pose.** Arms horizontal along ±X at shoulder height. Knees and elbows perfectly straight. | Matches Mixamo's T-pose, which simplifies the retarget pose. Straight knees need a pole or pre-bend hint in IK. |
| Scale | **16.17 units tall = 10× oversized** (glTF is metres) | Fixed by `Tools/prepare_rudeus_glb.py` → **1.617 m = 161.7 cm**. Verified numerically. |
| Ancestor transforms | Z-up→Y-up rotation × FBX 0.01 × 100 scales | Flattened to one pure rotation so no scale leaks into the root bone |
| Ground contact | Lowest skinned vertex at y = 0.000 | Feet sit exactly on the origin. Capsule half-height 81 cm with a −81 mesh offset. |
| Proportions | Hip 85.4 cm, shoulder 125.2 cm, head joint 136.8 cm | Realistic anime proportions (not chibi). Mixamo retarget is feasible. |

Preview (re-skinned bind pose, front/side/back/joints): `SourceArt/Characters/Rudeus/Rudeus_BindPose_Preview.png`.

Deformation of the shoulders, elbows, wrists, spine, neck, knees and ankles can't be judged in the bind pose. It needs posed playback in Unreal. That test is listed in Phase 1 QA as **not yet executed**.

## 3. What was built in this session

See `Docs/Implementation_Plan.md` for the full plan and `README.md` for the file map. In summary: a UE 5.5 C++ module (`MushokuRPG`) with a data-driven gameplay framework, JSON game data, Python editor automation, a normalized character asset, generated Fittoa terrain, and QA documents.

## 4. Hard limitations of this environment (read this)

- **No Unreal Engine in the container.** No C++ compile, no editor, no PIE and no output log exist here. Every claim about in-engine behaviour is therefore **unverified** until the project is opened in UE 5.5. The code was written against well-known UE5 APIs, but a first compile will likely surface some errors.
- **Mixamo requires an Adobe login in a browser.** Animations can't be downloaded from here. `Docs/Animation_Pipeline.md` lists the exact clips to fetch and the scripts that retarget them automatically.
- **No Orsted model exists.** A spec sheet is written (`Docs/Orsted_Model_Spec.md`). Until the model is made, Orsted falls back to a clearly-flagged placeholder.
