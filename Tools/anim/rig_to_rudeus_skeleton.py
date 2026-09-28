"""Re-rig any T-posed humanoid GLB (e.g. the generated Orsted mesh) onto Rudeus's exact skeleton so
both characters share ONE Unreal skeleton and every authored animation.

    python3 Tools/anim/rig_to_rudeus_skeleton.py <input.glb> <output.glb> [--height 1.95] [--coat]

How it works (topology based, independent of the source rig's bone names):
 1. Landmarks are found from the source skeleton's layout (pelvis = root of the three biggest
    branches, legs = the two downward chains, spine = the upward chain, arms = the horizontal
    chains) or, if the input is unrigged, from the mesh silhouette.
 2. Rudeus's armature is duplicated; every bone keeps Rudeus's rest ORIENTATION (so local
    rotations of shared animations mean the same thing) and only its head position is moved to the
    fitted landmark. Secondary chains (coat, hair, hood, bag, sleeves) are placed by scaling their
    Rudeus offsets.
 3. Skin weights: vertices are weighted to the fitted bones by a heat-like falloff over bone
    segments inside each body region; coat vertices below the waist and outside the leg volumes go
    to the 8 coat chains so the coat follows the legs instead of splitting like trousers.
 4. A QA sheet renders the rig in rest pose and in a few authored poses.
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector, Matrix  # noqa: E402
import scene  # noqa: E402
import rudeus_anim_lib as L  # noqa: E402

LEG_RADIUS_FRACTION = 0.075   # of body height


def import_source(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    arms = [o for o in new if o.type == "ARMATURE"]
    mesh = max(meshes, key=lambda o: len(o.data.vertices))
    return mesh, (arms[0] if arms else None), new


def world_verts(mesh):
    dg = bpy.context.evaluated_depsgraph_get()
    ev = mesh.evaluated_get(dg)
    me = ev.to_mesh()
    n = len(me.vertices)
    co = np.empty(n * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    m = np.array(mesh.matrix_world)
    co = co @ m[:3, :3].T + m[:3, 3]
    ev.to_mesh_clear()
    return co


def normalise_mesh(mesh, height):
    """Scale/translate the source so it stands on z=0, centred, facing -Y, at the requested height."""
    co = world_verts(mesh)
    lo, hi = co.min(0), co.max(0)
    s = height / (hi[2] - lo[2])
    centre = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]))
    for o in [mesh] + ([mesh.parent] if mesh.parent else []):
        pass
    root = mesh
    while root.parent:
        root = root.parent
    root.matrix_world = Matrix.Scale(s, 4) @ Matrix.Translation(-centre) @ root.matrix_world
    bpy.context.view_layer.update()
    return s


def landmarks_from_mesh(co, height):
    """Silhouette landmarks for a T-posed body (fallback when the source has no usable rig)."""
    z = co[:, 2]
    x = co[:, 0]

    def slab(z0, z1):
        return co[(z >= z0) & (z < z1)]
    lm = {}
    # arms: widest horizontal band
    band = slab(0.72 * height, 0.86 * height)
    span_l, span_r = band[:, 0].max(), band[:, 0].min()
    shoulder_z = float(np.median(band[np.abs(band[:, 0]) > 0.35 * span_l][:, 2])) if len(band) else 0.8 * height
    lm["shoulder_z"] = shoulder_z
    lm["hand_L"] = Vector((span_l, 0, shoulder_z))
    lm["hand_R"] = Vector((span_r, 0, shoulder_z))
    torso = slab(0.55 * height, 0.7 * height)
    lm["torso_half_width"] = float(np.percentile(np.abs(torso[:, 0]), 95)) if len(torso) else 0.12 * height
    lm["crotch_z"] = 0.47 * height
    lm["head_top"] = float(z.max())
    return lm


def weighted_bones(mesh):
    names = {g.index: g.name for g in mesh.vertex_groups}
    used = set()
    for v in mesh.data.vertices:
        for g in v.groups:
            if g.weight > 1e-4:
                used.add(names[g.group])
    return used


def landmarks_from_armature(arm, height, weighted=None):
    """Topology-based landmarks from any humanoid rig in T-pose (names are ignored). Only bones that
    carry skin weights (or have weighted descendants) are followed, so helper joints are ignored."""
    mw = arm.matrix_world
    bones = arm.data.bones
    pos = {b.name: (mw @ b.head_local) for b in bones}
    if weighted is None:
        weighted = {b.name for b in bones}

    def alive(b):
        return b.name in weighted or any(c.name in weighted for c in b.children_recursive)

    def chain_down(b):
        out = [b]
        while True:
            kids = [c for c in b.children if alive(c)]
            if not kids:
                return out
            b = max(kids, key=lambda c: len([d for d in c.children_recursive if d.name in weighted]))
            out.append(b)
    def down_chains(b):
        z = pos[b.name].z
        return [chain_down(c) for c in b.children if alive(c) and pos[chain_down(c)[-1].name].z < z - 0.25 * height]
    # pelvis: the bone that owns exactly two chains reaching down toward the feet (highest such bone)
    cands = [b for b in bones if alive(b) and len(down_chains(b)) == 2]
    pel = max(cands, key=lambda b: pos[b.name].z) if cands else [b for b in bones if b.parent is None][0]
    pz = pos[pel.name].z
    legs = down_chains(pel)
    ups = [chain_down(c) for c in pel.children if pos[chain_down(c)[-1].name].z > pz + 0.25 * height]
    spine = max(ups, key=len) if ups else chain_down(pel)
    out = {L.PELVIS: pos[pel.name]}
    for ch in legs:
        side = "L" if pos[ch[0].name].x > 0 else "R"
        names = (L.THIGH[side], L.SHIN[side], L.FOOT[side], L.TOE[side])
        for tb, sb in zip(names, ch):
            out[tb] = pos[sb.name]
    # chest = the spine bone whose child chains reach far out sideways (the arms)
    def side_chains(b):
        return [chain_down(c) for c in b.children if abs(pos[chain_down(c)[-1].name].x) > 0.25 * height]
    chest = next((b for b in spine if len(side_chains(b)) >= 2), spine[min(len(spine) - 1, 3)])
    cz = pos[chest.name]
    for tb, f in zip(L.SPINE, (0.18, 0.55, 1.0)):
        out[tb] = pos[pel.name].lerp(cz, f)
    ups = [chain_down(c) for c in chest.children if pos[chain_down(c)[-1].name].z > cz.z + 0.05 * height
           and abs(pos[chain_down(c)[-1].name].x) < 0.1 * height]
    if ups:
        neck_chain = max(ups, key=len)
        out[L.NECK] = pos[neck_chain[0].name]
        if len(neck_chain) > 1:
            out[L.HEAD] = pos[neck_chain[1].name]
    for ch in side_chains(chest):
        side = "L" if pos[ch[-1].name].x > 0 else "R"
        # clavicle, upper arm, forearm, hand
        names = (L.CLAV[side], L.UPPER[side], L.FORE[side], L.HAND[side])
        for tb, sb in zip(names, ch):
            out[tb] = pos[sb.name]
    return out


def fit_template(template_arm, height_ratio, lm_positions):
    """Move template bone heads to the landmark positions while keeping every bone's orientation."""
    bpy.context.view_layer.objects.active = template_arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = template_arm.data.edit_bones
    rest = {b.name: (b.head.copy(), b.tail.copy(), b.roll) for b in eb}
    # default: uniform scale of Rudeus's rest (keeps secondary chains proportional)
    new_head = {n: h * height_ratio for n, (h, t, r) in rest.items()}
    for name, pos in lm_positions.items():
        if name in new_head:
            new_head[name] = pos
    # children that were not fitted follow their parent's displacement (rigid offsets, scaled)
    order = sorted(rest, key=lambda n: len(eb[n].parent_recursive))
    fitted = set(lm_positions)
    for n in order:
        b = eb[n]
        if n in fitted or b.parent is None:
            continue
        p = b.parent.name
        new_head[n] = new_head[p] + (rest[n][0] - rest[p][0]) * height_ratio
    for n in order:
        b = eb[n]
        h, t, r = rest[n]
        b.head = new_head[n]
        b.tail = new_head[n] + (t - h) * height_ratio
        b.roll = r
    bpy.ops.object.mode_set(mode="OBJECT")


def segment_distance(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(1e-9, ab @ ab), 0.0, 1.0)
    return np.linalg.norm(p - (a + np.outer(t, ab)), axis=1)


def bind_weights(mesh, arm, height, coat=True):
    """Heat-like weights over the fitted bone segments, region-gated, plus coat chains."""
    co = world_verts(mesh)
    for g in list(mesh.vertex_groups):
        mesh.vertex_groups.remove(g)
    bones = arm.data.bones
    seg = {}
    for b in bones:
        a = np.array(arm.matrix_world @ b.head_local)
        kids = [c for c in b.children if not c.name.startswith(("skirt", "hair", "hood", "bag", "belt", "sleeve", "eye", "tear", "item"))]
        end = np.array(arm.matrix_world @ kids[0].head_local) if len(kids) == 1 else np.array(arm.matrix_world @ b.tail_local)
        seg[b.name] = (a, end)
    deform = [L.PELVIS] + L.SPINE + [L.NECK, L.HEAD]
    for s in "LR":
        deform += [L.CLAV[s], L.UPPER[s], L.FORE[s], L.HAND[s], L.THIGH[s], L.SHIN[s], L.FOOT[s], L.TOE[s]]
        for chain in L.FINGERS[s]:
            deform += chain
    dist = np.stack([segment_distance(co, *seg[n]) for n in deform], 1)
    # region gating: arms only for |x| beyond the torso, legs only below the crotch, etc.
    x, z = co[:, 0], co[:, 2]
    shoulder_x = abs(seg[L.UPPER["L"]][0][0])
    crotch = seg[L.THIGH["L"]][0][2] - 0.06 * height
    penalty = np.zeros_like(dist)
    for k, n in enumerate(deform):
        side = 1.0 if "_L" in n else (-1.0 if "_R" in n else 0.0)
        is_arm = any(n in (L.UPPER[s], L.FORE[s], L.HAND[s]) or any(n in c for c in L.FINGERS[s]) for s in "LR")
        is_leg = any(n in (L.THIGH[s], L.SHIN[s], L.FOOT[s], L.TOE[s]) for s in "LR")
        if is_arm:
            penalty[:, k] += np.where(x * side < shoulder_x * 0.85, 10.0, 0.0)
        if is_leg:
            penalty[:, k] += np.where(z > crotch + 0.02 * height, 10.0, 0.0) + np.where(x * side < -0.02 * height, 10.0, 0.0)
    d = dist + penalty
    k_near = 3
    idx = np.argsort(d, axis=1)[:, :k_near]
    dn = np.take_along_axis(d, idx, 1)
    w = 1.0 / np.maximum(dn, 1e-4) ** 4
    w /= w.sum(1, keepdims=True)
    groups = {n: mesh.vertex_groups.new(name=n) for n in deform}
    coat_verts = set()
    if coat:
        # robe below the waist and outside both leg volumes -> coat chains by angle around the body
        leg_r = LEG_RADIUS_FRACTION * height
        dl = np.minimum(segment_distance(co, *seg[L.THIGH["L"]]), segment_distance(co, *seg[L.SHIN["L"]]))
        dr = np.minimum(segment_distance(co, *seg[L.THIGH["R"]]), segment_distance(co, *seg[L.SHIN["R"]]))
        waist = seg[L.PELVIS][0][2]
        coat_mask = (z < waist) & (z > 0.12 * height) & (np.minimum(dl, dr) > leg_r)
        roots = list(L.SKIRT.keys())
        cg = {}
        for root in roots:
            chain = [root]
            n = root
            while True:
                nxt = [c.name for c in bones[n].children if c.name.startswith(root.split("_jnt")[0][:-1])]
                if not nxt:
                    break
                n = nxt[0]
                chain.append(n)
            for b in chain:
                cg[b] = mesh.vertex_groups.new(name=b)
            angle_root = math.atan2(seg[root][0][0], -seg[root][0][1])
            L.SKIRT[root] = L.SKIRT[root]  # keep mapping
            cg[root + "__angle"] = angle_root
            cg[root + "__chain"] = chain
        ang = np.arctan2(co[:, 0], -co[:, 1])
        for vi in np.nonzero(coat_mask)[0]:
            best = min(roots, key=lambda r: abs(math.remainder(ang[vi] - cg[r + "__angle"], 2 * math.pi)))
            chain = cg[best + "__chain"]
            h = (waist - z[vi]) / max(1e-6, waist - 0.12 * height)   # 0 at waist .. 1 at hem
            pos = min(len(chain) - 1.001, h * (len(chain) - 1))
            i0 = int(pos)
            f = pos - i0
            cg[chain[i0]].add([int(vi)], 1.0 - f, "REPLACE")
            if f > 0.01:
                cg[chain[i0 + 1]].add([int(vi)], f, "REPLACE")
            coat_verts.add(int(vi))
    for vi in range(len(co)):
        if vi in coat_verts:
            continue
        for j in range(k_near):
            wt = float(w[vi, j])
            if wt > 0.02:
                groups[deform[idx[vi, j]]].add([vi], wt, "REPLACE")
    # parent to the armature with an Armature modifier
    for m in list(mesh.modifiers):
        if m.type == "ARMATURE":
            mesh.modifiers.remove(m)
    world = mesh.matrix_world.copy()
    mesh.parent = arm
    mesh.matrix_world = world
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return len(coat_verts)


def render_qa(arm, mesh, path, texture):
    """Pose the re-rigged mesh with authored Rudeus clips and render a contact sheet."""
    import preview
    import clips as C
    from pose_compose import compose
    if texture:
        preview.load_texture(texture)
    rig = L.Rig(arm)
    shots = [("rest", None), ("Idle", 0.0), ("Walk", 0.0), ("Walk", 0.31), ("Run", 0.1), ("CombatIdle", 0.0),
             ("StoneCannon_Release", 0.07), ("DodgeForward", 0.1), ("Knockdown", 0.62)]
    imgs, labels = [], []
    for name, t in shots:
        if name == "rest":
            for pb in arm.pose.bones:
                pb.rotation_quaternion = (1, 0, 0, 0)
                pb.location = (0, 0, 0)
        else:
            D, pel = compose(rig, C.CLIPS[name][2](t))
            rig.apply(D, pel)
        bpy.context.view_layer.update()
        co, tri, uv = preview.evaluate(arm, mesh)
        for view in ("side", "front"):
            imgs.append(preview.draw(co, tri, uv, view, size=320, frame_box=((-1.1, -0.05), (1.1, 2.2))))
            labels.append("%s %s" % (name, view))
    preview.sheet(imgs, 6, labels).save(path)
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    print("QA sheet:", path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inp")
    ap.add_argument("out")
    ap.add_argument("--height", type=float, default=1.95)
    ap.add_argument("--no-coat", action="store_true")
    ap.add_argument("--qa-sheet", default="")
    ap.add_argument("--texture", default="")
    a = ap.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
    template, rudeus_mesh = scene.load_rudeus()
    bpy.data.objects.remove(rudeus_mesh, do_unlink=True)
    src_mesh, src_arm, imported = import_source(a.inp)
    if src_arm:
        # measure the rig in its REST (bind) pose: drop any imported animation
        if src_arm.animation_data:
            src_arm.animation_data.action = None
        for pb in src_arm.pose.bones:
            pb.location = (0, 0, 0)
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = (1, 0, 0, 0)
            pb.scale = (1, 1, 1)
        bpy.context.view_layer.update()
    normalise_mesh(src_mesh, a.height)
    lm_pos = landmarks_from_armature(src_arm, a.height, weighted_bones(src_mesh)) if src_arm else {}
    print("landmarks from source rig:", len(lm_pos))
    if src_arm:
        # drop the source rig: bake its rest pose into the mesh, remove the armature
        for m in list(src_mesh.modifiers):
            if m.type == "ARMATURE":
                bpy.context.view_layer.objects.active = src_mesh
                bpy.ops.object.modifier_apply(modifier=m.name)
        world = src_mesh.matrix_world.copy()
        src_mesh.parent = None
        src_mesh.matrix_world = world
    for o in imported:
        if o is not src_mesh and o.name in bpy.data.objects:
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.context.view_layer.update()
    co = world_verts(src_mesh)
    print("source bbox after normalisation:", co.min(0).round(3), co.max(0).round(3))
    lm = landmarks_from_mesh(co, a.height)
    print("silhouette: hand_L %s hand_R %s shoulder_z %.3f" % (tuple(round(v, 3) for v in lm["hand_L"]), tuple(round(v, 3) for v in lm["hand_R"]), lm["shoulder_z"]))
    ratio = a.height / 1.617
    fit_template(template, ratio, lm_pos)       # joint landmarks from the source rig (if any)
    # unrigged sources only: refine arms to the T-pose silhouette span (rig landmarks are exact)
    bones = template.data.bones
    arm_scale = 1.0
    if L.HAND["L"] in lm_pos:
        pass
    else:
        arm_scale = None
    if arm_scale is None:
        hand_x = abs(bones[L.HAND["L"]].head_local.x)
        tip_x = (lm["hand_L"].x - lm["hand_R"].x) / 2.0
        # Rudeus's wrist sits at 77.5% of the finger-tip span; use the same hand proportion
        arm_scale = tip_x * 0.775 / max(1e-6, hand_x)
        pos = {}
        for side in "LR":
            for b in (L.UPPER[side], L.FORE[side], L.HAND[side]):
                h = bones[b].head_local.copy()
                h.x *= arm_scale
                pos[b] = h
        fit_template(template, 1.0, pos)
    n_coat = bind_weights(src_mesh, template, a.height, coat=not a.no_coat)
    if a.qa_sheet:
        render_qa(template, src_mesh, a.qa_sheet, a.texture)
    src_mesh.name = "Orsted_Mesh"
    template.name = "Orsted_Armature"
    bpy.ops.object.select_all(action="DESELECT")
    template.select_set(True)
    src_mesh.select_set(True)
    bpy.context.view_layer.objects.active = template
    bpy.ops.export_scene.gltf(filepath=a.out, export_format="GLB", use_selection=True, export_animations=False,
                              export_skins=True, export_yup=True)
    print("rigged %s -> %s (coat vertices re-weighted: %d, arm scale %.3f)" % (a.inp, a.out, n_coat, arm_scale))


if __name__ == "__main__":
    main()
