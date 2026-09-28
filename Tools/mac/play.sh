#!/bin/bash
# Starts the game in its own window, without the editor. The "LA PLACE" desktop icon runs this.
#   Tools/mac/play.sh                 arena: you play Rudeus against an AI Orsted
#   Tools/mac/play.sh --as Orsted     arena: you play Orsted against an AI Rudeus
#   Tools/mac/play.sh --explore       the open world with no arena opponent
#   Tools/mac/play.sh --editor        opens the project in the Unreal editor instead
#   Tools/mac/play.sh --make-icon     (re)creates the "LA PLACE" launcher on the Desktop
# UE_ROOT=/path/to/UE_5.8 overrides the engine location.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
EDITOR_APP="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app"
MAP="/Game/Maps/L_Fittoa"
AS="Rudeus"
MODE="arena"

make_icon() {
	local app="$HOME/Desktop/LA PLACE.app"
	local script="$ROOT/Tools/mac/play.sh"
	rm -rf "$app"
	osacompile -o "$app" <<EOF
set choices to {"Play as Rudeus (vs AI Orsted)", "Play as Orsted (vs AI Rudeus)", "Explore the world", "Open in the Unreal editor"}
set picked to choose from list choices with title "LA PLACE" with prompt "Mushoku Tensei RPG: how do you want to play?" default items {item 1 of choices} OK button name "Play" cancel button name "Cancel"
if picked is false then return
set picked to item 1 of picked
if picked starts with "Play as Orsted" then
	set args to "--as Orsted"
else if picked starts with "Explore" then
	set args to "--explore"
else if picked starts with "Open in" then
	set args to "--editor"
else
	set args to "--as Rudeus"
end if
do shell script quoted form of "$script" & " " & args & " >> \$HOME/Library/Logs/MushokuRPG/launcher.log 2>&1 &"
EOF
	local icon="$EDITOR_APP/Contents/Resources/AppIcon.icns"
	if [ -f "$ROOT/Tools/mac/LaPlace.icns" ]; then
		icon="$ROOT/Tools/mac/LaPlace.icns"
	fi
	if [ -f "$icon" ]; then
		# The applet's asset catalog would win over applet.icns, so drop it and its plist key.
		cp "$icon" "$app/Contents/Resources/applet.icns"
		rm -f "$app/Contents/Resources/Assets.car"
		/usr/libexec/PlistBuddy -c "Delete :CFBundleIconName" "$app/Contents/Info.plist" 2>/dev/null || true
		codesign --force --deep --sign - "$app" 2>/dev/null || true
		touch "$app"
	fi
	echo "Created $app"
}

while [ $# -gt 0 ]; do
	case "$1" in
		--as) AS="${2:-Rudeus}"; shift 2 ;;
		--explore) MODE="explore"; shift ;;
		--editor) MODE="editor"; shift ;;
		--make-icon) make_icon; exit 0 ;;
		-h|--help) sed -n '2,9p' "$0"; exit 0 ;;
		*) echo "unknown option: $1" >&2; exit 2 ;;
	esac
done

mkdir -p "$HOME/Library/Logs/MushokuRPG"
if [ ! -d "$EDITOR_APP" ]; then
	echo "Unreal Engine 5.8 was not found at $UE_ROOT (set UE_ROOT)." >&2
	exit 1
fi
if [ ! -f "$ROOT/Binaries/Mac/libUnrealEditor-MushokuRPG.dylib" ] || [ ! -f "$ROOT/Content/Maps/L_Fittoa.umap" ]; then
	echo "First run: building the game and importing its content with Tools/mac/build_and_setup.sh. This takes a while."
	"$ROOT/Tools/mac/build_and_setup.sh"
fi

case "$MODE" in
	editor) exec open -n -a "$EDITOR_APP" --args "$ROOT/MushokuRPG.uproject" ;;
	explore) URL="$MAP" ;;
	*) URL="$MAP?game=/Script/MushokuRPG.MTTestArenaGameMode?PlayAs=$AS" ;;
esac
echo "$(date '+%F %T') starting $URL"
exec open -n -a "$EDITOR_APP" --args "$ROOT/MushokuRPG.uproject" "$URL" -game -windowed -ResX=1600 -ResY=900
