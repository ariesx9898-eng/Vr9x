"""Orsted's clip set: same skeleton as Rudeus, different personality (see Docs/Orsted_Model_Spec.md §6).
Nothing wasted: upright, still, economical; open-palm technique; counters with minimal motion."""
import math
import clips as R
from clips import keyed, gait_pose, TAU, FPS

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


STAND = {"ik_w": 1.0, "L_fx": 0.10, "L_fy": 0.0, "R_fx": -0.10, "R_fy": 0.0, "L_fyaw": 4.0, "R_fyaw": -4.0,
         "pel_z": -0.02, "sp_pitch": 0.0, "head_pitch": 0.0,
         # hands loosely clasped behind the back
         "L_lower": 78.0, "R_lower": 78.0, "L_swing": -26.0, "R_swing": -26.0, "L_elbow": 62.0, "R_elbow": 60.0,
         "L_wyaw": 20.0, "R_wyaw": 20.0, "L_curl": 30.0, "R_curl": 26.0}
GUARD = {"ik_w": 1.0, "L_fx": 0.13, "L_fy": -0.08, "L_fyaw": 0.0, "R_fx": -0.13, "R_fy": 0.10, "R_fyaw": -20.0,
         "pel_z": -0.05, "pel_yaw": -10.0, "sp_yaw": 7.0, "sp_pitch": 1.5, "head_yaw": 3.0,
         # open palms, low and relaxed: economical ready position
         "R_lower": 60.0, "R_fwd": 26.0, "R_elbow": 48.0, "R_wpitch": -12.0, "R_curl": 6.0, "R_thumb": 6.0,
         "L_lower": 64.0, "L_fwd": 18.0, "L_elbow": 40.0, "L_wpitch": -10.0, "L_curl": 8.0, "L_thumb": 8.0,
         "coat_flare": 2.0}
PALM_R = {"R_lower": 8.0, "R_fwd": 86.0, "R_elbow": 4.0, "R_wpitch": -70.0, "R_curl": 2.0, "R_thumb": 4.0}


def idle(t):
    w = TAU * t / 4.0
    P = dict(STAND)
    # a third of Rudeus's sway: almost perfectly still
    P.update({"pel_x": 0.003 * math.sin(w), "sp_pitch": 0.5 * math.sin(w - 0.5), "head_yaw": 1.2 * math.sin(w + 1.2),
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
    # palm strike: short step-in, strike, immediate recovery
    "CastBasic": oneshot(0.45, [
        (0.00, GUARD),
        (0.08, _m(GUARD, {"pel_z": -0.07, "R_elbow": 70.0, "R_fwd": 30.0, "pel_yaw": -16.0})),
        (0.14, _m(GUARD, PALM_R, {"pel_z": -0.08, "pel_yaw": -4.0, "sp_yaw": -6.0, "L_swing": -12.0, "L_elbow": 70.0,
                                  "L_fy": -0.16, "coat_trail": 4.0})),
        (0.45, GUARD),
    ], GUARD),
    # Disturb Magic: minimal - the off hand rises, fingers flick through the timing window, hold
    "DisturbMagic": oneshot(0.55, [
        (0.00, GUARD),
        (0.05, _m(GUARD, {"L_lower": 30.0, "L_fwd": 62.0, "L_elbow": 70.0, "L_wpitch": -45.0, "L_curl": 4.0, "head_pitch": -2.0})),
        (0.20, _m(GUARD, {"L_lower": 26.0, "L_fwd": 66.0, "L_elbow": 64.0, "L_wpitch": -55.0, "L_curl": 22.0, "L_thumb": 20.0})),
        (0.40, _m(GUARD, {"L_lower": 30.0, "L_fwd": 62.0, "L_elbow": 70.0, "L_wpitch": -45.0, "L_curl": 6.0})),
        (0.55, GUARD),
    ], GUARD),
    # Dragon Step: launch frame, low glide (legs off the ground), plant
    "DragonStep": oneshot(0.50, [
        (0.00, _m(GUARD, {"ik_w": 1.0})),
        (0.06, _m(GUARD, {"ik_w": 1.0, "pel_z": -0.12, "sp_pitch": 12.0, "L_fpitch": -20.0, "R_fpitch": -24.0})),
        (0.12, _m(GUARD, {"ik_w": 0.0, "legs": "fk", "L_hip": 30.0, "L_knee": 38.0, "R_hip": -18.0, "R_knee": 30.0,
                          "pel_z": -0.07, "pel_pitch": 10.0, "sp_pitch": 16.0, "R_swing": -30.0, "L_swing": -26.0,
                          "R_elbow": 30.0, "L_elbow": 28.0, "coat_trail": 30.0, "hair_lag": 14.0})),
        (0.30, _m(GUARD, {"ik_w": 0.0, "legs": "fk", "L_hip": 24.0, "L_knee": 30.0, "R_hip": -10.0, "R_knee": 24.0,
                          "pel_z": -0.06, "pel_pitch": 8.0, "sp_pitch": 12.0, "R_swing": -20.0, "L_swing": -18.0,
                          "coat_trail": 24.0, "hair_lag": 10.0})),
        (0.40, _m(GUARD, {"ik_w": 1.0, "pel_z": -0.10, "sp_pitch": 6.0, "coat_trail": 8.0})),
        (0.50, GUARD),
    ], GUARD),
    # Saint Dragon Aura: stillness, then the chest opens and the hands turn outward slightly
    "Aura": oneshot(0.90, [
        (0.00, GUARD),
        (0.35, _m(GUARD, {"pel_z": -0.03, "sp_pitch": -3.0, "head_pitch": -3.0, "R_lower": 66.0, "L_lower": 66.0,
                          "R_spread": 10.0, "L_spread": 10.0, "R_fwd": 8.0, "L_fwd": 8.0, "R_twist": 30.0, "L_twist": 30.0,
                          "coat_flare": 6.0})),
        (0.90, _m(GUARD, {"coat_flare": 4.0})),
    ], GUARD),
    # Dragon God: complete stillness, pressure builds (coat and hair lift), reveal
    "Awakening": oneshot(1.50, [
        (0.00, GUARD),
        (0.40, _m(STAND, {"L_swing": 0.0, "R_swing": 0.0, "L_lower": 80.0, "R_lower": 80.0, "L_elbow": 8.0, "R_elbow": 8.0,
                          "L_wyaw": 0.0, "R_wyaw": 0.0, "head_pitch": 4.0})),
        (1.10, _m(STAND, {"L_swing": 0.0, "R_swing": 0.0, "L_lower": 70.0, "R_lower": 70.0, "L_spread": 8.0, "R_spread": 8.0,
                          "L_elbow": 10.0, "R_elbow": 10.0, "L_wyaw": 0.0, "R_wyaw": 0.0, "L_curl": 10.0, "R_curl": 10.0,
                          "head_pitch": -2.0, "sp_pitch": -2.0, "coat_flare": 16.0, "coat_trail": -6.0, "hair_lag": -10.0})),
        (1.50, _m(GUARD, {"coat_flare": 6.0})),
    ], GUARD),
}

# Everything not overridden (air, dodges, hits, stagger, knockdown, death, and the element / race casts any
# lineage can use: CastTwoHand, CastGround, the Stone Cannon charge set, Demon Eye ...) re-authors Rudeus's
# definitions on Orsted's own proportions (IK targets and the ground clamp re-fit them). Every AnimSet key
# therefore exists as A_Orsted_<Key>, so his mesh never needs Rudeus's skeleton.
for _name in R.CLIPS:
    CLIPS.setdefault(_name, R.CLIPS[_name])
