"""Semantic pose -> bone rotation deltas for the Rudeus rig.

A pose is a dict of named parameters (all optional, degrees / metres). compose(rig, P) returns
(D, pelvis_offset). Legs use analytic IK with planted feet unless P['legs'] == 'fk'.
Axes (armature space): +X = character's left, -Y = forward, +Z = up.
"""
import math
from mathutils import Matrix, Vector
import rudeus_anim_lib as L
from rudeus_anim_lib import rot

DEFAULTS = {
    # pelvis
    "pel_x": 0.0, "pel_y": 0.0, "pel_z": -0.03, "pel_yaw": 0.0, "pel_pitch": 0.0, "pel_roll": 0.0,
    # spine (totals, distributed over three joints), neck, head
    "sp_pitch": 3.0, "sp_yaw": 0.0, "sp_roll": 0.0,
    "neck_pitch": 0.0, "neck_yaw": 0.0, "head_pitch": -2.0, "head_yaw": 0.0, "head_roll": 0.0,
    # arms
    "L_lower": 74.0, "L_swing": 0.0, "L_fwd": 0.0, "L_twist": 0.0, "L_elbow": 14.0, "L_wpitch": 0.0, "L_wyaw": 0.0, "L_spread": 0.0,
    "R_lower": 74.0, "R_swing": 0.0, "R_fwd": 0.0, "R_twist": 0.0, "R_elbow": 14.0, "R_wpitch": 0.0, "R_wyaw": 0.0, "R_spread": 0.0,
    "L_curl": 18.0, "R_curl": 18.0, "L_thumb": 12.0, "R_thumb": 12.0,
    "L_clav": 0.0, "R_clav": 0.0,
    # legs: IK foot targets (ground-relative ankle placement) or FK angles
    "legs": "ik",
    "L_fx": 0.095, "L_fy": 0.0, "L_fz": 0.0, "L_fyaw": 4.0, "L_fpitch": 0.0, "L_pivot": 0.0,
    "R_fx": -0.095, "R_fy": 0.0, "R_fz": 0.0, "R_fyaw": -4.0, "R_fpitch": 0.0, "R_pivot": 0.0,
    "L_knee_out": 0.12, "R_knee_out": 0.12,
    "L_hip": 0.0, "L_hipabd": 0.0, "L_knee": 0.0, "L_ankle": 0.0,
    "R_hip": 0.0, "R_hipabd": 0.0, "R_knee": 0.0, "R_ankle": 0.0,
    # secondary
    "coat_trail": 0.0, "coat_flare": 0.0, "hair_lag": 0.0, "hair_side": 0.0,
}

BALL_FWD = 0.080   # ball (toe joint) is 8 cm in front of the ankle, 1.9 cm above ground
BALL_H = 0.019
HEEL_BACK = 0.045  # heel contact point behind the ankle


def rel(pitch=0.0, yaw=0.0, roll=0.0):
    """Rotation in rest (world) axes: +pitch bends forward, +yaw turns left, +roll bends left."""
    return rot((0, 0, 1), yaw) @ rot((1, 0, 0), pitch) @ rot((0, 1, 0), roll)


def foot_orientation(yaw, pitch_up):
    """World orientation of a foot: yaw about Z, then pitch (toes up positive) about the foot's lateral axis."""
    return rot((0, 0, 1), yaw) @ rot((1, 0, 0), -pitch_up)


def ankle_from_contact(rig, side, P):
    """Ankle target + foot orientation from the foot's ground placement.
    pitch > 0: toes up, rotating about the heel contact; pitch < 0: heel up, rotating about the ball."""
    s = side
    yaw, pitch = P[s + "_fyaw"], P[s + "_fpitch"]
    base = Vector((P[s + "_fx"], P[s + "_fy"], P[s + "_fz"]))   # ground point under the ankle (flat foot)
    fwd = rot((0, 0, 1), yaw) @ Vector((0.0, -1.0, 0.0))
    R = foot_orientation(yaw, pitch)
    h = rig.ankle_h
    if pitch < 0.0:
        ball = base + fwd * BALL_FWD + Vector((0.0, 0.0, BALL_H))
        return ball + R @ Vector((0.0, BALL_FWD, h - BALL_H)), R
    if pitch > 0.0:
        heel = base - fwd * HEEL_BACK
        return heel + R @ Vector((0.0, -HEEL_BACK, h)), R
    return base + Vector((0.0, 0.0, h)), R


def chain_of(rig, root):
    prefix = root.split("_jnt")[0][:-1]          # "skirt_L0_0" -> "skirt_L0_"
    chain, n = [root], root
    while True:
        nxt = [c for c in rig.children[n] if c.startswith(prefix)]
        if not nxt:
            return chain
        n = nxt[0]
        chain.append(n)


def compose(rig, params):
    P = dict(DEFAULTS)
    P.update(params)
    D = {}
    pel_off = Vector((P["pel_x"], P["pel_y"], P["pel_z"]))
    d_pel = rel(P["pel_pitch"], P["pel_yaw"], P["pel_roll"])
    D[L.PELVIS] = d_pel
    # spine distribution 40/35/25
    acc = d_pel
    for bone, w in zip(L.SPINE, (0.40, 0.35, 0.25)):
        acc = acc @ rel(P["sp_pitch"] * w, P["sp_yaw"] * w, P["sp_roll"] * w)
        D[bone] = acc
    chest = acc
    d_neck = chest @ rel(P["neck_pitch"], P["neck_yaw"], 0.0)
    D[L.NECK] = d_neck
    d_head = d_neck @ rel(P["head_pitch"], P["head_yaw"], P["head_roll"])
    D[L.HEAD] = d_head
    for h in L.HAIR:
        D[h] = d_head @ rel(P["hair_lag"], 0.0, P["hair_side"])

    # arms relative to the chest
    for s in "LR":
        sign = L.SIDES[s]
        D[L.CLAV[s]] = chest @ rot((0, 1, 0), -sign * P[s + "_clav"])
        a = L.arm_D(s, lower=P[s + "_lower"], swing=P[s + "_swing"], twist=P[s + "_twist"], elbow=P[s + "_elbow"],
                    wrist_pitch=P[s + "_wpitch"], wrist_yaw=P[s + "_wyaw"], raise_fwd=P[s + "_fwd"], spread=P[s + "_spread"])
        for bone, m in a.items():
            D[bone] = chest @ m
        D.update(L.finger_D(s, D[L.HAND[s]], P[s + "_curl"], P[s + "_thumb"]))

    # solve once for hip positions (legs depend on the pelvis pose)
    full = rig.apply(D, pel_off)
    if P.get("auto_reach") and P.get("ik_w", 1.0 if P["legs"] == "ik" else 0.0) >= 1.0:
        # Lower the pelvis just enough that neither leg over-extends: planted feet can then be
        # reached exactly (no sliding), and the dip lands naturally at the widest stride.
        max_len = 0.985 * (rig.l_thigh + rig.l_shin)
        drop = 0.0
        for s in "LR":
            hip = rig.world_head(L.THIGH[s], full, pel_off)
            tgt, _ = ankle_from_contact(rig, s, P)
            v = tgt - hip
            horiz = math.hypot(v.x, v.y)
            if v.length > max_len and horiz < max_len:
                drop = max(drop, (-v.z) - math.sqrt(max_len * max_len - horiz * horiz))
        if drop > 0.0:
            pel_off = pel_off - Vector((0.0, 0.0, drop))
            full = rig.apply(D, pel_off)
    thigh_fwd = {}
    ik_w = P.get("ik_w")
    if ik_w is None:
        ik_w = 0.0 if P["legs"] == "fk" else 1.0
    for s in "LR":
        sign = L.SIDES[s]
        leg_bones = (L.THIGH[s], L.SHIN[s], L.FOOT[s], L.TOE[s])
        fk, ik = None, None
        if ik_w < 1.0:
            # legs hang down: flexing forward is a negative rotation about +X, knee flexion positive,
            # abduction rotates the foot outward (away from the midline)
            d_th = d_pel @ rel(-P[s + "_hip"], 0.0, -sign * P[s + "_hipabd"])
            d_sh = d_th @ rel(P[s + "_knee"], 0.0, 0.0)
            d_ft = d_sh @ rel(-P[s + "_ankle"], 0.0, 0.0)
            fk = dict(zip(leg_bones, (d_th, d_sh, d_ft, d_ft)))
        if ik_w > 0.0:
            hip = rig.world_head(L.THIGH[s], full, pel_off)
            ankle, foot_R = ankle_from_contact(rig, s, P)
            yaw_mid = 0.5 * (P["pel_yaw"] + P[s + "_fyaw"])
            pole = rot((0, 0, 1), yaw_mid) @ Vector((sign * P[s + "_knee_out"], -1.0, 0.0))
            legD, _ = rig.leg_ik(s, hip, ankle, pole=pole.normalized())
            toe = rot((0, 0, 1), P[s + "_fyaw"]) if P[s + "_fpitch"] < 0 else foot_R
            ik = {L.THIGH[s]: legD[L.THIGH[s]], L.SHIN[s]: legD[L.SHIN[s]], L.FOOT[s]: foot_R, L.TOE[s]: toe}
        for b in leg_bones:
            if fk is None:
                D[b] = ik[b]
            elif ik is None:
                D[b] = fk[b]
            else:
                D[b] = fk[b].to_quaternion().slerp(ik[b].to_quaternion(), ik_w).to_matrix()
        # thigh swing measured in the pelvis frame (the panels are parented to the pelvis)
        th_dir = d_pel.transposed() @ (D[L.THIGH[s]] @ (rig.head[L.SHIN[s]] - rig.head[L.THIGH[s]]))
        thigh_fwd[s] = math.degrees(math.atan2(-th_dir.y, -th_dir.z))  # + = thigh swung forward

    # coat panels follow the thighs so legs never poke through, plus trailing / flare
    for root, (side, pos) in L.SKIRT.items():
        a = thigh_fwd[side]
        fa = max(0.0, a)
        fa = fa if fa < 35.0 else 35.0 + (fa - 35.0) * 0.55   # big knee lifts push the robe less than 1:1
        follow_front = fa * (1.0 - pos) * 1.05
        follow_back = max(0.0, -a) * pos * 0.95
        other = thigh_fwd["R" if side == "L" else "L"]
        follow_front = max(follow_front, max(0.0, other) * (1.0 - pos) * 0.35)
        swing = follow_front - follow_back - P["coat_trail"]
        sign = L.SIDES[side]
        # panels hang down: swinging them forward is a negative rotation about +X; flare pushes them outward
        d_root = d_pel @ rel(-swing * 0.85, 0.0, -sign * P["coat_flare"] * (0.3 + 0.7 * (0.5 - abs(pos - 0.5))))
        chain = chain_of(rig, root)
        D[chain[0]] = d_root
        acc = d_root
        for k, b in enumerate(chain[1:], start=1):
            acc = acc @ rel(-swing * 0.06 * k, 0.0, 0.0)
            D[b] = acc
    return D, pel_off


def apply_pose(rig, params):
    D, pel = compose(rig, params)
    rig.apply(D, pel)
    return D, pel
