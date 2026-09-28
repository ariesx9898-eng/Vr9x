"""Procedural animation authoring library for the Rudeus rig (runs inside Blender's bpy).

Why this exists: Mixamo needs an interactive Adobe login, so the locomotion/combat set is authored
here directly on Rudeus's own 136-joint skeleton (no retargeting losses).

Conventions (armature space of the imported GLB): +Z up, character faces -Y, character's left = +X.
Every pose is described semantically (pelvis offset/rotation, spine bend, arm lower/swing, elbow bend,
IK foot targets ...). The solver converts that into armature-space rotation deltas D (final
orientation = D @ rest orientation) and then into Blender pose-bone local rotations, so bone-axis
conventions of the imported rig never matter.
"""
import math
import bpy
from mathutils import Matrix, Vector, Quaternion

UP = Vector((0.0, 0.0, 1.0))
FWD = Vector((0.0, -1.0, 0.0))
LEFT = Vector((1.0, 0.0, 0.0))

PELVIS = "spine_C0_0_jnt_05"
SPINE = ["spine_C0_1_jnt_061", "spine_C0_2_jnt_062", "spine_C0_3_jnt_063"]
NECK = "neck_C0_0_jnt_066"
HEAD = "head_C0_0_jnt_067"
SIDES = {"L": 1.0, "R": -1.0}
CLAV = {"L": "shoulder_L0_0_jnt_091", "R": "shoulder_R0_0_jnt_0114"}
UPPER = {"L": "arm_L0_0_jnt_092", "R": "arm_R0_0_jnt_0115"}
FORE = {"L": "arm_L0_1_jnt_093", "R": "arm_R0_1_jnt_0116"}
HAND = {"L": "arm_L0_2_jnt_094", "R": "arm_R0_2_jnt_0117"}
THIGH = {"L": "leg_L0_0_jnt_08", "R": "leg_R0_0_jnt_013"}
SHIN = {"L": "leg_L0_1_jnt_09", "R": "leg_R0_1_jnt_014"}
FOOT = {"L": "leg_L0_2_jnt_010", "R": "leg_R0_2_jnt_015"}
TOE = {"L": "leg_L0_3_jnt_011", "R": "leg_R0_3_jnt_016"}
FINGERS = {
    "L": [["finger_L0_0_jnt_096", "finger_L0_1_jnt_097", "finger_L0_2_jnt_098"],
          ["finger_L1_0_jnt_099", "finger_L1_1_jnt_0100", "finger_L1_2_jnt_0101"],
          ["finger_L2_0_jnt_0102", "finger_L2_1_jnt_0103", "finger_L2_2_jnt_0104"],
          ["finger_L3_0_jnt_0105", "finger_L3_1_jnt_0106", "finger_L3_2_jnt_0107"],
          ["finger_L4_0_jnt_0108", "finger_L4_1_jnt_0109", "finger_L4_2_jnt_0110"]],
    "R": [["finger_R0_0_jnt_01", "finger_R0_1_jnt_0118", "finger_R0_2_jnt_0119"],
          ["finger_R1_0_jnt_0120", "finger_R1_1_jnt_0121", "finger_R1_2_jnt_0122"],
          ["finger_R2_0_jnt_0123", "finger_R2_1_jnt_0124", "finger_R2_2_jnt_0125"],
          ["finger_R3_0_jnt_0126", "finger_R3_1_jnt_0127", "finger_R3_2_jnt_0128"],
          ["finger_R4_0_jnt_0129", "finger_R4_1_jnt_0130", "finger_R4_2_jnt_0131"]],
}
SKIRT = {  # coat panels: (chain root, side, angular position: 0 front .. 1 back)
    "skirt_L0_0_jnt_018": ("L", 0.0), "skirt_L1_0_jnt_023": ("L", 0.35),
    "skirt_L2_0_jnt_028": ("L", 0.65), "skirt_L3_0_jnt_033": ("L", 1.0),
    "skirt_R0_0_jnt_038": ("R", 0.0), "skirt_R1_0_jnt_046": ("R", 0.35),
    "skirt_R2_0_jnt_051": ("R", 0.65), "skirt_R3_0_jnt_056": ("R", 1.0),
}
HAIR = ["hair_C0_0_jnt_075", "hair_C1_0_jnt_077", "hair_C2_0_jnt_079", "hair_L0_0_jnt_081",
        "hair_L1_0_jnt_083", "hair_R0_0_jnt_085", "hair_R1_0_jnt_087"]


def rot(axis, deg):
    return Matrix.Rotation(math.radians(deg), 3, Vector(axis).normalized())


def frame(u, v):
    """Orthonormal frame with first axis u and second axis the part of v perpendicular to u."""
    u = u.normalized()
    v = (v - u * v.dot(u)).normalized()
    w = u.cross(v)
    m = Matrix.Identity(3)
    m.col[0], m.col[1], m.col[2] = u, v, w
    return m


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


class Rig:
    def __init__(self, arm_obj):
        self.obj = arm_obj
        self.bones = arm_obj.data.bones
        self.pose = arm_obj.pose.bones
        self.rest3 = {b.name: b.matrix_local.to_3x3() for b in self.bones}
        self.head = {b.name: b.head_local.copy() for b in self.bones}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in self.bones}
        depth = {}
        for b in self.bones:
            d, p = 0, b.parent
            while p:
                d, p = d + 1, p.parent
            depth[b.name] = d
        self.order = sorted(self.parent, key=lambda n: depth[n])
        self.children = {n: [c.name for c in self.bones[n].children] for n in self.parent}
        for pb in self.pose:
            pb.rotation_mode = "QUATERNION"
        self.l_thigh = (self.head[SHIN["L"]] - self.head[THIGH["L"]]).length
        self.l_shin = (self.head[FOOT["L"]] - self.head[SHIN["L"]]).length
        self.ankle_h = self.head[FOOT["L"]].z
        self.pelvis_h = self.head[PELVIS].z

    def descendants(self, name):
        out, stack = [], [name]
        while stack:
            n = stack.pop()
            out.append(n)
            stack.extend(self.children[n])
        return out

    # ---------------------------------------------------------------- solve
    def apply(self, D, pelvis_offset=Vector((0, 0, 0))):
        """D: bone -> armature-space rotation delta (3x3). Unspecified bones inherit their parent's."""
        full = {}
        for n in self.order:
            p = self.parent[n]
            full[n] = D.get(n, full[p] if p else Matrix.Identity(3))
        for n in self.order:
            p = self.parent[n]
            rb = full[n] @ self.rest3[n]
            if p:
                rp = full[p] @ self.rest3[p]
                local_rest = self.rest3[p].transposed() @ self.rest3[n]
                local_final = rp.transposed() @ rb
            else:
                local_rest, local_final = self.rest3[n], rb
            basis = local_rest.transposed() @ local_final
            pb = self.pose[n]
            pb.rotation_quaternion = basis.to_quaternion()
            pb.location = (0, 0, 0)
        # Pelvis translation expressed in its rest frame (parents above it are never rotated).
        self.pose[PELVIS].location = self.rest3[PELVIS].transposed() @ Vector(pelvis_offset)
        return full

    def world_head(self, name, full, pelvis_offset):
        """Armature-space position of a bone head for a solved pose (rigid chain walk)."""
        chain = []
        n = name
        while n:
            chain.append(n)
            n = self.parent[n]
        chain.reverse()
        pos = self.head[chain[0]].copy()
        for i in range(1, len(chain)):
            c, p = chain[i], chain[i - 1]
            pos = pos + full[p] @ (self.head[c] - self.head[p])
            if c == PELVIS:
                pos = pos + Vector(pelvis_offset)
        return pos

    # ---------------------------------------------------------------- IK
    def leg_ik(self, side, hip_pos, ankle_target, pole=FWD, foot_D=None):
        """Analytic two-bone IK. Returns D for thigh, shin (and foot when foot_D given)."""
        l1, l2 = self.l_thigh, self.l_shin
        to = ankle_target - hip_pos
        d = max(abs(l1 - l2) + 1e-4, min(to.length, (l1 + l2) * 0.9995))
        dirv = to.normalized()
        a = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
        h = math.sqrt(max(0.0, l1 * l1 - a * a))
        pp = (pole - dirv * pole.dot(dirv))
        pp = pp.normalized() if pp.length > 1e-6 else FWD
        knee = hip_pos + dirv * a + pp * h
        ankle = hip_pos + dirv * d
        t, s = THIGH[side], SHIN[side]
        rest_thigh_dir = self.head[s] - self.head[t]
        rest_shin_dir = self.head[FOOT[side]] - self.head[s]
        # Rest legs are straight: use forward as the reference "knee" direction for the frames.
        d_thigh = frame(knee - hip_pos, pp) @ frame(rest_thigh_dir, FWD).transposed()
        d_shin = frame(ankle - knee, pp) @ frame(rest_shin_dir, FWD).transposed()
        out = {t: d_thigh, s: d_shin}
        if foot_D is not None:
            out[FOOT[side]] = foot_D
        return out, knee


def arm_D(side, lower=75.0, swing=0.0, twist=0.0, elbow=15.0, wrist_pitch=0.0, wrist_yaw=0.0, raise_fwd=0.0, spread=0.0):
    """Semantic arm pose from the T-pose.
    lower: degrees the arm drops from horizontal toward the body side.
    swing: forward (+) / backward (-) swing about the lateral axis (after lowering).
    raise_fwd: horizontal sweep toward the front at shoulder height (casting reach).
    elbow: flexion (forearm toward the front / chest)."""
    s = SIDES[side]
    r_twist = rot((1, 0, 0), s * twist)
    r_lower = rot((0, 1, 0), s * lower)
    r_sweep = rot((0, 0, 1), -s * raise_fwd)
    r_swing = rot((1, 0, 0), -swing)
    r_spread = rot((0, 0, 1), s * spread)
    d_upper = r_swing @ r_sweep @ r_spread @ r_lower @ r_twist
    d_fore = d_upper @ rot((0, 0, 1), -s * elbow)
    d_hand = d_fore @ rot((0, 1, 0), s * wrist_pitch) @ rot((0, 0, 1), -s * wrist_yaw)
    return {UPPER[side]: d_upper, FORE[side]: d_fore, HAND[side]: d_hand}


def finger_D(side, hand_D, curl=20.0, thumb=15.0):
    """Curl fingers about the hand's hinge axis (rest: fingers point along the arm)."""
    s = SIDES[side]
    out = {}
    for i, chain in enumerate(FINGERS[side]):
        amount = thumb if i == 0 else curl * (1.0 + 0.12 * (i - 1))
        acc = hand_D
        for j, bone in enumerate(chain):
            ax = (0, 0, 1) if i == 0 else (0, 1, 0)
            acc = acc @ rot(ax, (s if i else -s) * amount * (0.8 + 0.25 * j))
            out[bone] = acc
    return out
