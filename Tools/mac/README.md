# macOS one-command build and setup (UE 5.8, Apple Silicon)

`Tools/mac/build_and_setup.sh` takes a fresh clone to a built editor with Rudeus and Orsted imported and animated, the Fittoa map set up and validated, and the runtime tests run. You don't need to open the editor.

```bash
# UE 5.8 from the Epic Games Launcher, Xcode installed (xcode-select -p prints a path)
cd <repo>
Tools/mac/build_and_setup.sh
```

The script runs these steps in order. Each step writes its own log in `Saved/Logs/`.

| Step | What runs | Log |
|---|---|---|
| build | `Engine/Build/BatchFiles/Mac/Build.sh MushokuRPGEditor Mac Development -Project=<abs uproject> -WaitMutex` | `mt_build.log` |
| data | `python3 Tools/validate_data.py` (offline JSON check, no Unreal needed) | `mt_data.log` |
| materials | `Content/Python/mt_create_materials.py` | `mt_materials.log` |
| rudeus | `Content/Python/mt_setup_rudeus.py`: imports `SourceArt/Characters/Rudeus/Rudeus_Animated.glb`, renames every clip to `/Game/Characters/Rudeus/Animations/A_Rudeus_<Key>`, adds the cast sockets, logs PASS/FAIL per clip | `mt_rudeus.log` |
| orsted | `Content/Python/mt_setup_orsted.py`: the same steps for `SourceArt/Characters/Orsted/Orsted_Animated.glb` (40 clips; built by `Tools/anim/make_orsted.sh`). Skipped with a note if that file is missing. | `mt_orsted.log` |
| world | `Content/Python/mt_world_setup.py` | `mt_world.log` |
| validation | the `MTValidateWorld` commandlet on `/Game/Maps/L_Fittoa`, with every World Partition actor loaded (overlaps, duplicates, coplanar and floating geometry, broken references). `mt_validate_world.py` stays available for the level open in the editor. | `mt_validation.log` |
| tests | `Tools/mac/run_automation_tests.sh`: `Automation RunTests MushokuRPG` in `UnrealEditor-Cmd` (lineage data, body and pose, lock-on facing, Dragon Step, Disturb Magic, presentation fallbacks). JSON report in `Saved/Automation/`. | `mt_tests.log` |

Each editor step runs headless:

```
UnrealEditor.app/Contents/MacOS/UnrealEditor <uproject> -run=pythonscript -script=<abs script> \
  -unattended -nosplash -nullrhi -nocrashreports -stdout -FullStdOutLogOutput
```

At the end, the script prints a summary built from the logs. The summary shows:

- compiler errors (`file:line: error:`)
- UE `Error:` lines
- validator `ERROR:` lines
- every `PASS` / `FAIL` / `MANUAL` line the setup scripts print
- the world validation totals and every automation test result

It exits `0` (`RESULT: OK`) only when the build and every step succeeded and no `FAIL`, Python error or failed test was logged. Last full run: 2026-09-27 on UE 5.8.3, `RESULT: OK` in about 2.5 minutes (build already warm).

## Options

| Option / variable | Effect |
|---|---|
| `--build-only` | Compile only. This is the fast loop for fixing compile errors. |
| `--skip-build` | Run only the data and editor steps against an already built editor. |
| `UE_ROOT=/path/to/UE_5.8` | Engine location. The default is `/Users/Shared/Epic Games/UE_5.8`. |
| `UE_EDITOR=/path/to/UnrealEditor` | Use a different editor binary. |
| `PYTHON=python3.12` | Python used for `validate_data.py`. |

## Iterating with Claude on the Mac

The first compile (2026-09-27) needed three fixes, and the first editor runs surfaced script and engine-API issues; all are fixed and listed in `Docs/QA_Orsted.md` §3. A Claude session running on the Mac itself can run this script, read the summary, fix the code, and re-run until `RESULT: OK`. Two ways to start one:

- **Claude Desktop app:** open a Claude Code session on the repository folder.
- **Terminal:** run `claude remote-control` in the repository folder. The session then also shows up in the Claude Code app.

A good prompt: *"Run `Tools/mac/build_and_setup.sh`, fix every compile error, FAIL line and failed test, and re-run until `RESULT: OK`."*

A cloud session cannot reach the Mac, so it cannot do this step.

## Notes

- **glTF import** uses the engine's Interchange glTF importer, which is enabled by default in UE 5.x. If `mt_rudeus.log` says `no SkeletalMesh produced by the glTF import`, enable *Edit > Plugins > Interchange* (glTF) and re-run with `--skip-build`.
- **Re-running is safe.** Each run deletes what the previous run generated for a character (the Interchange import folder, `SK_<C>`, the `A_<C>_*` clips), garbage-collects, and imports fresh, because the Interchange importer does not re-import onto the renamed `SK_<C>`. Hand-made assets in the character folder (an Animation Blueprint, a tuned physics asset) are kept.
- **Sockets** are created through `UMTEditorScriptingLibrary::AddOrUpdateSkeletalMeshSocket` (C++): UE 5.8's Python cannot set a socket's name or bone.
- **Crash reports are off** (`-nocrashreports`) for every headless editor call: a crash reporter would keep the log pipe open and hang the script. A crash still prints its callstack into the step's log.
- **The world step ran headless** on 5.8.3 (the Open World template map, World Partition, HLOD layer, data layers).
- **MANUAL lines are not failures.** They mark steps that this engine version's Python API couldn't automate. Example: the optional IK Rig / Retargeter setup, whose API changed in UE 5.6+. Retargeting isn't needed, because the clips are authored on Rudeus's own skeleton.
- **If the world step fails headless** on another engine version, run `mt_world_setup.py` once from the editor (*Tools > Execute Python Script*).
