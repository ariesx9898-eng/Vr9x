"""Re-rig any humanoid GLB (e.g. the generated Orsted mesh) onto Rudeus's exact skeleton so both characters share
ONE bone layout (names, hierarchy, rest orientations) and every animation authoring tool.

    python3 Tools/anim/rig_to_rudeus_skeleton.py <input.glb> <output.glb> [--height 1.95] [--no-coat]
        [--qa-sheet out.png] [--texture atlas.png] [--atlas-out atlas.png]

How it works (topology based, independent of the source rig's bone names):
 1. Landmarks are found from the source skeleton's layout (pelvis = root of the two downward leg chains,
    spine = the upward chain, arms = the sideways chains) or, if the input is unrigged, from the mesh silhouette.
    A-posed sources are first posed into a T-pose with their own skin weights (arms straight out sideways) and
    baked, knee joints that an auto-rigger left outside the leg are moved back onto the hip-ankle line, and the
    left and right landmarks are mirror-averaged (the authoring tools assume a symmetric skeleton).
 2. Rudeus's armature is duplicated; every bone keeps Rudeus's rest ORIENTATION (so local rotations of shared
    animations mean the same thing) and only its head position is moved to the fitted landmark. The fingers are
    scaled to the mesh's hands, the 8 coat chains are spread from the waist to the coat's hem, and the other
    secondary chains (hair, hood, bag, sleeves) are placed by scaling their Rudeus offsets.
 3. Skin weights: vertices are weighted to the fitted bones by a heat-like falloff over bone segments inside each
    body region; coat cloth below the waist (outside the leg volumes, or pale cloth hanging close to them) goes to
    the 8 coat chains so the coat follows the legs instead of splitting like trousers.
 4. Material: the colour atlas stays on a plain lit surface. Generated GLBs often also bind it as full-strength
    emission, which would make the character glow in any engine. A QA sheet renders the rig in rest pose and in a
    few authored poses.
"""
import argparse
import math
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector, Matrix  # noqa: E402
import scene  # noqa: E402
import rudeus_anim_lib as L  # noqa: E402

LEG_RADIUS_FRACTION = 0.075   # of body height
COAT_NEAR_FRACTION = 0.035    # pale cloth this far (of body height) from a leg axis is coat, not trousers
COAT_LIGHTNESS = 0.55         # texture luminance above which cloth counts as pale (coat, not trousers/boots)
MIN_ARM_DROOP = 8.0           # degrees below horizontal before an A-posed source is straightened
KNEE_TOLERANCE = 0.02         # of body height: a knee farther than this from the hip-ankle line is refitted
TEMPLATE_HEIGHT = 1.617       # Rudeus_Greyrat_UE.glb


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
    root = mesh
    while root.parent:
        root = root.parent
    root.matrix_world = Matrix.Scale(s, 4) @ Matrix.Translation(-centre) @ root.matrix_world
    bpy.context.view_layer.update()
    return s


# ------------------------------------------------------------------------------------------------ material
def base_color_image(mesh):
    for slot in mesh.material_slots:
        mat = slot.material
        if not mat or not mat.use_nodes:
            continue
        bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        sock = bsdf.inputs.get("Base Color") if bsdf else None
        if not sock or not sock.links:
            continue
        node = sock.links[0].from_node
        while node.type != "TEX_IMAGE" and node.inputs and any(i.links for i in node.inputs):
            node = next(i.links[0].from_node for i in node.inputs if i.links)  # through mix / colour nodes
        if node.type == "TEX_IMAGE" and node.image:
            return node.image
    return None


def clean_material(mesh):
    """Keep the colour atlas on a plain rough, non-metallic lit surface: drop emission (generated GLBs often bind
    the atlas as full-strength emission, so the character would look self-lit) and any exaggerated specular."""
    changed = []
    for slot in mesh.material_slots:
        mat = slot.material
        if not mat or not mat.use_nodes:
            continue
        nt = mat.node_tree
        bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if not bsdf:
            continue
        for name, value in (("Emission Color", (0.0, 0.0, 0.0, 1.0)), ("Emission Strength", 0.0),
                            ("Specular Tint", (1.0, 1.0, 1.0, 1.0)), ("Specular IOR Level", 0.5), ("IOR", 1.5),
                            ("Metallic", 0.0), ("Roughness", 0.75)):
            sock = bsdf.inputs.get(name)
            if not sock:
                continue
            for link in list(sock.links):
                nt.links.remove(link)
            sock.default_value = value
        changed.append(mat.name)
    return changed


def save_atlas(mesh, path):
    """Writes the source's base-colour texture byte-for-byte (no colour management) to path."""
    img = base_color_image(mesh)
    if img is None:
        return ""
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    if img.packed_file:
        with open(path, "wb") as f:
            f.write(img.packed_file.data)
    else:
        shutil.copyfile(bpy.path.abspath(img.filepath), path)
    return path


def vertex_lightness(mesh):
    """Per-vertex luminance of the base-colour texture (None without a textured material and UVs)."""
    img = base_color_image(mesh)
    me = mesh.data
    if img is None or not me.uv_layers.active or img.size[0] == 0:
        return None
    w, h = img.size
    px = np.empty(w * h * img.channels, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, img.channels)
    nl = len(me.loops)
    uv = np.empty(nl * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(nl, 2)
    vi = np.empty(nl, dtype=np.int64)
    me.loops.foreach_get("vertex_index", vi)
    tx = np.clip((uv[:, 0] % 1.0) * w, 0, w - 1).astype(int)
    ty = np.clip((uv[:, 1] % 1.0) * h, 0, h - 1).astype(int)  # pixel rows start at the bottom, like UV v
    lum = px[ty, tx, :3] @ np.array([0.2126, 0.7152, 0.0722])
    acc = np.zeros(len(me.vertices))
    cnt = np.zeros(len(me.vertices))
    np.add.at(acc, vi, lum)
    np.add.at(cnt, vi, 1.0)
    return acc / np.maximum(cnt, 1.0)


# ------------------------------------------------------------------------------------------------ landmarks
def landmarks_from_mesh(co, height):
    """Silhouette landmarks for a T-posed body (fallback when the source has no usable rig)."""
    z = co[:, 2]

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


def source_topology(arm, height, weighted=None):
    """Names of the source rig's pelvis, leg, spine, neck and arm chains, found from the layout alone (names are
    ignored). Only bones that carry skin weights (or have weighted descendants) are followed, so helper joints are
    ignored. Positions are read in pose space."""
    mw = arm.matrix_world
    bones = arm.data.bones
    pos = {b.name: (mw @ arm.pose.bones[b.name].head) for b in bones}
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
    topo = {"pelvis": pel.name, "legs": {}, "arms": {}, "neck": []}
    for ch in down_chains(pel):
        topo["legs"]["L" if pos[ch[0].name].x > 0 else "R"] = [b.name for b in ch]
    ups = [chain_down(c) for c in pel.children if pos[chain_down(c)[-1].name].z > pz + 0.25 * height]
    spine = max(ups, key=len) if ups else chain_down(pel)

    # chest = the spine bone whose child chains reach far out sideways (the arms)
    def side_chains(b):
        return [chain_down(c) for c in b.children if abs(pos[chain_down(c)[-1].name].x) > 0.25 * height]
    chest = next((b for b in spine if len(side_chains(b)) >= 2), spine[min(len(spine) - 1, 3)])
    topo["chest"] = chest.name
    cz = pos[chest.name]
    ups = [chain_down(c) for c in chest.children if pos[chain_down(c)[-1].name].z > cz.z + 0.05 * height
           and abs(pos[chain_down(c)[-1].name].x) < 0.1 * height]
    if ups:
        topo["neck"] = [b.name for b in max(ups, key=len)]
    for ch in side_chains(chest):
        topo["arms"]["L" if pos[ch[-1].name].x > 0 else "R"] = [b.name for b in ch]
    return topo


def straighten_arms(arm, topo, min_droop=MIN_ARM_DROOP):
    """Generated meshes often come A-posed (arms 30-50 degrees below horizontal). The fitted skeleton keeps Rudeus's
    T-pose rest orientations, so each arm segment (upper arm, then forearm) is rotated in pose space until it points
    straight out sideways; the hand follows its forearm. The source rig's own skin weights carry the mesh, and the
    pose is baked into it later. Returns the angle each arm was off a T-pose, in degrees."""
    mw = arm.matrix_world
    mw3 = mw.to_3x3().normalized()
    inv3 = mw3.inverted()
    pb = arm.pose.bones
    out = {}
    for side, ch in sorted(topo["arms"].items()):
        if len(ch) < 4:
            continue
        target = Vector((L.SIDES[side], 0.0, 0.0))

        def head(n):
            return mw @ pb[n].head
        off = math.degrees((head(ch[3]) - head(ch[1])).angle(target))
        out[side] = off
        if off < min_droop:
            continue
        for a, b in ((ch[1], ch[2]), (ch[2], ch[3])):
            q = (head(b) - head(a)).rotation_difference(target)
            R = (inv3 @ q.to_matrix() @ mw3).to_4x4()
            M = pb[a].matrix.copy()
            pivot = M.to_translation()
            pb[a].matrix = Matrix.Translation(pivot) @ R @ Matrix.Translation(-pivot) @ M
            bpy.context.view_layer.update()
    return out


def landmarks_from_armature(arm, topo):
    """Template bone -> landmark position, read from the source rig's POSE (after any A-pose correction)."""
    mw = arm.matrix_world

    def pos(n):
        return mw @ arm.pose.bones[n].head
    out = {L.PELVIS: pos(topo["pelvis"])}
    for side, ch in topo["legs"].items():
        for tb, sb in zip((L.THIGH[side], L.SHIN[side], L.FOOT[side], L.TOE[side]), ch):
            out[tb] = pos(sb)
    cz = pos(topo["chest"])
    for tb, f in zip(L.SPINE, (0.18, 0.55, 1.0)):
        out[tb] = out[L.PELVIS].lerp(cz, f)
    if topo["neck"]:
        out[L.NECK] = pos(topo["neck"][0])
        if len(topo["neck"]) > 1:
            out[L.HEAD] = pos(topo["neck"][1])
    for side, ch in topo["arms"].items():
        # clavicle, upper arm, forearm, hand
        for tb, sb in zip((L.CLAV[side], L.UPPER[side], L.FORE[side], L.HAND[side]), ch):
            out[tb] = pos(sb)
    return out


def rest_knee_forward(arm):
    """How far (m) Rudeus's rest knee sits in front of his straight hip-ankle line. Fitted knees keep that small
    bias, which is what tells two-bone IK and the rest pose which way the knee bends."""
    b = arm.data.bones
    hip, knee, ankle = (b[n].head_local for n in (L.THIGH["L"], L.SHIN["L"], L.FOOT["L"]))
    ha = ankle - hip
    t = (knee - hip).dot(ha) / ha.length_squared
    return -(knee - (hip + ha * t)).y


def fix_knees(lm, height, knee_fwd):
    """Auto-riggers can drop the knee joint inside a long coat's silhouette, well in front of the actual leg (the
    rest legs are straight). Such a knee is moved onto the hip-ankle line at the same height fraction, plus the
    template's forward bias. Returns each knee's distance from that spot before the fix, in cm."""
    out = {}
    for s in "LR":
        hip, knee, ankle = lm.get(L.THIGH[s]), lm.get(L.SHIN[s]), lm.get(L.FOOT[s])
        if hip is None or knee is None or ankle is None:
            continue
        ha = ankle - hip
        t = max(0.25, min(0.75, (knee - hip).dot(ha) / ha.length_squared))
        spot = hip + ha * t + Vector((0.0, -knee_fwd, 0.0))
        err = (knee - spot).length
        out[s] = round(err * 100, 1)
        if err > KNEE_TOLERANCE * height:
            lm[L.SHIN[s]] = spot
    return out


def symmetrize_landmarks(lm):
    """Mirror-average the left and right landmarks and centre the spine. Generated meshes are near-symmetric but their
    auto-rigs are not (Orsted's: right shin 6 mm shorter, right ankle 4 mm higher, right shoulder 12 mm lower). Every
    authoring tool measures one side and mirrors it (leg IK uses the left leg's lengths for both legs), so an asymmetric
    skeleton would miss its IK targets and slide planted feet. Returns the largest left/right mismatch in cm."""
    worst = 0.0
    for table in (L.THIGH, L.SHIN, L.FOOT, L.TOE, L.CLAV, L.UPPER, L.FORE, L.HAND):
        a, b = lm.get(table["L"]), lm.get(table["R"])
        if a is None or b is None:
            continue
        mirrored = Vector((-b.x, b.y, b.z))
        worst = max(worst, (a - mirrored).length)
        avg = (a + mirrored) * 0.5
        lm[table["L"]] = avg
        lm[table["R"]] = Vector((-avg.x, avg.y, avg.z))
    for n in [L.PELVIS] + L.SPINE + [L.NECK, L.HEAD]:
        if n in lm:
            lm[n] = Vector((0.0, lm[n].y, lm[n].z))
    return round(worst * 100, 1)


# ------------------------------------------------------------------------------------------------ fitting
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


def fit_fingers(template_arm, co, height):
    """Scale the finger chains about the wrist so the middle finger ends at the mesh's hand tip (the template's fingers
    are Rudeus's, scaled with the body). Both hands get the mean of the two measurements, so the skeleton stays
    symmetric. Returns the scale measured per side and the one applied."""
    bones = template_arm.data.bones
    measured = {}
    for s in "LR":
        sign = L.SIDES[s]
        wrist = bones[L.HAND[s]].head_local.copy()
        near = (np.abs(co[:, 2] - wrist.z) < 0.06 * height) & (np.abs(co[:, 1] - wrist.y) < 0.06 * height)
        beyond = co[near & (co[:, 0] * sign > wrist.x * sign)]
        if not len(beyond):
            continue
        mesh_len = float((beyond[:, 0] * sign).max() - wrist.x * sign)
        mid = [bones[b].head_local for b in L.FINGERS[s][2]]
        tip = mid[-1] + (mid[-1] - mid[-2])            # one more phalanx past the last joint
        tmpl_len = (tip.x - wrist.x) * sign
        measured[s] = min(1.4, max(0.7, 0.97 * mesh_len / max(1e-6, tmpl_len)))
    if not measured:
        return {}
    f = sum(measured.values()) / len(measured)
    pos = {}
    for s in "LR":
        wrist = bones[L.HAND[s]].head_local.copy()
        for chain in L.FINGERS[s]:
            for b in chain:
                pos[b] = wrist + (bones[b].head_local - wrist) * f
    fit_template(template_arm, 1.0, pos)
    return {**{s: round(v, 3) for s, v in measured.items()}, "applied": round(f, 3)}


def chain_names(arm, root):
    prefix = root.split("_jnt")[0][:-1]          # "skirt_L0_0" -> "skirt_L0_"
    bones = arm.data.bones
    chain, n = [root], root
    while True:
        nxt = [c.name for c in bones[n].children if c.name.startswith(prefix)]
        if not nxt:
            return chain
        n = nxt[0]
        chain.append(n)


def body_next_joint():
    """Deforming bone -> the joint its body part runs to. Bone tails in this rig point sideways and several joints
    have helper children (shin: foot + twist, forearm: hand + twist + sleeve, chest: neck + clavicles + hood), so the
    segment a body part is weighted along must come from anatomy, not from the tail or the child count."""
    nxt = {a: b for a, b in zip(L.SPINE, L.SPINE[1:])}
    nxt[L.SPINE[-1]] = L.NECK
    nxt[L.NECK] = L.HEAD
    for s in "LR":
        nxt.update({L.CLAV[s]: L.UPPER[s], L.UPPER[s]: L.FORE[s], L.FORE[s]: L.HAND[s], L.HAND[s]: L.FINGERS[s][2][0],
                    L.THIGH[s]: L.SHIN[s], L.SHIN[s]: L.FOOT[s], L.FOOT[s]: L.TOE[s]})
        for chain in L.FINGERS[s]:
            nxt.update(zip(chain, chain[1:]))
    return nxt


def bone_segments(arm):
    bones = arm.data.bones
    mw = arm.matrix_world

    def head(n):
        return np.array(mw @ bones[n].head_local)
    nxt = body_next_joint()
    ends = [chain[-1] for s in "LR" for chain in L.FINGERS[s]] + [L.TOE[s] for s in "LR"]
    seg = {}
    for b in bones:
        a = head(b.name)
        if b.name in nxt:
            end = head(nxt[b.name])
        elif b.name in ends:        # finger tips and toes: continue the chain by 60% of the last segment
            end = a + (a - head(b.parent.name)) * 0.6
        elif b.name == L.HEAD:      # up through the skull, continuing the neck line
            end = a + (a - head(L.NECK)) * 1.2
        elif b.name == L.PELVIS:    # down between the hip joints
            end = (head(L.THIGH["L"]) + head(L.THIGH["R"])) * 0.5
        else:
            kids = [c for c in b.children if not c.name.startswith(("skirt", "hair", "hood", "bag", "belt", "sleeve", "eye", "tear", "item"))]
            end = head(kids[0].name) if len(kids) == 1 else np.array(mw @ b.tail_local)
        seg[b.name] = (a, end)
    return seg


def segment_distance(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(1e-9, ab @ ab), 0.0, 1.0)
    return np.linalg.norm(p - (a + np.outer(t, ab)), axis=1)


def coat_mask(co, seg, height, light=None):
    """Coat cloth below the waist: outside both leg volumes, or pale cloth (by the texture) hanging close to them.
    Trousers and boots stay on the legs."""
    dl = np.minimum(segment_distance(co, *seg[L.THIGH["L"]]), segment_distance(co, *seg[L.SHIN["L"]]))
    dr = np.minimum(segment_distance(co, *seg[L.THIGH["R"]]), segment_distance(co, *seg[L.SHIN["R"]]))
    d = np.minimum(dl, dr)
    z = co[:, 2]
    waist = seg[L.PELVIS][0][2]
    loose = d > LEG_RADIUS_FRACTION * height
    if light is not None:
        loose |= (light > COAT_LIGHTNESS) & (d > COAT_NEAR_FRACTION * height)
    return (z < waist) & (z > 0.12 * height) & loose


def fit_coat_chains(template_arm, waist_z, hem_z):
    """Spread each coat chain from the waist down to the coat's hem (the template's chains are sized for Rudeus's
    shorter robe). Joints keep their fitted place around the body and are re-spaced evenly in height; the chain's
    end joint sits half a segment below the hem."""
    bones = template_arm.data.bones
    pos = {}
    for root in L.SKIRT:
        chain = chain_names(template_arm, root)
        n = len(chain)
        if n < 3:
            continue
        heads = [bones[b].head_local.copy() for b in chain]
        for k in range(n - 1):
            h = heads[k].copy()
            h.z = waist_z - (waist_z - hem_z) * k / (n - 2)
            pos[chain[k]] = h
        last, prev = pos[chain[n - 2]], pos[chain[n - 3]]
        pos[chain[n - 1]] = last + (last - prev) * 0.5
    fit_template(template_arm, 1.0, pos)


def weld_index(co, tol=1e-5):
    """Generated meshes are split at every UV seam (Orsted: 55.9k vertices at 19.6k positions). Copies of a position
    must deform identically or the seams crack open, so weights are computed once per welded position.
    Returns (vertex -> welded index, welded index -> one representative vertex)."""
    key = np.round(co / tol).astype(np.int64)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    return inv.ravel(), first


def welded_edges(mesh, inv):
    me = mesh.data
    e = np.empty(len(me.edges) * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", e)
    e = inv[e.reshape(-1, 2)]
    e = e[e[:, 0] != e[:, 1]]
    return np.unique(np.sort(e, axis=1), axis=0)


def neighbour_sum(values, edges, n):
    """Sum over graph neighbours of a per-vertex array (1-D, or 2-D column by column)."""
    if values.ndim == 1:
        return (np.bincount(edges[:, 0], weights=values[edges[:, 1]], minlength=n)
                + np.bincount(edges[:, 1], weights=values[edges[:, 0]], minlength=n))
    return np.stack([neighbour_sum(values[:, c], edges, n) for c in range(values.shape[1])], 1)


def smooth_rows(W, edges, rows, iterations, alpha=0.5):
    """Laplacian smoothing of weight rows over the welded surface, only for the selected rows."""
    n = len(W)
    deg = np.bincount(edges[:, 0], minlength=n) + np.bincount(edges[:, 1], minlength=n)
    upd = rows & (deg > 0)
    for _ in range(iterations):
        avg = neighbour_sum(W, edges, n) / np.maximum(deg, 1)[:, None]
        W[upd] = (1.0 - alpha) * W[upd] + alpha * avg[upd]
    return W


def majority_vote(mask, edges, region, iterations=2):
    """Isolated misclassified vertices (a dark hem edge on pale coat, a pale fleck on the trousers) take the
    majority of their neighbourhood, so no single vertex is pulled between coat and leg."""
    n = len(mask)
    deg = np.bincount(edges[:, 0], minlength=n) + np.bincount(edges[:, 1], minlength=n)
    m = mask.astype(float)
    for _ in range(iterations):
        frac = (m + neighbour_sum(m, edges, n)) / (1.0 + deg)
        m = np.where(region, (frac > 0.5).astype(float), m)
    return m > 0.5


LOWER_SMOOTH_ITERATIONS = 6   # below the waist: spread the coat / trouser boundary over a few rings
SHOULDER_SMOOTH_ITERATIONS = 4
SHOULDER_SMOOTH_RADIUS = 0.06  # of body height, around each shoulder joint
BODY_SMOOTH_ITERATIONS = 1    # everywhere: soften the region-gating edges of the heat weights
MAX_INFLUENCES = 4


def bind_weights(mesh, arm, height, coat=True, light=None, hem_z=None):
    """Heat-like weights over the fitted bone segments, region-gated, plus coat chains. Computed once per welded
    position (UV-seam copies deform identically), then smoothed over the welded surface, strongest below the
    waist, so the coat / trouser boundary bends instead of tearing. At most 4 influences per vertex."""
    co_all = world_verts(mesh)
    inv, first = weld_index(co_all)
    co = co_all[first]
    nu = len(co)
    if light is not None:
        light = np.bincount(inv, weights=light, minlength=nu) / np.maximum(np.bincount(inv, minlength=nu), 1)
    edges = welded_edges(mesh, inv)
    for g in list(mesh.vertex_groups):
        mesh.vertex_groups.remove(g)
    seg = bone_segments(arm)
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
    cols = list(deform)
    coat_rows = np.zeros(nu, dtype=bool)
    W = None
    waist = seg[L.PELVIS][0][2]
    if coat:
        roots = list(L.SKIRT.keys())
        chains = {}
        for root in roots:
            chain = chain_names(arm, root)
            cols += chain
            # with a measured hem the chain's end joint (below the hem) carries no cloth
            chains[root] = chain[:-1] if hem_z is not None and len(chain) > 2 else chain
    col = {n: k for k, n in enumerate(cols)}
    W = np.zeros((nu, len(cols)))
    if coat:
        # robe below the waist -> coat chains by angle around the body, waist (0) .. hem (1) along the chain
        bottom = hem_z if hem_z is not None else 0.12 * height
        region = (z < waist) & (z > 0.12 * height)
        coat_rows = majority_vote(coat_mask(co, seg, height, light), edges, region) & region
        angle = {r: math.atan2(seg[r][0][0], -seg[r][0][1]) for r in roots}
        # Around the body the cloth blends linearly between the two neighbouring chains, so adjacent panels never
        # tear apart; only the front opening (between the two front chains) stays a hard split.
        ring = sorted(roots, key=lambda r: angle[r])
        front = {r for r in roots if L.SKIRT[r][1] == 0.0}
        ang = np.arctan2(co[:, 0], -co[:, 1])
        for vi in np.nonzero(coat_rows)[0]:
            a = ang[vi]
            below = max((r for r in ring if angle[r] <= a), key=lambda r: angle[r], default=ring[-1])
            above = min((r for r in ring if angle[r] > a), key=lambda r: angle[r], default=ring[0])
            span = math.remainder(angle[above] - angle[below], 2 * math.pi) % (2 * math.pi)
            t = (math.remainder(a - angle[below], 2 * math.pi) % (2 * math.pi)) / max(1e-6, span)
            pair = [(below, 1.0 - t), (above, t)]
            if below in front and above in front:     # the open front: nearest front panel only
                pair = [(below, 1.0)] if t < 0.5 else [(above, 1.0)]
            h = min(1.0, max(0.0, (waist - z[vi]) / max(1e-6, waist - bottom)))   # 0 at waist .. 1 at hem
            for root, wa in pair:
                chain = chains[root]
                pos = min(len(chain) - 1.001, h * (len(chain) - 1))
                i0 = int(pos)
                f = pos - i0
                W[vi, col[chain[i0]]] += wa * (1.0 - f)
                W[vi, col[chain[i0 + 1]]] += wa * f
    body = np.nonzero(~coat_rows)[0]
    for j in range(k_near):
        np.add.at(W, (body, idx[body, j]), np.where(w[body, j] > 0.02, w[body, j], 0.0))
    W = smooth_rows(W, edges, z < waist + 0.03 * height, LOWER_SMOOTH_ITERATIONS)
    # Shoulder caps: the rest pose is a T-pose, so lowering the arms turns the shoulder 75-80 degrees; spread the
    # clavicle / upper-arm / chest hand-over across the cap instead of a crease on top of the joint.
    shoulders = np.minimum(np.linalg.norm(co - seg[L.UPPER["L"]][0], axis=1),
                           np.linalg.norm(co - seg[L.UPPER["R"]][0], axis=1)) < SHOULDER_SMOOTH_RADIUS * height
    W = smooth_rows(W, edges, shoulders, SHOULDER_SMOOTH_ITERATIONS)
    W = smooth_rows(W, edges, np.ones(nu, dtype=bool), BODY_SMOOTH_ITERATIONS)
    # keep the strongest influences, renormalised
    top = np.argsort(-W, axis=1)[:, :MAX_INFLUENCES]
    tw = np.take_along_axis(W, top, 1)
    tw[tw < 0.01] = 0.0
    tw /= np.maximum(tw.sum(1, keepdims=True), 1e-9)
    groups = [mesh.vertex_groups.new(name=n) for n in cols]
    for vi in range(len(co_all)):
        u = inv[vi]
        for k in range(MAX_INFLUENCES):
            if tw[u, k] > 0.0:
                groups[top[u, k]].add([vi], float(tw[u, k]), "REPLACE")
    # parent to the armature with an Armature modifier
    for m in list(mesh.modifiers):
        if m.type == "ARMATURE":
            mesh.modifiers.remove(m)
    world = mesh.matrix_world.copy()
    mesh.parent = arm
    mesh.matrix_world = world
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return int(coat_rows[inv].sum())


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
    ap.add_argument("--atlas-out", default="", help="also save the source's colour texture here (QA sheets, docs)")
    a = ap.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
    template, rudeus_mesh = scene.load_rudeus()
    knee_fwd = rest_knee_forward(template) * a.height / TEMPLATE_HEIGHT
    bpy.data.objects.remove(rudeus_mesh, do_unlink=True)
    src_mesh, src_arm, imported = import_source(a.inp)
    print("material cleaned (emission off, plain specular):", clean_material(src_mesh))
    atlas = save_atlas(src_mesh, a.atlas_out) if a.atlas_out else ""
    if atlas:
        print("colour atlas:", atlas)
    topo = None
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
        src_height = float(np.ptp(world_verts(src_mesh)[:, 2]))
        topo = source_topology(src_arm, src_height, weighted_bones(src_mesh))
        off = straighten_arms(src_arm, topo)
        print("arms off a T-pose (deg):", {s: round(v, 1) for s, v in off.items()},
              "-> straightened" if any(v >= MIN_ARM_DROOP for v in off.values()) else "-> kept")
    normalise_mesh(src_mesh, a.height)
    lm_pos = landmarks_from_armature(src_arm, topo) if src_arm else {}
    print("landmarks from source rig:", len(lm_pos))
    if lm_pos:
        print("knees off the hip-ankle line (cm, refitted above %.1f):" % (KNEE_TOLERANCE * a.height * 100),
              fix_knees(lm_pos, a.height, knee_fwd))
        print("left/right landmark mismatch before symmetrising (cm):", symmetrize_landmarks(lm_pos))
    if src_arm:
        # drop the source rig: bake its (T-)pose into the mesh, remove the armature
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
    ratio = a.height / TEMPLATE_HEIGHT
    fit_template(template, ratio, lm_pos)       # joint landmarks from the source rig (if any)
    # unrigged sources only: refine arms to the T-pose silhouette span (rig landmarks are exact)
    bones = template.data.bones
    arm_scale = 1.0
    if L.HAND["L"] not in lm_pos:
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
    print("finger chains scaled to the hands:", fit_fingers(template, co, a.height))
    light = vertex_lightness(src_mesh)
    hem_z = None
    if not a.no_coat:
        seg = bone_segments(template)
        mask = coat_mask(co, seg, a.height, light)
        if mask.sum() > 50:
            hem_z = float(np.percentile(co[mask][:, 2], 1.0))
            waist_z = float(seg[L.PELVIS][0][2])
            fit_coat_chains(template, waist_z, hem_z)
            print("coat chains: waist %.3f m -> hem %.3f m (%d cloth vertices)" % (waist_z, hem_z, int(mask.sum())))
    n_coat = bind_weights(src_mesh, template, a.height, coat=not a.no_coat, light=light, hem_z=hem_z)
    if a.qa_sheet:
        render_qa(template, src_mesh, a.qa_sheet, a.texture or atlas)
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
