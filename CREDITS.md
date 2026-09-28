# Credits and licences

## Rudeus Greyrat 3D model

- **Title:** "Rudeus Greyrat - Mushoku Tensei"
- **Author:** pamogss (https://sketchfab.com/pamogss)
- **Source:** https://sketchfab.com/3d-models/rudeus-greyrat-mushoku-tensei-c6769b0bed5d4a8c913e64a573187a60
- **Licence:** CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/), as stated in the GLB's own metadata.
- **Changes:** `SourceArt/Characters/Rudeus/Rudeus_Greyrat_UE.glb` is a derivative made by `Tools/prepare_rudeus_glb.py`. It is uniformly scaled ×0.1, its ancestor transforms are flattened, and the `KHR_materials_unlit` extension is removed. The geometry, weights, texture and proportions are otherwise unchanged. The original upload is kept alongside it.

**Provenance note:** the rig uses production-style joint names (`*_C0_0_jnt`, `item_L0` / `item_R0` attach points, `tear_*` joints). Rigs like that often come from a game pipeline. Confirm the model's origin with its author before any public or commercial release.

## Orsted 3D model

- **What:** an original character design for this project (not traced from anime key art or ripped from any game),
  written from `Docs/Orsted_Model_Spec.md`: silver hair, golden eyes, off-white fur-collared greatcoat, charcoal
  undercoat, knee boots, no weapon.
- **How it was made:** generated with **Higgsfield** in the project owner's own account: a T-pose design sheet
  (image job `7c154109-2510-4954-84c2-d9fe83e02e6f`), then `image_to_3d` with rigging
  (job `20688114-b544-40bb-a4d4-da56354a1f46`). AI-generated; check Higgsfield's terms for your plan before any
  commercial release.
- **Files:** `SourceArt/Characters/Orsted/Orsted_Source.glb` is the generated file, unmodified. `Orsted_Rigged.glb`,
  `Orsted_Animated.glb` and `Orsted_Atlas.png` are derived from it by `Tools/anim/make_orsted.sh` (arms straightened
  from the generated A-pose, re-rigged onto the shared skeleton, re-weighted, emissive material removed, 40 authored
  clips). The mesh and texture are otherwise unchanged.

## Franchise

Mushoku Tensei: Jobless Reincarnation © Rifujin na Magonote / MFBooks, and the anime by Studio Bind. This is a non-commercial fan project. No anime footage, audio, music or extracted game assets are used.

## Engine

Unreal Engine © Epic Games. Engine basic shapes (`/Engine/BasicShapes`) serve only as labelled blockout placeholders.
