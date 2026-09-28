#!/bin/bash
# Runs one Content/Python script in a headless editor and prints its log lines and errors.
#   Tools/mac/run_editor_python.sh mt_setup_laplace.py
#   RENDER=1 Tools/mac/run_editor_python.sh mt_build_world.py   (GPU available: landscape edit layers, HLODs)
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
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
