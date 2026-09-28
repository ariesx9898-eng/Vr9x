# Mushoku Tensei RPG: Unreal Engine 5 vertical slice

An open-world action RPG set in the Mushoku Tensei world, built in **Unreal Engine 5.8 (C++)**, with code kept to APIs that have been stable since 5.3. The progression structure is inspired by Shindo Life: character, element and race are separate axes, with spins, mastery and missions. The game does not copy Shindo Life's UI or content.

**Current scope:** two character lineages only (**Rudeus Greyrat**, **Orsted**), four elements (Fire / Water / Earth / Wind, three abilities each), a race framework, and the **Fittoa region with Buena Village**.

> **Status: not yet compiled or played.** This repository was created in a cloud container that has no Unreal Engine. The code targets UE 5.8, but nothing has run in-engine yet. See `Docs/Phase1_QA.md` for the honest grade and `Docs/Implementation_Plan.md` for the next steps.

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
   - imports **Rudeus with his 37 authored animations** (`SourceArt/Characters/Rudeus/Rudeus_Animated.glb`), plus Orsted once his model has been processed (see below)
   - sets up the `L_Fittoa` world and validates it

   It ends with a PASS/FAIL summary; the logs are in `Saved/Logs/mt_*.log`. `Tools/mac/README.md` has the options (`--build-only`, `--skip-build`, `UE_ROOT=...`) and how a Claude session running on the Mac (the Claude Desktop app, or `claude remote-control` in the repo) can run the script and iterate on compile errors.
3. Open `MushokuRPG.uproject` and import the heightmap from `SourceArt/Terrain/Fittoa/`. The world setup log prints the settings.
4. Press Play.

**Orsted:** once his generated mesh (GLB) is in the repo, run `Tools/anim/make_orsted.sh <path/to/orsted.glb>` (it needs Python 3.11 with `pip install bpy==5.0.1 numpy pillow`, the versions the clips were authored with). It re-rigs him, authors his 40 clips, verifies them and points his data at them. Then run `Tools/mac/build_and_setup.sh`, which imports him too.

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
- `Tools/mac/`: the one-command macOS build and headless editor setup. It has not been run against a real engine yet; only a dry run with a stub engine was done.
- `Docs/`: audit, plan, QA grades, lore research, animation pipeline, Orsted model spec, world notes

## Credits

See `CREDITS.md`. The Rudeus model is by **pamogss**, licensed CC-BY-4.0.
