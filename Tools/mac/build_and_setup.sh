#!/usr/bin/env bash
# One command on macOS (Apple Silicon): build the editor target, then run every editor setup script headless.
#
#   Tools/mac/build_and_setup.sh                 # build + data check + materials + Rudeus (+ Orsted) + world + validation + tests
#   Tools/mac/build_and_setup.sh --skip-build    # only the setup scripts (editor already built)
#   Tools/mac/build_and_setup.sh --build-only    # only compile (fast compile-error loop)
#
# Environment:
#   UE_ROOT    engine install (default "/Users/Shared/Epic Games/UE_5.8")
#   UE_EDITOR  editor binary override (default $UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor)
#   PYTHON     python3 used for the offline data check (default python3)
#
# Logs: Saved/Logs/mt_build.log, mt_data.log and mt_<step>.log. The summary at the end greps error / FAIL / PASS lines.
# Exit code: 0 only if the build and every step succeeded and no FAIL / Python error line was logged.
# Written for the bash 3.2 that ships with macOS (no empty-array expansion under `set -u`).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
UPROJECT="$REPO_ROOT/MushokuRPG.uproject"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
UE_EDITOR="${UE_EDITOR:-$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
PYTHON="${PYTHON:-python3}"
LOG_DIR="$REPO_ROOT/Saved/Logs"

DO_BUILD=1
DO_SETUP=1
for arg in "$@"; do
  case "$arg" in
    --skip-build) DO_BUILD=0 ;;
    --build-only) DO_SETUP=0 ;;
    -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg (see --help)" >&2; exit 2 ;;
  esac
done

mkdir -p "$LOG_DIR"
FAILED_STEPS=""          # space-separated step names (plain string: bash 3.2 friendly)
LOG_FILES=""             # newline-separated log paths, in run order

add_log() { LOG_FILES="${LOG_FILES}$1"$'\n'; }
fail_step() { FAILED_STEPS="${FAILED_STEPS}${FAILED_STEPS:+, }$1"; }

# Runs "$@" with its output tee'd into the log given as first argument; returns the command's exit code.
run_logged() {
  local log="$1"
  shift
  add_log "$log"
  set +e
  "$@" 2>&1 | tee "$log"
  local status=${PIPESTATUS[0]}
  set -e
  return "$status"
}

if [[ ! -f "$UPROJECT" ]]; then
  echo "ERROR: $UPROJECT not found" >&2
  exit 2
fi

# ---------------------------------------------------------------------------------------------------- build
if [[ $DO_BUILD -eq 1 ]]; then
  if [[ ! -x "$BUILD_SH" ]]; then
    echo "ERROR: $BUILD_SH not found. Install UE 5.8 or set UE_ROOT=/path/to/UE_5.8" >&2
    exit 2
  fi
  echo "== build: MushokuRPGEditor Mac Development (log: Saved/Logs/mt_build.log)"
  if ! run_logged "$LOG_DIR/mt_build.log" \
      "$BUILD_SH" MushokuRPGEditor Mac Development -Project="$UPROJECT" -WaitMutex; then
    fail_step "build"
    echo "== build FAILED: editor steps skipped. Fix the errors in the summary below and re-run."
    DO_SETUP=0
  fi
fi

# ---------------------------------------------------------------------------------------------------- setup
if [[ $DO_SETUP -eq 1 ]]; then
  # Offline data validation first (no Unreal needed; catches JSON typos before the editor loads them).
  echo "== data: Tools/validate_data.py (log: Saved/Logs/mt_data.log)"
  run_logged "$LOG_DIR/mt_data.log" "$PYTHON" "$REPO_ROOT/Tools/validate_data.py" || fail_step "data"

  if [[ ! -x "$UE_EDITOR" ]]; then
    echo "ERROR: editor binary not found: $UE_EDITOR (set UE_ROOT or UE_EDITOR)" >&2
    fail_step "editor-binary-missing"
  else
    # LA PLACE generated sources (only when missing; each generator is deterministic): effect textures, UI art from the
    # committed Higgsfield originals, synthesized audio, the Blender kits and the world terrain.
    BPY="${BPY:-$HOME/.venvs/mushoku-bpy311/bin/python}"
    if [[ -x "$BPY" ]]; then
      gen() { # <marker file> <script> [args]
        local marker="$REPO_ROOT/$1"; shift
        if [[ ! -e "$marker" ]]; then
          echo "== generate: $*"
          run_logged "$LOG_DIR/mt_generate.log" "$BPY" "$REPO_ROOT/$@" || fail_step "generate:$(basename "$1")"
        fi
      }
      # Markers are the newest file each generator makes, so a machine that ran an older generator still gets the new assets.
      gen SourceArt/VFX/Textures/T_VFX_AimLine.png Tools/vfx/make_vfx_textures.py
      gen SourceArt/UI/Icons Tools/ui/prepare_ui_art.py
      gen SourceArt/Audio/Rudeus/barrage_finale.wav Tools/audio/synth_sfx.py
      gen SourceArt/Kit/VFX/SM_VFX_Cone.glb Tools/kit/build_architecture_kit.py
      gen SourceArt/Kit/Trees/SM_Tree_Oak_A.glb Tools/kit/build_nature_kit.py
      [[ -f "$REPO_ROOT/Tools/world/generate_world.py" ]] && gen SourceArt/World/Height.r16 Tools/world/generate_world.py
      # World content after the terrain: city layouts (streets, buildings), the final paint layers + macro maps, the
      # nature scatter, and the terrain / kit textures (from the committed Higgsfield originals).
      [[ -f "$REPO_ROOT/Tools/world/generate_cities.py" ]] && gen SourceArt/World/Scatter/Cities.json Tools/world/generate_cities.py
      gen SourceArt/World/Macro/GrassMix.png Tools/world/finalize_world.py
      gen SourceArt/World/Scatter/Foliage.json Tools/world/scatter_foliage.py
      [[ -f "$REPO_ROOT/Tools/textures/build_textures.py" ]] && gen SourceArt/Textures/Terrain/T_Ground_Grass_D.png Tools/textures/build_textures.py
    else
      echo "== generate: skipped (no Blender/numpy venv at $BPY; see README)"
    fi

    # Editor steps, in order: <log name>:<script in Content/Python>
    STEPS="materials:mt_create_materials.py laplace:mt_setup_laplace.py rudeus:mt_setup_rudeus.py"
    # Orsted's animated GLB exists once Tools/anim/make_orsted.sh has run on his generated mesh.
    if [[ -f "$REPO_ROOT/SourceArt/Characters/Orsted/Orsted_Animated.glb" ]]; then
      STEPS="$STEPS orsted:mt_setup_orsted.py"
    else
      echo "== orsted: skipped (no SourceArt/Characters/Orsted/Orsted_Animated.glb yet; see Tools/anim/make_orsted.sh)"
    fi
    STEPS="$STEPS world:mt_world_setup.py kit:mt_setup_kit.py"
    for step in $STEPS; do
      name="${step%%:*}"
      script="$REPO_ROOT/Content/Python/${step#*:}"
      log="$LOG_DIR/mt_${name}.log"
      echo "== $name: $(basename "$script") (log: Saved/Logs/mt_${name}.log)"
      if [[ ! -f "$script" ]]; then
        echo "ERROR: $script not found" | tee "$log"
        add_log "$log"
        fail_step "$name"
        continue
      fi
      run_logged "$log" "$UE_EDITOR" "$UPROJECT" -run=pythonscript -script="$script" \
        -unattended -nosplash -nullrhi -nocrashreports -stdout -FullStdOutLogOutput || fail_step "$name"
    done

    # LA PLACE world map (landscape import needs the GPU: the landscape edit layers are merged by the renderer).
    if [[ -f "$REPO_ROOT/SourceArt/World/Height.r16" ]]; then
      log="$LOG_DIR/mt_laplace_world.log"
      echo "== laplace world: mt_build_world.py (log: Saved/Logs/mt_laplace_world.log)"
      fresh=0
      [[ -f "$REPO_ROOT/Content/Maps/L_LaPlace.umap" ]] || fresh=1
      MT_WORLD_FRESH=$fresh run_logged "$log" "$UE_EDITOR" "$UPROJECT" -run=pythonscript -script="$REPO_ROOT/Content/Python/mt_build_world.py" \
        -unattended -nosplash -AllowCommandletRendering -nocrashreports -stdout -FullStdOutLogOutput || fail_step "laplace-world"
      echo "== laplace HLODs: Tools/mac/build_hlods.sh (log: Saved/Logs/mt_hlods.log)"
      add_log "$LOG_DIR/mt_hlods.log"
      "$SCRIPT_DIR/build_hlods.sh" || fail_step "laplace-hlods"
    fi

    # World validation on the saved map with every World Partition actor loaded (overlaps, duplicates, coplanar
    # geometry, broken references). The in-editor mt_validate_world.py only sees whatever level is open.
    echo "== validation: MTValidateWorld commandlet on /Game/Maps/L_Fittoa (log: Saved/Logs/mt_validation.log)"
    run_logged "$LOG_DIR/mt_validation.log" "$UE_EDITOR" "$UPROJECT" -run=MTValidateWorld -map=/Game/Maps/L_Fittoa \
      -unattended -nosplash -nullrhi -nocrashreports -stdout -FullStdOutLogOutput || fail_step "validation"

    # Runtime integration tests (Rudeus + Orsted: data, body and pose, lock-on facing, signature abilities).
    add_log "$LOG_DIR/mt_tests.log"
    "$SCRIPT_DIR/run_automation_tests.sh" || fail_step "tests"
  fi
fi

# ---------------------------------------------------------------------------------------------------- summary
echo
echo "================================================================ summary"
# Compiler / UBT errors, UE "Error:" lines, validator ERROR lines and the PASS / FAIL / MANUAL lines of the scripts.
PATTERN=': (fatal )?error:|error [A-Z]+[0-9]+:|[Ee]rror: |ERROR: |(^|[^A-Za-z])(PASS|FAIL|MANUAL) |ALL CHECKS PASSED|ERROR\(S\) FOUND|MTValidateWorld: [0-9]+ errors|Result=\{(Success|Fail)\}'
PROBLEMS=0
while IFS= read -r log; do
  [[ -n "$log" && -f "$log" ]] || continue
  echo "--- $(basename "$log")"
  matches="$(grep -E "$PATTERN" "$log" | head -n 200 || true)"
  if [[ -n "$matches" ]]; then
    echo "$matches"
  else
    echo "(no error / PASS / FAIL lines)"
  fi
  # Failures that do not change a process exit code: script FAIL lines and Python errors/tracebacks.
  count="$(grep -cE '(^|[^A-Za-z])FAIL |LogPython: Error:|Result=\{Fail\}' "$log" || true)"
  PROBLEMS=$((PROBLEMS + ${count:-0}))
done <<EOF
$LOG_FILES
EOF
echo "----------------------------------------------------------------"
[[ -z "$FAILED_STEPS" ]] || echo "FAILED steps: $FAILED_STEPS"
echo "FAIL / Python error lines: $PROBLEMS"
if [[ -z "$FAILED_STEPS" && $PROBLEMS -eq 0 ]]; then
  echo "RESULT: OK"
  exit 0
fi
echo "RESULT: needs attention (see the lines above and Saved/Logs/mt_*.log)"
exit 1
