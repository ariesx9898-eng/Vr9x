"""Orsted's clip set: same skeleton as Rudeus, different personality (see Docs/Orsted_Model_Spec.md §6).
Nothing wasted: upright, still, economical; open-palm technique; counters with minimal motion.

Footing rule: every standing clip starts and ends on the guard's footing (GUARD), and his relaxed stance uses that same
footing, so idle <-> combat crossfades and every montage blend-in / blend-out leave his planted feet exactly where they
are. Steps are always lifted (a foot never moves while it is on the ground)."""
import math
import clips as R
from clips import keyed, gait_pose, TAU, FPS
from pose_compose import DEFAULTS

GAITS = {
    "Walk": dict(speed=1.45, cycle=1.00, duty=0.60, dir=(0.0, -1.0), width=0.085, bias=0.03, lift=0.07,
                 heel=14.0, toe=22.0, pel_z=-0.03, bob=0.006, mode="walk", sway=0.008, yaw=3.0, roll=1.2,
                 lean=1.0, twist=1.1, arm_lower=80.0, arm_swing=7.0, elbow=10.0, elbow_swing=4.0,
                 coat_trail=3.0, hair=1.5),
    "WalkBack": dict(speed=1.10, cycle=1.00, duty=0.62, dir=(0.0, 1.0), width=0.085, bias=-0.03, lift=0.05,
                     heel=5.0, toe=10.0, pel_z=-0.035, bob=0.005, mode="walk", sway=0.008, yaw=2.0, roll=1.0,
                     lean=-1.0, twist=1.0, arm_lower=80.0, arm_swing=4.0, elbow=12.0, elbow_swing=2.0,
                     coat_trail=-3.0, hair=1.0),
    "StrafeLeft": dict(speed=1.15, cycle=0.60, duty=0.50, dir=(1.0, 0.0), width=0.0, bias=0.0, lift=0.05, centre=0.25,
                       heel=3.0, toe=8.0, pel_z=-0.05, bob=0.006, mode="walk", sway=0.0, yaw=0.0, roll=1.0,
                       lean=2.0, twist=0.0, arm_lower=78.0, arm_swing=3.0, elbow=16.0, elbow_swing=3.0,
                       coat_trail=0.0, hair=1.0, strafe=True),
    "StrafeRight": dict(speed=1.15, cycle=0.60, duty=0.50, dir=(-1.0, 0.0), width=0.0, bias=0.0, lift=0.05, centre=0.25,
                        heel=3.0, toe=8.0, pel_z=-0.05, bob=0.006, mode="walk", sway=0.0, yaw=0.0, roll=1.0,
                        lean=2.0, twist=0.0, arm_lower=78.0, arm_swing=3.0, elbow=16.0, elbow_swing=3.0,
                        coat_trail=0.0, hair=1.0, strafe=True),
    "Run": dict(speed=4.20, cycle=0.70, duty=0.30, dir=(0.0, -1.0), width=0.075, bias=0.05, lift=0.20,
                heel=8.0, toe=26.0, pel_z=-0.07, bob=0.018, mode="run", sway=0.008, yaw=6.0, roll=2.0,
                lean=7.0, twist=1.2, arm_lower=72.0, arm_swing=22.0, elbow=70.0, elbow_swing=8.0,
                coat_trail=18.0, hair=6.0),
    # locked-on running: hips toward the travel direction, chest and gaze stay on the target (as Rudeus, calmer)
    "RunStrafeLeft": dict(speed=3.80, cycle=0.70, duty=0.32, dir=(1.0, 0.0), width=0.075, bias=0.04, lift=0.15,
                          heel=7.0, toe=22.0, pel_z=-0.07, bob=0.014, mode="run", sway=0.006, yaw=4.0, roll=1.5,
                          lean=5.0, twist=1.1, arm_lower=72.0, arm_swing=14.0, elbow=68.0, elbow_swing=6.0,
                          coat_trail=14.0, hair=5.0, body_yaw=60.0, body_counter=0.7),
    "RunStrafeRight": dict(speed=3.80, cycle=0.70, duty=0.32, dir=(-1.0, 0.0), width=0.075, bias=0.04, lift=0.15,
                           heel=7.0, toe=22.0, pel_z=-0.07, bob=0.014, mode="run", sway=0.006, yaw=4.0, roll=1.5,
                           lean=5.0, twist=1.1, arm_lower=72.0, arm_swing=14.0, elbow=68.0, elbow_swing=6.0,
                           coat_trail=14.0, hair=5.0, body_yaw=-60.0, body_counter=0.7),
    "RunBack": dict(speed=3.30, cycle=0.70, duty=0.36, dir=(0.0, 1.0), width=0.08, bias=-0.04, lift=0.11,
                    heel=3.0, toe=10.0, pel_z=-0.065, bob=0.012, mode="run", sway=0.008, yaw=4.0, roll=1.5,
                    lean=0.0, twist=1.1, arm_lower=72.0, arm_swing=10.0, elbow=66.0, elbow_swing=5.0,
                    coat_trail=-12.0, hair=-3.0),
    "Sprint": dict(speed=6.60, cycle=0.62, duty=0.24, dir=(0.0, -1.0), width=0.07, bias=0.07, lift=0.26,
                   heel=5.0, toe=32.0, pel_z=-0.085, bob=0.022, mode="run", sway=0.006, yaw=8.0, roll=2.5,
                   lean=14.0, twist=1.3, arm_lower=68.0, arm_swing=34.0, elbow=84.0, elbow_swing=8.0,
                   coat_trail=30.0, hair=10.0),
}


def loop_gait(name):
    G = GAITS[name]
    G["cycle"] = round(G["cycle"] * FPS) / FPS
    return G["cycle"], True, (lambda t, G=G: gait_pose(G, (t / G["cycle"]) % 1.0))


FEET = ("L_fx", "L_fy", "L_fyaw", "R_fx", "R_fy", "R_fyaw")
GUARD_FEET = {"L_fx": 0.13, "L_fy": -0.08, "L_fyaw": 0.0, "R_fx": -0.13, "R_fy": 0.10, "R_fyaw": -20.0}
GUARD = dict(GUARD_FEET)
GUARD.update({"ik_w": 1.0, "pel_z": -0.05, "pel_yaw": -10.0, "sp_yaw": 7.0, "sp_pitch": 1.5, "head_yaw": 3.0,
              # open palms, low and relaxed: economical ready position
              "R_lower": 60.0, "R_fwd": 26.0, "R_elbow": 48.0, "R_wpitch": -12.0, "R_curl": 6.0, "R_thumb": 6.0,
              "L_lower": 64.0, "L_fwd": 18.0, "L_elbow": 40.0, "L_wpitch": -10.0, "L_curl": 8.0, "L_thumb": 8.0,
              "coat_flare": 2.0})
# Relaxed: the guard's footing (so leaving or entering combat never slides a foot), hips and shoulders nearly square,
# hands loosely clasped behind the back.
STAND = dict(GUARD_FEET)
STAND.update({"ik_w": 1.0, "pel_z": -0.03, "pel_yaw": -6.0, "sp_yaw": 4.5, "sp_pitch": 0.0, "head_pitch": 0.0, "head_yaw": 1.5,
              "L_lower": 78.0, "R_lower": 78.0, "L_swing": -26.0, "R_swing": -26.0, "L_elbow": 62.0, "R_elbow": 60.0,
              "L_wyaw": 20.0, "R_wyaw": 20.0, "L_curl": 30.0, "R_curl": 26.0})
# Open palm at full reach (wrist opened to 60 degrees: the wind-up pre-opens it, so the strike never snaps the wrist).
PALM_R = {"R_lower": 8.0, "R_fwd": 86.0, "R_elbow": 4.0, "R_wpitch": -60.0, "R_curl": 2.0, "R_thumb": 4.0}
# Compact arrival palm of Dragon Step (elbow still bent: the step itself carries the force).
PALM_COMPACT_R = {"R_lower": 22.0, "R_fwd": 74.0, "R_elbow": 24.0, "R_wpitch": -50.0, "R_curl": 4.0, "R_thumb": 6.0}


def idle(t):
    w = TAU * t / 4.0
    P = dict(STAND)
    # a third of Rudeus's sway: almost perfectly still
    P.update({"pel_x": 0.003 * math.sin(w), "sp_pitch": 0.5 * math.sin(w - 0.5), "head_yaw": 1.5 + 1.2 * math.sin(w + 1.2),
              "hair_lag": 0.3 * math.sin(w), "coat_trail": 0.2 * math.sin(w)})
    return P


def guard(t):
    w = TAU * t / 2.0
    P = dict(GUARD)
    P.update({"sp_pitch": GUARD["sp_pitch"] + 0.5 * math.sin(w), "R_elbow": GUARD["R_elbow"] + 1.0 * math.sin(w),
              "hair_lag": 0.4 * math.sin(2 * w)})
    return P


def oneshot(duration, keys, base):
    return duration, False, (lambda t, keys=keys, base=base: keyed(keys, t, base))


def _m(*dicts):
    out = {}
    for d in dicts:
        out.update(d)
    return out


def on_guard_footing(clip, src_feet):
    """Rudeus's clip on Orsted's footing: its feet are shifted from Rudeus's stance (src_feet) to the guard's, so it
    starts and ends where Orsted stands (no foot slide while the montage blends in or out). Steps inside the clip
    keep their shape; FK (airborne) legs are untouched."""
    duration, loop, fn = clip

    def shifted(t, fn=fn):
        P = fn(t)
        for k in FEET:
            P[k] = P.get(k, DEFAULTS[k]) - src_feet[k] + GUARD_FEET[k]
        return P
    return duration, loop, shifted


def _hit(pitch, roll, arms):
    """Minimal flinch from the guard (180 poise: light hits barely move him)."""
    return oneshot(0.33, [
        (0.00, GUARD),
        (0.08, _m(GUARD, {"sp_pitch": 1.5 + pitch, "sp_roll": roll, "head_pitch": pitch * 0.6, "head_roll": roll * 0.6,
                          "pel_y": -0.012 * math.copysign(1, pitch) if pitch else 0.0, "pel_z": -0.065, "hair_lag": 4.0}, arms)),
        (0.33, GUARD),
    ], GUARD)


KNEEL = _m(GUARD, {
    # down on the right (rear) knee: toes planted, heel up; the front foot never moves
    "pel_z": -0.47, "pel_y": 0.07, "pel_pitch": 4.0, "pel_yaw": -12.0, "sp_pitch": 20.0, "sp_yaw": 6.0, "head_pitch": -8.0,
    "R_fy": 0.34, "R_fpitch": -58.0, "R_fyaw": -12.0,
    "L_lower": 70.0, "L_fwd": 26.0, "L_elbow": 64.0, "L_wpitch": -10.0, "L_curl": 20.0,   # forearm over the front knee
    "R_lower": 62.0, "R_fwd": 0.0, "R_spread": 12.0, "R_elbow": 24.0, "R_curl": 30.0,
    "coat_trail": -4.0, "coat_flare": 6.0, "hair_lag": 2.0})

CLIPS = {
    "Idle": (4.0, True, idle),
    "CombatIdle": (2.0, True, guard),
    "Walk": loop_gait("Walk"),
    "WalkBack": loop_gait("WalkBack"),
    "StrafeLeft": loop_gait("StrafeLeft"),
    "StrafeRight": loop_gait("StrafeRight"),
    "Run": loop_gait("Run"),
    "Sprint": loop_gait("Sprint"),
    "RunStrafeLeft": loop_gait("RunStrafeLeft"),
    "RunStrafeRight": loop_gait("RunStrafeRight"),
    "RunBack": loop_gait("RunBack"),
    # Palm strike (Orsted_Basic: hit at 0.12 s, recovered by 0.40 s). The hips coil while the front foot lifts for the
    # half-step and the palm opens; the strike lands with the foot; the foot draws back lifted.
    "CastBasic": oneshot(0.45, [
        (0.00, GUARD),
        (0.05, _m(GUARD, {"pel_z": -0.065, "pel_yaw": -16.0, "sp_yaw": 10.0, "R_lower": 56.0, "R_fwd": 32.0, "R_elbow": 70.0,
                          "R_wpitch": -38.0, "L_fy": -0.11, "L_fz": 0.035})),
        (0.15, _m(GUARD, PALM_R, {"pel_z": -0.08, "pel_yaw": -3.0, "sp_yaw": -6.0, "L_swing": -12.0, "L_elbow": 70.0,
                                  "L_fy": -0.15, "coat_trail": 4.0})),
        (0.24, _m(GUARD, PALM_R, {"pel_z": -0.075, "pel_yaw": -4.0, "sp_yaw": -5.0, "L_swing": -10.0, "L_elbow": 66.0,
                                  "L_fy": -0.15, "R_elbow": 12.0, "R_wpitch": -52.0, "coat_trail": 2.0})),
        (0.34, _m(GUARD, {"L_fy": -0.115, "L_fz": 0.03, "R_lower": 44.0, "R_fwd": 40.0, "R_elbow": 36.0, "R_wpitch": -28.0})),
        (0.45, GUARD),
    ], GUARD),
    # Disturb Magic (window 0.05-0.40 s, 0.45 s recovery on a whiff): the off hand rises toward the spell over three
    # frames, the fingers flick through the window, then a readable, unhurried reset of the hand.
    "DisturbMagic": oneshot(0.85, [
        (0.00, GUARD),
        (0.12, _m(GUARD, {"L_lower": 32.0, "L_fwd": 60.0, "L_elbow": 66.0, "L_wpitch": -30.0, "L_curl": 4.0, "L_thumb": 6.0,
                          "head_pitch": -2.0, "sp_yaw": 5.0})),
        (0.24, _m(GUARD, {"L_lower": 26.0, "L_fwd": 66.0, "L_elbow": 62.0, "L_wpitch": -48.0, "L_curl": 22.0, "L_thumb": 20.0,
                          "head_pitch": -2.0, "sp_yaw": 5.0})),
        (0.40, _m(GUARD, {"L_lower": 29.0, "L_fwd": 63.0, "L_elbow": 66.0, "L_wpitch": -42.0, "L_curl": 8.0, "L_thumb": 8.0,
                          "head_pitch": -1.0, "sp_yaw": 6.0})),
        (0.62, _m(GUARD, {"L_lower": 46.0, "L_fwd": 40.0, "L_elbow": 52.0, "L_wpitch": -24.0, "L_curl": 8.0})),
        (0.85, GUARD),
    ], GUARD),
    # Dragon Step (launch 0.08 s, 0.18 s glide, arrival strike at 0.26 s, 0.3 s recovery): sink into the launch, a low
    # glide with the feet just off the ground and the coat snapping flat behind, plant on arrival with a compact palm.
    "DragonStep": oneshot(0.56, [
        (0.00, GUARD),
        (0.06, _m(GUARD, {"pel_z": -0.11, "sp_pitch": 11.0, "L_fpitch": -16.0, "R_fpitch": -22.0, "R_swing": -8.0, "L_swing": -6.0})),
        (0.14, _m(GUARD, {"ik_w": 0.0, "legs": "fk", "L_hip": 22.0, "L_knee": 30.0, "R_hip": -12.0, "R_knee": 26.0,
                          "pel_z": -0.075, "pel_pitch": 9.0, "sp_pitch": 14.0, "R_swing": -16.0, "L_swing": -14.0,
                          "R_elbow": 44.0, "L_elbow": 36.0, "coat_trail": 28.0, "hair_lag": 12.0})),
        (0.22, _m(GUARD, {"ik_w": 0.0, "legs": "fk", "L_hip": 20.0, "L_knee": 26.0, "R_hip": -8.0, "R_knee": 22.0,
                          "pel_z": -0.065, "pel_pitch": 7.0, "sp_pitch": 10.0, "L_swing": -8.0, "R_lower": 50.0, "R_fwd": 40.0,
                          "R_elbow": 58.0, "R_wpitch": -30.0, "coat_trail": 22.0, "hair_lag": 9.0})),
        (0.29, _m(GUARD, PALM_COMPACT_R, {"ik_w": 1.0, "pel_z": -0.10, "sp_pitch": 5.0, "coat_trail": 8.0, "hair_lag": 3.0})),
        (0.40, _m(GUARD, PALM_COMPACT_R, {"pel_z": -0.08, "sp_pitch": 3.0, "R_elbow": 30.0, "coat_trail": 2.0})),
        (0.56, GUARD),
    ], GUARD),
    # Saint Dragon Aura: stillness, then the chest opens and the hands turn outward slightly (buff at 0.35 s)
    "Aura": oneshot(0.90, [
        (0.00, GUARD),
        (0.35, _m(GUARD, {"pel_z": -0.03, "sp_pitch": -3.0, "head_pitch": -3.0, "R_lower": 66.0, "L_lower": 66.0,
                          "R_spread": 10.0, "L_spread": 10.0, "R_fwd": 8.0, "L_fwd": 8.0, "R_twist": 30.0, "L_twist": 30.0,
                          "coat_flare": 6.0})),
        (0.90, _m(GUARD, {"coat_flare": 4.0})),
    ], GUARD),
    # Dragon God (1.5 s transformation): complete stillness on the guard's footing, pressure builds (coat and hair
    # lift), the reveal, and control returns in the guard.
    "Awakening": oneshot(1.50, [
        (0.00, GUARD),
        (0.40, _m(GUARD, {"pel_z": -0.035, "pel_yaw": -6.0, "sp_yaw": 5.0, "sp_pitch": 0.0, "head_pitch": 4.0, "head_yaw": 1.0,
                          "L_lower": 80.0, "R_lower": 80.0, "L_fwd": 0.0, "R_fwd": 0.0, "L_elbow": 8.0, "R_elbow": 8.0,
                          "L_wpitch": 0.0, "R_wpitch": 0.0, "L_curl": 14.0, "R_curl": 14.0})),
        (1.10, _m(GUARD, {"pel_z": -0.03, "pel_yaw": -6.0, "sp_yaw": 5.0, "sp_pitch": -2.0, "head_pitch": -2.0, "head_yaw": 1.0,
                          "L_lower": 70.0, "R_lower": 70.0, "L_fwd": 0.0, "R_fwd": 0.0, "L_spread": 8.0, "R_spread": 8.0,
                          "L_elbow": 10.0, "R_elbow": 10.0, "L_wpitch": 0.0, "R_wpitch": 0.0, "L_curl": 10.0, "R_curl": 10.0,
                          "coat_flare": 16.0, "coat_trail": -6.0, "hair_lag": -10.0})),
        (1.50, _m(GUARD, {"coat_flare": 6.0})),
    ], GUARD),
    # Reactions from the guard: staggers are rare and short and read as surprise, not pain.
    "HitFront": _hit(-9.0, 0.0, {"L_swing": 10.0, "R_swing": 8.0, "L_elbow": 48.0, "R_elbow": 54.0}),
    "HitBack": _hit(9.0, 0.0, {"L_swing": -10.0, "R_swing": -9.0}),
    "HitLeft": _hit(0.0, -8.0, {"L_spread": 12.0, "L_lower": 56.0}),
    "HitRight": _hit(0.0, 8.0, {"R_spread": 12.0, "R_lower": 52.0}),
    "Stagger": oneshot(0.80, [
        (0.00, GUARD),
        (0.10, _m(GUARD, {"sp_pitch": -12.0, "head_pitch": -8.0, "pel_y": 0.04, "pel_z": -0.07, "L_spread": 12.0, "R_spread": 14.0,
                          "L_lower": 50.0, "R_lower": 48.0, "hair_lag": 6.0, "coat_trail": -6.0})),
        (0.22, _m(GUARD, {"sp_pitch": -6.0, "head_pitch": -4.0, "pel_y": 0.08, "pel_z": -0.08, "R_fy": 0.20, "R_fz": 0.06,
                          "L_spread": 8.0, "R_spread": 10.0, "hair_lag": 3.0})),
        (0.34, _m(GUARD, {"sp_pitch": 2.0, "pel_y": 0.09, "pel_z": -0.085, "R_fy": 0.24, "R_fz": 0.0})),
        (0.46, _m(GUARD, {"sp_pitch": 2.0, "pel_y": 0.07, "pel_z": -0.075, "R_fy": 0.235, "R_fz": 0.03})),
        (0.60, _m(GUARD, {"pel_y": 0.03, "pel_z": -0.06, "R_fy": 0.14, "R_fz": 0.04})),
        (0.70, _m(GUARD, {"pel_y": 0.01, "R_fy": 0.10, "R_fz": 0.0})),
        (0.80, GUARD),
    ], GUARD),
    # Knockdown: driven back a step and down onto the rear knee, then up again. He never rolls or lies on his back.
    "Knockdown": oneshot(1.40, [
        (0.00, GUARD),
        (0.10, _m(GUARD, {"sp_pitch": -18.0, "head_pitch": -12.0, "pel_y": 0.07, "pel_z": -0.09, "L_spread": 18.0, "R_spread": 22.0,
                          "L_lower": 46.0, "R_lower": 44.0, "L_elbow": 34.0, "R_elbow": 30.0, "hair_lag": 8.0, "coat_trail": -8.0})),
        (0.26, _m(GUARD, {"sp_pitch": -4.0, "head_pitch": -6.0, "pel_y": 0.13, "pel_z": -0.20, "R_fy": 0.26, "R_fz": 0.07,
                          "L_spread": 14.0, "R_spread": 18.0, "L_lower": 52.0, "R_lower": 50.0, "hair_lag": 5.0, "coat_trail": -6.0})),
        (0.40, _m(GUARD, {"sp_pitch": 12.0, "head_pitch": -8.0, "pel_y": 0.10, "pel_z": -0.36, "R_fy": 0.34, "R_fz": 0.0,
                          "R_fpitch": -40.0, "R_fyaw": -12.0, "L_lower": 62.0, "R_lower": 58.0, "R_spread": 12.0, "coat_trail": -4.0})),
        (0.60, KNEEL),
        (0.92, _m(KNEEL, {"head_pitch": 0.0, "sp_pitch": 16.0})),
        (1.08, _m(KNEEL, {"pel_z": -0.34, "sp_pitch": 18.0, "head_pitch": -2.0, "R_fpitch": -44.0})),
        (1.22, _m(GUARD, {"pel_z": -0.12, "pel_y": 0.03, "sp_pitch": 6.0, "R_fy": 0.20, "R_fz": 0.06, "R_fpitch": -10.0})),
        (1.40, GUARD),
    ], GUARD),
}

# Everything else re-authors Rudeus's definitions on Orsted's own proportions (IK targets and the ground clamp re-fit
# them): air and landing clips as they are; reactions and element / race casts on his guard footing, so they blend in
# and out of his stance without moving a planted foot. Every AnimSet key therefore exists as A_Orsted_<Key>, so his
# mesh never needs Rudeus's skeleton.
READY_FEET = {k: R.READY[k] for k in FEET}
COMBAT_FEET = {k: R.COMBAT[k] for k in FEET}
for _name in ("DodgeForward", "DodgeBack", "DodgeLeft", "DodgeRight", "Death"):
    CLIPS[_name] = on_guard_footing(R.CLIPS[_name], READY_FEET)
for _name in ("StoneCannon_Charge", "StoneCannon_Hold", "StoneCannon_Release", "Quagmire", "Barrage", "DemonEye",
              "CastTwoHand", "CastGround"):
    CLIPS[_name] = on_guard_footing(R.CLIPS[_name], COMBAT_FEET)
for _name in R.CLIPS:
    CLIPS.setdefault(_name, R.CLIPS[_name])
