"""Authors Rudeus's animation set in Blender (bpy) and exports SourceArt/Characters/Rudeus/Rudeus_Animated.glb.

    python3 Tools/anim/build_rudeus_anims.py                  # all clips, export + QA sheets
    python3 Tools/anim/build_rudeus_anims.py --clips Walk Run --no-export

QA printed per clip: max IK reach ratio (1.0 = fully straight leg), max ankle target error (cm),
stance-foot slip (cm/s deviation from the ground speed), loop seam error (degrees).
"""
import argparse
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
from mathutils import Vector  # noqa: E402
import scene  # noqa: E402
import preview  # noqa: E402
import rudeus_anim_lib as L  # noqa: E402
from pose_compose import compose, ankle_from_contact, DEFAULTS  # noqa: E402
import importlib  # noqa: E402
import clips as C  # noqa: E402

OUT_GLB = os.path.join(scene.ROOT, "SourceArt", "Characters", "Rudeus", "Rudeus_Animated.glb")
QA_DIR = os.path.join(scene.ROOT, "Docs", "Images", "Animation")
TEX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "SourceArt", "Characters", "Rudeus", "Rudeus_Atlas.png")


GROUND_CLAMP = {"Knockdown", "Death", "DodgeForward", "DodgeBack", "DodgeLeft", "DodgeRight", "HardLand", "Land", "JumpStart"}


def ground_corrected(rig, mesh, name, params):
    """Compose a pose; for clips that touch the floor with the body, raise the pelvis so the lowest
    skinned vertex rests exactly on the ground (no penetration, no floating when lying down)."""
    D, pel = compose(rig, params)
    if name not in GROUND_CLAMP:
        return D, pel
    p2 = dict(params)
    base_z = params.get("pel_z", DEFAULTS["pel_z"])
    raise_by = 0.0
    for _ in range(6):  # iterate: with IK partly active, lifting the pelvis only partly lifts the legs
        rig.apply(D, pel)
        bpy.context.view_layer.update()
        co, _, _ = preview.evaluate(rig.obj, mesh)
        low = float(co[:, 2].min())
        if low >= -0.002:
            break
        raise_by += -low
        p2["pel_z"] = base_z + raise_by
        D, pel = compose(rig, p2)
    return D, pel


def frames_for(duration):
    return max(2, int(round(duration * C.FPS)))


def key_pose(rig, frame):
    for pb in rig.pose:
        pb.keyframe_insert("rotation_quaternion", frame=frame, group=pb.name)
    rig.pose[L.PELVIS].keyframe_insert("location", frame=frame, group=L.PELVIS)


def qa_clip(rig, name, duration, loop, fn):
    n = frames_for(duration)
    reach, target_err, seam = 0.0, 0.0, 0.0
    ankles = {"L": [], "R": []}
    first_q = None
    G = C.GAITS.get(name)
    for i in range(n + 1):
        t = i / C.FPS
        P = dict(DEFAULTS)
        P.update(fn(min(t, duration) if not loop else t % duration if i < n else duration * 0.999999))
        D, pel = compose(rig, P)
        full = rig.apply(D, pel)
        if P.get("legs", "ik") == "ik":
            for s in "LR":
                hip = rig.world_head(L.THIGH[s], full, pel)
                tgt, _ = ankle_from_contact(rig, s, P)
                act = rig.world_head(L.FOOT[s], full, pel)
                reach = max(reach, (tgt - hip).length / (rig.l_thigh + rig.l_shin))
                target_err = max(target_err, (tgt - act).length * 100)
                ankles[s].append((t, act.copy(), P[s + "_fz"], P[s + "_fpitch"]))
        q = [pb.rotation_quaternion.copy() for pb in rig.pose]
        if i == 0:
            first_q = q
        if loop and i == n:
            seam = max(math.degrees(a.rotation_difference(b).angle) for a, b in zip(first_q, q))
    slip = 0.0
    if G:
        speed = G["speed"]
        d = Vector((G["dir"][0], G["dir"][1], 0.0))
        for s in "LR":
            seq = ankles[s]
            for (t0, a0, z0, p0), (t1, a1, z1, p1) in zip(seq, seq[1:]):
                if z0 == 0.0 and z1 == 0.0 and p0 == 0.0 and p1 == 0.0:  # flat planted foot
                    v = (a1 - a0) / (t1 - t0)
                    slip = max(slip, (v + d * speed).length * 100)
    return {"clip": name, "frames": n + 1, "max_leg_reach": round(reach, 3), "max_target_err_cm": round(target_err, 2),
            "planted_slip_cm_s": round(slip, 2), "loop_seam_deg": round(seam, 3)}


PREFIX = "A_Rudeus_"


def author_clip(rig, mesh, name, duration, loop, fn):
    arm = rig.obj
    act = bpy.data.actions.new(PREFIX + name)
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = act
    n = frames_for(duration)
    for i in range(n + 1):
        t = i / C.FPS
        tt = (t % duration) if (loop and i < n) else min(t, duration)
        if loop and i == n:
            tt = 0.0  # exact seam: last frame == first frame
        D, pel = ground_corrected(rig, mesh, name, fn(tt))
        rig.apply(D, pel)
        key_pose(rig, i)
    arm.animation_data.action = None
    return act


def contact_sheet(rig, mesh, name, duration, loop, fn, views=("side", "front"), count=6):
    imgs, labels = [], []
    for view in views:
        for k in range(count):
            t = duration * k / count if loop else duration * k / max(1, count - 1)
            D, pel = ground_corrected(rig, mesh, name, fn(t))
            rig.apply(D, pel)
            bpy.context.view_layer.update()
            co, tri, uv = preview.evaluate(rig.obj, mesh)
            imgs.append(preview.draw(co, tri, uv, view, size=300, frame_box=((-0.95, -0.05), (0.95, 1.85))))
            labels.append("%s %s t=%.2f" % (name, view, t))
    return preview.sheet(imgs, count, labels)


def export(arm, mesh, path):
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = arm
    arm.animation_data_create()
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    props = bpy.ops.export_scene.gltf.get_rna_type().properties.keys()
    kw = dict(filepath=path, export_format="GLB", use_selection=True, export_animations=True,
              export_animation_mode="ACTIONS", export_force_sampling=True, export_frame_step=1,
              export_skins=True, export_yup=True, export_apply=False)
    for k, v in (("export_anim_slide_to_zero", True), ("export_optimize_animation_size", False),
                 ("export_def_bones", False), ("export_anim_single_armature", True), ("export_reset_pose_bones", True),
                 ("export_texcoords", True), ("export_normals", True), ("export_materials", "EXPORT")):
        if k in props:
            kw[k] = v
    bpy.ops.export_scene.gltf(**kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clips", nargs="*")
    ap.add_argument("--no-export", action="store_true")
    ap.add_argument("--sheets", action="store_true", help="render QA contact sheets")
    ap.add_argument("--character", default="Rudeus", help="Rudeus | Orsted")
    ap.add_argument("--rig", default="", help="skinned GLB on Rudeus's skeleton (default: the character's source)")
    ap.add_argument("--texture", default="")
    ap.add_argument("--out", default="")
    args = ap.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
    global C, PREFIX, OUT_GLB
    if args.character != "Rudeus":
        C = importlib.import_module("clips_" + args.character.lower())
        C.FPS = 30
    PREFIX = "A_%s_" % args.character
    rig_path = args.rig or (scene.GLB if args.character == "Rudeus" else os.path.join(
        scene.ROOT, "SourceArt", "Characters", args.character, args.character + "_Rigged.glb"))
    OUT_GLB = args.out or os.path.join(scene.ROOT, "SourceArt", "Characters", args.character, args.character + "_Animated.glb")
    arm, mesh = scene.load_rudeus(rig_path)
    scn = bpy.context.scene
    scn.render.fps, scn.render.fps_base = C.FPS, 1.0   # keys are authored at 30 fps; exporter converts frames->seconds with this
    rig = L.Rig(arm)
    names = args.clips or list(C.CLIPS.keys())
    report = []
    if args.sheets:
        os.makedirs(QA_DIR, exist_ok=True)
        tex = args.texture or (TEX if args.character == "Rudeus" else "")
        if not tex:
            imgs = [im for im in bpy.data.images if im.size[0] > 0]
            if imgs:
                tex = os.path.join(QA_DIR, "_tex_%s.png" % args.character)
                imgs[0].save_render(tex)
        if tex and os.path.exists(tex):
            preview.load_texture(tex)
    for name in names:
        duration, loop, fn = C.CLIPS[name]
        q = qa_clip(rig, name, duration, loop, fn)
        report.append(q)
        print(json.dumps(q))
        if args.sheets:
            contact_sheet(rig, mesh, name, duration, loop, fn).save(os.path.join(QA_DIR, "%s%s.png" % (PREFIX, name)))
    if not args.no_export:
        for name in names:
            duration, loop, fn = C.CLIPS[name]
            author_clip(rig, mesh, name, duration, loop, fn)
        os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
        export(arm, mesh, OUT_GLB)
        meta = {name: {"duration": C.CLIPS[name][0], "loop": C.CLIPS[name][1]} for name in names}
        for name, G in C.GAITS.items():
            if name in meta:
                meta[name]["ref_speed_cm_s"] = round(G["speed"] * 100, 1)
        os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
        with open(OUT_GLB.replace(".glb", ".anim.json"), "w") as f:
            json.dump({"clips": meta, "qa": report}, f, indent=1)
        print("exported", OUT_GLB)


if __name__ == "__main__":
    main()
