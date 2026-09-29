"""Generates Tools/anim/clips_spells.py from the hand-target designs (spells_rudeus.py, spells_orsted.py).

    python3.11 Tools/anim/cast_design/generate.py            # solve every design, rewrite clips_spells.py
    python3.11 Tools/anim/cast_design/generate.py --check    # solve and compare only (exit 1 if the file would change)
    python3.11 Tools/anim/cast_design/generate.py --sheet Cast_Fireball --character Rudeus   # review sheet of one design

Each design key is solved into composer parameters on the lineage's own rig (design.py, armik.py), in order, a release
clip starting exactly on its anticipation's solved hold pose. The generated module stores the parameters as deltas from
the stance, with the hold poses shared by name, and the template adds the hold loops, the events and the builders.
Then rebuild the GLBs (build_rudeus_anims.py) and re-run the QA.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import design  # noqa: E402  (sets up the Tools/anim import path and bpy)
from pose_compose import DEFAULTS  # noqa: E402
import clips as C  # noqa: E402
import clips_orsted as O  # noqa: E402
import spells_rudeus  # noqa: E402
import spells_orsted  # noqa: E402

OUT = os.path.join(HERE, "..", "clips_spells.py")
TEMPLATE = os.path.join(HERE, "clips_spells.template")

BODY_ORDER = ["pel_x", "pel_y", "pel_z", "pel_yaw", "pel_pitch", "pel_roll", "sp_pitch", "sp_yaw", "sp_roll", "neck_pitch",
              "neck_yaw", "head_pitch", "head_yaw", "head_roll", "coat_trail", "coat_flare", "hair_lag", "hair_side"]
FEET = ["L_fx", "L_fy", "L_fz", "L_fyaw", "L_fpitch", "R_fx", "R_fy", "R_fz", "R_fyaw", "R_fpitch"]
ARM = ["lower", "fwd", "swing", "elbow", "roll", "wpitch", "wyaw", "twist", "spread", "clav", "curl", "fan", "thumb", "ci", "cm", "cr", "cp"]


def fmt(k, v):
    if isinstance(v, str):
        return repr(v)
    metres = k.startswith("pel_") and k not in ("pel_yaw", "pel_pitch", "pel_roll") or k[1:] in ("_fx", "_fy", "_fz")
    v = round(v, 3) if metres else round(v, 1)
    if v == 0:
        v = 0.0
    return repr(float(v))


def delta(stance, P):
    d = {}
    for k, v in P.items():
        if k in ("ik_w", "legs"):
            continue
        sv = stance.get(k, DEFAULTS.get(k))
        if isinstance(v, (int, float)) and isinstance(sv, (int, float)):
            metres = k.startswith("pel_") and k not in ("pel_yaw", "pel_pitch", "pel_roll") or k[1:] in ("_fx", "_fy", "_fz")
            if abs(round(v, 3 if metres else 1) - round(sv, 3 if metres else 1)) < 1e-9:
                continue
        elif v == sv:
            continue
        d[k] = v
    return d


def order(d):
    keys = [k for k in BODY_ORDER if k in d] + [k for k in FEET if k in d]
    for s in "RL":
        keys += [s + "_" + a for a in ARM if s + "_" + a in d]
    keys += sorted(k for k in d if k not in keys)
    return keys


def dict_code(d, indent):
    ks = order(d)
    if not ks:
        return "{}"
    parts = ['"%s": %s' % (k, fmt(k, d[k])) for k in ks]
    # wrap at ~118 chars
    lines, cur = [], ""
    for p in parts:
        if len(cur) + len(p) + 2 > 118 - indent:
            lines.append(cur.rstrip())
            cur = ""
        cur += p + ", "
    lines.append(cur.rstrip().rstrip(","))
    return "{" + ("\n" + " " * (indent + 1)).join(lines) + "}"


def frames(d):
    return int(round(d * 30))


def clip_block(name, dur, keys, stance, shared, indent=4):
    n = frames(dur)
    out = ['    "%s": (F(%d), [' % (name, n)]
    for i, (t, P) in enumerate(keys):
        tt = n / 30.0 if i == len(keys) - 1 else t
        tstr = "F(%d)" % n if i == len(keys) - 1 else "%.3f" % tt
        if i == 0 and not delta(stance, P):
            out.append("        (0.000, {}),")
            continue
        sh = shared.get((name, i)) or shared.get((name, i - len(keys)))
        if sh:
            out.append("        (%s, %s)," % (tstr, sh))
            continue
        out.append("        (%s, %s)," % (tstr, dict_code(delta(stance, P), 12 + len(tstr))))
    out.append("    ]),")
    return "\n".join(out)


def const_block(cname, stance, P, comment):
    return "# %s\n%s = %s\n" % (comment, cname, dict_code(delta(stance, P), len(cname) + 3))


R_SHARED = {("Cast_Fireball", -2): "FB_HOLD", ("Cast_Fireball", -1): "FB_HOLD", ("Cast_Fireball_Release", 0): "FB_HOLD",
            ("Cast_StoneCannon", -2): "SCS_HOLD", ("Cast_StoneCannon", -1): "SCS_HOLD", ("Cast_StoneCannon_Release", 0): "SCS_HOLD",
            ("StoneCannon_Charge", -2): "SC_HOLD", ("StoneCannon_Charge", -1): "SC_HOLD", ("StoneCannon_Release", 0): "SC_HOLD",
            ("Quagmire", -2): "QG_HOLD", ("Quagmire", -1): "QG_HOLD", ("Quagmire_Release", 0): "QG_HOLD"}
O_SHARED = {("Cast_Fireball", -1): "OFB_HOLD", ("Cast_Fireball_Release", 0): "OFB_HOLD",
            ("Cast_StoneCannon", -1): "OSC_HOLD", ("Cast_StoneCannon_Release", 0): "OSC_HOLD"}

R_ORDER = ["Cast_Fireball", "Cast_Fireball_Release", "Cast_FlameWave", "Cast_Inferno", "Cast_WaterBullet", "Cast_WaterDragon",
           "Cast_Flood", "Cast_StoneCannon", "Cast_StoneCannon_Release", "Cast_EarthWall", "Cast_EarthSpikes", "Cast_WindBlade",
           "Cast_Tornado", "Cast_WindBurst", "StoneCannon_Charge", "StoneCannon_Release", "Quagmire", "Quagmire_Release", "Barrage"]
O_ORDER = ["Cast_Fireball", "Cast_Fireball_Release", "Cast_FlameWave", "Cast_Inferno", "Cast_WaterBullet", "Cast_WaterDragon",
           "Cast_Flood", "Cast_StoneCannon", "Cast_StoneCannon_Release", "Cast_EarthWall", "Cast_EarthSpikes", "Cast_WindBlade",
           "Cast_Tornado", "Cast_WindBurst", "DisturbMagic", "DragonCrush"]


def solve_all(character, spells_module, verbose=False):
    stance = spells_module.ST
    solved = {}
    for name, (duration, keys) in spells_module.SPELLS.items():
        anticipation = spells_module.RELEASE_OF.get(name)
        start = solved[anticipation][-1][1] if anticipation else None
        solved[name] = design.solve_keys(character, stance, keys, verbose=verbose, start=start)
    return solved


def render_module(R, OS):
    dur_r = {n: spells_rudeus.SPELLS[n][0] for n in R_ORDER}
    dur_o = {n: spells_orsted.SPELLS[n][0] for n in O_ORDER}
    tpl = open(TEMPLATE).read()
    consts_r = "".join([
        const_block("FB_HOLD", C.COMBAT, R["Cast_Fireball"][-1][1], "Fireball held: right palm up, fingers cupped round the sphere, left palm hovering above it"),
        const_block("SCS_HOLD", C.COMBAT, R["Cast_StoneCannon"][-1][1], "Stone Cannon (element) held: right hand forward at the chest, palm to the target, fingers clawed"),
        const_block("SC_HOLD", C.COMBAT, R["StoneCannon_Charge"][-1][1], "Signature Stone Cannon held: right hand raised, fingers and wrist compressed, left hand bracing the forearm"),
        const_block("QG_HOLD", C.COMBAT, R["Quagmire"][-1][1], "Quagmire held: deep crouch, left palm pressed flat toward the ground, fingers spread"),
    ])
    consts_o = "".join([
        const_block("OFB_HOLD", O.GUARD, OS["Cast_Fireball"][-1][1], "Fireball held: right palm turned up at the waist, fingers barely closed"),
        const_block("OSC_HOLD", O.GUARD, OS["Cast_StoneCannon"][-1][1], "Stone Cannon held: right hand forward at the chest, fingers closed a little"),
    ])
    rk = "\n".join(clip_block(n, dur_r[n], R[n], C.COMBAT, R_SHARED) for n in R_ORDER)
    ok = "\n".join(clip_block(n, dur_o[n], OS[n], O.GUARD, O_SHARED) for n in O_ORDER)
    s = tpl.replace("%%R_CONSTS%%", consts_r.rstrip()).replace("%%O_CONSTS%%", consts_o.rstrip())
    return s.replace("%%R_KEYS%%", rk).replace("%%O_KEYS%%", ok)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="compare with the current clips_spells.py, write nothing")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--sheet", default="", help="render a review sheet of one design (with --character) and stop")
    ap.add_argument("--character", default="Rudeus")
    a = ap.parse_args()
    if a.sheet:
        mod = spells_rudeus if a.character == "Rudeus" else spells_orsted
        solved = solve_all(a.character, mod, a.verbose)
        keys = solved[a.sheet]
        duration = mod.SPELLS[a.sheet][0]
        fn = lambda t, keys=keys: C.keyed(keys, t, mod.ST)  # noqa: E731
        path = os.path.join(os.getcwd(), "design_%s_%s.png" % (a.character, a.sheet))
        design.render(a.character, fn, [duration * k / 7 for k in range(8)], path)
        print(path)
        return 0
    R = solve_all("Rudeus", spells_rudeus, a.verbose)
    OS = solve_all("Orsted", spells_orsted, a.verbose)
    text = render_module(R, OS)
    if a.check:
        same = os.path.exists(a.out) and open(a.out).read() == text
        print("clips_spells.py is %s" % ("up to date" if same else "OUT OF DATE (run without --check)"))
        return 0 if same else 1
    open(a.out, "w").write(text)
    print("wrote", os.path.relpath(a.out), len(text.splitlines()), "lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
