"""Arm IK design aid for the casting clips: solves the composer's semantic arm parameters (lower, fwd, swing, elbow, roll,
wpitch, wyaw) for a wrist target, a palm normal and a finger direction, given the rest of a pose.

Multi-start Levenberg-Marquardt (no scipy here) with box limits on anatomical ranges and a soft pull away from their last
12 degrees: position first, then the hand's orientation; among the starts, the one closest to the previous key wins unless
another reaches the target clearly better (continuity: interpolating the stored parameters then never flips branch).
Numerical Jacobians over arm-chain-only forward kinematics. Coordinates: x = character's left, f = forward (-Y),
z = up, metres, ground at 0. Only the design scripts use this: the clips store the solved parameters."""
import math
import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import numpy as np
from mathutils import Vector, Matrix
import rudeus_anim_lib as L
from rudeus_anim_lib import rot
from pose_compose import compose, DEFAULTS, CLAV_FOLLOW, CLAV_LIFT

NAMES = ("lower", "fwd", "swing", "elbow", "roll", "wpitch", "wyaw")
LIMITS = {"lower": (-85.0, 100.0), "fwd": (-70.0, 135.0), "swing": (-70.0, 110.0), "elbow": (2.0, 145.0),
          "roll": (-85.0, 95.0), "wpitch": (-70.0, 70.0), "wyaw": (-30.0, 22.0)}
SEEDS = [dict(lower=10, fwd=80, elbow=30), dict(lower=70, fwd=20, elbow=60), dict(lower=-60, fwd=20, elbow=20),
         dict(lower=20, fwd=110, elbow=70), dict(lower=10, fwd=0, elbow=20), dict(lower=60, fwd=-30, elbow=40),
         dict(lower=40, fwd=50, elbow=90), dict(lower=-20, fwd=60, elbow=60)]


class ArmIK:
    def __init__(self, rig):
        self.rig = rig

    def base(self, P):
        Q = dict(DEFAULTS); Q.update(P)
        D, pel = compose(self.rig, Q)
        return Q, D, pel

    def _chain_pos(self, D, pel, names):
        """World head positions along a root->bone chain (bones missing from D inherit their parent's delta)."""
        rig = self.rig
        pos = rig.head[names[0]].copy()
        acc = D.get(names[0], Matrix.Identity(3))
        out = {names[0]: pos}
        for c in names[1:]:
            p = rig.parent[c]
            pos = pos + acc @ (rig.head[c] - rig.head[p])
            if c == L.PELVIS:
                pos = pos + Vector(pel)
            acc = D.get(c, acc)
            out[c] = pos
        return out

    def chain(self, s):
        n, names = L.HAND[s], []
        while n:
            names.append(n)
            n = self.rig.parent[n]
        return names[::-1]

    def fk(self, Q, D, pel, s, x):
        v = dict(zip(NAMES, x))
        sign = L.SIDES[s]
        chest = D[L.SPINE[2]]
        D2 = dict(D)
        drop = CLAV_FOLLOW * v["lower"] - CLAV_LIFT * max(0.0, v["fwd"] - 60.0) - Q[s + "_clav"]
        D2[L.CLAV[s]] = chest @ rot((0, 1, 0), sign * drop)
        a = L.arm_D(s, lower=v["lower"], swing=v["swing"], twist=Q[s + "_twist"], elbow=v["elbow"], wrist_pitch=v["wpitch"],
                    wrist_yaw=v["wyaw"], raise_fwd=v["fwd"], spread=Q[s + "_spread"], roll=v["roll"])
        for b, m in a.items():
            D2[b] = chest @ m
        pos = self._chain_pos(D2, pel, self.chain(s))
        w, e = pos[L.HAND[s]], pos[L.FORE[s]]
        Dh = D2[L.HAND[s]]
        n = Dh @ Vector((0, 0, -1))
        f = Dh @ Vector((sign, 0, 0))
        c = lambda p: np.array([p.x, -p.y, p.z])
        return c(w), c(n), c(f), c(e)

    def _lm(self, resid, xf, lo, hi, iters=40):
        lam = 1.0
        r = resid(xf)
        for _ in range(iters):
            J = np.zeros((len(r), len(xf)))
            for j in range(len(xf)):
                d = np.zeros(len(xf)); d[j] = 0.5
                J[:, j] = (resid(xf + d) - r) / 0.5
            A = J.T @ J
            g = J.T @ r
            moved = False
            while lam < 1e8:
                step = -np.linalg.solve(A + lam * np.diag(np.diag(A) + 1e-6), g)
                cand = np.clip(xf + step, lo, hi)
                r2 = resid(cand)
                if r2 @ r2 < r @ r:
                    moved = np.linalg.norm(cand - xf) > 1e-3
                    xf, r = cand, r2
                    lam = max(1e-4, lam * 0.3)
                    break
                lam *= 4.0
            if not moved:
                break
        return xf, r

    def solve(self, P, s, wrist=None, palm=None, fingers=None, w_pos=6.0, w_n=0.25, w_f=0.25, reg=0.002, x0=None,
              fixed=None, iters=40, continuity=1.5):
        Q, D, pel = self.base(P)
        fixed = fixed or {}
        free = [k for k in NAMES if k not in fixed]
        x_all = np.array([Q[s + "_" + k] for k in NAMES], float)
        prev = x_all.copy()
        for k, val in fixed.items():
            x_all[NAMES.index(k)] = val
        idx = [NAMES.index(k) for k in free]
        lo = np.array([LIMITS[NAMES[i]][0] for i in idx]); hi = np.array([LIMITS[NAMES[i]][1] for i in idx])
        tn = None if palm is None else np.array(palm, float) / np.linalg.norm(palm)
        tf = None if fingers is None else np.array(fingers, float) / np.linalg.norm(fingers)

        def make(orient, ref):
            def resid(xf):
                xa = x_all.copy(); xa[idx] = xf
                wp, n, f, _ = self.fk(Q, D, pel, s, xa)
                r = []
                if wrist is not None:
                    r += list((wp - np.array(wrist)) * 100.0 * math.sqrt(w_pos))
                if orient and tn is not None:
                    r += list((n - tn) * 100.0 * math.sqrt(w_n) * 0.3)
                if orient and tf is not None:
                    r += list((f - tf) * 100.0 * math.sqrt(w_f) * 0.3)
                wreg = np.array([4.0 if NAMES[i] in ("roll", "wpitch", "wyaw") else 1.0 for i in idx])
                r += list((xf - ref[idx]) * math.sqrt(reg) * wreg)
                for j, i in enumerate(idx):
                    a, b = LIMITS[NAMES[i]]
                    r.append(max(0.0, (a + 12.0) - xf[j], xf[j] - (b - 12.0)) * 0.4)
                return np.array(r)
            return resid

        seeds = [prev.copy()]
        for sd in SEEDS:
            x = prev.copy()
            for k, v in sd.items():
                x[NAMES.index(k)] = v
            x[NAMES.index("roll")], x[NAMES.index("wpitch")], x[NAMES.index("wyaw")] = 0.0, 0.0, 0.0
            seeds.append(x)
        best = None
        for sd in seeds:
            x0f = np.clip(sd[idx], lo, hi)
            x1, _ = self._lm(make(False, prev), x0f, lo, hi, iters)       # position first
            x2, r2 = self._lm(make(True, prev), x1, lo, hi, iters)        # then the hand's orientation
            cost = float(r2 @ r2) + continuity * float(np.abs(x2 - prev[idx]).sum()) ** 1.0
            if best is None or cost < best[0]:
                best = (cost, x2)
        xa = x_all.copy(); xa[idx] = best[1]
        wp, n, f, e = self.fk(Q, D, pel, s, xa)
        out = {s + "_" + k: round(float(v), 1) for k, v in zip(NAMES, xa)}
        return out, dict(wrist=np.round(wp, 3), palm=np.round(n, 2), fingers=np.round(f, 2), elbow=np.round(e, 3),
                         err_cm=round(float(np.linalg.norm(wp - np.array(wrist))) * 100, 1) if wrist is not None else None)

    def where(self, P, s):
        Q, D, pel = self.base(P)
        x = np.array([Q[s + "_" + k] for k in NAMES], float)
        wp, n, f, e = self.fk(Q, D, pel, s, x)
        return dict(wrist=np.round(wp, 3), palm=np.round(n, 2), fingers=np.round(f, 2), elbow=np.round(e, 3))

    def landmarks(self, P):
        Q, D, pel = self.base(P)
        full = {}
        for n in self.rig.order:
            p = self.rig.parent[n]
            full[n] = D.get(n, full[p] if p else Matrix.Identity(3))
        c = lambda p: tuple(round(v, 3) for v in (p.x, -p.y, p.z))
        return {k: c(self.rig.world_head(b, full, pel)) for k, b in
                (("pelvis", L.PELVIS), ("chest", L.SPINE[2]), ("neck", L.NECK), ("head", L.HEAD),
                 ("shoulder_L", L.UPPER["L"]), ("shoulder_R", L.UPPER["R"]))}
