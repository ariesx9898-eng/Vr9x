"""Phase 1 editor automation for Rudeus. Run inside Unreal (Tools > Execute Python Script,
or `py mt_setup_rudeus.py` in the Output Log console). Idempotent: safe to re-run.

Steps
  1. Import SourceArt/Characters/Rudeus/Rudeus_Greyrat_UE.glb (pre-normalised to 161.7 cm by
     Tools/prepare_rudeus_glb.py) into /Game/Characters/Rudeus as SK_Rudeus.
  2. Create the lit toon material instance from the imported atlas texture.
  3. Add sockets used by gameplay (hand_r / hand_l cast sockets, foot sockets for IK).
  4. Create IK_Rudeus (retarget chains + full-body IK goals) and IK_Mixamo.
  5. Create RTG_Mixamo_To_Rudeus and batch-retarget every animation found in
     /Game/Animation/Mixamo/Source into /Game/Characters/Rudeus/Animations.
Each step logs PASS/FAIL so problems are visible in the Output Log.
"""
import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
GLB = os.path.join(PROJECT_DIR, "SourceArt", "Characters", "Rudeus", "Rudeus_Greyrat_UE.glb")
DEST = "/Game/Characters/Rudeus"
MIXAMO_SRC = "/Game/Animation/Mixamo/Source"
MIXAMO_MESH = "/Game/Animation/Mixamo/SK_Mixamo_YBot"
OUT_ANIMS = DEST + "/Animations"

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


def log(ok, msg):
    (unreal.log if ok else unreal.log_error)(("PASS " if ok else "FAIL ") + msg)


def bone_names(skeletal_mesh):
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeletal_mesh.skeleton)
        return set(str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose))
    except Exception as exc:  # API differs between engine versions
        unreal.log_warning("Could not read bone names: %s" % exc)
        return None


def import_rudeus():
    if not os.path.exists(GLB):
        log(False, "normalised GLB missing - run python3 Tools/prepare_rudeus_glb.py first: " + GLB)
        return None
    task = unreal.AssetImportTask()
    task.filename = GLB
    task.destination_path = DEST
    task.destination_name = "SK_Rudeus"
    task.automated = True
    task.replace_existing = True
    task.save = True
    asset_tools.import_asset_tasks([task])
    meshes = [p for p in eal.list_assets(DEST, recursive=False) if eal.find_asset_data(p).asset_class_path.asset_name == "SkeletalMesh"]
    if not meshes:
        log(False, "no SkeletalMesh produced by the glTF import (check Interchange glTF is enabled)")
        return None
    mesh = eal.load_asset(meshes[0])
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2.0
    log(150.0 < height < 175.0, "Rudeus imported, height %.1f cm (expected ~161.7)" % height)
    return mesh


def make_materials(mesh):
    tex_paths = [p for p in eal.list_assets(DEST, recursive=True) if eal.find_asset_data(p).asset_class_path.asset_name == "Texture2D"]
    master = eal.load_asset("/Game/Materials/M_MT_ToonCharacter")
    if not master or not tex_paths:
        unreal.log_warning("Toon master or atlas texture missing; run mt_create_materials.py first. Keeping imported material.")
        return
    mi_path = DEST + "/MI_Rudeus_Toon"
    mi = eal.load_asset(mi_path) or asset_tools.create_asset("MI_Rudeus_Toon", DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "BaseTexture", eal.load_asset(tex_paths[0]))
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    materials = mesh.get_editor_property("materials")
    for slot in materials:
        slot.set_editor_property("material_interface", mi)
    mesh.set_editor_property("materials", materials)
    eal.save_asset(mi_path)
    log(True, "Rudeus uses MI_Rudeus_Toon (lit, original colours preserved from the atlas)")


def add_sockets(mesh):
    names = bone_names(mesh)
    wanted = {"hand_r": "arm_R0_2_jnt_0117", "hand_l": "arm_L0_2_jnt_094", "foot_l": "leg_L0_2_jnt_010", "foot_r": "leg_R0_2_jnt_015"}
    skeleton = mesh.skeleton
    for socket, bone in wanted.items():
        if names is not None and bone not in names:
            log(False, "bone %s missing for socket %s" % (bone, socket))
            continue
        try:
            existing = [s.socket_name for s in mesh.get_editor_property("sockets") or []]
        except Exception:
            existing = []
        if socket in existing:
            continue
        s = unreal.SkeletalMeshSocket(mesh)
        s.set_editor_property("socket_name", socket)
        s.set_editor_property("bone_name", bone)
        # Palm offset so spells form in front of the hand, not inside the wrist.
        s.set_editor_property("relative_location", unreal.Vector(8.0, 0.0, 0.0))
        try:
            mesh.add_socket(s)
        except Exception as exc:
            unreal.log_warning("add_socket unavailable (%s); add socket %s on %s manually" % (exc, socket, bone))
    eal.save_loaded_asset(mesh)
    log(True, "sockets hand_r/hand_l/foot_l/foot_r ensured")


def build_ik_rig(name, mesh, chains, pelvis):
    path = DEST + "/Rig/" + name
    rig = eal.load_asset(path) or asset_tools.create_asset(name, DEST + "/Rig", unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    names = bone_names(mesh)
    ctrl.set_retarget_root(pelvis)
    try:
        solver_index = ctrl.add_solver(unreal.IKRigFBIKSolver)
    except Exception:
        solver_index = 0  # UE 5.6+: solvers are structs; add Full Body IK in the editor if this fails
    for chain, start, end, goal in chains:
        if names is not None and (start not in names or end not in names):
            log(False, "%s: chain %s bones missing (%s..%s)" % (name, chain, start, end))
            continue
        goal_name = ""
        if goal:
            try:
                goal_name = ctrl.add_new_goal(goal, end)
                ctrl.connect_goal_to_solver(goal_name, solver_index)
            except Exception as exc:
                unreal.log_warning("goal %s: %s" % (goal, exc))
        ctrl.add_retarget_chain(chain, start, end, goal_name or "")
    eal.save_asset(path)
    log(True, "%s built with %d chains" % (name, len(chains)))
    return rig


def build_retargeter(source_rig, target_rig):
    path = DEST + "/Rig/RTG_Mixamo_To_Rudeus"
    rtg = eal.load_asset(path) or asset_tools.create_asset("RTG_Mixamo_To_Rudeus", DEST + "/Rig", unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    # Chains share names on purpose, so exact mapping is deterministic.
    try:
        ctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    except Exception:
        for chain, _, _, _ in RUDEUS_CHAINS:
            try:
                ctrl.set_source_chain(chain, chain)
            except Exception as exc:
                unreal.log_warning("map %s: %s" % (chain, exc))
    eal.save_asset(path)
    log(True, "RTG_Mixamo_To_Rudeus mapped. Both rigs are T-pose; verify arm/shoulder retarget pose in the editor.")
    return rtg


def batch_retarget(rtg, source_mesh, target_mesh):
    if not eal.does_directory_exist(MIXAMO_SRC):
        unreal.log_warning("No Mixamo animations in %s yet (see Docs/Animation_Pipeline.md)" % MIXAMO_SRC)
        return
    anims = [eal.find_asset_data(p) for p in eal.list_assets(MIXAMO_SRC, recursive=True)
             if eal.find_asset_data(p).asset_class_path.asset_name == "AnimSequence"]
    if not anims:
        unreal.log_warning("No AnimSequences under " + MIXAMO_SRC)
        return
    result = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        anims, source_mesh, target_mesh, rtg, "", "", "", "_Rudeus", True)
    log(len(result) > 0, "retargeted %d Mixamo animations onto Rudeus" % len(result))


def main():
    mesh = import_rudeus()
    if not mesh:
        return
    make_materials(mesh)
    add_sockets(mesh)
    rudeus_rig = build_ik_rig("IK_Rudeus", mesh, RUDEUS_CHAINS, RUDEUS_PELVIS)
    mixamo_mesh = eal.load_asset(MIXAMO_MESH)
    if not mixamo_mesh:
        unreal.log_warning("Import a Mixamo Y Bot (T-pose, with skin) as %s to enable retargeting." % MIXAMO_MESH)
        return
    mixamo_rig = build_ik_rig("IK_Mixamo", mixamo_mesh, MIXAMO_CHAINS, "Hips")
    rtg = build_retargeter(mixamo_rig, rudeus_rig)
    batch_retarget(rtg, mixamo_mesh, mesh)


main()
