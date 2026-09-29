"""Rudeus's casting clips as hand targets (the source of Tools/anim/clips_spells.py's Rudeus keys; see generate.py).

Stance: clips.COMBAT (every clip starts and ends there). Personality: the hands shape the magic (fingers curl, fan and
roll, wrists turn, the left hand shapes or braces), build-up is visible, releases are explosive and big ones recoil.
"""
import math

from design import H, arm_of
import clips as C

ST = C.COMBAT

COMBAT_R = dict(raw=arm_of(ST, "R"), curl=ST["R_curl"], fan=0.0, thumb=ST["R_thumb"], ci=0.0, cm=0.0, cr=0.0, cp=0.0)
COMBAT_L = dict(raw=arm_of(ST, "L"), curl=ST["L_curl"], fan=0.0, thumb=12.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)
END = (C.COMBAT, COMBAT_R, COMBAT_L)
POINT = dict(curl=82.0, ci=-84.0, cm=0.0, cr=0.0, cp=0.0, thumb=48.0, fan=-2.0)          # index point
TWO = dict(curl=82.0, ci=-84.0, cm=-86.0, cr=0.0, cp=0.0, thumb=50.0, fan=-5.0)          # index + middle (water bullet)
FIST = dict(curl=96.0, thumb=62.0, fan=-4.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)
OPEN = dict(curl=3.0, fan=20.0, thumb=12.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)
KNIFE = dict(curl=3.0, fan=-8.0, thumb=26.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)
CLAW = dict(curl=58.0, fan=4.0, thumb=40.0, ci=0.0, cm=0.0, cr=0.0, cp=0.0)

SPELLS = {}

# ------------------------------------------------------------------ Fireball
FB_BODY = {"pel_z": -0.10, "pel_yaw": -16.0, "sp_pitch": 10.0, "sp_yaw": 12.0, "head_pitch": 6.0, "coat_flare": 5.0, "hair_lag": 1.0}
FB_R = H((-0.07, 0.30, 0.93), (0.15, -0.1, 1.0), (0.45, 0.9, 0.0), dict(curl=40.0, fan=10.0, thumb=28.0))
FB_L = H((0.03, 0.30, 1.15), (-0.1, 0.0, -1.0), (-0.5, 0.85, -0.1), dict(curl=34.0, fan=8.0, thumb=20.0))
SPELLS["Cast_Fireball"] = (0.333, [
    (0.00, {}, None, None),
    (0.13, {"pel_z": -0.095, "pel_yaw": -15.0, "sp_pitch": 8.0, "sp_yaw": 10.0, "head_pitch": 3.0, "coat_flare": 4.5},
     H((-0.16, 0.30, 0.88), (0.9, 0.0, 0.35), (0.25, 1.0, 0.0), dict(curl=4.0, fan=18.0, thumb=10.0)),
     H((0.10, 0.27, 1.08), (-0.3, 0.0, -1.0), (-0.4, 0.9, 0.0), dict(curl=18.0, fan=14.0, thumb=12.0))),
    (0.25, FB_BODY, FB_R, FB_L),
    (0.333, FB_BODY, FB_R, FB_L),
])
SPELLS["Cast_Fireball_Release"] = (0.567, [
    (0.00, FB_BODY, FB_R, FB_L),
    (0.10, {"pel_z": -0.10, "pel_yaw": -8.0, "pel_y": -0.015, "sp_pitch": 13.0, "sp_yaw": -2.0, "head_pitch": 2.0, "coat_trail": 6.0, "hair_lag": 4.0},
     H((-0.12, 0.50, 1.05), (0.0, 0.35, 0.94), (0.05, 0.94, -0.35), dict(curl=6.0, fan=18.0, thumb=14.0)),
     H((0.10, 0.22, 1.08), (-0.6, 0.0, -0.8), None, dict(curl=30.0, fan=0.0, thumb=16.0))),
    (0.17, {"pel_z": -0.10, "pel_yaw": -7.0, "pel_y": -0.02, "sp_pitch": 14.0, "sp_yaw": -3.0, "head_pitch": 2.0, "coat_trail": 5.0, "hair_lag": 4.0},
     H((-0.12, 0.53, 1.09), (0.0, -0.2, 1.0), (0.05, 0.9, 0.4), dict(curl=2.0, fan=20.0, thumb=12.0)), None),
    (0.28, {"pel_z": -0.095, "pel_yaw": -10.0, "pel_y": 0.01, "sp_pitch": 3.0, "sp_yaw": 2.0, "head_pitch": -4.0, "coat_trail": -2.0, "hair_lag": -2.0},
     H((-0.18, 0.38, 1.10), (0.3, 0.3, 0.9), (0.1, 0.4, 0.9), dict(curl=14.0, fan=10.0, thumb=12.0)),
     H((0.14, 0.20, 1.00), (-0.8, 0.0, -0.6), None, dict(curl=30.0, thumb=16.0))),
    (0.567,) + END,
])

# ------------------------------------------------------------------ Flame Wave (sweep left -> right, palm out pushing a wall)
SPELLS["Cast_FlameWave"] = (1.0, [
    (0.00, {}, None, None),
    (0.16, {"pel_z": -0.13, "pel_yaw": -2.0, "sp_yaw": 26.0, "sp_pitch": 10.0, "head_yaw": -18.0, "coat_flare": 6.0, "hair_lag": 2.0},
     H((0.14, 0.22, 0.86), (0.3, -0.4, -0.85), (1.0, 0.1, -0.2), dict(curl=26.0, fan=6.0, thumb=16.0)),
     H((0.26, 0.08, 0.92), (0.0, 0.0, -1.0), None, dict(curl=30.0, fan=0.0, thumb=14.0, wn=0.1))),
    (0.32, {"pel_z": -0.12, "pel_yaw": -8.0, "sp_yaw": 14.0, "sp_pitch": 10.0, "head_yaw": -8.0, "coat_flare": 6.0},
     H((0.10, 0.44, 0.98), (0.3, 0.6, -0.75), (0.7, 0.6, 0.2), dict(curl=8.0, fan=14.0, thumb=12.0)), None),
    (0.42, {"pel_z": -0.11, "pel_yaw": -14.0, "sp_yaw": 0.0, "sp_pitch": 11.0, "head_yaw": 0.0, "coat_trail": 4.0, "coat_flare": 7.0, "hair_lag": 4.0},
     H((-0.14, 0.48, 1.02), (0.0, 0.6, -0.8), (-0.1, 1.0, -0.1), dict(curl=2.0, fan=20.0, thumb=10.0)), None),
    (0.54, {"pel_z": -0.11, "pel_yaw": -20.0, "sp_yaw": -16.0, "sp_pitch": 9.0, "head_yaw": 10.0, "coat_trail": 3.0, "coat_flare": 8.0, "hair_lag": 3.0},
     H((-0.48, 0.24, 1.06), (-0.3, 0.6, -0.7), (-0.9, 0.4, 0.1), dict(curl=2.0, fan=22.0, thumb=10.0)),
     H((0.22, 0.20, 0.98), (-0.5, 0.3, -0.8), None, dict(curl=30.0, thumb=14.0))),
    (0.66, {"pel_z": -0.105, "pel_yaw": -19.0, "sp_yaw": -14.0, "sp_pitch": 8.0, "head_yaw": 8.0, "coat_flare": 6.0},
     H((-0.46, 0.24, 1.00), (-0.3, 0.6, -0.7), (-0.9, 0.4, -0.1), dict(curl=14.0, fan=12.0, thumb=12.0)), None),
    (1.0,) + END,
])

# ------------------------------------------------------------------ Inferno (raise overhead, clench, slam down; full body)
SPELLS["Cast_Inferno"] = (1.6, [
    (0.00, {}, None, None),
    (0.24, {"pel_z": -0.07, "pel_y": 0.01, "sp_pitch": 2.0, "sp_yaw": 6.0, "head_pitch": -4.0, "coat_flare": 5.0},
     H((-0.16, 0.30, 1.30), (0.2, 0.2, 1.0), (0.2, 0.8, 0.3), dict(curl=10.0, fan=16.0, thumb=12.0)),
     H((0.40, 0.10, 0.95), (0.0, 0.0, -1.0), None, dict(curl=16.0, fan=10.0, thumb=12.0))),
    (0.46, {"pel_z": -0.04, "pel_y": 0.03, "sp_pitch": -6.0, "sp_yaw": 4.0, "head_pitch": -16.0, "coat_flare": 7.0, "hair_lag": -3.0},
     H((-0.14, 0.12, 1.60), (0.3, 0.6, 0.7), (0.0, 0.0, 1.0), dict(curl=5.0, fan=22.0, thumb=10.0)),
     H((0.42, 0.05, 0.90), (0.0, 0.0, -1.0), None, dict(curl=16.0, fan=12.0, thumb=12.0))),
    (0.58, {"pel_z": -0.035, "pel_y": 0.03, "sp_pitch": -7.0, "sp_yaw": 4.0, "head_pitch": -17.0, "coat_flare": 7.0, "hair_lag": -3.0},
     H((-0.14, 0.13, 1.60), (0.3, 0.6, 0.7), (0.0, 0.0, 1.0), FIST), None),
    (0.76, {"pel_z": -0.12, "pel_y": -0.01, "sp_pitch": 12.0, "sp_yaw": 0.0, "head_pitch": -2.0, "coat_flare": 8.0, "hair_lag": 2.0},
     H((-0.12, 0.40, 1.28), None, None, FIST),
     H((0.36, -0.05, 0.88), (0.0, 0.0, -1.0), None, dict(curl=24.0, thumb=14.0))),
    (0.90, {"pel_z": -0.27, "pel_y": -0.05, "sp_pitch": 30.0, "sp_yaw": -4.0, "head_pitch": -10.0, "coat_flare": 10.0, "coat_trail": 6.0, "hair_lag": 6.0},
     H((-0.10, 0.42, 0.74), None, (0.0, 0.3, -0.95), dict(FIST, wf=0.08)),
     H((0.35, -0.12, 0.95), (0.0, -1.0, 0.0), None, dict(curl=30.0, thumb=16.0))),
    (1.10, {"pel_z": -0.25, "pel_y": -0.045, "sp_pitch": 27.0, "sp_yaw": -3.0, "head_pitch": -8.0, "coat_flare": 8.0, "hair_lag": 3.0},
     H((-0.11, 0.41, 0.77), None, (0.0, 0.3, -0.95), dict(FIST, wf=0.08)), None),
    (1.6,) + END,
])

# ------------------------------------------------------------------ Water Bullet (two fingers, snap forward)
SPELLS["Cast_WaterBullet"] = (0.6, [
    (0.00, {}, None, None),
    (0.11, {"pel_yaw": -17.0, "sp_yaw": 12.0, "sp_pitch": 8.0, "head_pitch": -3.0},
     H((-0.20, 0.27, 1.08), (0.8, 0.0, -0.6), (0.1, 0.8, 0.6), TWO), None),
    (0.22, {"pel_yaw": -10.0, "sp_yaw": 4.0, "sp_pitch": 10.0, "coat_trail": 3.0, "hair_lag": 2.0},
     H((-0.15, 0.39, 1.12), (0.9, 0.0, -0.4), (0.0, 1.0, -0.05), TWO), None),
    (0.29, {"pel_yaw": -11.0, "sp_yaw": 5.0, "sp_pitch": 7.0, "head_pitch": -5.0, "hair_lag": -1.0},
     H((-0.16, 0.37, 1.16), (0.9, 0.0, -0.4), (0.0, 0.75, 0.65), TWO), None),
    (0.42, {"pel_yaw": -13.0, "sp_yaw": 8.0, "sp_pitch": 7.0},
     H((-0.22, 0.34, 1.00), None, None, dict(curl=40.0, ci=-20.0, cm=-20.0, thumb=24.0, fan=0.0)), None),
    (0.6,) + END,
])

# ------------------------------------------------------------------ Water Dragon (hands spiral up around each other, then point)
def dragon_keys():
    keys = [(0.00, {}, None, None),
            (0.10, {"pel_z": -0.14, "sp_pitch": 8.0, "head_pitch": 4.0, "coat_flare": 5.0},
             H((-0.14, 0.26, 0.90), (1.0, 0.0, 0.2), (0.0, 1.0, 0.1), dict(curl=24.0, fan=12.0, thumb=14.0, wn=0.1, wf=0.1)),
             H((0.14, 0.26, 0.90), (-1.0, 0.0, 0.2), (0.0, 1.0, 0.1), dict(curl=24.0, fan=12.0, thumb=14.0, wn=0.1, wf=0.1)))]
    r = 0.07
    n = 5
    for k in range(1, n + 1):
        t = 0.10 + 0.09 * k
        u = k / n
        th = math.radians(90.0 * k)
        z = 0.90 + (1.42 - 0.90) * u
        body = {"pel_z": -0.14 + 0.10 * u, "sp_pitch": 8.0 - 12.0 * u, "head_pitch": 4.0 - 16.0 * u,
                "sp_yaw": 4.0 * math.sin(th), "coat_flare": 5.0 + 4.0 * u, "hair_lag": -2.0 * u}
        sides = {}
        for s_, cx, sg in (("R", -0.14, -1.0), ("L", 0.14, 1.0)):
            x = cx + sg * r * (1.0 - math.cos(th))      # mirrored circles, starting at each hand's inner point
            f = 0.26 + r * math.sin(th)
            sides[s_] = H((x, f, z), (-sg, 0.0, 0.3), (0.0, 0.2, 1.0), dict(curl=22.0, fan=14.0, thumb=14.0, wn=0.04, wf=0.06))
        keys.append((t, body, sides["R"], sides["L"]))
    keys.append((0.62, {"pel_z": -0.04, "sp_pitch": -5.0, "head_pitch": -14.0, "coat_flare": 9.0, "hair_lag": -3.0,
                        "L_fpitch": -8.0, "R_fpitch": -8.0},
                 H((-0.15, 0.24, 1.52), (0.8, 0.2, 0.2), (0.0, 0.2, 1.0), dict(curl=12.0, fan=18.0, thumb=12.0, wn=0.1, wf=0.1)),
                 H((0.15, 0.24, 1.52), (-0.8, 0.2, 0.2), (0.0, 0.2, 1.0), dict(curl=12.0, fan=18.0, thumb=12.0, wn=0.1, wf=0.1))))
    keys.append((0.75, {"pel_z": -0.09, "pel_yaw": -20.0, "sp_yaw": 8.0, "sp_pitch": 7.0, "head_pitch": 0.0, "coat_flare": 8.0,
                        "coat_trail": 5.0, "hair_lag": 4.0},
                 H((-0.12, 0.47, 1.28), (0.9, 0.0, -0.3), (0.05, 0.9, 0.35), dict(POINT, wn=0.15, wf=0.2)),
                 H((0.22, 0.24, 1.40), (-0.6, 0.3, 0.6), None, dict(curl=16.0, fan=14.0, thumb=12.0, wn=0.05))))
    keys.append((0.95, {"pel_z": -0.09, "pel_yaw": -19.0, "sp_yaw": 8.0, "sp_pitch": 6.0, "head_pitch": 0.0, "coat_flare": 6.0},
                 H((-0.13, 0.46, 1.25), (0.9, 0.0, -0.3), (0.05, 0.92, 0.3), dict(POINT, wn=0.15, wf=0.2)),
                 H((0.28, 0.14, 1.10), None, None, dict(curl=20.0, fan=8.0, thumb=12.0))))
    keys.append((1.5,) + END)
    return keys
SPELLS["Cast_WaterDragon"] = (1.5, dragon_keys())

# ------------------------------------------------------------------ Flood (gather back, slam forward; stomp)
SPELLS["Cast_Flood"] = (1.5, [
    (0.00, {}, None, None),
    (0.28, {"pel_z": -0.12, "pel_y": 0.05, "pel_yaw": -8.0, "sp_pitch": -6.0, "sp_yaw": 2.0, "head_pitch": 4.0, "coat_trail": -6.0, "hair_lag": -3.0},
     H((-0.32, -0.14, 0.94), (0.0, 0.5, -0.85), None, dict(curl=20.0, fan=10.0, thumb=14.0, swing=True)),
     H((0.32, -0.14, 0.94), (0.0, 0.5, -0.85), None, dict(curl=20.0, fan=10.0, thumb=14.0, swing=True))),
    (0.46, {"pel_z": -0.12, "pel_y": 0.06, "pel_yaw": -8.0, "sp_pitch": -10.0, "sp_yaw": 2.0, "head_pitch": 2.0, "coat_trail": -8.0, "hair_lag": -4.0,
            "L_fz": 0.05, "L_fpitch": 6.0},
     H((-0.33, -0.20, 1.04), (0.0, 0.5, -0.85), None, dict(curl=16.0, fan=14.0, thumb=12.0, swing=True)),
     H((0.33, -0.20, 1.04), (0.0, 0.5, -0.85), None, dict(curl=16.0, fan=14.0, thumb=12.0, swing=True))),
    (0.61, {"pel_z": -0.17, "pel_y": 0.0, "pel_yaw": -9.0, "sp_pitch": 12.0, "sp_yaw": 2.0, "head_pitch": -2.0, "coat_trail": 2.0, "hair_lag": 2.0,
            "L_fz": 0.02, "L_fpitch": 2.0},
     H((-0.28, 0.18, 0.96), (0.0, 0.9, -0.4), (0.0, 0.4, 0.9), dict(curl=8.0, fan=18.0, thumb=12.0, swing=True)),
     H((0.24, 0.18, 0.96), (0.0, 0.9, -0.4), (0.0, 0.4, 0.9), dict(curl=8.0, fan=18.0, thumb=12.0, swing=True))),
    (0.70, {"pel_z": -0.22, "pel_y": -0.06, "pel_yaw": -10.0, "sp_pitch": 26.0, "sp_yaw": 2.0, "head_pitch": -8.0, "coat_trail": 8.0, "coat_flare": 8.0,
            "hair_lag": 6.0},
     H((-0.17, 0.44, 0.88), (0.0, 0.8, -0.6), (0.0, 0.6, 0.8), dict(OPEN, swing=True)),
     H((0.12, 0.44, 0.88), (0.0, 0.8, -0.6), (0.0, 0.6, 0.8), dict(OPEN, swing=True))),
    (0.90, {"pel_z": -0.20, "pel_y": -0.05, "pel_yaw": -10.0, "sp_pitch": 22.0, "sp_yaw": 2.0, "head_pitch": -6.0, "coat_trail": 4.0, "coat_flare": 7.0},
     H((-0.18, 0.43, 0.87), (0.0, 0.8, -0.6), (0.0, 0.6, 0.8), dict(curl=8.0, fan=14.0, swing=True)),
     H((0.13, 0.43, 0.87), (0.0, 0.8, -0.6), (0.0, 0.6, 0.8), dict(curl=8.0, fan=14.0, swing=True))),
    (1.12, {"pel_z": -0.14, "pel_y": -0.02, "pel_yaw": -12.0, "sp_pitch": 12.0, "sp_yaw": 5.0, "head_pitch": -4.0, "coat_trail": 2.0, "coat_flare": 5.0},
     H((-0.24, 0.32, 0.88), None, None, dict(curl=14.0, fan=6.0, thumb=10.0, swing=True)),
     H((0.16, 0.30, 0.90), None, None, dict(curl=24.0, fan=4.0, thumb=12.0, swing=True))),
    (1.5,) + END,
])

# ------------------------------------------------------------------ Earth Wall (drive up from low)
SPELLS["Cast_EarthWall"] = (0.9, [
    (0.00, {}, None, None),
    (0.20, {"pel_z": -0.20, "sp_pitch": 28.0, "head_pitch": -8.0, "coat_flare": 6.0, "sp_yaw": 8.0},
     H((-0.14, 0.36, 0.64), (0.5, 0.0, 0.85), (0.0, 1.0, 0.1), dict(curl=36.0, fan=16.0, thumb=24.0)),
     H((0.16, 0.20, 0.64), (0.0, 0.0, -1.0), None, dict(curl=24.0, thumb=14.0))),
    (0.40, {"pel_z": -0.09, "sp_pitch": 6.0, "head_pitch": -2.0, "coat_flare": 7.0, "sp_yaw": 6.0, "hair_lag": -2.0},
     H((-0.12, 0.34, 1.12), (0.0, 0.0, 1.0), (0.0, 0.3, 1.0), dict(curl=20.0, fan=16.0, thumb=16.0)),
     H((0.25, 0.10, 0.88), (0.0, 0.0, -1.0), None, dict(curl=24.0, thumb=14.0))),
    (0.56, {"pel_z": -0.06, "sp_pitch": -2.0, "head_pitch": -8.0, "coat_flare": 8.0, "sp_yaw": 5.0, "hair_lag": -3.0},
     H((-0.10, 0.30, 1.46), (0.0, 0.2, 1.0), None, OPEN), None),
    (0.66, {"pel_z": -0.065, "sp_pitch": -1.0, "head_pitch": -6.0, "coat_flare": 6.0, "sp_yaw": 5.0},
     H((-0.11, 0.30, 1.43), (0.0, 0.2, 1.0), None, dict(curl=10.0, fan=14.0, thumb=12.0)), None),
    (0.9,) + END,
])

# ------------------------------------------------------------------ Earth Spikes (drive down along the aim)
SPELLS["Cast_EarthSpikes"] = (0.9, [
    (0.00, {}, None, None),
    (0.18, {"pel_z": -0.07, "sp_pitch": -2.0, "sp_yaw": 10.0, "head_pitch": -4.0, "coat_flare": 5.0},
     H((-0.22, 0.22, 1.36), (0.0, 0.5, -0.85), (0.0, 0.85, 0.5), dict(curl=10.0, fan=16.0, thumb=12.0)), None),
    (0.40, {"pel_z": -0.20, "pel_y": -0.03, "sp_pitch": 30.0, "sp_yaw": 2.0, "head_pitch": -10.0, "coat_trail": 5.0, "coat_flare": 7.0, "hair_lag": 5.0},
     H((-0.08, 0.48, 0.72), (0.0, 0.2, -1.0), (0.0, 0.9, -0.4), OPEN),
     H((0.30, 0.00, 0.95), (0.0, 0.0, -1.0), None, dict(curl=24.0, thumb=14.0))),
    (0.52, {"pel_z": -0.21, "pel_y": -0.03, "sp_pitch": 31.0, "sp_yaw": 2.0, "head_pitch": -9.0, "coat_flare": 6.0, "hair_lag": 3.0},
     H((-0.08, 0.47, 0.69), (0.0, 0.2, -1.0), (0.0, 0.9, -0.4), dict(curl=8.0, fan=18.0, thumb=12.0)), None),
    (0.9,) + END,
])

# ------------------------------------------------------------------ Wind Blade (knife-hand horizontal slash)
SPELLS["Cast_WindBlade"] = (0.6, [
    (0.00, {}, None, None),
    (0.14, {"pel_yaw": -8.0, "sp_yaw": 20.0, "sp_pitch": 8.0, "head_yaw": -12.0, "coat_flare": 4.0},
     H((0.04, 0.36, 1.15), (0.0, 0.0, -1.0), (0.75, 0.65, 0.0), dict(KNIFE, wn=0.15, wf=0.15)),
     H((0.22, 0.10, 0.95), (0.0, 0.0, -1.0), None, dict(curl=30.0, thumb=14.0))),
    (0.25, {"pel_yaw": -14.0, "sp_yaw": 4.0, "sp_pitch": 9.0, "head_yaw": 0.0, "coat_trail": 3.0, "hair_lag": 3.0},
     H((-0.12, 0.45, 1.14), (0.0, 0.0, -1.0), (-0.5, 0.85, 0.0), dict(KNIFE, wn=0.15, wf=0.15)), None),
    (0.40, {"pel_yaw": -19.0, "sp_yaw": -14.0, "sp_pitch": 8.0, "head_yaw": 8.0, "coat_trail": 2.0, "hair_lag": 3.0},
     H((-0.40, 0.30, 1.10), (0.0, 0.0, -1.0), (-1.0, 0.1, 0.0), dict(KNIFE, wn=0.15, wf=0.15)), None),
    (0.46, {"pel_yaw": -18.0, "sp_yaw": -11.0, "sp_pitch": 8.0, "head_yaw": 6.0},
     H((-0.40, 0.28, 1.04), (0.0, 0.2, -1.0), (-0.9, 0.3, -0.1), dict(curl=12.0, fan=0.0, thumb=16.0)), None),
    (0.6,) + END,
])

# ------------------------------------------------------------------ Tornado (circular stirring, rising, fling up)
def tornado_keys():
    keys = [(0.00, {}, None, None)]
    cx, cf, r = -0.13, 0.36, 0.06
    n = 8
    for k in range(n + 1):
        t = 0.06 + 0.0475 * k
        u = k / n
        th = math.radians(-90 + 360.0 * u)
        z = 0.98 + 0.20 * u
        x, f = cx + r * math.sin(th), cf + r * math.cos(th)
        body = {"pel_z": -0.10 + 0.02 * u, "sp_yaw": 10.0 + 5.0 * math.sin(th), "sp_pitch": 9.0 - 4.0 * u,
                "head_pitch": 4.0 - 6.0 * u, "coat_flare": 5.0 + 3.0 * u}
        R = H((x, f, z), (0.9, 0.0, 0.3), (0.0, 0.25, -1.0), dict(POINT, wn=0.08, wf=0.12))
        keys.append((t, body, R, None))
    keys.append((0.62, {"pel_z": -0.06, "sp_pitch": -4.0, "head_pitch": -12.0, "sp_yaw": 8.0, "coat_flare": 9.0, "hair_lag": -3.0},
                 H((-0.14, 0.28, 1.48), (0.9, 0.2, 0.0), (0.0, 0.1, 1.0), dict(POINT, wn=0.1, wf=0.15)), None))
    keys.append((0.74, {"pel_z": -0.065, "sp_pitch": -3.0, "head_pitch": -10.0, "sp_yaw": 8.0, "coat_flare": 7.0},
                 H((-0.15, 0.28, 1.45), (0.9, 0.2, 0.0), (0.0, 0.1, 1.0), dict(POINT, wn=0.1, wf=0.15)), None))
    keys.append((1.1,) + END)
    return keys
SPELLS["Cast_Tornado"] = (1.1, tornado_keys())

# ------------------------------------------------------------------ Wind Burst (compress, spread)
SPELLS["Cast_WindBurst"] = (0.6, [
    (0.00, {}, None, None),
    (0.10, {"pel_z": -0.12, "sp_pitch": 13.0, "head_pitch": 6.0, "R_clav": 5.0, "L_clav": 5.0, "coat_flare": 2.0, "sp_yaw": 8.0},
     H((-0.12, 0.26, 0.96), None, None, FIST),
     H((0.10, 0.24, 0.98), None, None, FIST)),
    (0.28, {"pel_z": -0.07, "sp_pitch": -8.0, "head_pitch": -8.0, "coat_flare": 14.0, "hair_lag": -6.0, "sp_yaw": 6.0},
     H((-0.40, 0.30, 1.12), (-1.0, 0.4, 0.0), None, dict(OPEN, wn=0.1)),
     H((0.40, 0.26, 1.12), (1.0, 0.4, 0.0), None, dict(OPEN, wn=0.1))),
    (0.38, {"pel_z": -0.075, "sp_pitch": -6.0, "head_pitch": -6.0, "coat_flare": 10.0, "hair_lag": -3.0, "sp_yaw": 6.0},
     H((-0.40, 0.29, 1.08), (-1.0, 0.4, 0.0), None, dict(curl=10.0, fan=14.0, thumb=12.0, wn=0.1)),
     H((0.40, 0.25, 1.08), (1.0, 0.4, 0.0), None, dict(curl=10.0, fan=14.0, thumb=12.0, wn=0.1))),
    (0.6,) + END,
])

# ------------------------------------------------------------------ Earth Stone Cannon (shared: hand forward, compress, thrust)
SCS_BODY = {"pel_z": -0.10, "sp_pitch": 9.0, "sp_yaw": 11.0, "pel_yaw": -16.0, "coat_flare": 5.0}
SCS_R = H((-0.15, 0.37, 1.02), (0.2, 0.95, 0.0), (0.1, 0.1, 1.0), CLAW)
SPELLS["Cast_StoneCannon"] = (0.367, [
    (0.00, {}, None, None),
    (0.18, {"pel_z": -0.095, "sp_pitch": 8.0, "sp_yaw": 10.0, "pel_yaw": -15.0},
     H((-0.16, 0.36, 1.02), (0.2, 0.95, 0.0), (0.1, 0.1, 1.0), dict(curl=24.0, fan=14.0, thumb=14.0)), None),
    (0.30, SCS_BODY, SCS_R, None),
    (0.367, SCS_BODY, SCS_R, None),
])
SPELLS["Cast_StoneCannon_Release"] = (0.5, [
    (0.00, SCS_BODY, SCS_R, None),
    (0.10, {"pel_z": -0.10, "pel_yaw": -9.0, "sp_yaw": 3.0, "sp_pitch": 12.0, "coat_trail": 4.0, "hair_lag": 3.0},
     H((-0.12, 0.50, 1.06), (0.1, 1.0, 0.1), (0.0, -0.1, 1.0), OPEN), None),
    (0.19, {"pel_z": -0.10, "pel_yaw": -11.0, "sp_yaw": 5.0, "sp_pitch": 6.0, "head_pitch": -3.0, "hair_lag": -1.0},
     H((-0.14, 0.44, 1.13), (0.2, 0.9, 0.4), (0.0, -0.4, 1.0), dict(curl=10.0, fan=12.0, thumb=12.0)), None),
    (0.5,) + END,
])

# ------------------------------------------------------------------ signature Stone Cannon (raise, compress with a braced forearm, thrust, RECOIL)
SC_BODY = {"pel_z": -0.12, "sp_pitch": 9.0, "sp_yaw": 14.0, "pel_yaw": -17.0, "head_pitch": 2.0, "coat_flare": 6.0}
SC_R = H((-0.23, 0.35, 1.20), (0.3, 0.9, 0.2), (0.0, -0.2, 1.0), dict(curl=64.0, fan=-4.0, thumb=46.0, raw={"roll": -12.0}), fix={"roll": -12.0})
SC_L = H((-0.15, 0.24, 1.06), (-0.7, 0.0, 0.7), None, dict(curl=58.0, fan=0.0, thumb=40.0))
SPELLS["StoneCannon_Charge"] = (0.367, [
    (0.00, {}, None, None),
    (0.14, {"pel_z": -0.10, "sp_pitch": 6.0, "sp_yaw": 12.0, "pel_yaw": -16.0, "coat_flare": 5.0},
     H((-0.24, 0.30, 1.10), (0.5, 0.4, -0.4), None, dict(curl=10.0, fan=18.0, thumb=12.0)),
     H((-0.02, 0.24, 1.02), (-0.7, 0.0, 0.7), None, dict(curl=24.0, thumb=16.0))),
    (0.30, SC_BODY, SC_R, SC_L),
    (0.367, SC_BODY, SC_R, SC_L),
])
SPELLS["StoneCannon_Release"] = (0.7, [
    (0.00, SC_BODY, SC_R, SC_L),
    (0.10, {"pel_z": -0.12, "pel_y": -0.015, "pel_yaw": -8.0, "sp_yaw": 2.0, "sp_pitch": 13.0, "head_pitch": 1.0, "coat_trail": 6.0, "hair_lag": 4.0},
     H((-0.13, 0.52, 1.22), (0.1, 1.0, 0.1), (0.0, -0.1, 1.0), dict(OPEN, raw={"roll": 0.0})),
     None),
    (0.14, {"pel_z": -0.12, "pel_y": -0.016, "pel_yaw": -8.0, "sp_yaw": 2.0, "sp_pitch": 13.0, "coat_trail": 6.0, "hair_lag": 4.0},
     H((-0.13, 0.52, 1.23), (0.1, 1.0, 0.15), (0.0, -0.15, 1.0), OPEN), None),
    (0.29, {"pel_z": -0.11, "pel_y": 0.035, "pel_yaw": -14.0, "sp_yaw": 8.0, "sp_pitch": -6.0, "head_pitch": -8.0, "coat_trail": -4.0, "hair_lag": -4.0},
     H((-0.20, 0.34, 1.36), (0.2, 0.7, 0.6), (0.0, -0.5, 1.0), dict(curl=18.0, fan=14.0, thumb=14.0)),
     H((0.06, 0.20, 1.02), None, None, dict(curl=30.0, thumb=16.0))),
    (0.45, {"pel_z": -0.10, "pel_y": 0.01, "pel_yaw": -15.0, "sp_yaw": 9.0, "sp_pitch": 4.0, "head_pitch": -3.0},
     H((-0.24, 0.32, 1.10), None, None, dict(curl=18.0, fan=4.0, thumb=12.0)), None),
    (0.7,) + END,
])

# ------------------------------------------------------------------ Quagmire (left palm lowered, pressing down, fingers spread)
QG_BODY = {"pel_z": -0.26, "sp_pitch": 34.0, "head_pitch": -12.0, "coat_flare": 9.0, "sp_yaw": 4.0, "pel_yaw": -12.0}
QG_L = H((0.18, 0.40, 0.52), (0.0, 0.1, -1.0), (0.0, 1.0, 0.05), dict(curl=2.0, fan=22.0, thumb=10.0))
QG_R = H((-0.36, 0.12, 0.92), (0.0, 0.0, -1.0), None, dict(curl=26.0, fan=6.0, thumb=14.0, wn=0.1))
SPELLS["Quagmire"] = (0.467, [
    (0.00, {}, None, None),
    (0.20, {"pel_z": -0.18, "sp_pitch": 22.0, "head_pitch": -8.0, "coat_flare": 6.0, "sp_yaw": 6.0, "pel_yaw": -13.0},
     H((-0.33, 0.16, 0.94), (0.0, 0.0, -1.0), None, dict(curl=24.0, fan=4.0, thumb=14.0, wn=0.1)),
     H((0.19, 0.38, 0.70), (0.0, 0.1, -1.0), (0.0, 1.0, 0.0), dict(curl=10.0, fan=14.0, thumb=12.0))),
    (0.40, QG_BODY, QG_R, QG_L),
    (0.467, QG_BODY, QG_R, QG_L),
])
SPELLS["Quagmire_Release"] = (0.6, [
    (0.00, QG_BODY, QG_R, QG_L),
    (0.07, {"pel_z": -0.28, "sp_pitch": 36.0, "head_pitch": -12.0, "coat_flare": 11.0, "sp_yaw": 4.0, "pel_yaw": -12.0, "hair_lag": 4.0},
     None, H((0.18, 0.41, 0.46), (0.0, 0.2, -1.0), (0.0, 1.0, 0.1), dict(curl=-2.0, fan=26.0, thumb=8.0))),
    (0.20, {"pel_z": -0.27, "sp_pitch": 35.0, "head_pitch": -10.0, "coat_flare": 9.0, "sp_yaw": 4.0, "pel_yaw": -12.0},
     None, H((0.18, 0.41, 0.47), (0.0, 0.2, -1.0), (0.0, 1.0, 0.1), dict(curl=4.0, fan=20.0, thumb=10.0))),
    (0.6,) + END,
])


# ------------------------------------------------------------------ Elemental Barrage (4.6 s: formations, point, paired directing gestures, converge, push)
def barrage_keys():
    R_READY = H((-0.16, 0.40, 1.14), (0.9, 0.0, -0.3), (0.05, 1.0, 0.05), dict(POINT, wn=0.1, wf=0.15))
    L_READY = H((0.14, 0.28, 1.04), (-0.3, 0.8, -0.5), None, dict(curl=22.0, fan=8.0, thumb=16.0, ci=0.0, cm=0.0, wn=0.1))
    BODY = {"pel_z": -0.10, "pel_yaw": -14.0, "sp_yaw": 8.0, "sp_pitch": 8.0, "head_pitch": -2.0, "coat_flare": 6.0}
    def body(**kw):
        b = dict(BODY); b.update(kw); return b
    keys = [
        (0.00, {}, None, None),
        (0.26, {"pel_z": -0.08, "sp_pitch": -3.0, "head_pitch": -6.0, "coat_flare": 6.0, "sp_yaw": 6.0, "hair_lag": -2.0},
         H((-0.40, 0.02, 1.30), (-0.4, 0.2, 0.9), (-0.9, 0.1, 0.4), dict(OPEN, wn=0.1, wf=0.1)),
         H((0.40, 0.02, 1.30), (0.4, 0.2, 0.9), (0.9, 0.1, 0.4), dict(OPEN, wn=0.1, wf=0.1))),
        (0.44, {"pel_z": -0.12, "sp_pitch": 5.0, "head_pitch": 2.0, "coat_flare": 8.0, "sp_yaw": 6.0, "hair_lag": 2.0},
         H((-0.42, 0.04, 0.84), (-0.3, 0.1, -0.95), (-0.9, 0.2, -0.2), OPEN),
         H((0.42, 0.04, 0.84), (0.3, 0.1, -0.95), (0.9, 0.2, -0.2), OPEN)),
    ]
    # shots every 0.2 s from 0.6 (15): Earth(L low) Water(R up) Wind(R low) Fire(L up) ...
    R_UP_COCK = H((-0.20, 0.36, 1.24), None, None, dict(POINT))
    R_LOW_COCK = H((-0.22, 0.38, 1.04), None, None, dict(POINT))
    R_ACC = H((-0.14, 0.45, 1.18), None, None, dict(POINT))
    R_ACC_LOW = H((-0.14, 0.45, 1.10), None, None, dict(POINT))
    L_UP_COCK = H((0.20, 0.30, 1.18), None, None, dict(curl=24.0, fan=4.0, thumb=16.0))
    L_LOW_COCK = H((0.22, 0.30, 1.00), None, None, dict(curl=24.0, fan=4.0, thumb=16.0))
    L_ACC = H((0.10, 0.40, 1.10), None, None, dict(OPEN))
    L_ACC_LOW = H((0.10, 0.40, 1.04), None, None, dict(OPEN))
    # t = 0.60: point with the right hand while the left sends the first (earth) shot
    keys.append((0.60, body(sp_yaw=10.0), R_READY, H((0.18, 0.34, 1.00), None, None, dict(OPEN))))
    keys.append((0.70, body(sp_yaw=6.0), R_UP_COCK, L_READY))
    seq = []
    for g in range(7):
        side = "R" if g % 2 == 0 else "L"
        t1 = 0.80 + 0.4 * g
        seq.append((side, t1))
    for side, t1 in seq:
        up, low, acc, acc_low = ((R_UP_COCK, R_LOW_COCK, R_ACC, R_ACC_LOW) if side == "R" else (L_UP_COCK, L_LOW_COCK, L_ACC, L_ACC_LOW))
        other_ready = L_READY if side == "R" else R_READY
        yaw = -2.0 if side == "R" else 12.0
        # accent 1: upper formation (water for R, fire for L); accent 2: lower (wind for R, earth for L)
        keys.append((round(t1, 3), body(sp_yaw=yaw, head_pitch=-4.0, pel_z=-0.10), acc if side == "R" else other_ready, other_ready if side == "R" else acc))
        keys.append((round(t1 + 0.1, 3), body(sp_yaw=yaw + 2.0, pel_z=-0.11), low if side == "R" else other_ready, other_ready if side == "R" else low))
        keys.append((round(t1 + 0.2, 3), body(sp_yaw=yaw, head_pitch=-1.0, pel_z=-0.115), acc_low if side == "R" else other_ready, other_ready if side == "R" else acc_low))
        nxt_up = L_UP_COCK if side == "R" else R_UP_COCK
        if t1 + 0.4 < 3.45:
            keys.append((round(t1 + 0.3, 3), body(sp_yaw=(yaw + 10.0) / 2.0, pel_z=-0.105),
                         R_READY if side == "R" else nxt_up, nxt_up if side == "R" else L_READY))
    # t1 of the last group = 3.2 (R): accents at 3.2 and 3.4
    keys.append((3.55, body(sp_yaw=6.0, pel_z=-0.11), R_READY, L_READY))
    keys.append((3.70, {"pel_z": -0.12, "pel_yaw": -13.0, "sp_yaw": 6.0, "sp_pitch": 6.0, "head_pitch": 0.0, "coat_flare": 7.0},
                 H((-0.34, 0.24, 1.08), (1.0, 0.2, 0.0), (-0.2, 0.9, 0.3), dict(OPEN, wn=0.1, wf=0.1)),
                 H((0.34, 0.24, 1.08), (-1.0, 0.2, 0.0), (0.2, 0.9, 0.3), dict(OPEN, wn=0.1, wf=0.1))))
    keys.append((3.87, {"pel_z": -0.15, "pel_yaw": -12.0, "sp_yaw": 4.0, "sp_pitch": 12.0, "head_pitch": 4.0, "coat_flare": 8.0},
                 H((-0.07, 0.34, 1.06), (0.7, 0.7, 0.0), (0.0, 0.0, 1.0), dict(curl=36.0, fan=4.0, thumb=24.0)),
                 H((0.07, 0.34, 1.06), (-0.7, 0.7, 0.0), (0.0, 0.0, 1.0), dict(curl=36.0, fan=4.0, thumb=24.0))))
    keys.append((3.95, {"pel_z": -0.14, "pel_y": -0.03, "pel_yaw": -8.0, "sp_yaw": 2.0, "sp_pitch": 14.0, "head_pitch": 2.0, "coat_flare": 9.0,
                        "coat_trail": 8.0, "hair_lag": 5.0},
                 H((-0.09, 0.50, 1.10), (0.0, 1.0, 0.1), (0.0, -0.1, 1.0), OPEN),
                 H((0.09, 0.50, 1.10), (0.0, 1.0, 0.1), (0.0, -0.1, 1.0), OPEN)))
    keys.append((4.03, {"pel_z": -0.14, "pel_y": -0.032, "pel_yaw": -8.0, "sp_yaw": 2.0, "sp_pitch": 14.0, "head_pitch": 2.0, "coat_flare": 8.0,
                        "coat_trail": 7.0, "hair_lag": 5.0},
                 H((-0.09, 0.51, 1.11), (0.0, 1.0, 0.15), (0.0, -0.15, 1.0), OPEN),
                 H((0.09, 0.51, 1.11), (0.0, 1.0, 0.15), (0.0, -0.15, 1.0), OPEN)))
    keys.append((4.18, {"pel_z": -0.12, "pel_y": 0.03, "pel_yaw": -12.0, "sp_yaw": 6.0, "sp_pitch": -6.0, "head_pitch": -8.0, "coat_flare": 6.0,
                        "coat_trail": -4.0, "hair_lag": -4.0},
                 H((-0.14, 0.36, 1.30), (0.2, 0.7, 0.6), (0.0, -0.5, 1.0), dict(curl=16.0, fan=12.0, thumb=12.0)),
                 H((0.14, 0.36, 1.30), (-0.2, 0.7, 0.6), (0.0, -0.5, 1.0), dict(curl=16.0, fan=12.0, thumb=12.0))))
    keys.append((4.36, {"pel_z": -0.10, "pel_y": 0.01, "pel_yaw": -14.0, "sp_yaw": 8.0, "sp_pitch": 4.0, "head_pitch": -3.0},
                 H((-0.22, 0.32, 1.02), None, None, dict(curl=16.0, fan=4.0, thumb=12.0)),
                 H((0.18, 0.26, 0.98), None, None, dict(curl=26.0, fan=2.0, thumb=12.0))))
    keys.append((4.6,) + END)
    return keys
SPELLS["Barrage"] = (4.6, barrage_keys())

RELEASE_OF = {"Cast_Fireball_Release": "Cast_Fireball", "Cast_StoneCannon_Release": "Cast_StoneCannon",
              "StoneCannon_Release": "StoneCannon_Charge", "Quagmire_Release": "Quagmire"}
