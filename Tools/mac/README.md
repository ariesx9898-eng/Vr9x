# macOS one-command build and setup (UE 5.8, Apple Silicon)

`Tools/mac/build_and_setup.sh` takes a fresh clone to a built editor with Rudeus imported and animated. You don't need to open the editor.

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
| orsted | `Content/Python/mt_setup_orsted.py`: the same steps for `SourceArt/Characters/Orsted/Orsted_Animated.glb`. It only runs once `Tools/anim/make_orsted.sh` has produced that file; otherwise it is skipped with a note. | `mt_orsted.log` |
| world | `Content/Python/mt_world_setup.py` | `mt_world.log` |
| validation | `Content/Python/mt_validate_world.py` | `mt_validation.log` |

Each editor step runs headless:

```
UnrealEditor.app/Contents/MacOS/UnrealEditor <uproject> -run=pythonscript -script=<abs script> \
  -unattended -nosplash -nullrhi -stdout -FullStdOutLogOutput
```

At the end, the script prints a summary built from the logs. The summary shows:

- compiler errors (`file:line: error:`)
- UE `Error:` lines
- validator `ERROR:` lines
- every `PASS` / `FAIL` / `MANUAL` line the setup scripts print

It exits `0` (`RESULT: OK`) only when the build and every step succeeded and no `FAIL` or Python error line was logged.

## Options

| Option / variable | Effect |
|---|---|
| `--build-only` | Compile only. This is the fast loop for fixing compile errors. |
| `--skip-build` | Run only the data and editor steps against an already built editor. |
| `UE_ROOT=/path/to/UE_5.8` | Engine location. The default is `/Users/Shared/Epic Games/UE_5.8`. |
| `UE_EDITOR=/path/to/UnrealEditor` | Use a different editor binary. |
| `PYTHON=python3.12` | Python used for `validate_data.py`. |

## Iterating with Claude on the Mac

Nothing in this repository has been compiled yet, because it was written in a cloud container with no Unreal Engine. The first build will almost certainly report errors. A Claude session running on the Mac itself can run this script, read the summary, fix the code, and re-run until `RESULT: OK`. Two ways to start one:

- **Claude Desktop app:** open a Claude Code session on the repository folder.
- **Terminal:** run `claude remote-control` in the repository folder. The session then also shows up in the Claude Code app.

A good prompt: *"Run `Tools/mac/build_and_setup.sh --build-only`, fix the compile errors in `Source/`, repeat until it builds. Then run it without flags and fix every FAIL line."*

A cloud session cannot reach the Mac, so it cannot do this step.

## Notes

- **glTF import** uses the engine's Interchange glTF importer, which is enabled by default in UE 5.x. If `mt_rudeus.log` says `no SkeletalMesh produced by the glTF import`, enable *Edit > Plugins > Interchange* (glTF) and re-run with `--skip-build`.
- **Re-running is safe.** Re-running re-imports the GLB, replaces `SK_Rudeus` and the `A_Rudeus_*` clips in place, and deletes leftover redirectors.
- **MANUAL lines are not failures.** They mark steps that this engine version's Python API couldn't automate. Example: the optional IK Rig / Retargeter setup, whose API changed in UE 5.6+. Retargeting isn't needed, because the clips are authored on Rudeus's own skeleton.
- **World steps may need the editor UI.** `mt_world_setup.py` creates a World Partition map from the Open World template. If that step fails headless, run it once from the editor (*Tools > Execute Python Script*).
