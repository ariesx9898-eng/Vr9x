"""Automated animation quality checks on the Blender-evaluated (actually skinned) mesh, every frame.

    python3 Tools/anim/qa_animation_quality.py [--character Rudeus] [--clips ...]

Per clip it reports:
  pop_deg        largest single-frame rotation change of any major body bone (snapping detector)
  knee/elbow     flexion range in degrees (negative = hyperextension)
  wrist_twist    largest twist of the hand relative to the forearm about the forearm axis
  stretch        largest triangle edge-length ratio vs the bind pose (skin stretching / candy-wrapper)
                 and the bone region where it happens
  hand_in_body   closest distance (cm) from a wrist to the spine axis (hands passing through the torso)
  ankle_gap      closest distance (cm) between the two ankles (feet colliding)
  foot_slide     non-locomotion clips: largest horizontal travel (cm) of a foot while it stays flat
                 on the ground (planted feet must not skate; locomotion slip is measured by the builder)
  lowest_vertex  lowest skinned vertex over every frame (cm; negative = through the floor). Rise / Fall only play
                 in the air.
"""
import argparse
import importlib
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy  # noqa: E402
import numpy as np  # noqa: E402
import scene  # noqa: E402
import preview  # noqa: E402
import rudeus_anim_lib as L  # noqa: E402
from build_rudeus_anims import ground_corrected  # noqa: E402


MAJOR = [L.PELVIS] + L.SPINE + [L.NECK, L.HEAD] + [b for s in "LR" for b in (L.CLAV[s], L.UPPER[s], L.FORE[s], L.HAND[s], L.THIGH[s], L.SHIN[s], L.FOOT[s])]


def bone_world(arm, name):
    return arm.matrix_world @ arm.pose.bones[name].matrix


def angle_between(u, v):
    u = u.normalized()
    v = v.normalized()
    return math.degrees(math.acos(max(-1.0, min(1.0, u.dot(v)))))


def dominant_bone_per_vertex(mesh):
    names = {g.index: g.name for g in mesh.vertex_groups}
    dom = []
    for v in mesh.data.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        dom.append(names[best.group] if best else "")
    return dom


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--character", default="Rudeus")
    ap.add_argument("--rig", default="")
    ap.add_argument("--clips", nargs="*")
    ap.add_argument("--json", default="")
    a = ap.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])
    C = importlib.import_module("clips" if a.character == "Rudeus" else "clips_" + a.character.lower())
    rig_path = a.rig or (scene.GLB if a.character == "Rudeus" else os.path.join(scene.ROOT, "SourceArt", "Characters", a.character, a.character + "_Rigged.glb"))
    arm, mesh = scene.load_rudeus(rig_path)
    rig = L.Rig(arm)
    # bind pose reference for stretch
    for pb in arm.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)
    bpy.context.view_layer.update()
    co0, tri, _ = preview.evaluate(arm, mesh)
    e0 = np.stack([np.linalg.norm(co0[tri[:, i]] - co0[tri[:, (i + 1) % 3]], axis=1) for i in range(3)], 1)
    valid = e0 > 0.004  # ignore degenerate slivers
    dom = dominant_bone_per_vertex(mesh)
    results = []
    for name in (a.clips or list(C.CLIPS.keys())):
        duration, loop, fn = C.CLIPS[name]
        n = max(2, int(round(duration * 30)))
        prev_q = None
        pop, pop_bone = 0.0, ""
        knee = [999.0, -999.0]
        elbow = [999.0, -999.0]
        twist = 0.0
        stretch, stretch_region = 1.0, ""
        hand_body = 999.0
        ankle_gap = 999.0
        is_gait = name in getattr(C, "GAITS", {})
        slide = 0.0
        run_len = {"L": 0.0, "R": 0.0}
        prev_ankle = {"L": None, "R": None}
        lowest = 999.0
        for i in range(n + 1):
            t = min(i / 30.0, duration) if not loop else (i / 30.0) % duration if i < n else 0.0
            D, pel = ground_corrected(rig, mesh, name, fn(t))
            rig.apply(D, pel)
            bpy.context.view_layer.update()
            W = {b: bone_world(arm, b) for b in MAJOR}
            q = {b: W[b].to_quaternion() for b in MAJOR}
            if prev_q is not None and not (loop and i == n):
                for b in MAJOR:
                    d = math.degrees(prev_q[b].rotation_difference(q[b]).angle)
                    d = min(d, 360.0 - d)  # q and -q are the same rotation
                    if d > pop:
                        pop, pop_bone = d, b
            prev_q = q
            head = {b: arm.matrix_world @ arm.pose.bones[b].head for b in MAJOR}
            for s in "LR":
                th = head[L.SHIN[s]] - head[L.THIGH[s]]
                sh = head[L.FOOT[s]] - head[L.SHIN[s]]
                k = angle_between(th, sh)
                # sign: knee must bend forward (shin rotates backward relative to thigh)
                fwd = W[L.PELVIS].to_3x3() @ (rig.rest3[L.PELVIS].transposed() @ L.FWD)
                if th.cross(sh).dot(W[L.PELVIS].to_3x3() @ (rig.rest3[L.PELVIS].transposed() @ L.LEFT)) < -1e-4:
                    k = -k
                knee = [min(knee[0], k), max(knee[1], k)]
                ua = head[L.FORE[s]] - head[L.UPPER[s]]
                fa = head[L.HAND[s]] - head[L.FORE[s]]
                e = angle_between(ua, fa)
                elbow = [min(elbow[0], e), max(elbow[1], e)]
                # wrist twist: hand vs forearm rotation about the forearm axis
                rel = W[L.FORE[s]].to_quaternion().inverted() @ W[L.HAND[s]].to_quaternion()
                rest_rel = rig.rest3[L.FORE[s]].to_quaternion().inverted() @ rig.rest3[L.HAND[s]].to_quaternion()
                dq = rest_rel.inverted() @ rel
                if dq.w < 0.0:
                    dq.negate()  # q and -q are the same rotation: measure the short way round
                axis_local = (rig.rest3[L.FORE[s]].transposed() @ (rig.head[L.HAND[s]] - rig.head[L.FORE[s]])).normalized()
                sw_axis = dq.axis
                twist_part = abs(math.degrees(dq.angle) * sw_axis.dot(axis_local)) if dq.angle > 1e-6 else 0.0
                twist = max(twist, twist_part)
                # hands vs torso axis (pelvis -> chest)
                a0, a1 = head[L.PELVIS], head[L.SPINE[2]]
                for p in (head[L.HAND[s]],):
                    ab = a1 - a0
                    tt = max(0.0, min(1.0, (p - a0).dot(ab) / ab.length_squared))
                    hand_body = min(hand_body, (p - (a0 + ab * tt)).length)
            ankle_gap = min(ankle_gap, (head[L.FOOT["L"]] - head[L.FOOT["R"]]).length)
            if not is_gait and not (loop and i == n):
                for s in "LR":
                    ank = head[L.FOOT[s]]
                    ball = arm.matrix_world @ arm.pose.bones[L.TOE[s]].head
                    planted = ank.z < rig.ankle_h + 0.012 and ball.z < rig.ball_h + 0.012
                    if planted and prev_ankle[s] is not None:
                        run_len[s] += math.hypot(ank.x - prev_ankle[s].x, ank.y - prev_ankle[s].y)
                        slide = max(slide, run_len[s])
                    elif not planted:
                        run_len[s] = 0.0
                    prev_ankle[s] = ank.copy() if planted else None
            co, _, _ = preview.evaluate(arm, mesh)
            lowest = min(lowest, float(co[:, 2].min()))
            if i % 3 == 0:
                e = np.stack([np.linalg.norm(co[tri[:, j]] - co[tri[:, (j + 1) % 3]], axis=1) for j in range(3)], 1)
                ratio = np.where(valid, e / np.maximum(e0, 1e-6), 1.0)
                k_worst = np.unravel_index(np.argmax(ratio), ratio.shape)
                if ratio[k_worst] > stretch:
                    stretch = float(ratio[k_worst])
                    stretch_region = dom[tri[k_worst[0], 0]]
        r = {"clip": name, "pop_deg": round(pop, 1), "pop_bone": pop_bone.split("_jnt")[0],
             "knee_deg": [round(knee[0], 1), round(knee[1], 1)], "elbow_deg": [round(elbow[0], 1), round(elbow[1], 1)],
             "wrist_twist_deg": round(twist, 1), "stretch": round(stretch, 2), "stretch_region": stretch_region.split("_jnt")[0],
             "hand_to_spine_cm": round(hand_body * 100, 1), "ankle_gap_cm": round(ankle_gap * 100, 1),
             "foot_slide_cm": None if is_gait else round(slide * 100, 1), "lowest_vertex_cm": round(lowest * 100, 1)}
        results.append(r)
        print(json.dumps(r))
    if a.json:
        with open(a.json, "w") as f:
            json.dump(results, f, indent=1)


if __name__ == "__main__":
    main()
