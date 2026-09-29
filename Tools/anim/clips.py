"""Clip definitions for Rudeus. Each clip: name -> (duration seconds, loop?, fn(t) -> pose params).
Locomotion clips are in place with planted feet whose ground speed equals the reference speed,
so the native anim instance can scale play rate by actual speed with no foot sliding."""
import math
import sys
from rudeus_anim_lib import smooth, lerp
from pose_compose import DEFAULTS

TAU = 2.0 * math.pi
FPS = 30


def minjerk(u):
    u = max(0.0, min(1.0, u))
    return u * u * u * (10.0 - 15.0 * u + 6.0 * u * u)


def _slopes(ts, vs):
    """Steffen (1990) monotone cubic slopes: continuous velocity through intermediate keys, no
    overshoot past any key value, and zero velocity at the first and last key (clips ease in/out)."""
    n = len(ts)
    h = [ts[i + 1] - ts[i] for i in range(n - 1)]
    sec = [(vs[i + 1] - vs[i]) / h[i] if h[i] > 1e-9 else 0.0 for i in range(n - 1)]
    m = [0.0] * n
    for i in range(1, n - 1):
        a, b = sec[i - 1], sec[i]
        if a * b > 0.0:
            p = (a * h[i] + b * h[i - 1]) / (h[i - 1] + h[i])
            m[i] = 2.0 * math.copysign(min(abs(a), abs(b), 0.5 * abs(p)), b)
    return m


FK_LEG_PARAMS = tuple(side + name for side in "LR" for name in ("_hip", "_hipabd", "_knee", "_ankle"))
_HOLD_CACHE = {}


def _effective_ik_w(kv, base):
    w = kv.get("ik_w", base.get("ik_w"))
    if w is None:
        return 0.0 if kv.get("legs", base.get("legs", "ik")) == "fk" else 1.0
    return w


def _hold_fk_through_ik(keys, base):
    """FK leg angles mean nothing while a key is pure IK (ik_w == 1), but they are the *source* pose of
    every FK <-> IK blend next to it. Left unset they would default to straight legs, so the leg would
    straighten (and the foot swing down) mid-blend. Pure-IK keys therefore hold the FK angles of the
    nearest FK key (previous first)."""
    cached = _HOLD_CACHE.get(id(keys))
    if cached is not None and cached[0] is keys:
        return cached[1]
    w = [_effective_ik_w(kv, base) for _, kv in keys]
    out = []
    for i, (t, kv) in enumerate(keys):
        kv = dict(kv)
        if w[i] >= 1.0:
            for n in FK_LEG_PARAMS:
                if n in kv:
                    continue
                order = list(range(i - 1, -1, -1)) + list(range(i + 1, len(keys)))
                src = next((j for j in order if w[j] < 1.0 and n in keys[j][1]), None)
                if src is not None:
                    kv[n] = keys[src][1][n]
        out.append((t, kv))
    _HOLD_CACHE[id(keys)] = (keys, out)
    return out


def keyed(keys, t, base):
    """keys: [(time, {param: value})] sorted by time. Every numeric parameter follows a monotone
    cubic curve through its key values, so motion flows through intermediate keys instead of
    stopping at each one, eases into extremes (holds, strikes, contacts) and never overshoots.
    A parameter absent from a key takes the base value (else the composer default) at that key;
    string parameters switch halfway through a segment."""
    keys = _hold_fk_through_ik(keys, base)
    out = dict(base)
    ts = [k for k, _ in keys]
    if t <= ts[0] or len(keys) == 1:
        out.update(keys[0][1])
        return out
    if t >= ts[-1]:
        out.update(keys[-1][1])
        return out
    i = max(j for j in range(len(ts) - 1) if ts[j] <= t)
    h = max(1e-6, ts[i + 1] - ts[i])
    u = (t - ts[i]) / h
    names = set()
    for _, kv in keys:
        names |= set(kv)
    for n in names:
        vals = [kv.get(n, base.get(n, DEFAULTS.get(n, 0.0))) for _, kv in keys]
        if any(isinstance(v, str) for v in vals):
            out[n] = vals[i + 1] if u > 0.5 else vals[i]
            continue
        m = _slopes(ts, vals)
        u2, u3 = u * u, u * u * u
        out[n] = ((2 * u3 - 3 * u2 + 1) * vals[i] + (u3 - 2 * u2 + u) * h * m[i]
                  + (-2 * u3 + 3 * u2) * vals[i + 1] + (u3 - u2) * h * m[i + 1])
    return out


# ----------------------------------------------------------------------------- gait
GAITS = {
    # speed m/s, cycle s, duty, move dir (x,y), lateral foot centre, forward bias, lift m, heel/toe pitch,
    # pelvis height/bob/sway/yaw/roll, lean, arms, coat trailing
    "Walk": dict(speed=1.30, cycle=0.92, duty=0.57, dir=(0.0, -1.0), width=0.090, bias=0.035, lift=0.075,
                 heel=16.0, toe=28.0, pel_z=-0.035, bob=0.012, mode="walk", sway=0.018, yaw=6.0, roll=2.5,
                 lean=5.0, twist=1.25, arm_lower=76.0, arm_swing=16.0, elbow=16.0, elbow_swing=12.0,
                 coat_trail=3.0, hair=2.0),
    "WalkBack": dict(speed=1.00, cycle=0.92, duty=0.60, dir=(0.0, 1.0), width=0.095, bias=-0.03, lift=0.06,
                     heel=6.0, toe=14.0, pel_z=-0.045, bob=0.008, mode="walk", sway=0.015, yaw=4.0, roll=2.0,
                     lean=1.0, twist=1.1, arm_lower=74.0, arm_swing=9.0, elbow=22.0, elbow_swing=6.0,
                     coat_trail=-3.0, hair=1.0),
    "StrafeLeft": dict(speed=1.00, cycle=0.60, duty=0.50, dir=(1.0, 0.0), width=0.0, bias=0.0, lift=0.07, centre=0.215,
                       heel=4.0, toe=10.0, pel_z=-0.055, bob=0.010, mode="walk", sway=0.0, yaw=0.0, roll=2.0,
                       lean=4.0, twist=0.0, arm_lower=72.0, arm_swing=6.0, elbow=24.0, elbow_swing=6.0,
                       coat_trail=0.0, hair=1.5, strafe=True),
    "StrafeRight": dict(speed=1.00, cycle=0.60, duty=0.50, dir=(-1.0, 0.0), width=0.0, bias=0.0, lift=0.07, centre=0.215,
                        heel=4.0, toe=10.0, pel_z=-0.055, bob=0.010, mode="walk", sway=0.0, yaw=0.0, roll=2.0,
                        lean=4.0, twist=0.0, arm_lower=72.0, arm_swing=6.0, elbow=24.0, elbow_swing=6.0,
                        coat_trail=0.0, hair=1.5, strafe=True),
    "Run": dict(speed=3.60, cycle=0.64, duty=0.28, dir=(0.0, -1.0), width=0.075, bias=0.05, lift=0.20,
                heel=10.0, toe=30.0, pel_z=-0.075, bob=0.028, mode="run", sway=0.012, yaw=9.0, roll=3.0,
                lean=12.0, twist=1.4, arm_lower=70.0, arm_swing=34.0, elbow=78.0, elbow_swing=14.0,
                coat_trail=16.0, hair=7.0),
    # Locked-on running. Strafe runs turn the hips and legs 60 deg toward the travel direction while the chest
    # and head keep facing the target (orientation warping baked into the clip); RunBack is a backpedal.
    "RunStrafeLeft": dict(speed=3.30, cycle=0.64, duty=0.30, dir=(1.0, 0.0), width=0.075, bias=0.04, lift=0.16,
                          heel=8.0, toe=26.0, pel_z=-0.075, bob=0.024, mode="run", sway=0.010, yaw=6.0, roll=2.5,
                          lean=9.0, twist=1.2, arm_lower=70.0, arm_swing=24.0, elbow=76.0, elbow_swing=12.0,
                          coat_trail=12.0, hair=5.0, body_yaw=60.0, body_counter=0.65),
    "RunStrafeRight": dict(speed=3.30, cycle=0.64, duty=0.30, dir=(-1.0, 0.0), width=0.075, bias=0.04, lift=0.16,
                           heel=8.0, toe=26.0, pel_z=-0.075, bob=0.024, mode="run", sway=0.010, yaw=6.0, roll=2.5,
                           lean=9.0, twist=1.2, arm_lower=70.0, arm_swing=24.0, elbow=76.0, elbow_swing=12.0,
                           coat_trail=12.0, hair=5.0, body_yaw=-60.0, body_counter=0.65),
    "RunBack": dict(speed=3.00, cycle=0.66, duty=0.34, dir=(0.0, 1.0), width=0.080, bias=-0.04, lift=0.12,
                    heel=3.0, toe=12.0, pel_z=-0.070, bob=0.020, mode="run", sway=0.012, yaw=6.0, roll=2.5,
                    lean=2.0, twist=1.2, arm_lower=68.0, arm_swing=18.0, elbow=72.0, elbow_swing=10.0,
                    coat_trail=-10.0, hair=-3.0),
    "Sprint": dict(speed=5.80, cycle=0.56, duty=0.22, dir=(0.0, -1.0), width=0.070, bias=0.07, lift=0.25,
                   heel=6.0, toe=36.0, pel_z=-0.090, bob=0.032, mode="run", sway=0.010, yaw=11.0, roll=3.5,
                   lean=20.0, twist=1.5, arm_lower=66.0, arm_swing=48.0, elbow=92.0, elbow_swing=10.0,
                   coat_trail=28.0, hair=12.0),
}


def gait_pose(G, phase):
    P = {}
    stance = G["speed"] * G["cycle"] * G["duty"]  # distance the planted foot travels relative to the hip
    dx, dy = G["dir"]
    body_yaw = G.get("body_yaw", 0.0)                  # hips/legs turned toward the travel direction (deg)
    by = math.radians(body_yaw)
    px, py = math.cos(by), math.sin(by)                # the turned body's left axis: feet sit either side of it
    for side, off in (("L", 0.0), ("R", 0.5)):
        sign = 1.0 if side == "L" else -1.0
        ph = (phase + off) % 1.0
        if ph < G["duty"]:
            u = ph / G["duty"]
            along = stance * (0.5 - u)
            lift = 0.0
            pitch = G["heel"] * (1.0 - smooth(u / 0.2)) if u < 0.2 else 0.0
            if u > 0.6:
                pitch = -G["toe"] * smooth((u - 0.6) / 0.4)
        else:
            u = (ph - G["duty"]) / (1.0 - G["duty"])
            along = stance * (-0.5 + minjerk(u))
            lift = G["lift"] * math.sin(math.pi * min(1.0, u * 1.05)) ** 1.3
            pitch = lerp(-G["toe"], G["heel"], smooth(u)) + 8.0 * math.sin(math.pi * u)
        along += G["bias"]
        if G.get("strafe"):
            # feet side by side around fixed lateral centres; the closest they come is
            # 2 * centre - stance (13 cm for Rudeus), so the feet never touch
            centre = G.get("centre", 0.19) * sign
            fx, fy = centre + dx * along, (0.02 if side == "L" else -0.02) * (1 if dx > 0 else -1)
        else:
            fx, fy = sign * G["width"] * px + dx * along, sign * G["width"] * py + dy * along
        P[side + "_fx"], P[side + "_fy"] = fx, fy
        P[side + "_fz"] = lift
        P[side + "_fpitch"] = pitch
        P[side + "_fyaw"] = sign * 5.0 + body_yaw
    ms = G["duty"] * 0.5
    if G["mode"] == "walk":
        P["pel_z"] = G["pel_z"] + G["bob"] * math.cos(2 * TAU * (phase - ms))
    else:
        P["pel_z"] = G["pel_z"] - G["bob"] * math.cos(2 * TAU * (phase - ms))
    lateral = G["sway"] * math.cos(TAU * (phase - ms))
    if G.get("strafe"):
        P["pel_x"] = 0.0
        P["pel_roll"] = G["roll"] * (1 if G["dir"][0] > 0 else -1)
    else:
        P["pel_x"], P["pel_y"] = lateral * px, lateral * py
        P["pel_roll"] = -G["roll"] * math.cos(TAU * (phase - ms))
    swing_yaw = -G["yaw"] * math.cos(TAU * phase)
    P["pel_yaw"] = body_yaw + swing_yaw
    P["pel_pitch"] = G["lean"] * 0.35
    P["sp_pitch"] = G["lean"] * 0.65 + 1.5 * math.cos(2 * TAU * (phase - ms))
    # the spine undoes the stride's hip swing (x twist) and most of the body turn; the head does the rest
    P["sp_yaw"] = -swing_yaw * G["twist"] - body_yaw * G.get("body_counter", 0.0)
    P["sp_roll"] = -P["pel_roll"] * 0.6
    P["head_pitch"] = -G["lean"] * 0.8 - 2.0
    P["head_yaw"] = -(P["pel_yaw"] + P["sp_yaw"]) * 0.9
    P["neck_pitch"] = -1.0
    swing_l = G["arm_swing"] * math.cos(TAU * (phase - 0.5))
    swing_r = G["arm_swing"] * math.cos(TAU * phase)
    for s, sw in (("L", swing_l), ("R", swing_r)):
        P[s + "_lower"] = G["arm_lower"]
        P[s + "_swing"] = sw
        P[s + "_elbow"] = G["elbow"] + G["elbow_swing"] * max(0.0, sw) / max(1.0, G["arm_swing"])
        P[s + "_curl"] = 22.0 if G["speed"] < 3 else 38.0
        P[s + "_thumb"] = 14.0 if G["speed"] < 3 else 22.0
    P["coat_trail"] = G["coat_trail"] + 2.0 * math.sin(2 * TAU * phase)
    P["auto_reach"] = True
    P["hair_lag"] = G["hair"] + 1.5 * math.cos(2 * TAU * (phase - ms))
    return P


def loop_gait(name):
    G = GAITS[name]
    G["cycle"] = round(G["cycle"] * FPS) / FPS  # whole frames: exact, hitch-free loop seam
    return G["cycle"], True, (lambda t, G=G: gait_pose(G, (t / G["cycle"]) % 1.0))


# ----------------------------------------------------------------------------- poses
STAND = {"ik_w": 1.0, "L_fx": 0.095, "L_fy": -0.01, "R_fx": -0.10, "R_fy": 0.025, "L_fyaw": 6.0, "R_fyaw": -8.0}

COMBAT = {
    "ik_w": 1.0, "pel_z": -0.085, "pel_yaw": -14.0, "pel_pitch": 3.0, "sp_pitch": 7.0, "sp_yaw": 9.0, "head_yaw": 6.0, "head_pitch": -4.0,
    "L_fx": 0.13, "L_fy": -0.13, "L_fyaw": -2.0, "R_fx": -0.15, "R_fy": 0.14, "R_fyaw": -30.0,
    "R_lower": 38.0, "R_fwd": 48.0, "R_elbow": 38.0, "R_wpitch": -18.0, "R_curl": 10.0, "R_thumb": 8.0,
    "L_lower": 58.0, "L_fwd": 22.0, "L_elbow": 78.0, "L_curl": 30.0,
    "coat_flare": 4.0,
}


def idle(t):
    T = 4.0
    w = TAU * t / T
    P = dict(STAND)
    P.update({
        "pel_x": 0.012 + 0.008 * math.sin(w), "pel_z": -0.028 + 0.004 * math.sin(2 * w),
        "pel_roll": -1.2 + 0.8 * math.sin(w), "pel_yaw": 1.5 * math.sin(w + 0.6),
        "sp_pitch": 3.0 + 1.3 * math.sin(w - 0.5), "sp_roll": 0.8 * math.sin(w), "sp_yaw": -1.0 * math.sin(w + 0.6),
        "head_yaw": 3.0 * math.sin(w + 1.2), "head_pitch": -3.0 + 1.0 * math.sin(2 * w),
        "L_lower": 75.0 + 1.5 * math.sin(w - 0.4), "R_lower": 75.0 + 1.5 * math.sin(w - 0.2),
        "L_swing": 2.0 + 1.0 * math.sin(w), "R_swing": 3.0 + 1.0 * math.sin(w + 0.4),
        "L_elbow": 16.0 + 2.0 * math.sin(w - 0.8), "R_elbow": 18.0 + 2.0 * math.sin(w - 0.6),
        "L_curl": 22.0, "R_curl": 24.0,
        "hair_lag": 0.8 * math.sin(w), "coat_trail": 0.6 * math.sin(w),
    })
    return P


def combat_idle(t):
    T = 2.0
    w = TAU * t / T
    P = dict(COMBAT)
    P.update({
        "pel_z": COMBAT["pel_z"] + 0.006 * math.sin(2 * w), "pel_x": 0.006 * math.sin(w),
        "sp_pitch": COMBAT["sp_pitch"] + 1.4 * math.sin(w - 0.4),
        "R_elbow": COMBAT["R_elbow"] + 3.0 * math.sin(w), "R_fwd": COMBAT["R_fwd"] + 2.0 * math.sin(w + 0.5),
        "L_elbow": COMBAT["L_elbow"] + 2.0 * math.sin(w + 1.0),
        "hair_lag": 1.0 * math.sin(2 * w), "coat_trail": 0.8 * math.sin(w),
    })
    return P



# ----------------------------------------------------------------------------- one-shots
READY = dict(STAND)                       # neutral stand (matches Idle at t=0 closely)
READY.update({"pel_z": -0.028, "L_lower": 75.0, "R_lower": 75.0, "L_elbow": 16.0, "R_elbow": 18.0, "L_curl": 22.0, "R_curl": 24.0})
COMBAT_FEET = {k: COMBAT[k] for k in ("L_fx", "L_fy", "L_fyaw", "R_fx", "R_fy", "R_fyaw")}
HOLD = dict(COMBAT)  # same feet as COMBAT: switching between stance and charge never moves a planted foot
HOLD.update({"pel_z": -0.11, "pel_yaw": -18.0, "sp_pitch": 10.0, "sp_yaw": 12.0, "head_yaw": 6.0,
             "R_lower": 22.0, "R_fwd": 72.0, "R_elbow": 46.0, "R_wpitch": -30.0, "R_curl": 34.0, "R_thumb": 20.0,
             "L_lower": 26.0, "L_fwd": 62.0, "L_elbow": 72.0, "L_wpitch": -10.0, "L_curl": 34.0, "L_thumb": 20.0,
             "coat_flare": 5.0, "hair_lag": 2.0})
THRUST_R = {"R_lower": 8.0, "R_fwd": 86.0, "R_elbow": 4.0, "R_wpitch": -55.0, "R_curl": 6.0, "R_thumb": 6.0}
THRUST_L = {"L_lower": 8.0, "L_fwd": 86.0, "L_elbow": 4.0, "L_wpitch": -55.0, "L_curl": 6.0, "L_thumb": 6.0}
RETRACT_R = {"R_lower": 40.0, "R_fwd": 45.0, "R_elbow": 70.0, "R_wpitch": -28.0, "R_curl": 26.0}
RETRACT_L = {"L_lower": 45.0, "L_fwd": 40.0, "L_elbow": 75.0, "L_wpitch": -28.0, "L_curl": 26.0}
AIR_RISE = {"ik_w": 0.0, "legs": "fk", "L_hip": 38.0, "L_knee": 62.0, "R_hip": 16.0, "R_knee": 34.0, "L_ankle": -10.0, "R_ankle": -15.0,
            "L_lower": 52.0, "R_lower": 50.0, "L_swing": 22.0, "R_swing": 12.0, "L_elbow": 42.0, "R_elbow": 38.0,
            "L_spread": 8.0, "R_spread": 8.0, "sp_pitch": 7.0, "pel_z": 0.0, "coat_trail": -6.0, "hair_lag": -5.0}
AIR_FALL = {"ik_w": 0.0, "legs": "fk", "L_hip": 22.0, "L_knee": 26.0, "R_hip": 8.0, "R_knee": 16.0, "L_ankle": 8.0, "R_ankle": 6.0,
            "L_lower": 34.0, "R_lower": 30.0, "L_swing": 8.0, "R_swing": -4.0, "L_elbow": 28.0, "R_elbow": 24.0,
            "L_spread": 18.0, "R_spread": 20.0, "sp_pitch": 2.0, "pel_z": 0.0, "coat_trail": -12.0, "coat_flare": 12.0, "hair_lag": 9.0}


def oneshot(duration, keys, base=READY):
    return duration, False, (lambda t, keys=keys, base=base: keyed(keys, t, base))


def loop_keys(duration, keys, base):
    # keys must start and end with identical values for a seamless loop
    return duration, True, (lambda t, keys=keys, base=base: keyed(keys, t % duration, base))


def rise_loop(t):
    w = TAU * t / 0.6
    P = dict(READY); P.update(AIR_RISE)
    P.update({"L_hip": 38.0 + 3.0 * math.sin(w), "R_hip": 16.0 + 3.0 * math.sin(w + 1.5), "hair_lag": -5.0 + 1.5 * math.sin(w),
              "L_swing": 22.0 + 2.0 * math.sin(w + 0.5)})
    return P


def fall_loop(t):
    w = TAU * t / 0.6
    P = dict(READY); P.update(AIR_FALL)
    P.update({"L_spread": 18.0 + 4.0 * math.sin(w), "R_spread": 20.0 + 4.0 * math.sin(w + 2.0), "coat_trail": -12.0 + 3.0 * math.sin(2 * w),
              "hair_lag": 9.0 + 2.0 * math.sin(2 * w), "L_hip": 22.0 + 2.0 * math.sin(w)})
    return P


def _m(*dicts):
    out = {}
    for d in dicts:
        out.update(d)
    return out


ONESHOTS = {
    "JumpStart": oneshot(0.26, [
        (0.00, {}),
        (0.10, {"pel_z": -0.13, "sp_pitch": 16.0, "L_swing": -24.0, "R_swing": -24.0, "L_elbow": 24.0, "R_elbow": 24.0, "head_pitch": -8.0}),
        (0.22, {"pel_z": -0.005, "sp_pitch": 4.0, "L_fpitch": -30.0, "R_fpitch": -30.0, "L_swing": 26.0, "R_swing": 22.0,
                "L_elbow": 36.0, "R_elbow": 34.0, "hair_lag": -4.0}),
        (0.26, {"pel_z": 0.0, "sp_pitch": 5.0, "L_fpitch": -34.0, "R_fpitch": -34.0, "L_swing": 24.0, "R_swing": 14.0,
                "L_elbow": 42.0, "R_elbow": 38.0, "hair_lag": -5.0}),
    ]),
    "Land": oneshot(0.40, [
        (0.00, _m({"pel_z": 0.0, "ik_w": 1.0, "L_spread": 14.0, "R_spread": 14.0, "L_lower": 38.0, "R_lower": 36.0, "coat_trail": -10.0, "hair_lag": 8.0})),
        (0.08, {"pel_z": -0.15, "sp_pitch": 17.0, "L_lower": 60.0, "R_lower": 58.0, "L_swing": 16.0, "R_swing": 14.0, "coat_trail": 4.0, "hair_lag": 6.0}),
        (0.40, {}),
    ]),
    "HardLand": oneshot(0.80, [
        (0.00, {"pel_z": 0.0, "L_spread": 16.0, "R_spread": 16.0, "L_lower": 34.0, "R_lower": 30.0, "coat_trail": -12.0, "hair_lag": 9.0}),
        (0.06, {"pel_z": -0.17, "sp_pitch": 18.0, "head_pitch": -8.0, "R_lower": 62.0, "R_swing": 24.0, "R_elbow": 16.0, "R_wpitch": 8.0,
                "L_lower": 52.0, "L_swing": -4.0, "L_elbow": 24.0, "L_spread": 9.0, "R_spread": 7.0, "coat_trail": -3.0, "hair_lag": 10.0}),
        (0.15, {"pel_z": -0.31, "sp_pitch": 34.0, "head_pitch": -18.0, "R_lower": 82.0, "R_swing": 48.0, "R_elbow": 8.0, "R_wpitch": 40.0,
                "L_lower": 64.0, "L_swing": -10.0, "L_elbow": 30.0, "coat_trail": 6.0, "hair_lag": 10.0}),
        (0.38, {"pel_z": -0.29, "sp_pitch": 30.0, "head_pitch": -14.0, "R_lower": 80.0, "R_swing": 44.0, "R_elbow": 10.0, "R_wpitch": 35.0,
                "L_lower": 64.0, "L_swing": -8.0, "L_elbow": 30.0}),
        (0.80, {}),
    ]),
    "DodgeForward": oneshot(0.40, [
        (0.00, {"ik_w": 1.0}),
        (0.10, {"ik_w": 0.0, "legs": "fk", "L_hip": 50.0, "L_knee": 70.0, "L_ankle": 6.0, "R_hip": -20.0, "R_knee": 38.0, "R_ankle": 14.0,
                "pel_z": -0.07, "pel_pitch": 8.0,
                "sp_pitch": 22.0, "L_swing": -38.0, "R_swing": -30.0, "L_elbow": 30.0, "R_elbow": 30.0, "coat_trail": 22.0, "hair_lag": 10.0}),
        (0.28, {"ik_w": 0.0, "legs": "fk", "L_hip": 30.0, "L_knee": 40.0, "L_ankle": 4.0, "R_hip": -8.0, "R_knee": 30.0, "R_ankle": 8.0,
                "pel_z": -0.06, "pel_pitch": 6.0,
                "sp_pitch": 16.0, "L_swing": -20.0, "R_swing": -14.0, "L_elbow": 30.0, "R_elbow": 30.0, "coat_trail": 14.0, "hair_lag": 7.0}),
        (0.40, {"ik_w": 1.0}),
    ]),
    "DodgeBack": oneshot(0.40, [
        (0.00, {"ik_w": 1.0}),
        (0.10, {"ik_w": 0.0, "legs": "fk", "L_hip": 30.0, "L_knee": 50.0, "R_hip": 18.0, "R_knee": 40.0, "pel_z": -0.06, "sp_pitch": -6.0,
                "L_swing": 30.0, "R_swing": 26.0, "L_elbow": 40.0, "R_elbow": 36.0, "coat_trail": -14.0, "coat_flare": 8.0, "hair_lag": -6.0}),
        (0.28, {"ik_w": 0.0, "legs": "fk", "L_hip": 16.0, "L_knee": 30.0, "R_hip": 8.0, "R_knee": 24.0, "pel_z": -0.05, "sp_pitch": 0.0,
                "L_swing": 18.0, "R_swing": 14.0, "L_elbow": 34.0, "R_elbow": 30.0, "coat_trail": -8.0, "hair_lag": -3.0}),
        (0.40, {"ik_w": 1.0}),
    ]),
}


def _side_dodge(sign):
    # sign +1 = left (+X), -1 = right
    lead, trail = ("L", "R") if sign > 0 else ("R", "L")
    return oneshot(0.40, [
        (0.00, {"ik_w": 1.0}),
        (0.10, {"ik_w": 0.0, "legs": "fk", lead + "_hipabd": 30.0, lead + "_hip": 14.0, lead + "_knee": 34.0,
                trail + "_hipabd": 12.0, trail + "_hip": 8.0, trail + "_knee": 40.0, "pel_z": -0.07, "pel_roll": 10.0 * sign,
                "sp_roll": 10.0 * sign, "sp_pitch": 10.0, lead + "_spread": 20.0, trail + "_lower": 55.0, "coat_flare": 10.0, "hair_side": -8.0 * sign}),
        (0.28, {"ik_w": 0.0, "legs": "fk", lead + "_hipabd": 16.0, lead + "_hip": 8.0, lead + "_knee": 24.0,
                trail + "_hipabd": 6.0, trail + "_hip": 6.0, trail + "_knee": 26.0, "pel_z": -0.05, "pel_roll": 6.0 * sign,
                "sp_roll": 6.0 * sign, "sp_pitch": 8.0, "coat_flare": 5.0, "hair_side": -4.0 * sign}),
        (0.40, {"ik_w": 1.0}),
    ])


ONESHOTS["DodgeLeft"] = _side_dodge(1.0)
ONESHOTS["DodgeRight"] = _side_dodge(-1.0)


def _hit(pitch, roll, arms):
    return oneshot(0.35, [
        (0.00, {}),
        (0.08, _m({"sp_pitch": 3.0 + pitch, "sp_roll": roll, "head_pitch": -2.0 + pitch * 0.6, "head_roll": roll * 0.6,
                   "pel_y": -0.02 * math.copysign(1, pitch) if pitch else 0.0, "pel_z": -0.05, "hair_lag": 5.0}, arms)),
        (0.35, {}),
    ])


ONESHOTS["HitFront"] = _hit(-14.0, 0.0, {"L_swing": 18.0, "R_swing": 14.0, "L_elbow": 34.0, "R_elbow": 30.0})
ONESHOTS["HitBack"] = _hit(15.0, 0.0, {"L_swing": -18.0, "R_swing": -16.0, "L_elbow": 26.0, "R_elbow": 26.0})
ONESHOTS["HitLeft"] = _hit(0.0, -12.0, {"L_spread": 18.0, "L_lower": 55.0, "R_lower": 80.0})
ONESHOTS["HitRight"] = _hit(0.0, 12.0, {"R_spread": 18.0, "R_lower": 55.0, "L_lower": 80.0})

ONESHOTS["Stagger"] = oneshot(0.80, [
    (0.00, {}),
    (0.10, {"sp_pitch": -20.0, "head_pitch": -12.0, "pel_y": 0.05, "pel_z": -0.06, "L_spread": 22.0, "R_spread": 24.0,
            "L_lower": 45.0, "R_lower": 42.0, "L_elbow": 30.0, "R_elbow": 34.0, "hair_lag": 8.0, "coat_trail": -8.0}),
    (0.22, {"sp_pitch": -12.0, "pel_y": 0.10, "pel_z": -0.08, "R_fy": 0.20, "R_fz": 0.07, "L_lower": 50.0, "R_lower": 48.0,
            "L_spread": 16.0, "R_spread": 16.0, "hair_lag": 5.0}),
    (0.34, {"sp_pitch": 6.0, "pel_y": 0.12, "pel_z": -0.10, "R_fy": 0.25, "R_fz": 0.0, "L_lower": 62.0, "R_lower": 60.0}),
    (0.47, {"sp_pitch": 4.0, "pel_y": 0.09, "pel_z": -0.08, "R_fy": 0.25, "R_fz": 0.0, "R_fpitch": -12.0, "L_lower": 66.0, "R_lower": 64.0}),
    (0.53, {"sp_pitch": 3.5, "pel_y": 0.07, "pel_z": -0.07, "R_fy": 0.24, "R_fz": 0.035, "L_lower": 68.0, "R_lower": 66.0}),
    (0.62, {"sp_pitch": 3.0, "pel_y": 0.04, "pel_z": -0.05, "R_fy": 0.12, "R_fz": 0.06, "L_lower": 71.0, "R_lower": 70.0}),
    (0.69, {"pel_y": 0.015, "pel_z": -0.04, "R_fy": 0.035, "R_fz": 0.018, "R_fpitch": 4.0}),
    (0.74, {"pel_y": 0.005, "pel_z": -0.032, "R_fy": 0.025, "R_fz": 0.0}),
    (0.80, {"pel_y": 0.0, "R_fy": 0.025}),
])

ONESHOTS["Knockdown"] = oneshot(1.40, [
    (0.00, {"ik_w": 1.0}),
    (0.12, {"ik_w": 1.0, "sp_pitch": -24.0, "head_pitch": -16.0, "pel_y": 0.08, "pel_z": -0.10, "L_spread": 25.0, "R_spread": 25.0, "L_lower": 30.0, "R_lower": 30.0}),
    (0.40, {"ik_w": 0.0, "legs": "fk", "L_hip": 32.0, "R_hip": 26.0, "L_knee": 22.0, "R_knee": 30.0, "pel_z": -0.62, "pel_y": 0.30,
            "pel_pitch": -66.0, "sp_pitch": 2.0, "head_pitch": 14.0, "L_lower": 20.0, "R_lower": 25.0, "L_spread": 35.0, "R_spread": 30.0,
            "coat_trail": -20.0, "hair_lag": -10.0}),
    (0.62, {"ik_w": 0.0, "legs": "fk", "L_hip": 14.0, "R_hip": 20.0, "L_knee": 18.0, "R_knee": 28.0, "pel_z": -0.73, "pel_y": 0.34,
            "pel_pitch": -78.0, "sp_pitch": 6.0, "head_pitch": 22.0, "L_lower": 15.0, "R_lower": 18.0, "L_spread": 40.0, "R_spread": 36.0,
            "coat_trail": -24.0, "hair_lag": -12.0}),
    (0.84, {"ik_w": 0.0, "legs": "fk", "L_hip": 85.0, "R_hip": 80.0, "L_knee": 95.0, "R_knee": 90.0, "pel_z": -0.62, "pel_y": 0.22,
            "pel_pitch": -18.0, "sp_pitch": 34.0, "head_pitch": -4.0, "L_lower": 62.0, "R_lower": 60.0, "L_swing": 30.0, "R_swing": 26.0}),
    (0.98, {"ik_w": 1.0, "L_fz": 0.03, "R_fz": 0.04, "pel_z": -0.50, "pel_y": 0.14, "pel_pitch": -4.0, "sp_pitch": 36.0, "head_pitch": -6.0,
            "L_lower": 66.0, "R_lower": 64.0, "L_swing": 26.0, "R_swing": 22.0}),
    (1.06, {"ik_w": 1.0, "pel_z": -0.40, "pel_y": 0.08, "pel_pitch": 3.0, "sp_pitch": 32.0, "L_lower": 69.0, "R_lower": 67.0, "L_swing": 20.0, "R_swing": 18.0}),
    (1.21, {"ik_w": 1.0, "pel_z": -0.20, "pel_y": 0.03, "pel_pitch": 5.0, "sp_pitch": 18.0, "L_lower": 72.0, "R_lower": 71.0, "L_swing": 9.0, "R_swing": 8.0}),
    (1.40, {"ik_w": 1.0}),
])

ONESHOTS["Death"] = oneshot(1.60, [
    (0.00, {"ik_w": 1.0}),
    (0.15, {"ik_w": 1.0, "sp_pitch": -18.0, "head_pitch": -14.0, "pel_z": -0.06, "L_spread": 18.0, "R_spread": 18.0, "hair_lag": 8.0}),
    (0.55, {"ik_w": 1.0, "pel_z": -0.36, "sp_pitch": 26.0, "head_pitch": 22.0, "L_lower": 84.0, "R_lower": 84.0, "L_elbow": 6.0, "R_elbow": 8.0,
            "L_curl": 30.0, "R_curl": 30.0}),
    (0.95, {"ik_w": 0.0, "legs": "fk", "L_hip": 10.0, "R_hip": 6.0, "L_knee": 92.0, "R_knee": 96.0, "pel_z": -0.45, "pel_y": 0.02,
            "sp_pitch": 28.0, "head_pitch": 26.0, "L_lower": 84.0, "R_lower": 84.0, "L_elbow": 8.0, "R_elbow": 10.0}),
    (1.35, {"ik_w": 0.0, "legs": "fk", "L_hip": 2.0, "R_hip": 0.0, "L_knee": 6.0, "R_knee": 9.0, "pel_z": -0.73, "pel_y": -0.35,
            "pel_pitch": 87.0, "sp_pitch": 4.0, "head_pitch": 4.0, "head_yaw": 30.0, "L_lower": 10.0, "R_lower": 16.0, "L_swing": 60.0, "R_swing": 50.0,
            "L_elbow": 30.0, "R_elbow": 20.0, "coat_trail": -6.0, "hair_lag": 4.0}),
    (1.60, {"ik_w": 0.0, "legs": "fk", "L_hip": 2.0, "R_hip": 0.0, "L_knee": 5.0, "R_knee": 8.0, "pel_z": -0.74, "pel_y": -0.36,
            "pel_pitch": 88.0, "sp_pitch": 3.0, "head_pitch": 5.0, "head_yaw": 32.0, "L_lower": 8.0, "R_lower": 15.0, "L_swing": 62.0, "R_swing": 52.0,
            "L_elbow": 32.0, "R_elbow": 22.0, "coat_trail": -6.0, "hair_lag": 4.0}),
])

# ---- casting (Rudeus: chantless = short readable preparation, then release)
# Timing contract with Abilities.json: the projectile spawns at CastTime after activation, so the
# strike reaches full extension then (Basic 0.12-0.14 s, two-hand casts ~0.27 s). The element spells, Stone Cannon,
# Quagmire and Barrage clips (with their Release / Finale events) are in clips_spells.py, merged at the end of this file.
ONESHOTS["CastBasic"] = oneshot(0.45, [
    (0.00, {}),
    (0.06, _m(COMBAT, {"R_lower": 42.0, "R_fwd": 36.0, "R_elbow": 64.0, "R_wpitch": -26.0, "pel_yaw": -16.0, "sp_yaw": 10.0})),
    (0.14, _m(COMBAT, THRUST_R, {"R_lower": 12.0, "pel_yaw": -8.0, "sp_yaw": 2.0})),
    (0.22, _m(COMBAT, THRUST_R, {"R_lower": 14.0, "R_fwd": 84.0, "R_elbow": 9.0, "R_wpitch": -46.0, "pel_yaw": -9.0, "sp_yaw": 3.0})),
    (0.45, COMBAT),
], base=COMBAT)
ONESHOTS["DemonEye"] = oneshot(0.80, [
    (0.00, COMBAT),
    (0.26, _m(COMBAT, {"L_lower": 48.0, "L_fwd": 58.0, "L_elbow": 128.0, "L_swing": 30.0, "L_wpitch": -20.0, "L_curl": 16.0,
                       "head_pitch": 6.0, "head_yaw": 8.0})),
    (0.50, _m(COMBAT, {"L_lower": 48.0, "L_fwd": 58.0, "L_elbow": 124.0, "L_swing": 30.0, "L_wpitch": -20.0, "L_curl": 14.0,
                       "head_pitch": -4.0, "head_yaw": 4.0})),
    (0.80, COMBAT),
], base=COMBAT)
ONESHOTS["Awakening"] = oneshot(1.80, [
    (0.00, COMBAT),
    (0.30, _m(READY, COMBAT_FEET, {"pel_z": -0.06, "head_pitch": 16.0, "sp_pitch": 9.0, "L_lower": 60.0, "R_lower": 60.0, "L_spread": 12.0, "R_spread": 12.0,
                      "L_elbow": 20.0, "R_elbow": 20.0})),
    (0.90, _m(READY, COMBAT_FEET, {"pel_z": -0.08, "head_pitch": 2.0, "sp_pitch": 2.0, "L_lower": 22.0, "R_lower": 22.0, "L_spread": 8.0, "R_spread": 8.0,
                      "L_twist": 70.0, "R_twist": 70.0, "L_elbow": 26.0, "R_elbow": 26.0, "coat_flare": 8.0, "hair_lag": -4.0})),
    (1.30, _m(READY, COMBAT_FEET, {"pel_z": -0.05, "head_pitch": -10.0, "sp_pitch": -9.0, "L_lower": -12.0, "R_lower": -12.0, "L_spread": 4.0, "R_spread": 4.0,
                      "L_twist": 80.0, "R_twist": 80.0, "L_elbow": 18.0, "R_elbow": 18.0, "coat_flare": 16.0, "hair_lag": -10.0})),
    (1.60, _m(COMBAT, {"L_lower": 40.0, "R_lower": 34.0, "coat_flare": 10.0, "hair_lag": 6.0})),
    (1.80, COMBAT),
], base=COMBAT)
ONESHOTS["CastTwoHand"] = oneshot(0.65, [
    (0.00, COMBAT),
    (0.16, _m(HOLD, RETRACT_R, RETRACT_L)),
    (0.27, _m(HOLD, THRUST_R, THRUST_L, {"pel_yaw": -12.0, "sp_yaw": 2.0, "sp_pitch": 12.0, "coat_trail": 5.0})),
    (0.36, _m(HOLD, THRUST_R, THRUST_L, {"R_elbow": 9.0, "L_elbow": 9.0, "R_wpitch": -46.0, "L_wpitch": -46.0,
                                        "pel_yaw": -12.0, "sp_yaw": 2.0, "sp_pitch": 11.0, "coat_trail": 4.0})),
    (0.65, COMBAT),
], base=COMBAT)
ONESHOTS["CastGround"] = oneshot(0.80, [
    (0.00, COMBAT),
    (0.32, _m(COMBAT, {"pel_z": -0.24, "sp_pitch": 30.0, "head_pitch": -10.0, "R_lower": 60.0, "R_swing": 48.0, "R_fwd": 10.0, "R_elbow": 8.0,
                       "R_wpitch": 45.0, "R_curl": 6.0})),
    (0.42, _m(COMBAT, {"pel_z": -0.25, "sp_pitch": 31.0, "head_pitch": -8.0, "R_lower": 64.0, "R_swing": 52.0, "R_fwd": 14.0, "R_elbow": 6.0,
                       "R_wpitch": 52.0, "R_curl": 4.0})),
    (0.80, COMBAT),
], base=COMBAT)

CLIPS = {
    "Idle": (4.0, True, idle),
    "CombatIdle": (2.0, True, combat_idle),
    "Walk": loop_gait("Walk"),
    "WalkBack": loop_gait("WalkBack"),
    "StrafeLeft": loop_gait("StrafeLeft"),
    "StrafeRight": loop_gait("StrafeRight"),
    "Run": loop_gait("Run"),
    "Sprint": loop_gait("Sprint"),
    "RunStrafeLeft": loop_gait("RunStrafeLeft"),
    "RunStrafeRight": loop_gait("RunStrafeRight"),
    "RunBack": loop_gait("RunBack"),
    "Rise": (0.6, True, rise_loop),
    "Fall": (0.6, True, fall_loop),
}
CLIPS.update(ONESHOTS)

# Element spells and the signature upgrades of the ability overhaul (Docs/Ability_Overhaul.md §7): every spell has its own
# clip, and EVENTS carries each clip's gameplay events ({"Release": s, "Finale": s}) for the exporter sidecar.
import clips_spells  # noqa: E402  (imported last: it builds on COMBAT and keyed above)

_spell_clips, EVENTS = clips_spells.rudeus(sys.modules[__name__])
CLIPS.update(_spell_clips)
