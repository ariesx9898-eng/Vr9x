"""Orsted's casting clips as hand targets (the source of Tools/anim/clips_spells.py's Orsted keys; see generate.py).

Stance: clips_orsted.GUARD (every clip starts and ends there). Personality: minimal, upright, one small gesture of one
hand, 55-65% of Rudeus's length, and his Release earlier; the silhouette of each spell stays recognisable.
"""
import math

from design import H, arm_of
import clips_orsted as O

ST = O.GUARD

GUARD_R = dict(raw=arm_of(ST, "R"), curl=ST["R_curl"], fan=0.0, thumb=ST["R_thumb"], ci=0.0, cm=0.0, cr=0.0, cp=0.0)
GUARD_L = dict(raw=arm_of(ST, "L"), curl=ST["L_curl"], fan=0.0, thumb=ST["L_thumb"], ci=0.0, cm=0.0, cr=0.0, cp=0.0)
END = (O.GUARD, GUARD_R, GUARD_L)
POINT = dict(curl=80.0, ci=-82.0, cm=0.0, cr=0.0, cp=0.0, thumb=44.0, fan=-2.0)
TWO = dict(curl=80.0, ci=-82.0, cm=-84.0, cr=0.0, cp=0.0, thumb=46.0, fan=-5.0)
FIST = dict(curl=92.0, thumb=58.0, fan=-4.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)
OPEN = dict(curl=4.0, fan=12.0, thumb=8.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)
KNIFE = dict(curl=2.0, fan=-6.0, thumb=20.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)

SPELLS = {}
# His guard: R wrist (-0.30, 0.28, 1.16), L (0.31, 0.25, 1.12); shoulders z 1.575; head joint 1.73.

# ---- Fireball: the right palm turns up at the waist, fingers barely close; a short toss
OFB_BODY = {"sp_pitch": 3.0, "head_pitch": 3.0}
OFB_R = H((-0.20, 0.36, 1.24), (0.3, 0.0, 0.95), (0.2, 1.0, 0.0), dict(curl=32.0, fan=6.0, thumb=20.0, wn=0.15))
SPELLS["Cast_Fireball"] = (0.2, [
    (0.00, {}, None, None),
    (0.10, {"sp_pitch": 2.5, "head_pitch": 2.0}, H((-0.23, 0.34, 1.20), (0.8, 0.0, 0.5), (0.2, 1.0, 0.0), dict(curl=22.0, fan=8.0, thumb=14.0, wn=0.15)), None),
    (0.2, OFB_BODY, OFB_R, None),
])
SPELLS["Cast_Fireball_Release"] = (0.333, [
    (0.00, OFB_BODY, OFB_R, None),
    (0.07, {"sp_pitch": 3.5, "head_pitch": 2.0, "sp_yaw": 5.0, "coat_trail": 2.0},
     H((-0.18, 0.50, 1.30), (0.0, 0.4, 0.9), (0.05, 0.9, -0.4), OPEN), None),
    (0.14, {"sp_pitch": 3.0, "head_pitch": 1.5, "sp_yaw": 5.5},
     H((-0.18, 0.51, 1.32), (0.0, 0.2, 1.0), (0.05, 1.0, -0.2), OPEN), None),
    (0.333,) + END,
])

# ---- Flame Wave: a short horizontal sweep across the front at the waist
SPELLS["Cast_FlameWave"] = (0.6, [
    (0.00, {}, None, None),
    (0.14, {"sp_yaw": 13.0, "pel_yaw": -8.0, "head_yaw": -4.0},
     H((0.02, 0.40, 1.25), (0.2, 0.3, -0.95), (0.6, 0.8, 0.0), dict(curl=10.0, fan=8.0, thumb=10.0)), None),
    (0.26, {"sp_yaw": 4.0, "pel_yaw": -11.0, "head_yaw": 0.0, "coat_trail": 2.0},
     H((-0.22, 0.52, 1.28), (0.0, 0.5, -0.85), (-0.1, 1.0, -0.1), OPEN), None),
    (0.36, {"sp_yaw": -3.0, "pel_yaw": -13.0, "head_yaw": 3.0},
     H((-0.46, 0.36, 1.28), (-0.3, 0.5, -0.8), (-0.9, 0.4, 0.0), OPEN), None),
    (0.6,) + END,
])

# ---- Inferno: the hand rises overhead, closes, and comes down to the chest (small knee bend)
SPELLS["Cast_Inferno"] = (0.967, [
    (0.00, {}, None, None),
    (0.26, {"pel_z": -0.04, "sp_pitch": -1.0, "head_pitch": -6.0},
     H((-0.18, 0.32, 1.84), (0.2, 0.5, 0.8), None, dict(curl=6.0, fan=12.0, thumb=8.0, wn=0.1)), None),
    (0.36, {"pel_z": -0.04, "sp_pitch": -1.5, "head_pitch": -7.0},
     H((-0.18, 0.33, 1.83), (0.2, 0.5, 0.8), None, dict(FIST, wn=0.1)), None),
    (0.55, {"pel_z": -0.08, "pel_y": -0.02, "sp_pitch": 7.0, "head_pitch": -2.0, "coat_flare": 4.0},
     H((-0.16, 0.46, 1.42), None, None, FIST), None),
    (0.70, {"pel_z": -0.075, "pel_y": -0.018, "sp_pitch": 6.0, "head_pitch": -1.0, "coat_flare": 3.0},
     H((-0.17, 0.45, 1.43), None, None, FIST), None),
    (0.967,) + END,
])

# ---- Water Bullet: two fingers raised, a small snap
SPELLS["Cast_WaterBullet"] = (0.367, [
    (0.00, {}, None, None),
    (0.08, {"sp_yaw": 9.0}, H((-0.23, 0.38, 1.30), (0.8, 0.0, -0.6), (0.1, 0.8, 0.5), dict(TWO, wn=0.1, wf=0.15)), None),
    (0.14, {"sp_yaw": 6.0}, H((-0.21, 0.47, 1.34), (0.9, 0.0, -0.4), (0.0, 1.0, 0.0), dict(TWO, wn=0.1, wf=0.15)), None),
    (0.21, {"sp_yaw": 6.5}, H((-0.21, 0.46, 1.36), (0.9, 0.0, -0.4), (0.0, 0.85, 0.4), dict(TWO, wn=0.1, wf=0.15)), None),
    (0.367,) + END,
])

# ---- Water Dragon: one small rising spiral of both hands, then the right hand points
def odragon():
    keys = [(0.00, {}, None, None)]
    r, n = 0.05, 5
    for k in range(n + 1):
        t = 0.08 + 0.05 * k
        u = k / n
        th = math.radians(72.0 * k)
        z = 1.24 + (1.48 - 1.24) * u
        body = {"pel_z": -0.05 + 0.02 * u, "sp_pitch": 2.0 - 3.0 * u, "head_pitch": -4.0 * u, "sp_yaw": 2.0 * math.sin(th)}
        sides = {}
        for s_, cx, sg in (("R", -0.15, -1.0), ("L", 0.15, 1.0)):
            sides[s_] = H((cx + sg * r * (1.0 - math.cos(th)), 0.34 + r * math.sin(th), z), (-sg, 0.0, 0.3), None,
                          dict(curl=14.0, fan=8.0, thumb=10.0, wn=0.04))
        keys.append((t, body, sides["R"], sides["L"]))
    keys.append((0.45, {"pel_z": -0.04, "sp_yaw": 6.0, "pel_yaw": -12.0, "sp_pitch": 2.0},
                 H((-0.17, 0.52, 1.50), (0.9, 0.0, -0.3), (0.05, 0.95, 0.25), dict(POINT, wn=0.1, wf=0.15)),
                 H((0.22, 0.30, 1.40), None, None, dict(curl=12.0, fan=4.0, thumb=8.0))))
    keys.append((0.60, {"pel_z": -0.04, "sp_yaw": 6.0, "pel_yaw": -12.0, "sp_pitch": 2.0},
                 H((-0.18, 0.51, 1.48), (0.9, 0.0, -0.3), (0.05, 0.95, 0.2), dict(POINT, wn=0.1, wf=0.15)),
                 H((0.28, 0.26, 1.20), None, None, dict(curl=10.0, fan=0.0, thumb=8.0))))
    keys.append((0.9,) + END)
    return keys
SPELLS["Cast_WaterDragon"] = (0.9, odragon())

# ---- Flood: hands draw back to the hips, then both palms push forward at the waist
SPELLS["Cast_Flood"] = (0.9, [
    (0.00, {}, None, None),
    (0.19, {"pel_z": -0.05, "pel_y": 0.015, "sp_pitch": -1.0},
     H((-0.28, 0.16, 1.14), (0.0, 0.7, -0.7), None, dict(curl=16.0, fan=6.0, thumb=10.0, wn=0.1)),
     H((0.28, 0.16, 1.14), (0.0, 0.7, -0.7), None, dict(curl=16.0, fan=6.0, thumb=10.0, wn=0.1))),
    (0.42, {"pel_z": -0.08, "pel_y": -0.025, "sp_pitch": 6.0, "coat_trail": 3.0},
     H((-0.18, 0.50, 1.20), (0.0, 1.0, -0.2), None, dict(OPEN, wn=0.1)),
     H((0.16, 0.50, 1.20), (0.0, 1.0, -0.2), None, dict(OPEN, wn=0.1))),
    (0.58, {"pel_z": -0.075, "pel_y": -0.022, "sp_pitch": 5.0},
     H((-0.18, 0.49, 1.19), (0.0, 1.0, -0.2), None, dict(OPEN, wn=0.1)),
     H((0.16, 0.49, 1.19), (0.0, 1.0, -0.2), None, dict(OPEN, wn=0.1))),
    (0.9,) + END,
])

# ---- Earth Stone Cannon: hand forward, fingers close a little, short push
OSC_BODY = {"sp_yaw": 8.0}
OSC_R = H((-0.20, 0.48, 1.38), (0.2, 0.95, 0.0), (0.1, 0.1, 1.0), dict(curl=42.0, fan=2.0, thumb=30.0))
SPELLS["Cast_StoneCannon"] = (0.233, [
    (0.00, {}, None, None),
    (0.15, {"sp_yaw": 7.0}, H((-0.22, 0.45, 1.34), (0.4, 0.8, -0.3), None, dict(curl=26.0, fan=6.0, thumb=16.0, wn=0.1)), None),
    (0.233, OSC_BODY, OSC_R, None),
])
SPELLS["Cast_StoneCannon_Release"] = (0.3, [
    (0.00, OSC_BODY, OSC_R, None),
    (0.06, {"sp_yaw": 5.0, "coat_trail": 2.0}, H((-0.18, 0.58, 1.40), (0.1, 1.0, 0.1), (0.0, -0.1, 1.0), OPEN), None),
    (0.12, {"sp_yaw": 5.0}, H((-0.18, 0.58, 1.41), (0.1, 1.0, 0.1), (0.0, -0.1, 1.0), OPEN), None),
    (0.3,) + END,
])

# ---- Earth Wall: palm up low, lifted to the chest
SPELLS["Cast_EarthWall"] = (0.533, [
    (0.00, {}, None, None),
    (0.12, {"pel_z": -0.06, "sp_pitch": 4.0},
     H((-0.27, 0.34, 1.12), (0.6, 0.1, 0.75), None, dict(curl=26.0, fan=8.0, thumb=16.0, wn=0.1)), None),
    (0.25, {"pel_z": -0.045, "sp_pitch": 1.0, "head_pitch": -2.0},
     H((-0.22, 0.42, 1.40), (0.0, 0.1, 1.0), (0.1, 0.9, 0.3), dict(curl=14.0, fan=10.0, thumb=10.0)), None),
    (0.33, {"pel_z": -0.045, "sp_pitch": 0.0, "head_pitch": -3.0},
     H((-0.21, 0.40, 1.54), (0.0, 0.2, 1.0), (0.1, 0.5, 0.85), OPEN), None),
    (0.533,) + END,
])

# ---- Earth Spikes: palm down at the chest, pressed down and forward
SPELLS["Cast_EarthSpikes"] = (0.533, [
    (0.00, {}, None, None),
    (0.12, {"sp_pitch": 0.0, "sp_yaw": 6.0},
     H((-0.24, 0.38, 1.46), (0.0, 0.4, -0.9), (0.0, 0.9, 0.4), dict(curl=8.0, fan=8.0, thumb=8.0)), None),
    (0.25, {"pel_z": -0.07, "sp_pitch": 7.0, "sp_yaw": 3.0, "coat_trail": 2.0},
     H((-0.20, 0.50, 1.20), (0.0, 0.2, -1.0), (0.0, 0.9, -0.4), OPEN), None),
    (0.33, {"pel_z": -0.07, "sp_pitch": 7.0, "sp_yaw": 3.0},
     H((-0.20, 0.50, 1.18), (0.0, 0.2, -1.0), (0.0, 0.9, -0.4), OPEN), None),
    (0.533,) + END,
])

# ---- Wind Blade: a short knife-hand slash across the chest
SPELLS["Cast_WindBlade"] = (0.367, [
    (0.00, {}, None, None),
    (0.08, {"sp_yaw": 9.0, "pel_yaw": -9.0}, H((-0.15, 0.40, 1.28), (0.0, 0.0, -1.0), (0.4, 0.9, 0.0), dict(KNIFE, wn=0.1, wf=0.1)), None),
    (0.15, {"sp_yaw": 4.0, "pel_yaw": -11.0}, H((-0.27, 0.49, 1.32), (0.0, 0.0, -1.0), (-0.3, 0.95, 0.0), dict(KNIFE, wn=0.1, wf=0.1)), None),
    (0.25, {"sp_yaw": -2.0, "pel_yaw": -13.0}, H((-0.43, 0.40, 1.32), (0.0, 0.0, -1.0), (-0.8, 0.6, 0.0), dict(KNIFE, wn=0.1, wf=0.1)), None),
    (0.367,) + END,
])

# ---- Tornado: one small circle of the index finger, rising, then a flick up
def otornado():
    keys = [(0.00, {}, None, None)]
    cx, cf, r = -0.20, 0.44, 0.05
    n = 5
    for k in range(n + 1):
        t = 0.08 + 0.032 * k
        u = k / n
        th = math.radians(-90 + 360.0 * u)
        keys.append((t, {"sp_yaw": 7.0 + 2.0 * math.sin(th)},
                     H((cx + r * math.sin(th), cf + r * math.cos(th), 1.30 + 0.12 * u), (0.9, 0.0, 0.3), (0.0, 0.25, -1.0),
                       dict(POINT, wn=0.06, wf=0.1)), None))
    keys.append((0.40, {"sp_yaw": 6.0, "head_pitch": -5.0}, H((-0.20, 0.42, 1.66), (0.9, 0.2, 0.0), (0.0, 0.1, 1.0), dict(POINT, wn=0.06, wf=0.1)), None))
    keys.append((0.48, {"sp_yaw": 6.0, "head_pitch": -4.0}, H((-0.20, 0.42, 1.64), (0.9, 0.2, 0.0), (0.0, 0.1, 1.0), dict(POINT, wn=0.06, wf=0.1)), None))
    keys.append((0.667,) + END)
    return keys
SPELLS["Cast_Tornado"] = (0.667, otornado())

# ---- Wind Burst: hands close in a little, then open outward
SPELLS["Cast_WindBurst"] = (0.367, [
    (0.00, {}, None, None),
    (0.07, {"pel_z": -0.06, "sp_pitch": 3.0},
     H((-0.25, 0.30, 1.19), None, None, dict(curl=40.0, fan=0.0, thumb=26.0)),
     H((0.25, 0.28, 1.17), None, None, dict(curl=40.0, fan=0.0, thumb=26.0))),
    (0.20, {"pel_z": -0.05, "sp_pitch": -2.0, "coat_flare": 6.0, "hair_lag": -3.0},
     H((-0.38, 0.32, 1.24), (-1.0, 0.3, 0.0), None, dict(OPEN, wn=0.06)),
     H((0.38, 0.30, 1.24), (1.0, 0.3, 0.0), None, dict(OPEN, wn=0.06))),
    (0.24, {"pel_z": -0.05, "sp_pitch": -1.5, "coat_flare": 5.0},
     H((-0.37, 0.32, 1.23), (-1.0, 0.3, 0.0), None, dict(OPEN, wn=0.06)),
     H((0.37, 0.30, 1.23), (1.0, 0.3, 0.0), None, dict(OPEN, wn=0.06))),
    (0.367,) + END,
])

# ---- Disturb Magic (upgrade): the right hand (its cast socket) rises without a wind-up, the fingers flick at 0.06 s,
# the ward hand holds through the 0.35 s window, then an unhurried return
DM_HAND = H((-0.25, 0.40, 1.36), (0.6, 0.6, -0.3), None, dict(curl=4.0, fan=10.0, thumb=8.0, ci=0.0, cm=0.0, wn=0.15))
SPELLS["DisturbMagic"] = (0.85, [
    (0.00, {}, None, None),
    (0.05, {"sp_yaw": 8.0}, H((-0.28, 0.33, 1.24), (0.7, 0.3, -0.5), None, dict(curl=40.0, cm=30.0, thumb=48.0, fan=0.0, wn=0.15)), None),
    (0.14, {"sp_yaw": 8.5, "head_pitch": -1.0}, DM_HAND, None),
    (0.40, {"sp_yaw": 8.5, "head_pitch": -1.0}, H((-0.25, 0.41, 1.37), (0.6, 0.6, -0.3), None, dict(curl=6.0, fan=8.0, thumb=8.0, wn=0.15)), None),
    (0.62, {"sp_yaw": 7.5}, H((-0.29, 0.32, 1.22), None, None, dict(curl=8.0, fan=2.0, thumb=8.0)), None),
    (0.85,) + END,
])

# ---- Dragon Crush (new): the right arm pulled back, a driven palm at 0.40 (impact), held, recovered by 0.9 s.
# The palm keeps travelling a few centimetres past the impact frame, so the strike does not decelerate into it.
DC_IMPACT_BODY = {"pel_z": -0.16, "pel_y": -0.07, "pel_yaw": 4.0, "sp_yaw": 2.0, "sp_pitch": 12.0, "head_pitch": 2.0,
                  "coat_trail": 12.0, "coat_flare": 6.0, "hair_lag": 7.0, "L_fy": -0.17}
SPELLS["DragonCrush"] = (0.9, [
    (0.00, {}, None, None),
    (0.10, {"pel_z": -0.085, "pel_y": 0.02, "pel_yaw": -16.0, "sp_yaw": -4.0, "sp_pitch": 4.0, "head_yaw": 10.0, "coat_trail": -2.0},
     H((-0.31, 0.16, 1.17), None, None, dict(curl=14.0, fan=4.0, thumb=10.0, fix={"wpitch": -32.0})),
     H((0.22, 0.32, 1.18), None, None, dict(curl=16.0, fan=2.0, thumb=10.0))),
    (0.16, {"pel_z": -0.11, "pel_y": 0.035, "pel_yaw": -22.0, "sp_yaw": -7.0, "sp_pitch": 5.0, "head_yaw": 16.0, "coat_trail": -3.0,
            "L_fz": 0.025, "L_fy": -0.10},
     H((-0.32, 0.10, 1.16), None, None, dict(curl=20.0, fan=2.0, thumb=12.0, fix={"wpitch": -40.0})),
     H((0.23, 0.31, 1.18), None, None, dict(curl=16.0, fan=2.0, thumb=10.0))),
    (0.28, {"pel_z": -0.13, "pel_y": -0.01, "pel_yaw": -12.0, "sp_yaw": -3.0, "sp_pitch": 8.0, "head_yaw": 7.0, "coat_trail": 4.0,
            "L_fz": 0.025, "L_fy": -0.15},
     H((-0.27, 0.30, 1.20), None, None, dict(curl=8.0, fan=8.0, thumb=8.0, fix={"wpitch": -50.0})),
     H((0.25, 0.22, 1.18), None, None, dict(curl=26.0, thumb=14.0))),
    (0.40, DC_IMPACT_BODY, H((-0.12, 0.66, 1.26), (0.0, 0.95, -0.3), None, dict(curl=4.0, fan=14.0, thumb=8.0, wn=0.1)),
     H((0.27, 0.12, 1.18), None, None, dict(curl=34.0, thumb=18.0))),
    (0.45, dict(DC_IMPACT_BODY, pel_z=-0.162, pel_y=-0.072, coat_trail=11.0),
     H((-0.11, 0.69, 1.265), (0.0, 0.95, -0.28), None, dict(curl=5.0, fan=13.0, thumb=8.0, wn=0.1)), None),
    (0.55, dict(DC_IMPACT_BODY, pel_z=-0.155, coat_trail=7.0, hair_lag=4.0),
     H((-0.12, 0.68, 1.27), (0.0, 0.95, -0.25), None, dict(curl=6.0, fan=12.0, thumb=8.0, wn=0.1)), None),
    (0.67, {"pel_z": -0.11, "pel_y": -0.04, "pel_yaw": -4.0, "sp_yaw": 5.0, "sp_pitch": 6.0, "L_fy": -0.17, "coat_trail": 2.0},
     H((-0.24, 0.46, 1.20), None, None, dict(curl=10.0, fan=4.0, thumb=8.0)), "lerp"),
    (0.74, {"pel_z": -0.075, "pel_y": -0.02, "pel_yaw": -8.0, "sp_yaw": 6.0, "sp_pitch": 3.0, "L_fz": 0.035, "L_fy": -0.13},
     "lerp", "lerp"),
    (0.82, {"pel_z": -0.055, "pel_y": 0.0, "pel_yaw": -10.0, "sp_yaw": 7.0, "L_fz": 0.0, "L_fy": -0.08}, GUARD_R, GUARD_L),
    (0.9,) + END,
])


RELEASE_OF = {"Cast_Fireball_Release": "Cast_Fireball", "Cast_StoneCannon_Release": "Cast_StoneCannon",
              "StoneCannon_Release": "StoneCannon_Charge", "Quagmire_Release": "Quagmire"}
