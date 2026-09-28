"""Editor automation for a lineage's animated character (UE 5.3-5.8 editor Python). Idempotent: safe to re-run.
Rudeus by default; mt_setup_orsted.py runs the same steps for Orsted (environment variable MT_CHARACTER).

Run headless (Tools/mac/build_and_setup.sh does this):
    UnrealEditor MushokuRPG.uproject -run=pythonscript -script=<abs path to this file> -unattended -nullrhi
or inside the editor: Tools > Execute Python Script, or `py mt_setup_rudeus.py` in the Output Log.

Steps (each logs PASS / FAIL, so problems are greppable in the log)
  1. Import SourceArt/Characters/Rudeus/Rudeus_Animated.glb (skinned mesh + skeleton + every A_Rudeus_<Key> clip)
     into /Game/Characters/Rudeus; the mesh ends up as SK_Rudeus. Falls back to the mesh-only Rudeus_Greyrat_UE.glb.
  2. Move/rename every imported AnimSequence to /Game/Characters/Rudeus/Animations/A_Rudeus_<Key> (matched on the
     "A_Rudeus_<Key>" part of its name) and PASS/FAIL every key Content/Data/AnimSets.json expects. Loop clips get
     their Loop flag; lengths are checked against the exporter sidecar Rudeus_Animated.anim.json when present.
  3. Toon material instance and gameplay sockets (hand_r / hand_l / foot_l / foot_r).
  4. Optional: IK Rig + Retargeter + batch retarget of Mixamo clips. Not needed any more (the clips are authored on
     Rudeus's own skeleton); only runs when a Mixamo Y Bot has been imported, and every call is guarded because the
     IK Rig / Retargeter Python API changed in UE 5.6+.
"""
import json
import os
import re

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
# Same steps for every lineage: mt_setup_orsted.py sets MT_CHARACTER=Orsted and runs this file.
CHARACTER = os.environ.get("MT_CHARACTER", "Rudeus")
ART_DIR = os.path.join(PROJECT_DIR, "SourceArt", "Characters", CHARACTER)
GLB_ANIMATED = os.path.join(ART_DIR, CHARACTER + "_Animated.glb")
GLB_MESH_ONLY = os.path.join(ART_DIR, "Rudeus_Greyrat_UE.glb" if CHARACTER == "Rudeus" else CHARACTER + "_Rigged.glb")
SIDECAR = os.path.join(ART_DIR, CHARACTER + "_Animated.anim.json")
ANIMSETS_JSON = os.path.join(PROJECT_DIR, "Content", "Data", "AnimSets.json")
DEST = "/Game/Characters/" + CHARACTER
MESH_NAME = "SK_" + CHARACTER
TAG = "[mt_setup_%s]" % CHARACTER.lower()
# Expected imported height (cm): Rudeus 161.7 (normalised source), Orsted ~195 (Tools/anim/make_orsted.sh --height 1.95).
HEIGHT_RANGE = {"Rudeus": (150.0, 175.0), "Orsted": (185.0, 205.0)}.get(CHARACTER, (120.0, 230.0))
ANIM_DEST = DEST + "/Animations"
CLIP_PREFIX = "A_%s_" % CHARACTER
MIXAMO_SRC = "/Game/Animation/Mixamo/Source"
MIXAMO_MESH = "/Game/Animation/Mixamo/SK_Mixamo_YBot"

# Used when AnimSets.json cannot be read. TurnLeft90 / TurnRight90 are optional (not authored yet).
REQUIRED_KEYS = ["Idle", "CombatIdle", "Walk", "WalkBack", "StrafeLeft", "StrafeRight", "Run", "Sprint",
                 "RunStrafeLeft", "RunStrafeRight", "RunBack", "Rise",
                 "Fall", "JumpStart", "Land", "HardLand", "DodgeForward", "DodgeBack", "DodgeLeft", "DodgeRight",
                 "HitFront", "HitBack", "HitLeft", "HitRight", "Stagger", "Knockdown", "Death", "CastBasic",
                 "StoneCannon_Charge", "StoneCannon_Hold", "StoneCannon_Release", "Quagmire", "Barrage", "DemonEye",
                 "Awakening", "CastTwoHand", "CastGround"]
OPTIONAL_KEYS = ["TurnLeft90", "TurnRight90"]
LOOP_KEYS = {"Idle", "CombatIdle", "Walk", "WalkBack", "StrafeLeft", "StrafeRight", "Run", "Sprint", "RunStrafeLeft",
             "RunStrafeRight", "RunBack", "Rise", "Fall", "StoneCannon_Hold"}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# Bone names come straight from the GLB joint names (see Docs/Phase0_Audit.md).
RUDEUS_CHAINS = [
    # (chain, start, end, goal or None)
    ("Root", "_rootJoint", "_rootJoint", None),
    ("Spine", "spine_C0_1_jnt_061", "spine_C0_3_jnt_063", None),
    ("Neck", "neck_C0_0_jnt_066", "neck_C0_0_jnt_066", None),
    ("Head", "head_C0_0_jnt_067", "head_C0_0_jnt_067", None),
    ("LeftClavicle", "shoulder_L0_0_jnt_091", "shoulder_L0_0_jnt_091", None),
    ("LeftArm", "arm_L0_0_jnt_092", "arm_L0_2_jnt_094", "LeftHandGoal"),
    ("RightClavicle", "shoulder_R0_0_jnt_0114", "shoulder_R0_0_jnt_0114", None),
    ("RightArm", "arm_R0_0_jnt_0115", "arm_R0_2_jnt_0117", "RightHandGoal"),
    ("LeftLeg", "leg_L0_0_jnt_08", "leg_L0_2_jnt_010", "LeftFootGoal"),
    ("LeftToe", "leg_L0_3_jnt_011", "leg_L0_3_jnt_011", None),
    ("RightLeg", "leg_R0_0_jnt_013", "leg_R0_2_jnt_015", "RightFootGoal"),
    ("RightToe", "leg_R0_3_jnt_016", "leg_R0_3_jnt_016", None),
    ("LeftThumb", "finger_L0_0_jnt_096", "finger_L0_2_jnt_098", None),
    ("LeftIndex", "finger_L1_0_jnt_099", "finger_L1_2_jnt_0101", None),
    ("LeftMiddle", "finger_L2_0_jnt_0102", "finger_L2_2_jnt_0104", None),
    ("LeftRing", "finger_L3_0_jnt_0105", "finger_L3_2_jnt_0107", None),
    ("LeftPinky", "finger_L4_0_jnt_0108", "finger_L4_2_jnt_0110", None),
    ("RightThumb", "finger_R0_0_jnt_01", "finger_R0_2_jnt_0119", None),
    ("RightIndex", "finger_R1_0_jnt_0120", "finger_R1_2_jnt_0122", None),
    ("RightMiddle", "finger_R2_0_jnt_0123", "finger_R2_2_jnt_0125", None),
    ("RightRing", "finger_R3_0_jnt_0126", "finger_R3_2_jnt_0128", None),
    ("RightPinky", "finger_R4_0_jnt_0129", "finger_R4_2_jnt_0131", None),
]
RUDEUS_PELVIS = "spine_C0_0_jnt_05"
UPPER_SPINE = "spine_C0_1_jnt_061"  # UMTNativeAnimInstance upper-body layer root (AnimSets.json UpperBodyRootBone)

MIXAMO_CHAINS = [
    ("Root", "Hips", "Hips", None),
    ("Spine", "Spine", "Spine2", None),
    ("Neck", "Neck", "Neck", None),
    ("Head", "Head", "Head", None),
    ("LeftClavicle", "LeftShoulder", "LeftShoulder", None),
    ("LeftArm", "LeftArm", "LeftHand", "LeftHandGoal"),
    ("RightClavicle", "RightShoulder", "RightShoulder", None),
    ("RightArm", "RightArm", "RightHand", "RightHandGoal"),
    ("LeftLeg", "LeftUpLeg", "LeftFoot", "LeftFootGoal"),
    ("LeftToe", "LeftToeBase", "LeftToeBase", None),
    ("RightLeg", "RightUpLeg", "RightFoot", "RightFootGoal"),
    ("RightToe", "RightToeBase", "RightToeBase", None),
    ("LeftThumb", "LeftHandThumb1", "LeftHandThumb3", None),
    ("LeftIndex", "LeftHandIndex1", "LeftHandIndex3", None),
    ("LeftMiddle", "LeftHandMiddle1", "LeftHandMiddle3", None),
    ("LeftRing", "LeftHandRing1", "LeftHandRing3", None),
    ("LeftPinky", "LeftHandPinky1", "LeftHandPinky3", None),
    ("RightThumb", "RightHandThumb1", "RightHandThumb3", None),
    ("RightIndex", "RightHandIndex1", "RightHandIndex3", None),
    ("RightMiddle", "RightHandMiddle1", "RightHandMiddle3", None),
    ("RightRing", "RightHandRing1", "RightHandRing3", None),
    ("RightPinky", "RightHandPinky1", "RightHandPinky3", None),
]

RESULTS = {"PASS": 0, "FAIL": 0}


def log(ok, msg):
    RESULTS["PASS" if ok else "FAIL"] += 1
    (unreal.log if ok else unreal.log_error)(("PASS " if ok else "FAIL ") + TAG + " " + msg)


def note(msg):
    unreal.log(TAG + " " + msg)


def manual(msg):
    """Something the script could not do on this engine version: needs a person in the editor."""
    unreal.log_warning("MANUAL " + TAG + " " + msg)


# ---------------------------------------------------------------------------------------------------------- helpers
def package_path(path):
    """'/Game/A/B.B' -> '/Game/A/B' (EditorAssetLibrary accepts both; package paths compare cleanly)."""
    path = str(path)
    return path.split(".", 1)[0] if "." in path.rsplit("/", 1)[-1] else path


def asset_name(path):
    return package_path(path).rsplit("/", 1)[-1]


def class_name(path):
    data = eal.find_asset_data(path)
    try:
        return str(data.asset_class_path.asset_name)  # UE 5.1+
    except Exception:
        return str(getattr(data, "asset_class", ""))


def assets_of_class(directory, cls, recursive=True):
    if not eal.does_directory_exist(directory):
        return []
    return [package_path(p) for p in eal.list_assets(directory, recursive=recursive, include_folder=False)
            if class_name(p) == cls]


def delete_asset(path):
    try:
        return bool(eal.delete_asset(path))
    except Exception as exc:
        unreal.log_warning(TAG + " could not delete %s: %s" % (path, exc))
        return False


def rename_asset(src, dst):
    try:
        if eal.rename_asset(src, dst):
            return True
    except Exception as exc:
        unreal.log_warning(TAG + " EditorAssetLibrary.rename_asset(%s): %s" % (src, exc))
    try:  # UE 5.x subsystem equivalent
        subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        return bool(subsystem.rename_asset(src, dst))
    except Exception as exc:
        unreal.log_warning(TAG + " EditorAssetSubsystem.rename_asset(%s): %s" % (src, exc))
    return False


def delete_redirectors(directory):
    """Moves leave ObjectRedirectors behind; drop them so the next import can reuse the names."""
    for path in assets_of_class(directory, "ObjectRedirector"):
        delete_asset(path)


def bone_names(skeletal_mesh):
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeletal_mesh.skeleton)
        return set(str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose))
    except Exception as exc:  # API differs between engine versions
        unreal.log_warning("Could not read bone names: %s" % exc)
        return None


def play_length(anim):
    for getter in (lambda a: a.get_play_length(),
                   lambda a: unreal.AnimationLibrary.get_sequence_length(a)):
        try:
            return float(getter(anim))
        except Exception:
            continue
    return None


def load_expected():
    """Keys this character's clips must exist for: every AnimSets.json entry under /Game/Characters/Rudeus/Animations
    (the Orsted row reuses them too). Falls back to the built-in list."""
    keys = set()
    try:
        rows = json.load(open(ANIMSETS_JSON, encoding="utf-8"))
        prefix = ANIM_DEST + "/" + CLIP_PREFIX
        for row in rows:
            for key, path in (row.get("Anims") or {}).items():
                if isinstance(path, str) and path.startswith(prefix):
                    keys.add(path[len(prefix):].split(".", 1)[0])
    except Exception as exc:
        unreal.log_warning(TAG + " %s unreadable (%s): using the built-in key list" % (ANIMSETS_JSON, exc))
    if not keys:
        keys = set(REQUIRED_KEYS)
    sidecar = {}
    if os.path.exists(SIDECAR):
        try:
            sidecar = json.load(open(SIDECAR, encoding="utf-8")).get("clips", {})
        except Exception as exc:
            unreal.log_warning(TAG + " %s unreadable: %s" % (SIDECAR, exc))
    optional = set(OPTIONAL_KEYS) - keys
    return sorted(keys), sorted(optional), sidecar


# ----------------------------------------------------------------------------------------------------------- import
def import_glb():
    glb = GLB_ANIMATED if os.path.exists(GLB_ANIMATED) else GLB_MESH_ONLY
    if not os.path.exists(glb):
        log(False, "no GLB found: expected %s (or the mesh-only %s)" % (GLB_ANIMATED, GLB_MESH_ONLY))
        return None, []
    animated = glb == GLB_ANIMATED
    note("importing %s (%s)" % (glb, "mesh + skeleton + clips" if animated else "mesh only, no clips"))
    delete_redirectors(DEST)
    before = set(p for p in eal.list_assets(DEST, recursive=True, include_folder=False)) \
        if eal.does_directory_exist(DEST) else set()

    task = unreal.AssetImportTask()
    task.filename = glb
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    if not animated:
        task.destination_name = MESH_NAME  # single asset: safe to name directly
    asset_tools.import_asset_tasks([task])

    try:
        imported = [package_path(p) for p in (task.get_editor_property("imported_object_paths") or [])]
    except Exception:
        imported = []
    if not imported:  # older/other import paths: diff the folder instead
        after = set(eal.list_assets(DEST, recursive=True, include_folder=False))
        imported = [package_path(p) for p in after - before]
    note("import produced %d assets" % len(imported))

    mesh_path = ensure_mesh_name(imported)
    if not mesh_path:
        log(False, "no SkeletalMesh produced by the glTF import (Interchange glTF importer enabled?)")
        return None, imported
    mesh = eal.load_asset(mesh_path)
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2.0
    lo, hi = HEIGHT_RANGE
    log(lo < height < hi, "%s imported as %s, height %.1f cm (expected %.0f-%.0f)" % (CHARACTER, mesh_path, height, lo, hi))
    return mesh, imported


def ensure_mesh_name(imported):
    """Characters.json expects /Game/Characters/<C>/SK_<C>: rename the freshly imported mesh to it."""
    target = DEST + "/" + MESH_NAME
    fresh = [p for p in imported if class_name(p) == "SkeletalMesh"]
    if not fresh:
        return target if eal.does_asset_exist(target) else next(iter(assets_of_class(DEST, "SkeletalMesh")), None)
    src = fresh[0]
    if src == target:
        return target
    if eal.does_asset_exist(target) and not delete_asset(target):
        manual("could not replace %s with the new import %s: delete %s in the Content Browser and re-run"
               % (target, src, target))
        return src
    if rename_asset(src, target):
        return target
    manual("could not rename %s to %s; rename it by hand (Characters.json points at %s)" % (src, target, MESH_NAME))
    return src


def organize_animations(imported, mesh):
    """Every imported AnimSequence named ...A_<C>_<Key>... -> /Game/Characters/<C>/Animations/A_<C>_<Key>."""
    expected, optional, sidecar = load_expected()
    known = sorted(set(expected) | set(optional) | set(sidecar), key=len, reverse=True)  # longest match first
    # Key must be followed by a non-alphanumeric character (or the end), so "Walk" never matches "WalkBack".
    patterns = [(key, re.compile(re.escape(CLIP_PREFIX + key) + r"(?![A-Za-z0-9])")) for key in known]
    eal.make_directory(ANIM_DEST)

    fresh = [p for p in imported if class_name(p) == "AnimSequence"]
    if not fresh:  # the import task did not report its clips: look for them next to the mesh instead
        fresh = [p for p in assets_of_class(DEST, "AnimSequence")
                 if not p.startswith(ANIM_DEST + "/") and CLIP_PREFIX in asset_name(p)]
    moved = 0
    claimed = {}
    for src in fresh:
        name = asset_name(src)
        key = next((k for k, pattern in patterns if pattern.search(name)), None)
        if key is None:
            unreal.log_warning(TAG + " imported animation %s has no %s<Key> in its name - left in place"
                               % (src, CLIP_PREFIX))
            continue
        target = ANIM_DEST + "/" + CLIP_PREFIX + key
        if key in claimed:
            unreal.log_warning(TAG + " %s and %s both look like %s; keeping the first" % (claimed[key], src, key))
            continue
        claimed[key] = src
        if src == target:
            continue
        if eal.does_asset_exist(target) and not delete_asset(target):
            manual("could not replace %s with the re-imported %s" % (target, src))
            continue
        if rename_asset(src, target):
            moved += 1
        else:
            manual("could not move %s to %s" % (src, target))
    delete_redirectors(DEST)
    note("%d imported clips moved into %s" % (moved, ANIM_DEST))

    skeleton = None
    try:
        skeleton = mesh.skeleton if mesh else None
    except Exception:
        pass
    for key in expected + optional:
        target = ANIM_DEST + "/" + CLIP_PREFIX + key
        if not eal.does_asset_exist(target) or class_name(target) != "AnimSequence":
            if key in optional:
                note("SKIP optional clip %s%s (not authored yet)" % (CLIP_PREFIX, key))
            else:
                log(False, "clip %s%s missing (expected at %s)" % (CLIP_PREFIX, key, target))
            continue
        anim = eal.load_asset(target)
        problems = []
        try:
            anim_skeleton = anim.get_editor_property("skeleton")
            if skeleton and anim_skeleton and anim_skeleton.get_path_name() != skeleton.get_path_name():
                problems.append("skeleton %s != %s's %s" % (anim_skeleton.get_name(), MESH_NAME, skeleton.get_name()))
        except Exception:
            pass
        info = sidecar.get(key, {})
        wants_loop = bool(info.get("loop", key in LOOP_KEYS))
        if wants_loop:
            try:
                anim.set_editor_property("loop", True)  # previews / a future AnimBP; the native path loops by itself
                eal.save_loaded_asset(anim)
            except Exception:
                pass
        length = play_length(anim)
        authored = info.get("duration")
        if length is not None and authored is not None and abs(length - authored) > 0.034:  # > one 30 fps frame
            problems.append("length %.3f s != authored %.3f s" % (length, authored))
        length_text = "%.3f s" % length if length is not None else "length n/a"
        log(not problems, "clip %s%s (%s%s)%s" % (CLIP_PREFIX, key, length_text, ", loop" if wants_loop else "",
                                                ": " + "; ".join(problems) if problems else ""))


# ----------------------------------------------------------------------------------------------- material / sockets
def make_materials(mesh):
    tex_paths = assets_of_class(DEST, "Texture2D")
    master = eal.load_asset("/Game/Materials/M_MT_ToonCharacter") if eal.does_asset_exist("/Game/Materials/M_MT_ToonCharacter") else None
    if not master or not tex_paths:
        unreal.log_warning("Toon master or atlas texture missing; run mt_create_materials.py first. Keeping imported material.")
        return
    try:
        mi_name = "MI_%s_Toon" % CHARACTER
        mi_path = DEST + "/" + mi_name
        mi = eal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else asset_tools.create_asset(
            mi_name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", master)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseTexture", eal.load_asset(tex_paths[0]))
        unreal.MaterialEditingLibrary.update_material_instance(mi)
        materials = mesh.get_editor_property("materials")
        for slot in materials:
            slot.set_editor_property("material_interface", mi)
        mesh.set_editor_property("materials", materials)
        eal.save_asset(mi_path)
        eal.save_loaded_asset(mesh)
        log(True, "%s uses %s (lit, original colours preserved from the atlas)" % (CHARACTER, mi_name))
    except Exception as exc:
        manual("toon material setup failed (%s): assign MI_%s_Toon to %s by hand" % (exc, CHARACTER, MESH_NAME))


def add_sockets(mesh):
    names = bone_names(mesh)
    if names is not None:
        log(UPPER_SPINE in names, "upper-body layer bone %s present (native anim instance casts on the move)" % UPPER_SPINE)
    # Every CastSocket Abilities.json uses. hand_*: palm offset so spells form in front of the hand, not inside
    # the wrist; head: Demon Eye; spine_03: the chest (awakenings / auras).
    wanted = {"hand_r": ("arm_R0_2_jnt_0117", 8.0), "hand_l": ("arm_L0_2_jnt_094", 8.0),
              "foot_l": ("leg_L0_2_jnt_010", 0.0), "foot_r": ("leg_R0_2_jnt_015", 0.0),
              "head": ("head_C0_0_jnt_067", 0.0), "spine_03": ("spine_C0_3_jnt_063", 0.0)}
    for socket, (bone, offset) in wanted.items():
        if names is not None and bone not in names:
            log(False, "bone %s missing for socket %s" % (bone, socket))
            continue
        try:
            existing = [str(s.socket_name) for s in mesh.get_editor_property("sockets") or []]
        except Exception:
            existing = []
        if socket in existing:
            continue
        try:
            s = unreal.SkeletalMeshSocket(mesh)
            s.set_editor_property("socket_name", socket)
            s.set_editor_property("bone_name", bone)
            s.set_editor_property("relative_location", unreal.Vector(offset, 0.0, 0.0))
            mesh.add_socket(s)
        except Exception as exc:
            manual("add socket %s on bone %s by hand (%s)" % (socket, bone, exc))
    eal.save_loaded_asset(mesh)
    note("sockets %s ensured" % "/".join(wanted))


# ----------------------------------------------------------------------- optional: Mixamo retargeting (UE 5.6+ safe)
def guarded(what, fn):
    """Runs fn() (a lambda, so even looking up an unreal.* name that no longer exists is caught)."""
    try:
        return True, fn()
    except Exception as exc:
        manual("%s failed on this engine version (%s)" % (what, exc))
        return False, None


def build_ik_rig(name, mesh, chains, pelvis):
    path = DEST + "/Rig/" + name
    ok, rig = guarded("create IK Rig " + name, lambda: eal.load_asset(path) if eal.does_asset_exist(path) else
                      asset_tools.create_asset(name, DEST + "/Rig", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory()))
    if not ok or not rig:
        return None
    ok, ctrl = guarded("IKRigController.get_controller", lambda: unreal.IKRigController.get_controller(rig))
    if not ok or not ctrl:
        return None
    guarded("IKRigController.set_skeletal_mesh", lambda: ctrl.set_skeletal_mesh(mesh))
    guarded("IKRigController.set_retarget_root", lambda: ctrl.set_retarget_root(pelvis))
    solver_index = 0
    ok, index = guarded("add Full Body IK solver (UE 5.6+ uses solver structs: add it in the IK Rig editor)",
                        lambda: ctrl.add_solver(unreal.IKRigFBIKSolver))
    if ok and isinstance(index, int) and index >= 0:
        solver_index = index
    names = bone_names(mesh)
    for chain, start, end, goal in chains:
        if names is not None and (start not in names or end not in names):
            log(False, "%s: chain %s bones missing (%s..%s)" % (name, chain, start, end))
            continue
        goal_name = ""
        if goal:
            ok, created = guarded("goal %s on %s" % (goal, end), lambda: ctrl.add_new_goal(goal, end))
            if ok and created:
                goal_name = str(created)
                guarded("connect goal %s" % goal, lambda: ctrl.connect_goal_to_solver(goal_name, solver_index))
        guarded("retarget chain %s" % chain, lambda: ctrl.add_retarget_chain(chain, start, end, goal_name))
    guarded("save " + path, lambda: eal.save_asset(path))
    note("%s built with %d chains" % (name, len(chains)))
    return rig


def build_retargeter(source_rig, target_rig):
    path = DEST + "/Rig/RTG_Mixamo_To_Rudeus"
    ok, rtg = guarded("create IK Retargeter", lambda: eal.load_asset(path) if eal.does_asset_exist(path) else
                      asset_tools.create_asset("RTG_Mixamo_To_Rudeus", DEST + "/Rig", unreal.IKRetargeter, unreal.IKRetargetFactory()))
    if not ok or not rtg:
        return None
    ok, ctrl = guarded("IKRetargeterController.get_controller", lambda: unreal.IKRetargeterController.get_controller(rtg))
    if not ok or not ctrl:
        return None
    guarded("set source IK Rig", lambda: ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig))
    guarded("set target IK Rig", lambda: ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig))
    # Chains share names on purpose, so exact mapping is deterministic.
    ok, _ = guarded("auto-map chains (UE 5.6+: map chains in the retargeter's chain-mapping op)",
                    lambda: ctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True))
    if not ok:
        for chain, _, _, _ in RUDEUS_CHAINS:
            guarded("map chain %s" % chain, lambda: ctrl.set_source_chain(chain, chain))
    guarded("save " + path, lambda: eal.save_asset(path))
    note("RTG_Mixamo_To_Rudeus mapped. Both rigs are T-pose; verify arm/shoulder retarget pose in the editor.")
    return rtg


def batch_retarget(rtg, source_mesh, target_mesh):
    if not eal.does_directory_exist(MIXAMO_SRC):
        note("no Mixamo animations in %s (optional)" % MIXAMO_SRC)
        return
    anims = [eal.find_asset_data(p) for p in eal.list_assets(MIXAMO_SRC, recursive=True) if class_name(p) == "AnimSequence"]
    if not anims:
        note("no AnimSequences under " + MIXAMO_SRC)
        return
    ok, result = guarded("IKRetargetBatchOperation.duplicate_and_retarget",
                         lambda: unreal.IKRetargetBatchOperation.duplicate_and_retarget(
                             anims, source_mesh, target_mesh, rtg, "", "", "", "_Rudeus", True))
    if ok:
        log(bool(result), "retargeted %d Mixamo animations onto Rudeus" % len(result or []))


def optional_retargeting(mesh):
    if CHARACTER != "Rudeus":
        note("SKIP retargeting for %s: every clip is authored on his own skeleton" % CHARACTER)
        return
    mixamo_mesh = eal.load_asset(MIXAMO_MESH) if eal.does_asset_exist(MIXAMO_MESH) else None
    if not mixamo_mesh:
        note("SKIP retargeting: clips are authored on Rudeus's skeleton; import a Mixamo Y Bot as %s only if you "
             "want to retarget extra Mixamo clips" % MIXAMO_MESH)
        return
    rudeus_rig = build_ik_rig("IK_Rudeus", mesh, RUDEUS_CHAINS, RUDEUS_PELVIS)
    mixamo_rig = build_ik_rig("IK_Mixamo", mixamo_mesh, MIXAMO_CHAINS, "Hips")
    if not rudeus_rig or not mixamo_rig:
        manual("IK Rigs incomplete: finish IK_Rudeus / IK_Mixamo in the editor before retargeting")
        return
    rtg = build_retargeter(mixamo_rig, rudeus_rig)
    if rtg:
        batch_retarget(rtg, mixamo_mesh, mesh)


def main():
    mesh, imported = import_glb()
    if not mesh:
        return
    organize_animations(imported, mesh)
    make_materials(mesh)
    add_sockets(mesh)
    optional_retargeting(mesh)
    try:
        eal.save_directory(DEST, only_if_is_dirty=True, recursive=True)
    except Exception as exc:
        unreal.log_warning(TAG + " save_directory: %s" % exc)
    summary = "mt_setup_%s finished: %d passed, %d failed" % (CHARACTER.lower(), RESULTS["PASS"], RESULTS["FAIL"])
    (unreal.log if RESULTS["FAIL"] == 0 else unreal.log_error)(("PASS " if RESULTS["FAIL"] == 0 else "FAIL ") + summary)


main()
