#!/bin/bash
# Builds the World Partition HLODs of the LA PLACE map (instanced Nanite stand-ins for streamed-out cities and
# forests, so they stay visible across the world). Run after Content/Python/mt_build_world.py.
#   Tools/mac/build_hlods.sh [/Game/Maps/L_LaPlace]
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
MAP="${1:-/Game/Maps/L_LaPlace}"
LOG="$ROOT/Saved/Logs/mt_hlods.log"
mkdir -p "$ROOT/Saved/Logs"
"$EDITOR" "$ROOT/MushokuRPG.uproject" "$MAP" -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionHLODsBuilder \
	-AllowCommandletRendering -unattended -nosplash -nocrashreports -stdout -FullStdOutLogOutput > "$LOG" 2>&1
CODE=$?
grep -E "LogWorldPartitionHLODsBuilder|HLOD.*(built|Built|actors)|Error:|error:" "$LOG" | grep -v "LogInit\|LogConfig" | tail -30
echo "exit=$CODE log=$LOG"
exit $CODE
