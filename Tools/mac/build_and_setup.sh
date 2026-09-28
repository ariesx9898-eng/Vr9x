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
    # Editor steps, in order: <log name>:<script in Content/Python>
    STEPS="materials:mt_create_materials.py rudeus:mt_setup_rudeus.py"
    # Orsted's animated GLB exists once Tools/anim/make_orsted.sh has run on his generated mesh.
    if [[ -f "$REPO_ROOT/SourceArt/Characters/Orsted/Orsted_Animated.glb" ]]; then
      STEPS="$STEPS orsted:mt_setup_orsted.py"
    else
      echo "== orsted: skipped (no SourceArt/Characters/Orsted/Orsted_Animated.glb yet; see Tools/anim/make_orsted.sh)"
    fi
    STEPS="$STEPS world:mt_world_setup.py"
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
