#!/bin/bash
# Runs one Content/Python script in a headless editor and prints its log lines and errors.
#   Tools/mac/run_editor_python.sh mt_setup_laplace.py
#   RENDER=1 Tools/mac/run_editor_python.sh mt_build_world.py   (GPU available: landscape edit layers, HLODs)
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# One content-changing tool at a time (see mt_lock.sh); play.sh waits for it too.
# shellcheck source=mt_lock.sh
. "$ROOT/Tools/mac/mt_lock.sh"
mt_lock_acquire "run_editor_python.sh $1" || exit 3
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
# The console build of the editor: a commandlet started from the UnrealEditor app opens a log window, and closing
# that window kills the run.
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
SCRIPT="$ROOT/Content/Python/$1"
NAME="$(basename "$1" .py)"
LOG="$ROOT/Saved/Logs/py_${NAME}.log"
mkdir -p "$ROOT/Saved/Logs"
if [[ "${RENDER:-0}" == "1" ]]; then RHI="-AllowCommandletRendering"; else RHI="-nullrhi"; fi
"$EDITOR" "$ROOT/MushokuRPG.uproject" -run=pythonscript -script="$SCRIPT" -unattended -nosplash $RHI -nocrashreports -stdout -FullStdOutLogOutput > "$LOG" 2>&1
CODE=$?
grep -E "\[$NAME\]|Error|error:|Traceback|LogPython: Error" "$LOG" | grep -v -E "LogInit|LogConfig|0 error" | tail -60
echo "exit=$CODE log=$LOG"
exit $CODE
