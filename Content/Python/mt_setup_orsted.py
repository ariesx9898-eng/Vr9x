"""Editor automation for Orsted: the same steps as mt_setup_rudeus.py, for
SourceArt/Characters/Orsted/Orsted_Animated.glb (made by Tools/anim/make_orsted.sh), imported into
/Game/Characters/Orsted as SK_Orsted with every A_Orsted_<Key> clip in /Game/Characters/Orsted/Animations, and each clip's
gameplay events (Orsted_Animated.anim.json "events": his earlier Release times) added as MTAnimNotify_Event notifies.

Run headless (Tools/mac/build_and_setup.sh does this when the GLB exists):
    UnrealEditor MushokuRPG.uproject -run=pythonscript -script=<abs path to this file> -unattended -nullrhi
or inside the editor: `py mt_setup_orsted.py` in the Output Log.
"""
import os
import runpy

import unreal

os.environ["MT_CHARACTER"] = "Orsted"
_here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Python")
runpy.run_path(os.path.join(_here, "mt_setup_rudeus.py"), run_name="__main__")
