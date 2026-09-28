#!/bin/bash
# Generates every PCG component of the LA PLACE map (road dressing from Content/Python/mt_build_world.py stage "pcg")
# with the PCG World Partition builder, and saves the results. Run after the map exists, before build_hlods.sh.
#   Tools/mac/build_pcg.sh [/Game/Maps/L_LaPlace]
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
MAP="${1:-/Game/Maps/L_LaPlace}"
LOG="$ROOT/Saved/Logs/mt_pcg.log"
mkdir -p "$ROOT/Saved/Logs"
"$EDITOR" "$ROOT/MushokuRPG.uproject" "$MAP" -run=WorldPartitionBuilderCommandlet -Builder=PCGWorldPartitionBuilder \
	-AllowCommandletRendering -unattended -nosplash -nocrashreports -stdout -FullStdOutLogOutput > "$LOG" 2>&1
CODE=$?
grep -E "LogPCGWorldPartitionBuilder|LogPCG: (Error|Warning)|Error:|error:" "$LOG" | grep -v "LogInit\|LogConfig" | tail -30
echo "exit=$CODE log=$LOG"
exit $CODE
