# Mushoku Tensei RPG: Unreal Engine 5 vertical slice

An open-world action RPG set in the Mushoku Tensei world, built in **Unreal Engine 5.5 (C++)**. The progression structure is inspired by Shindo Life: character, element and race are separate axes, with spins, mastery and missions. The game does not copy Shindo Life's UI or content.

**Current scope:** two character lineages only (**Rudeus Greyrat**, **Orsted**), four elements (Fire / Water / Earth / Wind, three abilities each), a race framework, and the **Fittoa region with Buena Village**.

> **Status: not yet compiled or played.** This repository was created in a cloud container that has no Unreal Engine. All code targets UE 5.5 APIs, but nothing has run in-engine yet. See `Docs/Phase1_QA.md` for the honest grade and `Docs/Implementation_Plan.md` for the next steps.

## Quick start

1. Install **UE 5.5** and a C++ toolchain (Visual Studio 2022 or Rider).
2. Right-click `MushokuRPG.uproject` → *Generate project files*, then build the `MushokuRPGEditor` target.
3. Open the editor. In the Output Log (Python mode), run:
   - `py mt_create_materials.py`: creates the decal, spell, foresight and toon master materials
   - `py mt_setup_rudeus.py`: imports **your Rudeus model**, sets up its material, sockets, IK Rig and Retargeter, and batch-retargets Mixamo clips
   - `py mt_world_setup.py`: creates `L_Fittoa` (World Partition) and places Buena, the day/night controller, nav bounds and the player start
4. Import the heightmap from `SourceArt/Terrain/Fittoa/` (the settings are printed by the setup script).
5. Press Play.

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
- `Content/Data/*.json`: all gameplay data, hot-reloadable and loaded by `UMTDataRegistry`
- `Content/Python/`: editor automation scripts
- `SourceArt/`: the original and normalized Rudeus GLB, terrain source data
- `Tools/`: offline tools that were actually run in this session (GLB inspection and normalization, terrain generation, data validation, roll simulation, placement tests)
- `Docs/`: audit, plan, QA grades, lore research, animation pipeline, Orsted model spec, world notes

## Credits

See `CREDITS.md`. The Rudeus model is by **pamogss**, licensed CC-BY-4.0.
