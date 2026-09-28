#!/usr/bin/env bash
# One command from the generated Orsted mesh to an animated, UE-ready GLB on the shared skeleton.
#   Tools/anim/make_orsted.sh path/to/orsted_meshy.glb
# Needs: python3 with bpy (pip install bpy), numpy, pillow.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IN="${1:?usage: make_orsted.sh <orsted.glb>}"
OUT_DIR="$ROOT/SourceArt/Characters/Orsted"
mkdir -p "$OUT_DIR" "$ROOT/Docs/Images/Animation"
cp "$IN" "$OUT_DIR/Orsted_Source.glb"
echo "== 1/3 re-rig onto Rudeus's skeleton (195 cm)"
python3 "$ROOT/Tools/anim/rig_to_rudeus_skeleton.py" "$OUT_DIR/Orsted_Source.glb" "$OUT_DIR/Orsted_Rigged.glb" \
  --height 1.95 --qa-sheet "$ROOT/Docs/Images/Animation/Orsted_ReRig_QA.png"
echo "== 2/3 author Orsted's clips on his own proportions"
python3 "$ROOT/Tools/anim/build_rudeus_anims.py" --character Orsted --sheets
echo "== 3/3 independent verification"
python3 "$ROOT/Tools/anim/verify_glb.py" "$OUT_DIR/Orsted_Animated.glb" --source "$OUT_DIR/Orsted_Rigged.glb" \
  --clips A_Orsted_Walk A_Orsted_DisturbMagic A_Orsted_DragonStep --sheet "$ROOT/Docs/Images/Animation/Verify_Orsted.png"
