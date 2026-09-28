"""Loads the normalised Rudeus GLB into a clean Blender scene: armature at identity (Z-up,
facing -Y), mesh parented to it, no Sketchfab helper empties."""
import os
import bpy

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GLB = os.path.join(ROOT, "SourceArt", "Characters", "Rudeus", "Rudeus_Greyrat_UE.glb")


def load_rudeus(path=GLB):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.import_scene.gltf(filepath=path)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    mesh = next(o for o in bpy.data.objects if o.type == "MESH" and o.parent == arm)
    mw_mesh_rel = arm.matrix_world.inverted() @ mesh.matrix_world
    arm.parent = None
    arm.matrix_world.identity()
    mesh.matrix_world = arm.matrix_world @ mw_mesh_rel
    for o in list(bpy.data.objects):
        if o not in (arm, mesh):
            bpy.data.objects.remove(o, do_unlink=True)
    fix_root_weighted_eyes(mesh)
    arm.name = "Rudeus_Armature"
    arm.data.name = "Rudeus_Skeleton"
    mesh.name = "Rudeus_Mesh"
    bpy.context.view_layer.update()
    return arm, mesh


def fix_root_weighted_eyes(mesh, src="top_C0_0_jnt_02", dst="head_C0_0_jnt_067"):
    """Rig repair: 214 eye/pupil vertices in the source model are 100% weighted to the root control
    bone, so they would stay at standing eye height whenever the body moves (crouch, knockdown,
    death). Re-weight them to the head bone."""
    g_src = mesh.vertex_groups.get(src)
    g_dst = mesh.vertex_groups.get(dst)
    if not g_src or not g_dst:
        return 0
    moved = []
    for v in mesh.data.vertices:
        for g in v.groups:
            if g.group == g_src.index and g.weight > 0.0:
                moved.append((v.index, g.weight))
    for idx, w in moved:
        g_dst.add([idx], w, "ADD")
        g_src.remove([idx])
    print("rig repair: re-weighted %d vertices %s -> %s" % (len(moved), src, dst))
    return len(moved)
