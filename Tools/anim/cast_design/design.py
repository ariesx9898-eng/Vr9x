"""Casting-clip design framework: key poses written as hand targets, solved into composer parameters.

A key is (t, body, R, L):
  body   composer parameters merged over the lineage's stance (pelvis, spine, head, feet, coat, hair ...);
  R / L  a hand target dict, None (the arm keeps the previous key's parameters) or "lerp" (the arm's parameters go straight
         from the previous key to the next one, so an arm going home never plateaus at this key).
Hand target dict: w=(x, f, z) wrist position (character space: x = left, f = forward, z = up, metres), optional
n=(x, f, z) palm normal and fg=(x, f, z) finger direction with weights wn / wf (default 0.25), finger parameters (curl, fan,
thumb, ci, cm, cr, cp ...), fix={param: value} (held in the solve), raw={param: value} (set without solving),
swing=True to let the arm swing behind the body (otherwise swing stays 0, so arms move by lower / fwd like the stances).
Keys are solved in order and each starts from the previous one, so the stored parameters interpolate on one branch.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ANIM = os.path.join(HERE, "..")
sys.path.insert(0, HERE)
sys.path.insert(0, ANIM)
import bpy  # noqa: E402,F401
import scene  # noqa: E402
import rudeus_anim_lib as L  # noqa: E402
import armik  # noqa: E402

HANDP = ("curl", "fan", "thumb", "ci", "cm", "cr", "cp", "twist", "spread", "clav")
FOOT_KEYS = ("fx", "fy", "fz", "fyaw", "fpitch")

_RIGS = {}


def rig(character):
    """(armature object, mesh object, Rig, ArmIK) for a lineage's rigged source (cached)."""
    if character not in _RIGS:
        _RIGS.clear()  # loading a rig resets the Blender scene: an earlier character's objects are gone
        path = scene.GLB if character == "Rudeus" else os.path.join(scene.ROOT, "SourceArt", "Characters", character,
                                                                     character + "_Rigged.glb")
        arm, mesh = scene.load_rudeus(path)
        r = L.Rig(arm)
        _RIGS[character] = (arm, mesh, r, armik.ArmIK(r))
    return _RIGS[character]


def m(*dicts):
    out = {}
    for d in dicts:
        if d:
            out.update(d)
    return out


def H(w, n=None, fg=None, hand=None, **kw):
    """Hand target: wrist position, optional palm normal and finger direction, plus a hand-shape dict."""
    d = dict(w=w)
    if n is not None:
        d["n"] = n
    if fg is not None:
        d["fg"] = fg
    d.update(kw)
    if hand:
        d.update(hand)
    return d


def arm_of(P, side):
    return {k: P.get(side + "_" + k, 0.0) for k in armik.NAMES}


def solve_keys(character, stance, keys, verbose=False, start=None):
    """[(t, full parameter dict)] for a key list. start: an already solved pose to use verbatim as the first key (a
    release clip starts exactly on its anticipation's hold pose)."""
    _, _, _, ik = rig(character)
    out = []
    prev_arm = {k: v for k, v in stance.items() if k[:2] in ("L_", "R_")}
    if start is not None:
        out.append((keys[0][0], dict(start)))
        prev_arm = {k: v for k, v in start.items() if k[:2] in ("L_", "R_")}
        keys = keys[1:]
    lerp_marks = []
    for (t, body, RT, LT) in keys:
        if RT == "lerp" or LT == "lerp":
            lerp_marks.append((len(out), "R" if RT == "lerp" else None, "L" if LT == "lerp" else None))
            RT = None if RT == "lerp" else RT
            LT = None if LT == "lerp" else LT
        P = m(stance, body)
        for k, v in prev_arm.items():  # arms start from the previous key's solution
            if k not in body:
                P[k] = v
        for side, T in (("R", RT), ("L", LT)):
            if T is None:
                continue
            for k in HANDP:
                if k in T:
                    P[side + "_" + k] = T[k]
            for k, v in T.get("raw", {}).items():
                P[side + "_" + k] = v
            if "w" in T:
                fix = dict(T.get("fix", {}))
                if not T.get("swing") and "swing" not in fix:
                    fix["swing"] = 0.0
                sol, info = ik.solve(P, side, wrist=T["w"], palm=T.get("n"), fingers=T.get("fg"),
                                     w_n=T.get("wn", 0.25), w_f=T.get("wf", 0.25), fixed=fix, reg=T.get("reg", 0.002))
                P.update(sol)
                if verbose:
                    print("  t=%.2f %s wrist error %.1f cm  palm %s  fingers %s" % (t, side, info["err_cm"], info["palm"],
                                                                               info["fingers"]))
        prev_arm = {k: v for k, v in P.items() if k[:2] in ("L_", "R_")}
        out.append((t, P))
    for idx, *sides in lerp_marks:
        for side in sides:
            if side is None:
                continue
            j0 = idx - 1
            j1 = next(j for j in range(idx + 1, len(out)) if not any(mk[0] == j and side in mk[1:] for mk in lerp_marks))
            t0, t1, t = out[j0][0], out[j1][0], out[idx][0]
            u = (t - t0) / (t1 - t0)
            for k in list(out[idx][1]):
                if k.startswith(side + "_") and isinstance(out[idx][1][k], (int, float)) and k[2:] not in FOOT_KEYS:
                    a, b = out[j0][1].get(k, 0.0), out[j1][1].get(k, 0.0)
                    out[idx][1][k] = a + (b - a) * u
    return out


def render(character, fn, times, path, views=("side", "front"), size=260):
    """Contact sheet of a clip function at the given times (for reviewing a design before it is generated)."""
    import preview
    from build_rudeus_anims import ground_corrected
    arm, mesh, r, _ = rig(character)
    h = 1.95 if character == "Orsted" else 1.62
    box = ((-0.95 * h / 1.62, -0.05), (0.95 * h / 1.62, 1.85 * h / 1.62))
    imgs, labels = [], []
    for view in views:
        for t in times:
            D, pel = ground_corrected(r, mesh, "design", fn(t))
            r.apply(D, pel)
            bpy.context.view_layer.update()
            co, tri, uv = preview.evaluate(arm, mesh)
            imgs.append(preview.draw(co, tri, uv, view, size=size, frame_box=box))
            labels.append("%s t=%.2f" % (view, t))
    preview.sheet(imgs, len(times), labels).save(path)
