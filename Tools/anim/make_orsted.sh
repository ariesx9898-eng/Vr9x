#!/usr/bin/env bash
# One command from the generated Orsted mesh to an animated, UE-ready GLB on the shared skeleton.
#   Tools/anim/make_orsted.sh path/to/orsted_generated.glb
# Needs: Python 3.11 with bpy 5.0.1 (pip install bpy==5.0.1), numpy, pillow.
#   PYTHON=/path/to/python3.11 overrides the interpreter (default: python3). Unreal ships a Python 3.11 that works
#   as a base: "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Mac/bin/python3.11" -m venv ...
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IN="${1:?usage: make_orsted.sh <orsted.glb>}"
PY="${PYTHON:-python3}"
OUT_DIR="$ROOT/SourceArt/Characters/Orsted"
IMG_DIR="$ROOT/Docs/Images/Animation"
if ! "$PY" -c "import bpy" 2>/dev/null; then
  echo "ERROR: $PY cannot import bpy. Use Python 3.11 with 'pip install bpy==5.0.1 numpy pillow' (set PYTHON=...)." >&2
  exit 2
fi
mkdir -p "$OUT_DIR" "$IMG_DIR"
[[ "$(cd "$(dirname "$IN")" && pwd)/$(basename "$IN")" == "$OUT_DIR/Orsted_Source.glb" ]] || cp "$IN" "$OUT_DIR/Orsted_Source.glb"
echo "== 1/5 re-rig onto Rudeus's skeleton (195 cm; A-pose straightened, knees and coat refitted)"
"$PY" "$ROOT/Tools/anim/rig_to_rudeus_skeleton.py" "$OUT_DIR/Orsted_Source.glb" "$OUT_DIR/Orsted_Rigged.glb" \
  --height 1.95 --atlas-out "$OUT_DIR/Orsted_Atlas.png" --qa-sheet "$IMG_DIR/Orsted_ReRig_QA.png"
echo "== 2/5 author Orsted's clips on his own proportions"
"$PY" "$ROOT/Tools/anim/build_rudeus_anims.py" --character Orsted --sheets
echo "== 3/5 independent verification"
"$PY" "$ROOT/Tools/anim/verify_glb.py" "$OUT_DIR/Orsted_Animated.glb" --source "$OUT_DIR/Orsted_Rigged.glb" \
  --clips A_Orsted_Walk A_Orsted_DisturbMagic A_Orsted_DragonStep --sheet "$IMG_DIR/Verify_Orsted.png"
echo "== 4/5 motion quality on the skinned mesh, every frame"
"$PY" "$ROOT/Tools/anim/qa_animation_quality.py" --character Orsted --json "$ROOT/Docs/QA_Animation_Metrics_Orsted.json"
echo "== 5/5 point Orsted's data at his own clips, then validate"
"$PY" "$ROOT/Tools/anim/use_character_clips.py" Orsted
"$PY" "$ROOT/Tools/validate_data.py"
echo "Done. Next, in Unreal: Tools/mac/build_and_setup.sh (imports Orsted_Animated.glb via mt_setup_orsted.py)."
