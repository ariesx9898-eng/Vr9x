# Mushoku Tensei RPG: Unreal Engine 5 vertical slice

An open-world action RPG set in the Mushoku Tensei world, built in **Unreal Engine 5.8 (C++)**, with code kept to APIs that have been stable since 5.3. The progression structure is inspired by Shindo Life: character, element and race are separate axes, with spins, mastery and missions. The game does not copy Shindo Life's UI or content.

**Current scope:** two character lineages only (**Rudeus Greyrat**, **Orsted**), four elements (Fire / Water / Earth / Wind, three abilities each), a race framework, and **LA PLACE**: an 18 x 14 km World Partition map of the Mushoku Tensei world (`L_LaPlace`: Asura / Fittoa, Ranoa, the Demon Continent, Millis, Begaritt) with its title menu, world map and HUD. The older 2 km **Fittoa / Buena** map (`L_Fittoa`) stays as a test map.

> **Status: rebuilt and verified on UE 5.8.3 on 2026-09-28, not yet play-tested by a person.** Clean C++ build (0 warnings), the full Mac pipeline rebuilt every generated asset and the LA PLACE world with its 517 HLODs, 18/18 runtime automation tests pass (including all 17 must-keep abilities cast in a real game world and the ability overhaul), every asset loads with no missing references, and the ability showcase, UI tour and world tour were reviewed from real renders. Details, and what still needs a person: `Docs/QA_UE583_Rebuild.md`. Earlier grades: `Docs/QA_Orsted.md`, `Docs/Phase1_QA.md`.

## Quick start (UE 5.8 on macOS, Apple Silicon)

1. Install **UE 5.8** from the Epic Games Launcher (default location `/Users/Shared/Epic Games/UE_5.8`) and **Xcode**.
2. From the repository root, run:
   ```bash
   Tools/mac/build_and_setup.sh
   ```
   With no editor open, this does everything in one go:
   - builds `MushokuRPGEditor`
   - checks the JSON data
   - creates the master materials
   - imports **Rudeus with his 37 authored animations** (`SourceArt/Characters/Rudeus/Rudeus_Animated.glb`) and **Orsted with his 40** (`SourceArt/Characters/Orsted/Orsted_Animated.glb`), with materials and cast sockets
   - imports the LA PLACE UI, audio, effects and building / nature kit, builds the `L_LaPlace` world (terrain, water, cities, foliage) and its HLODs
   - sets up the `L_Fittoa` test map and validates it with the `MTValidateWorld` commandlet
   - runs the runtime automation tests (`Tools/mac/run_automation_tests.sh`: lineage data, body and pose, lock-on facing, Dragon Step, Disturb Magic, presentation fallbacks, all 17 must-keep abilities)

   It ends with a PASS/FAIL summary; the logs are in `Saved/Logs/mt_*.log`. Only one build runs at a time (a second one, or the game launcher, waits for it). `Tools/mac/README.md` has the options (`--build-only`, `--skip-build`, `UE_ROOT=...`) and how a Claude session running on the Mac (the Claude Desktop app, or `claude remote-control` in the repo) can run the script and iterate on compile errors.
3. Play: `Tools/mac/play.sh --explore` (LA PLACE title menu and world), `Tools/mac/play.sh` (arena: Rudeus vs an AI Orsted) or `--as Orsted`; `Tools/mac/play.sh --make-icon` puts a **LA PLACE** launcher on the Desktop. It recompiles by itself when the code changed.
4. Or open `MushokuRPG.uproject` in the editor (it opens `L_Fittoa`; `L_LaPlace` needs ~20 GB in the editor) and press Play.

**Orsted** is in the repo: `SourceArt/Characters/Orsted/Orsted_Source.glb` (an original model generated from the design in `Docs/Orsted_Model_Spec.md`) and everything derived from it. To rebuild him after changing the model or his clips, run `Tools/anim/make_orsted.sh SourceArt/Characters/Orsted/Orsted_Source.glb` with a Python 3.11 that has `bpy==5.0.1 numpy pillow` (the versions the clips were authored with). The Python that ships with UE works as a base: `"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3.11" -m venv ~/.venvs/mushoku-bpy311`, `pip install` into it, then `PYTHON=~/.venvs/mushoku-bpy311/bin/python Tools/anim/make_orsted.sh ...`. It straightens the A-posed arms, refits the skeleton, re-weights (coat chains included), authors and verifies his 40 clips, measures them every frame and points his data at them. Then run `Tools/mac/build_and_setup.sh`.

Characters animate with **no Animation Blueprint and no montage assets**: `UMTNativeAnimInstance` plays the clips listed in `Content/Data/AnimSets.json`. Swapping in an Animation Blueprint later is a one-line data change. See `Docs/Animation_Pipeline.md` §0.

On other platforms, do the same steps by hand:
1. Generate project files and build `MushokuRPGEditor`.
2. Open the editor. In the Output Log (Python), run `py mt_create_materials.py`, `py mt_setup_rudeus.py` and `py mt_world_setup.py`.

## Controls

| Action | Keyboard / mouse | Gamepad |
|---|---|---|
| Move / look | WASD / mouse | Left / right stick |
| Jump / sprint / walk toggle | Space / Shift / Alt | A / L3 |
| Dodge (directional) | Ctrl | B |
| Lock-on | Tab or middle mouse | R3 |
| Basic attack | LMB | RT |
| Character abilities 1–3 | 1, 2, 3 (hold to charge Stone Cannon) | RB, LB, LT |
| Special (Demon Eye) / Awakening | F / G | Y / – |
| Element slot A / B | 4–6 / 7–9 | – |
| Race active / transformation | R / T | – |
| Interact | E | X |
| Menus: character / roll / quests / map / inventory / settings | C / K / J / M / I / Esc | Start |

Dev console commands: `MTGiveSpins 10`, `MTSetCharacter Orsted`, `MTSaveNow`.

## Repository map

- `Source/MushokuRPG/`: the C++ game module (the architecture is in `Docs/Implementation_Plan.md`)
- `Content/Data/*.json`: all gameplay data, hot-reloadable and loaded by `UMTDataRegistry`. This includes `AnimSets.json`, the per-character animation clips.
- `Content/Python/`: editor automation scripts
- `SourceArt/`: the original and normalized Rudeus GLB, terrain source data
- `Tools/`: offline tools that were actually run in this session (GLB inspection and normalization, terrain generation, data validation, roll simulation, placement tests)
- `Tools/mac/`: the one-command macOS build, headless editor setup, world validation and runtime tests (`build_and_setup.sh`, `run_automation_tests.sh`). Run against UE 5.8.3 on 2026-09-27: `RESULT: OK`.
- `Docs/`: audit, plan, QA grades, lore research, animation pipeline, Orsted model spec, world notes

## Credits

See `CREDITS.md`. The Rudeus model is by **pamogss**, licensed CC-BY-4.0. The Orsted model is an original design generated for this project with Higgsfield (see `CREDITS.md`).
