#!/bin/bash
# Photographs the world (UMTWorldTourSubsystem) and waits for the game to quit. Screenshots land in
# Saved/Screenshots/WorldTour (cleared first).
#   Tools/mac/world_tour.sh                                  every spawn location + aerial views of L_LaPlace
#   Tools/mac/world_tour.sh /Game/Maps/L_Fittoa              another map
#   TOUR_OPTS="?TourOnly=Buena,Roa?TourAerial=0" Tools/mac/world_tour.sh
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
MAP="${1:-/Game/Maps/L_LaPlace}"
OUT="$ROOT/Saved/Screenshots/WorldTour"
rm -rf "$OUT"
mkdir -p "$OUT" "$ROOT/Saved/Logs"
"$EDITOR" "$ROOT/MushokuRPG.uproject" "$MAP?Menu=0?WorldTour=1${TOUR_OPTS:-}" -game -windowed -ResX=1920 -ResY=1080 -nosplash \
	-unattended -log -abslog="$ROOT/Saved/Logs/world_tour.log" > /dev/null 2>&1
echo "exit=$? shots:"
ls "$OUT"
grep -E "\[WorldTour\]|Error:" "$ROOT/Saved/Logs/world_tour.log" | tail -20
