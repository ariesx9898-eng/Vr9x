"""Procedural trees for the LA PLACE nature kit.

Branch skeletons are grown recursively (Weber-Penn style: per-level child counts, branching angles,
phyllotaxis, tropism and noise wander) inside an artist-controlled crown envelope, so every
species has a deliberate silhouette. Branches become tapered tubes with parallel-transport frames
(no twisting), trunks get a flared, lobed root collar sunk below the ground, and foliage is built
from alpha cards grouped in clumps (broadleaf), along boughs (conifers) or as arched fronds (palms).
Leaf-card normals are blended toward the crown's ellipsoid normal for soft painterly shading.
"""
import math
import random

from mathutils import Vector, Matrix

import nature_lib as L
from nature_lib import Geo, tube, dir_from_angles, fbm, nz, nvec, smoothstep

UP = Vector((0.0, 0.0, 1.0))


# --------------------------------------------------------------------------------------------
# Skeleton
# --------------------------------------------------------------------------------------------
class Branch:
    def __init__(self, pts, radii, level, parent=None, t_parent=0.0):
        self.pts = pts
        self.radii = radii
        self.level = level
        self.parent = parent
        self.t_parent = t_parent
        self.children = []

    @property
    def length(self):
        return sum((self.pts[i + 1] - self.pts[i]).length for i in range(len(self.pts) - 1))

    def point_at(self, t):
        """Point, tangent and radius at normalised arc position t."""
        lens = [0.0]
        for i in range(len(self.pts) - 1):
            lens.append(lens[-1] + (self.pts[i + 1] - self.pts[i]).length)
        target = t * lens[-1]
        for i in range(len(self.pts) - 1):
            if lens[i + 1] >= target or i == len(self.pts) - 2:
                seg = lens[i + 1] - lens[i]
                f = 0.0 if seg <= 0 else (target - lens[i]) / seg
                f = min(1.0, max(0.0, f))
                p = self.pts[i].lerp(self.pts[i + 1], f)
                d = (self.pts[i + 1] - self.pts[i]).normalized()
                r = self.radii[i] + (self.radii[i + 1] - self.radii[i]) * f
                return p, d, r
        return self.pts[-1], (self.pts[-1] - self.pts[-2]).normalized(), self.radii[-1]

    def tip_dir(self):
        return (self.pts[-1] - self.pts[-2]).normalized()


class Envelope:
    """Ellipsoidal crown envelope with a noisy radius (lumpy silhouette)."""

    def __init__(self, centre, radii, lump=0.12, lump_scale=1.6, seed_off=(0, 0, 0), flat_top=0.0,
                 flat_bottom=0.0):
        self.c = Vector(centre)
        self.r = Vector(radii)
        self.lump = lump
        self.lump_scale = lump_scale
        self.off = seed_off
        self.flat_top = flat_top
        self.flat_bottom = flat_bottom

    def scale_at(self, d):
        return 1.0 + self.lump * fbm(d, self.off, self.lump_scale, 3)

    def norm_dist(self, p):
        q = p - self.c
        e = Vector((q.x / self.r.x, q.y / self.r.y, q.z / self.r.z))
        if self.flat_top and e.z > 0:
            e.z *= 1.0 + self.flat_top * e.z * 2
        if self.flat_bottom and e.z < 0:
            e.z *= 1.0 + self.flat_bottom * (-e.z) * 2
        L0 = e.length
        if L0 < 1e-9:
            return 0.0
        return L0 / self.scale_at(e / L0)

    def dist_along(self, p, d, max_t=200.0):
        """Distance from p along d to where the ray leaves the envelope (0 if it never enters)."""
        step = max(0.1, min(self.r) * 0.04)
        t = 0.0
        if self.norm_dist(p) >= 1.0:
            while t < max_t:
                t += step
                if self.norm_dist(p + d * t) < 1.0:
                    break
            else:
                return 0.0
        lo, hi = t, t + step
        while self.norm_dist(p + d * hi) < 1.0 and hi < max_t:
            lo, hi = hi, hi + step * 4
        for _ in range(24):
            mid = 0.5 * (lo + hi)
            if self.norm_dist(p + d * mid) < 1.0:
                lo = mid
            else:
                hi = mid
        return lo

    def normal(self, p):
        q = p - self.c
        g = Vector((q.x / (self.r.x ** 2), q.y / (self.r.y ** 2), q.z / (self.r.z ** 2)))
        if g.length < 1e-9:
            return UP.copy()
        return g.normalized()


def grow(start, d0, length, seg_len, rng, tropism=0.0, wander=0.0, wander_scale=0.35, curl=0.0,
         outward=0.0, env=None, env_pull=0.0, noise_off=(0, 0, 0), min_segs=2, gravity_bend=0.0,
         axis_origin=None):
    """March a polyline. tropism > 0 bends toward +Z, < 0 droops. outward pushes away from the
    vertical axis through axis_origin. curl twists the heading around the growth axis (spiral).
    env_pull bends tips back inside the envelope when they approach its surface."""
    n = max(min_segs, int(math.ceil(length / seg_len)))
    step = length / n
    pts = [start.copy()]
    d = d0.normalized()
    side = L.perp(d)
    for i in range(n):
        t = (i + 0.5) / n
        p = pts[-1]
        nd = d.copy()
        if tropism:
            nd += UP * (tropism * step)
        if gravity_bend:
            # bend down progressively (weight of the bough), stronger toward the tip
            nd += -UP * (gravity_bend * step * (0.3 + t))
        if wander:
            w = nvec(p, noise_off, wander_scale)
            w = w - d * w.dot(d)
            nd += w * (wander * step)
        if outward and axis_origin is not None:
            o = Vector((p.x - axis_origin.x, p.y - axis_origin.y, 0.0))
            if o.length > 1e-6:
                nd += o.normalized() * (outward * step)
        if curl:
            side = L.perp(nd) if side.length < 1e-6 else (side - nd.normalized() * side.dot(nd.normalized()))
            side = side.normalized()
            nd += side.cross(nd.normalized()) * (curl * step)
            side = Matrix.Rotation(curl * step, 3, nd.normalized()) @ side
        if env is not None and env_pull:
            q = p + nd.normalized() * step
            nd_ = env.norm_dist(q)
            if nd_ > 0.9:
                nd += (env.c - q).normalized() * (env_pull * (nd_ - 0.9) * 4 * step)
        d = nd.normalized()
        pts.append(p + d * step)
    return pts


def taper(n, r0, r1, power=1.0):
    return [r0 + (r1 - r0) * ((i / (n - 1)) ** power) for i in range(n)]


def child_dir(parent_dir, azimuth, angle, ref=None):
    """Direction at `angle` from parent_dir, rotated `azimuth` around it."""
    a = L.perp(parent_dir) if ref is None else (ref - parent_dir * ref.dot(parent_dir)).normalized()
    d = Matrix.Rotation(angle, 3, a) @ parent_dir
    d = Matrix.Rotation(azimuth, 3, parent_dir) @ d
    return d.normalized()


# --------------------------------------------------------------------------------------------
# Meshing
# --------------------------------------------------------------------------------------------
def root_collar(n_lobes, amp, sharp, height, rng, depth_amp=0.0):
    """Returns ring_fn modulating trunk radius with buttress lobes that decay with height."""
    lobes = []
    base = rng.uniform(0, 2 * math.pi)
    for i in range(n_lobes):
        th = base + 2 * math.pi * i / n_lobes + rng.uniform(-0.35, 0.35) * 2 * math.pi / n_lobes
        lobes.append((th, rng.uniform(0.65, 1.25)))

    def fn(i, theta, p, r):
        h = max(0.0, p.z)
        fall = math.exp(-h / height) if p.z > -0.05 else 1.0
        s = 0.0
        for th, w in lobes:
            c = math.cos(theta - th)
            if c > 0:
                s += w * c ** sharp
        return r * (1.0 + amp * fall * s)

    return fn


def mesh_branch(geo, br, mat, sides, ring_fn=None, tip="point", u_repeat=None, twist=0.0):
    return tube(geo, br.pts, br.radii, mat, sides, ring_fn=ring_fn, tip=tip, u_repeat=u_repeat, twist=twist,
                tip_len=max(br.radii[-1] * 3.0, 0.03))


# --------------------------------------------------------------------------------------------
# Foliage
# --------------------------------------------------------------------------------------------
def make_normal_fn(env, clump_c=None, clump_r=1.0, w_env=0.55, w_clump=0.25, w_card=0.2):
    """Blend crown-ellipsoid normal, clump sphere normal and the card's own normal."""

    def fn(p, card_n):
        n = env.normal(p) * w_env + card_n * w_card
        if clump_c is not None:
            q = p - clump_c
            if q.length > 1e-6:
                n += q.normalized() * w_clump
        if n.length < 1e-6:
            n = card_n
        return n.normalized()

    return fn


def quad(geo, centre, nrm, upv, w, h, mat, normal_fn=None, bend=0.0, base_anchor=False):
    """Card centred at `centre` (or anchored at its bottom edge) facing nrm, texture up = upv."""
    nrm = nrm.normalized()
    upv = (upv - nrm * upv.dot(nrm))
    if upv.length < 1e-6:
        upv = L.perp(nrm)
    upv = upv.normalized()
    side = upv.cross(nrm).normalized()      # side x up = nrm  (front face = nrm)
    base = centre - upv * (0.0 if base_anchor else h * 0.5)
    return L.card(geo, base, upv, side, w, h, mat, segs=2 if bend else 1, bend=bend, normal_fn=normal_fn)


def leaf_clump(geo, c, R, n, size, mat, rng, env, up_bias=0.35, tilt=0.6, droop=0.0, flat=1.0,
               normal_w=(0.55, 0.25, 0.2), size_jit=0.2, out_dir=None, face_up=0.0, tuft=False):
    """A puffy clump of n cards around centre c (radius R). Cards face outward from c.
    tuft=True: cards are anchored at c by their bottom edge and fan outward (grass-type textures)."""
    nf = make_normal_fn(env, c, R, *normal_w)
    if tuft:
        golden = math.pi * (3 - math.sqrt(5))
        off = rng.uniform(0, 2 * math.pi)
        for i in range(n):
            z = 1 - 2 * (i + 0.5) / n
            z = max(-0.2, min(1.0, z * 0.6 + 0.45))
            rr = math.sqrt(max(0.0, 1 - z * z))
            a = off + golden * i
            d = Vector((rr * math.cos(a), rr * math.sin(a), z))
            if out_dir is not None:
                d = (d + out_dir * 0.5).normalized()
            side = d.cross(UP) if abs(d.z) < 0.95 else Vector((1, 0, 0))
            side = Matrix.Rotation(rng.uniform(-0.6, 0.6), 3, d) @ side.normalized()
            nrm = side.cross(d).normalized()
            s = size * rng.uniform(1 - size_jit, 1 + size_jit)
            quad(geo, c - d * (0.05 * s), nrm, d, s * 0.9, s, mat, normal_fn=nf, base_anchor=True)
        return []
    out = []
    golden = math.pi * (3 - math.sqrt(5))
    off = rng.uniform(0, 2 * math.pi)
    for i in range(n):
        z = 1 - 2 * (i + 0.5) / n
        z = max(-1.0, min(1.0, z * (1 - up_bias) + up_bias * 0.6))
        rr = math.sqrt(max(0.0, 1 - z * z))
        a = off + golden * i
        d = Vector((rr * math.cos(a), rr * math.sin(a), z))
        if out_dir is not None:
            d = (d + out_dir * 0.6).normalized()
        d = (d + Vector((rng.gauss(0, 0.25), rng.gauss(0, 0.25), rng.gauss(0, 0.2)))).normalized()
        pos = c + Vector((d.x, d.y, d.z * flat)) * (R * rng.uniform(0.35, 0.8))
        # card normal: mostly outward, tilted randomly so edge-on silhouettes still read
        tn = (d + Vector((rng.gauss(0, tilt), rng.gauss(0, tilt), rng.gauss(0, tilt)))).normalized()
        if tn.dot(d) < 0.2:
            tn = (tn + d).normalized()
        if face_up:
            tn = (tn + UP * face_up).normalized()
        upv = (UP * 0.8 + d * 0.6 + Vector((rng.gauss(0, 0.4), rng.gauss(0, 0.4), 0))) - UP * droop
        if face_up:
            upv = d + Vector((rng.gauss(0, 0.3), rng.gauss(0, 0.3), 0))
        s = size * rng.uniform(1 - size_jit, 1 + size_jit)
        quad(geo, pos, tn, upv, s, s, mat, normal_fn=nf)
        out.append(pos)
    return out


# --------------------------------------------------------------------------------------------
# Trunk helpers
# --------------------------------------------------------------------------------------------
def trunk_polyline(rng, height, lean=0.03, wiggle=0.02, below=0.5, seg=0.8, bow=0.0, lean_az=None,
                   noise_off=(0, 0, 0), s_curve=0.0, helix_r=0.0, helix_pitch=6.0, base=(0.0, 0.0)):
    """Mostly vertical trunk from z=-below to z=height with lean, bow, S-curve, helix and wiggle."""
    az = rng.uniform(0, 2 * math.pi) if lean_az is None else lean_az
    ld = Vector((math.cos(az), math.sin(az), 0.0))
    side = Vector((-ld.y, ld.x, 0.0))
    n = max(4, int(math.ceil((height + below) / seg)))
    pts = []
    for i in range(n + 1):
        t = i / n
        z = -below + (height + below) * t
        h = max(0.0, z)
        u = min(1.0, h / height)
        x = ld * (lean * h + bow * height * math.sin(math.pi * u) * 0.5)
        x += side * (s_curve * height * math.sin(2 * math.pi * u) * 0.5)
        w = Vector((fbm((0, 0, z), noise_off, 0.15, 2), fbm((5, 0, z), noise_off, 0.15, 2), 0.0)) * (wiggle * height)
        w *= smoothstep(0.0, 2.0, h)
        if helix_r:
            a = 2 * math.pi * h / helix_pitch
            x += Vector((math.cos(a) - 1.0, math.sin(a), 0.0)) * helix_r * smoothstep(0.0, 1.5, h)
        pts.append(Vector((base[0], base[1], z)) + x + w)
    return pts


def radius_profile(pts, r_base, r_top, flare=0.35, flare_h=1.0, power=1.0):
    zs = [p.z for p in pts]
    z0, z1 = zs[0], zs[-1]
    out = []
    for p in pts:
        t = (p.z - max(0.0, z0)) / max(1e-6, z1 - max(0.0, z0))
        t = min(1.0, max(0.0, t))
        r = r_base + (r_top - r_base) * (t ** power)
        r *= 1.0 + flare * math.exp(-max(0.0, p.z) / flare_h)
        out.append(r)
    return out


def surface_roots(geo, rng, mat, trunk_r, n, length, r0, rise, sink, sides=7, az0=None):
    """Big roots that leave the trunk above ground, snake outward and dive into the soil."""
    az0 = rng.uniform(0, 2 * math.pi) if az0 is None else az0
    for i in range(n):
        az = az0 + 2 * math.pi * i / n + rng.uniform(-0.3, 0.3) * 2 * math.pi / n
        ln = rng.uniform(*length)
        rr = rng.uniform(*r0)
        d = Vector((math.cos(az), math.sin(az), 0.0))
        s = Vector((-d.y, d.x, 0.0))
        pts, radii = [], []
        k = 8
        for j in range(k + 1):
            t = j / k
            dist = trunk_r * 0.35 + ln * t
            z = rise * (1 - t) ** 1.6 * rng.uniform(0.9, 1.1) - sink * smoothstep(0.55, 1.0, t)
            wob = s * (math.sin(t * 3.1 + i) * 0.12 * ln * t)
            pts.append(d * dist + wob + Vector((0, 0, z)))
            radii.append(rr * (1.0 - 0.75 * t) + 0.05)
        tube(geo, pts, radii, mat, sides, tip="point", tip_len=0.4)


# --------------------------------------------------------------------------------------------
# Broadleaf family (oak / birch / jungle / giant / demon / dead)
# --------------------------------------------------------------------------------------------
def broadleaf(name, P):
    """Generic broadleaf tree driven by a parameter dict P. Returns (Geo, info)."""
    rng = random.Random(L.seed_of(name))
    noff = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geo = Geo()
    H = P["height"]
    bark, leaves = P["bark"], P.get("leaves")
    cb = P["crown_base"] * H
    env = Envelope((P.get("crown_off", (0, 0))[0], P.get("crown_off", (0, 0))[1],
                    (cb + H) * 0.5 + P.get("crown_shift", 0.0)),
                   (P["crown_w"] * 0.5, P.get("crown_d", P["crown_w"]) * 0.5, (H - cb) * 0.5),
                   lump=P.get("lump", 0.12), lump_scale=P.get("lump_scale", 1.6), seed_off=noff,
                   flat_top=P.get("flat_top", 0.0), flat_bottom=P.get("flat_bottom", 0.0))
    masses = []
    if P.get("masses"):
        for mi, (mc, mr) in enumerate(P["masses"](rng, H, cb, P)):
            masses.append(Envelope(mc, mr, lump=P.get("mass_lump", 0.16), lump_scale=1.3,
                                   seed_off=(noff[0] + 17 * mi, noff[1], noff[2]), flat_bottom=P.get("mass_flat", 0.45),
                                   flat_top=P.get("mass_flat_top", 0.0)))
    branches = []

    # ---- trunk(s)
    trunks = []
    stems = P.get("stems") or [dict()]
    for si, st in enumerate(stems):
        th = P["trunk_h"] * H * st.get("h", 1.0)
        pts = trunk_polyline(rng, th, lean=st.get("lean", P.get("lean", 0.03)), wiggle=P.get("wiggle", 0.02),
                             below=P.get("below", 0.6), seg=P.get("trunk_seg", 0.8), bow=st.get("bow", P.get("bow", 0.0)),
                             lean_az=st.get("az"), noise_off=(noff[0] + si * 7, noff[1], noff[2]),
                             s_curve=st.get("s_curve", P.get("s_curve", 0.0)), helix_r=P.get("helix_r", 0.0),
                             helix_pitch=P.get("helix_pitch", 6.0), base=st.get("off", (0.0, 0.0)))
        r0 = P["trunk_r"] * st.get("r", 1.0)
        radii = radius_profile(pts, r0, r0 * P.get("trunk_taper", 0.45), flare=P.get("flare", 0.35),
                               flare_h=P.get("flare_h", 0.8), power=P.get("trunk_power", 1.0))
        tr = Branch(pts, radii, 0)
        tr.broken = False
        tr.env = env
        trunks.append(tr)
        branches.append(tr)

    lvl = P["levels"]

    def spawn_masses(parent, cfg):
        """One limb per canopy mass, aimed at the mass centre from a matching trunk height."""
        kids = []
        order = sorted(range(len(masses)), key=lambda i: masses[i].c.z)
        t0, t1 = cfg["t"]
        for rank, mi in enumerate(order):
            m = masses[mi]
            t = t0 + (t1 - t0) * (rank + rng.uniform(0.2, 0.8)) / len(order)
            p, d, pr = parent.point_at(t)
            to = m.c - p
            cd = (to.normalized() + Vector((rng.gauss(0, 0.08), rng.gauss(0, 0.08), 0.1))).normalized()
            ln = to.length * rng.uniform(0.8, 0.95)
            r0 = max(cfg.get("min_r", 0.02), min(pr * cfg["r_rel"], cfg.get("max_r", 9.0)))
            pts = grow(p, cd, ln, cfg["seg"], rng, tropism=cfg.get("tropism", 0.0), wander=cfg.get("wander", 0.0),
                       wander_scale=cfg.get("wander_scale", 0.35), noise_off=(noff[0] + mi * 5, noff[1], noff[2]))
            br = Branch(pts, taper(len(pts), r0, r0 * cfg.get("tip_r", 0.25), cfg.get("taper_pow", 1.0)), 1,
                        parent, t)
            br.broken = False
            br.env = m
            parent.children.append(br)
            kids.append(br)
            branches.append(br)
        return kids

    def spawn(parent, lv):
        cfg = lvl[lv - 1]
        if lv == 1 and masses:
            return spawn_masses(parent, cfg)
        env = parent.env
        plen = parent.length
        count = cfg["n"] if isinstance(cfg["n"], int) else max(1, int(round(cfg["n"] * plen)))
        t0, t1 = cfg["t"]
        kids = []
        az0 = rng.uniform(0, 2 * math.pi)
        for k in range(count):
            t = t0 + (t1 - t0) * ((k + rng.uniform(0.2, 0.8)) / count) if count > 1 else (t0 + t1) / 2
            p, d, pr = parent.point_at(t)
            az = az0 + k * cfg.get("phyllo", 2.39996) + rng.uniform(-0.35, 0.35)
            ang = math.radians(rng.uniform(*cfg["angle"]))
            if parent.level == 0 and cfg.get("absolute_elev"):
                el = math.radians(rng.uniform(*cfg["absolute_elev"]))
                if cfg.get("outward_from_stem") and (p.x or p.y):
                    base_az = math.atan2(p.y, p.x)
                    az = base_az + rng.uniform(-1.3, 1.3)
                cd = dir_from_angles(az, el)
            else:
                cd = child_dir(d, az, ang)
                o = Vector((p.x - env.c.x, p.y - env.c.y, 0.0))
                if o.length > 0.2 and cfg.get("outward_bias", 0.0):
                    cd = (cd + o.normalized() * cfg["outward_bias"]).normalized()
                if cfg.get("up_bias"):
                    cd = (cd + UP * cfg["up_bias"]).normalized()
            if cfg.get("to_env", True):
                ln = env.dist_along(p, cd) * rng.uniform(*cfg["len_env"])
            else:
                ln = plen * rng.uniform(*cfg["len_rel"])
            ln = min(ln, cfg.get("max_len", 99.0))
            broken = rng.random() < cfg.get("break_p", 0.0)
            if broken:
                ln *= rng.uniform(0.3, 0.6)
            if ln < cfg.get("min_len", 0.3):
                continue
            r0 = min(pr * cfg["r_rel"], cfg.get("max_r", 9.0))
            r0 = max(r0, cfg.get("min_r", 0.02))
            pts = grow(p, cd, ln, cfg["seg"], rng, tropism=cfg.get("tropism", 0.0), wander=cfg.get("wander", 0.0),
                       wander_scale=cfg.get("wander_scale", 0.35), curl=cfg.get("curl", 0.0),
                       outward=cfg.get("outward", 0.0), env=env, env_pull=cfg.get("env_pull", 0.0),
                       noise_off=(noff[0] + lv * 13 + k, noff[1] + k * 3, noff[2]), axis_origin=env.c,
                       gravity_bend=cfg.get("gravity", 0.0))
            tip_r = max(r0 * (cfg.get("tip_r", 0.25) if not broken else 0.6), cfg.get("min_tip_r", 0.012))
            radii = taper(len(pts), r0, tip_r, cfg.get("taper_pow", 1.0))
            br = Branch(pts, radii, lv, parent, t)
            br.broken = broken
            br.env = env
            parent.children.append(br)
            kids.append(br)
            branches.append(br)
        return kids

    frontier = list(trunks)
    for lv in range(1, len(lvl) + 1):
        nxt = []
        for b in frontier:
            if not getattr(b, "broken", False):
                nxt.extend(spawn(b, lv))
        frontier = nxt

    # ---- bark
    collar = None
    if P.get("collar"):
        c = P["collar"]
        collar = root_collar(c["lobes"], c["amp"], c["sharp"], c["h"], rng)
    for b in branches:
        sides = P["sides"][min(b.level, len(P["sides"]) - 1)]
        if b.level == 0:
            fn = collar
            if P.get("rope"):
                rp = P["rope"]

                def fn(i, th, p, r, base_fn=collar, rp=rp):
                    rr = base_fn(i, th, p, r) if base_fn else r
                    return rr * (1.0 + rp["amp"] * math.cos(rp["k"] * (th - rp["twist"] * p.z)))
            tip_len = P.get("trunk_tip", None)
            tube(geo, b.pts, b.radii, bark, sides, ring_fn=fn, tip="point",
                 tip_len=tip_len if tip_len is not None else max(b.radii[-1] * 3.0, 0.05))
        else:
            tube(geo, b.pts, b.radii, bark, sides, tip="point",
                 tip_len=(b.radii[-1] * 0.8 if getattr(b, "broken", False) else max(b.radii[-1] * 3.0, 0.03)))
    if P.get("roots"):
        r = P["roots"]
        surface_roots(geo, rng, bark, P["trunk_r"], r["n"], r["len"], r["r"], r["rise"], r["sink"], r.get("sides", 7))

    info = {"branches": len(branches)}
    if leaves:
        info["clumps"] = place_foliage(geo, branches, len(lvl), masses or [env], P["foliage"], leaves, bark, rng)
    return geo, info


def nearest_branch_point(branches, c, min_level=1, max_dist=99.0):
    best, bd = None, max_dist
    for b in branches:
        if b.level < min_level:
            continue
        for i, p in enumerate(b.pts):
            dd = (p - c).length
            if dd < bd:
                bd, best = dd, (b, i)
    return best, bd


def place_foliage(geo, branches, top_level, envs, F, leaves, bark, rng):
    """Clumps at branch tips (+ part-way along twigs), then shell-fill clumps on every crown envelope
    (or canopy mass), each wired to the nearest branch by a thin twig so nothing floats.
    Clumps closer than `min_gap` are skipped. Each clump is shaded by its own envelope's normal."""
    centres = []   # (c, R, n, size, out_dir, env)
    R0, n0, s0 = F["R"], F["n"], F["size"]
    levels = F.get("tip_levels", [top_level])
    tips = [b for b in branches if b.level in levels and not getattr(b, "broken", False)]
    for b in tips:
        if rng.random() > F.get("keep_p", 1.0):
            continue
        env = getattr(b, "env", envs[0])
        for t in F.get("along", [1.0]):
            p, d, _ = b.point_at(t)
            k = 1.0 if t >= 0.99 else F.get("along_scale", 0.8)
            c = p + d * R0 * F.get("tip_push", 0.3) * k
            if env.norm_dist(c) > F.get("max_env", 1.15):
                continue
            if c.z < F.get("min_z", -99):
                continue
            centres.append((c, R0 * rng.uniform(0.85, 1.15) * k, max(3, int(round(n0 * k))), s0 * k, d, env))
    gap = F.get("min_gap", 0.75) * R0
    kept = []
    for cc in centres:
        if all((cc[0] - k[0]).length > gap for k in kept):
            kept.append(cc)
    nfill = F.get("fill", 0)
    if nfill:
        golden = math.pi * (3 - math.sqrt(5))
        wire = F.get("wire_r", 0.035)
        areas = [e.r.x * e.r.y + e.r.y * e.r.z + e.r.x * e.r.z for e in envs]
        for env, area in zip(envs, areas):
            ne = max(8, int(round(nfill * area / sum(areas))))
            for i in range(ne):
                z = 1 - 2 * (i + 0.5) / ne
                rr = math.sqrt(max(0.0, 1 - z * z))
                a = golden * i + rng.uniform(-0.25, 0.25)
                d = Vector((rr * math.cos(a), rr * math.sin(a), z))
                if d.z < F.get("fill_min_z", -0.3):
                    continue
                room = env.dist_along(env.c, d)
                c = env.c + d * room * rng.uniform(*F.get("fill_depth", (0.74, 0.9)))
                if c.z < F.get("min_z", -99):
                    continue
                if any((c - k[0]).length < F.get("fill_gap", 1.1) * R0 for k in kept):
                    continue
                hit, dist = nearest_branch_point(branches, c, F.get("wire_min_level", 1), F.get("wire_max", 6.0))
                if hit is None:
                    continue
                b, idx = hit
                start = b.pts[idx]
                seg = c - start
                if seg.length > 0.3:
                    mid = start + seg * 0.5 + Vector((0, 0, seg.length * 0.12))
                    tube(geo, [start, mid, c - seg.normalized() * R0 * 0.3],
                         [max(wire, b.radii[idx] * 0.6), wire * 0.8, wire * 0.5], bark, 3, tip="point")
                kept.append((c, R0 * rng.uniform(0.9, 1.1), n0, s0, d, env))
    for c, R, n, s, d, env in kept:
        leaf_clump(geo, c, R, n, s, leaves, rng, env, up_bias=F.get("up_bias", 0.35), tilt=F.get("tilt", 0.6),
                   droop=F.get("droop", 0.0), flat=F.get("flat", 1.0), normal_w=F.get("normal_w", (0.55, 0.25, 0.2)),
                   out_dir=d if F.get("follow_dir") else None, face_up=F.get("face_up", 0.0),
                   tuft=F.get("tuft", False))
    return len(kept)


def canopy_masses(n_ring, top=(0.42, 0.28), ring_dist=(0.45, 0.62), ring_r=(0.30, 0.40), ring_rz=(0.18, 0.26),
                  z_range=(0.3, 0.85), extra_rings=()):
    """Canopy made of cloud-like masses: one crowning mass plus one or more rings of lower masses.
    extra_rings: [(n, dist, r, rz, z_range)] with the same meaning as the first ring's arguments."""
    rings = [(n_ring, ring_dist, ring_r, ring_rz, z_range)] + list(extra_rings)

    def gen(rng, H, cb, P):
        R = P["crown_w"] * 0.5
        out = []
        if top:
            rz = R * top[1]
            out.append((Vector((rng.uniform(-0.05, 0.05) * R, rng.uniform(-0.05, 0.05) * R, H - rz * 0.95)),
                        Vector((R * top[0], R * top[0] * rng.uniform(0.85, 1.0), rz))))
        for ri, (n, dist_r, r_r, rz_r, zr) in enumerate(rings):
            az0 = rng.uniform(0, 2 * math.pi)
            for i in range(n):
                az = az0 + 2 * math.pi * i / n + rng.uniform(-0.3, 0.3) * 2 * math.pi / n
                dist = R * rng.uniform(*dist_r)
                rr = R * rng.uniform(*r_r)
                rz = R * rng.uniform(*rz_r)
                z = cb + rz + max(0.0, H - cb - 2 * rz) * rng.uniform(*zr)
                out.append((Vector((dist * math.cos(az), dist * math.sin(az), z)),
                            Vector((rr, rr * rng.uniform(0.85, 1.0), rz))))
        return out
    return gen


# --------------------------------------------------------------------------------------------
# Oak
# --------------------------------------------------------------------------------------------
def oak(name, variant):
    base = dict(
        bark="MT_BarkOak", leaves="MT_LeavesOak", sides=[16, 9, 6, 4],
        below=0.6, trunk_seg=0.7, flare=0.45, flare_h=0.9, trunk_taper=0.55,
        collar=dict(lobes=5, amp=0.35, sharp=4, h=0.9),
        foliage=dict(R=1.25, n=7, size=1.9, along=[1.0, 0.55], along_scale=0.8, up_bias=0.3, tilt=0.55,
                     fill=160, fill_gap=1.05, min_gap=0.7, max_env=1.2, normal_w=(0.6, 0.25, 0.15), fill_min_z=-0.45,
                     tip_levels=[1, 2, 3]),
    )
    lv3 = dict(n=1.1, t=(0.35, 1.0), angle=(35, 65), len_env=(0.5, 0.95), r_rel=0.5, seg=0.9,
               tropism=0.08, wander=0.6, tip_r=0.4, max_len=1.8, min_len=0.4, up_bias=0.2)
    if variant == "A":     # classic broad oak, 12 m
        P = dict(base, height=12.0, crown_base=0.26, crown_w=14.0, trunk_h=0.42, trunk_r=0.55, lean=0.02,
                 lump=0.2, lump_scale=1.2, flat_bottom=0.3,
                 levels=[
                     dict(n=5, t=(0.62, 1.0), angle=(35, 60), absolute_elev=(22, 48), len_env=(0.78, 0.92),
                          r_rel=0.62, seg=1.0, tropism=0.05, wander=0.35, tip_r=0.3, max_len=8.5),
                     dict(n=0.75, t=(0.3, 1.0), angle=(30, 58), len_env=(0.55, 0.9), r_rel=0.5, seg=0.8,
                          tropism=0.06, wander=0.5, tip_r=0.3, max_len=4.0, outward_bias=0.35, up_bias=0.25),
                     lv3])
    elif variant == "B":   # tall upright oak, 15.5 m
        P = dict(base, height=14.2, crown_base=0.3, crown_w=11.5, trunk_h=0.62, trunk_r=0.5, lean=0.03,
                 lump=0.2, lump_scale=1.3, flat_bottom=0.2,
                 levels=[
                     dict(n=7, t=(0.45, 1.0), angle=(35, 60), absolute_elev=(28, 58), len_env=(0.72, 0.9),
                          r_rel=0.55, seg=1.0, tropism=0.06, wander=0.35, tip_r=0.3, max_len=7.5),
                     dict(n=0.8, t=(0.3, 1.0), angle=(30, 58), len_env=(0.55, 0.9), r_rel=0.5, seg=0.8,
                          tropism=0.06, wander=0.5, tip_r=0.3, max_len=3.6, outward_bias=0.35, up_bias=0.25),
                     lv3])
    else:                  # low, wide, gnarled oak, 10.5 m: short thick bole, near-horizontal limbs
        P = dict(base, height=10.5, crown_base=0.2, crown_w=16.5, crown_d=14.0, trunk_h=0.26, trunk_r=0.72,
                 lean=0.05, lump=0.22, lump_scale=1.1, flat_bottom=0.35, wiggle=0.05, trunk_taper=0.7,
                 collar=dict(lobes=6, amp=0.45, sharp=4, h=0.8),
                 levels=[
                     dict(n=4, t=(0.62, 1.0), angle=(40, 65), absolute_elev=(6, 26), len_env=(0.82, 0.95),
                          r_rel=0.68, seg=0.8, tropism=0.12, wander=0.8, wander_scale=0.25, tip_r=0.28,
                          max_len=9.5),
                     dict(n=0.85, t=(0.2, 1.0), angle=(30, 60), len_env=(0.55, 0.9), r_rel=0.5, seg=0.75,
                          tropism=0.1, wander=0.8, tip_r=0.3, max_len=4.2, outward_bias=0.2, up_bias=0.45),
                     dict(lv3, up_bias=0.25)])
    P["foliage"] = dict(P["foliage"], min_z=max(P["crown_base"] * P["height"] * 0.95, 3.0))
    return broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Birch
# --------------------------------------------------------------------------------------------
def birch(name, variant):
    base = dict(
        bark="MT_BarkBirch", leaves="MT_LeavesBirch", sides=[12, 5, 4, 3], below=0.5, trunk_seg=0.7,
        flare=0.3, flare_h=0.6, trunk_taper=0.12, trunk_power=0.8, collar=dict(lobes=4, amp=0.18, sharp=3, h=0.6),
        wiggle=0.012, lump=0.2, lump_scale=1.5,
        levels=[
            dict(n=1.3, t=(0.3, 0.97), angle=(40, 60), absolute_elev=(28, 60), len_env=(0.62, 0.95),
                 r_rel=0.42, seg=0.6, tropism=-0.02, gravity=0.06, wander=0.3, tip_r=0.3, max_len=3.4,
                 min_len=0.6, min_r=0.025),
            dict(n=1.1, t=(0.3, 1.0), angle=(35, 65), to_env=False, len_rel=(0.3, 0.55), r_rel=0.55, seg=0.5,
                 gravity=0.25, wander=0.3, tip_r=0.4, max_len=1.6, min_len=0.35, min_r=0.012),
        ],
        foliage=dict(R=0.75, n=5, size=1.25, along=[1.0, 0.5], along_scale=0.8, up_bias=0.05, tilt=0.6, droop=0.5,
                     fill=70, fill_gap=1.35, min_gap=0.75, max_env=1.25, normal_w=(0.5, 0.35, 0.15),
                     fill_min_z=-0.75, tip_levels=[1, 2], wire_r=0.02, wire_max=3.0),
    )
    if variant == "A":     # single slender white trunk, 13.5 m, narrow airy oval crown
        P = dict(base, height=13.5, crown_base=0.3, crown_w=5.6, trunk_h=0.93, trunk_r=0.21, lean=0.025,
                 s_curve=0.01)
    else:                  # three-stem clump, 14.5 m
        P = dict(base, height=14.5, crown_base=0.3, crown_w=8.0, crown_d=7.0, trunk_h=0.9, trunk_r=0.17,
                 stems=[dict(off=(0.0, 0.0), lean=0.035, az=0.4, h=1.0, r=1.0),
                        dict(off=(0.26, 0.12), lean=0.12, az=0.1, h=0.9, r=0.85, s_curve=0.015),
                        dict(off=(-0.1, 0.27), lean=0.11, az=2.1, h=0.82, r=0.8, s_curve=-0.02)])
        P["levels"] = [dict(P["levels"][0], n=0.8), dict(P["levels"][1], n=0.9)]
        P["foliage"] = dict(P["foliage"], fill=60)
    P["foliage"] = dict(P["foliage"], min_z=P["crown_base"] * P["height"] * 0.9)
    return broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Conifers
# --------------------------------------------------------------------------------------------
def cone_normal_fn(axis_top, H, w_card=0.2):
    def fn(p, card_n):
        o = Vector((p.x, p.y, 0.0))
        r = o.normalized() if o.length > 1e-6 else Vector((1, 0, 0))
        n = r * 0.62 + UP * 0.45 + card_n * w_card
        return n.normalized()
    return fn


def bough(geo, pts, width, mat, nfn, fold=0.22, fin=0.5, snow=None, rng=None):
    """Needle cards along a conifer bough: a folded 'roof' strip (Lambda profile) plus a
    vertical 'fin' strip; optional snow sheet (solid MT_Snow) lying on the roof."""
    n = len(pts)
    sides = []
    for i in range(n):
        tan = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
        s = tan.cross(UP)
        if s.length < 1e-4:
            s = L.perp(tan)
        sides.append(s.normalized())
    widths = [width * (0.75 + 0.25 * math.sin(math.pi * min(1.0, 0.2 + i / (n - 1)))) for i in range(n)]
    L.strip_card(geo, pts, sides, widths, mat, normal_fn=nfn, fold=[w * fold for w in widths])
    if fin:
        fpts = [p - UP * (widths[i] * 0.1) for i, p in enumerate(pts)]
        fsides = []
        for i in range(n):
            tan = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
            fsides.append(sides[i].cross(tan).normalized())    # roughly vertical
        L.strip_card(geo, fpts, fsides, [w * fin for w in widths], mat, normal_fn=nfn)
    if snow:
        # pillow of snow lying on the roof: thick along the ridge, edges tucked 1.5 cm above the needles
        rows = []
        idxs = list(range(1, n))
        for j, i in enumerate(idxs):
            t = j / max(1, len(idxs) - 1)
            tan = (pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized()
            nr = sides[i].cross(tan).normalized()
            w = widths[i]
            half = 0.5 * w * snow * (0.6 + 0.4 * math.sin(math.pi * (0.1 + 0.8 * t)))
            if i == n - 1:
                half *= 0.55
            bump = 0.06 + 0.05 * math.sin(math.pi * t)
            row = []
            for c in (-1.0, -0.55, 0.0, 0.55, 1.0):
                x = c * half
                h = fold * w - 2.0 * fold * abs(x) + 0.015 + bump * (1.0 - c * c)
                row.append(geo.vert(pts[i] + sides[i] * x + nr * h))
            rows.append((row, nr, tan, pts[i] + nr * (fold * w + 0.02)))

        def put(f, nr):
            a_, b_, c_ = (Vector(geo.V[x]) for x in f[:3])
            if (b_ - a_).cross(c_ - a_).dot(nr) < 0:
                f = tuple(reversed(f))
            geo.face(f, L.box_uv_face([geo.V[x] for x in f]), "MT_Snow", True)
        for j in range(len(rows) - 1):
            ra, nr, _, _ = rows[j]
            rb = rows[j + 1][0]
            for ci in range(4):
                put((ra[ci], ra[ci + 1], rb[ci + 1], rb[ci]), nr)
        for (row, nr, tan, ctr), sgn in ((rows[0], -1.0), (rows[-1], 1.0)):
            tip = geo.vert(ctr + tan * (0.16 * sgn))
            for ci in range(4):
                put((row[ci], row[ci + 1], tip), nr)


def conifer(name, P):
    rng = random.Random(L.seed_of(name))
    noff = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geo = Geo()
    H = P["height"]
    bark, needles = P["bark"], P["needles"]
    pts = trunk_polyline(rng, H, lean=P.get("lean", 0.01), wiggle=P.get("wiggle", 0.006), below=P.get("below", 0.5),
                         seg=0.9, noise_off=noff)
    r0 = P["trunk_r"]
    radii = radius_profile(pts, r0, r0 * 0.1, flare=P.get("flare", 0.4), flare_h=0.6, power=P.get("trunk_power", 0.9))
    trunk = Branch(pts, radii, 0)
    collar = root_collar(5, 0.3, 4, 0.5, rng)
    tube(geo, pts, radii, bark, P.get("trunk_sides", 12), ring_fn=collar, tip="point", tip_len=0.7)
    nfn = cone_normal_fn(pts[-1], H)
    snow = P.get("snow")

    # whorls of boughs
    z0 = P["z0"]
    z = z0
    whorls = []
    while z < H - P.get("top_gap", 1.0):
        whorls.append(z)
        z += P["spacing"] * rng.uniform(0.8, 1.2)
    golden = 2.39996
    az_w = rng.uniform(0, 2 * math.pi)
    for wi, zw in enumerate(whorls):
        u = (H - zw) / (H - z0)          # 1 at the lowest whorl, 0 at the top
        prof = P.get("profile", 0.9)
        Lb = P["L_min"] + (P["L_max"] - P["L_min"]) * (u ** prof) * (1.0 - 0.18 * smoothstep(0.85, 1.0, u))
        Lb *= rng.uniform(0.88, 1.1)
        nb = rng.choice(P["per_whorl"])
        az_w += golden
        tp, td, tr = trunk.point_at(max(0.0, min(1.0, (zw + P.get("below", 0.5)) / (H + P.get("below", 0.5)))))
        for k in range(nb):
            az = az_w + 2 * math.pi * k / nb + rng.uniform(-0.3, 0.3)
            el = math.radians(P["elev"][0] + (P["elev"][1] - P["elev"][0]) * (1 - u) + rng.uniform(-6, 6))
            d = dir_from_angles(az, el)
            start = tp + d * (tr * 0.2)
            bp = grow(start, d, Lb, max(0.3, Lb / 3.0), rng, gravity_bend=P.get("droop", 0.08),
                      tropism=P.get("upturn", 0.0), wander=0.25, noise_off=(noff[0] + wi, noff[1] + k, noff[2]),
                      min_segs=3)
            # low boughs sweep along the ground instead of diving into it (keeps fins above z = 0)
            floor = 0.3 + 0.25 * min(1.0, Lb / 3.0)
            bp = [Vector((q.x, q.y, max(q.z, min(floor, tp.z)))) for q in bp]
            br_r = max(0.025, P.get("bough_r", 0.07) * (0.35 + 0.65 * u))
            tube(geo, bp, taper(len(bp), br_r, 0.012), bark, 4 if br_r > 0.04 else 3, tip="point")
            # needle strip from just outside the trunk to slightly past the tip
            s_off = min(0.45, (tr + 0.12) / max(Lb, 0.1))
            b = Branch(bp, taper(len(bp), br_r, 0.012), 1)
            spts = []
            nseg = P.get("strip_segs", 3)
            for j in range(nseg + 1):
                t = s_off + (1.0 - s_off) * j / nseg
                p, dd, _ = b.point_at(t)
                spts.append(p)
            tipd = (spts[-1] - spts[-2]).normalized()
            spts[-1] = spts[-1] + tipd * (0.12 * Lb + 0.15)
            w = min(P.get("w_max", 1.9), P.get("w_rel", 0.75) * Lb + 0.35)
            bough(geo, spts, w, needles, nfn, fold=P.get("fold", 0.22), fin=P.get("fin", 0.5),
                  snow=snow if (snow and (u > 0.04)) else None, rng=rng)
    # leader: crossed vertical cards at the very top
    top = pts[-1]
    lh = P.get("leader_h", 1.8)
    for k in range(3):
        a = k * math.pi / 3 + rng.uniform(-0.2, 0.2)
        nrm = Vector((math.cos(a), math.sin(a), 0.0))
        quad(geo, top - UP * (lh * 0.55), nrm, UP, lh * 0.55, lh, needles, normal_fn=nfn, base_anchor=True)
    # dense core: crossed cards hugging the trunk so the cone never looks hollow from afar
    core = P.get("core", 0)
    if core:
        zc = z0 + 0.4
        while zc < H - 1.5:
            u = (H - zc) / (H - z0)
            Lc = (P["L_min"] + (P["L_max"] - P["L_min"]) * (u ** P.get("profile", 0.9))) * core
            hc = P.get("core_h", 2.2)
            for k in range(2):
                a = rng.uniform(0, math.pi) + k * math.pi / 2
                nrm = Vector((math.cos(a), math.sin(a), 0.0))
                quad(geo, Vector((0, 0, zc)) + (trunk.point_at((zc + 0.5) / (H + 0.5))[0] - Vector((0, 0, zc))),
                     nrm, UP, max(0.8, Lc * 1.6), hc, needles, normal_fn=nfn, base_anchor=True)
            zc += hc * 0.8
    if snow:
        # snow cap on the leader
        c = top - UP * 0.25
        prof = [(0.0, -0.05), (0.32, 0.05), (0.26, 0.2), (0.12, 0.34), (0.0, 0.4)]
        L.lathe(geo, [(r, c.z + zz) for r, zz in prof], "MT_Snow", 7, centre=(c.x, c.y))
    return geo, {"whorls": len(whorls)}


def scots_pine(name, P):
    """Tall bare trunk with an irregular, cloud-pruned crown of flat needle pads."""
    rng = random.Random(L.seed_of(name))
    noff = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    Pb = dict(
        bark=P["bark"], leaves=P["needles"], sides=[12, 6, 4, 3], height=P["height"], crown_base=P["crown_base"],
        crown_w=P["crown_w"], trunk_h=0.92, trunk_r=P["trunk_r"], lean=0.03, s_curve=0.02, below=0.5, flare=0.35,
        flare_h=0.6, trunk_taper=0.12, collar=dict(lobes=5, amp=0.25, sharp=4, h=0.6), lump=0.25, lump_scale=1.0,
        flat_top=0.5, flat_bottom=0.3,
        levels=[
            dict(n=0.8, t=(0.56, 0.98), angle=(40, 70), absolute_elev=(5, 40), len_env=(0.8, 0.98), r_rel=0.42,
                 seg=0.7, tropism=0.12, wander=0.6, tip_r=0.3, max_len=4.5, min_len=1.2),
            dict(n=0.9, t=(0.35, 1.0), angle=(30, 60), len_env=(0.5, 0.9), r_rel=0.5, seg=0.6, tropism=0.1,
                 wander=0.6, tip_r=0.4, max_len=1.8, min_len=0.5, up_bias=0.3),
        ],
        foliage=dict(R=1.1, n=8, size=1.7, along=[1.0, 0.6], along_scale=0.85, up_bias=0.75, tilt=0.35, flat=0.4,
                     face_up=1.1, fill=120, fill_gap=1.05, min_gap=0.8, max_env=1.3, normal_w=(0.45, 0.4, 0.15),
                     fill_min_z=-0.75,
                     tip_levels=[1, 2], wire_r=0.03, wire_max=3.0),
    )
    Pb["foliage"]["min_z"] = P["crown_base"] * P["height"] * 0.95
    return broadleaf(name, Pb)


def pine(name, variant):
    if variant == "A":     # spruce: dense cone almost to the ground, 14 m
        return conifer(name, dict(bark="MT_BarkPine", needles="MT_NeedlesPine", height=14.0, trunk_r=0.32,
                                  z0=0.9, spacing=0.62, per_whorl=[5, 6], L_max=3.4, L_min=0.45, profile=0.95,
                                  elev=(-20, 12), droop=0.1, upturn=0.05, w_rel=0.72, w_max=1.8, fold=0.2, fin=0.5,
                                  core=0.45, core_h=2.4, leader_h=1.9))
    if variant == "B":     # Scots-pine type: tall bare trunk, flat cloud crown, 18 m
        return scots_pine(name, dict(bark="MT_BarkPine", needles="MT_NeedlesPine", height=18.0, crown_base=0.5,
                                     crown_w=7.6, trunk_r=0.36))
    # C: tall slender fir, 21.5 m
    return conifer(name, dict(bark="MT_BarkPine", needles="MT_NeedlesPine", height=20.8, trunk_r=0.4, z0=2.6,
                              spacing=0.7, per_whorl=[5, 6], L_max=3.3, L_min=0.4, profile=1.05, elev=(-24, 8),
                              droop=0.12, upturn=0.03, w_rel=0.72, w_max=1.8, fold=0.2, fin=0.5, core=0.4,
                              core_h=2.6, leader_h=2.2, trunk_sides=12))


def pine_snow(name, variant):
    if variant == "A":     # snow-laden spruce, 12.5 m
        return conifer(name, dict(bark="MT_BarkPine", needles="MT_NeedlesSnow", height=12.5, trunk_r=0.3, z0=0.8,
                                  spacing=0.72, per_whorl=[5, 6], L_max=3.2, L_min=0.45, profile=0.95,
                                  elev=(-24, 8), droop=0.14, upturn=0.02, w_rel=0.75, w_max=1.8, fold=0.2, fin=0.45,
                                  core=0.45, core_h=2.4, leader_h=1.7, snow=0.66))
    return conifer(name, dict(bark="MT_BarkPine", needles="MT_NeedlesSnow", height=17.0, trunk_r=0.36, z0=1.6,
                              spacing=0.8, per_whorl=[5, 6], L_max=3.3, L_min=0.4, profile=1.0, elev=(-26, 6),
                              droop=0.15, upturn=0.02, w_rel=0.75, w_max=1.8, fold=0.2, fin=0.45, core=0.42,
                              core_h=2.6, leader_h=2.0, snow=0.66))


# --------------------------------------------------------------------------------------------
# Dead
# --------------------------------------------------------------------------------------------
def dead(name, variant):
    if variant == "A":     # dead oak: gnarled, broken limbs, fine twig tips, 9.5 m
        P = dict(bark="MT_BarkDead", leaves=None, sides=[16, 9, 6, 3], height=11.0, crown_base=0.3, crown_w=11.0,
                 trunk_h=0.45, trunk_r=0.45, lean=0.05, wiggle=0.05, flare=0.45, flare_h=0.8, trunk_taper=0.5,
                 collar=dict(lobes=5, amp=0.45, sharp=4, h=0.8), lump=0.25, lump_scale=1.2, below=0.6,
                 rope=dict(amp=0.06, k=5, twist=0.25),
                 levels=[
                     dict(n=5, t=(0.55, 1.0), angle=(35, 60), absolute_elev=(12, 50), len_env=(0.75, 0.95),
                          r_rel=0.62, seg=0.6, tropism=0.08, wander=1.1, wander_scale=0.4, tip_r=0.25, max_len=7.0,
                          break_p=0.25),
                     dict(n=0.9, t=(0.2, 1.0), angle=(30, 65), len_env=(0.5, 0.9), r_rel=0.5, seg=0.5, tropism=0.1,
                          wander=1.2, tip_r=0.25, max_len=3.2, up_bias=0.2, break_p=0.2),
                     dict(n=2.2, t=(0.25, 1.0), angle=(30, 70), len_env=(0.4, 0.9), r_rel=0.5, seg=0.4, tropism=0.1,
                          wander=1.3, tip_r=0.3, max_len=1.4, min_len=0.25, min_r=0.012, min_tip_r=0.008),
                 ])
    else:                  # tall dead snag: broken top, a few spiky limbs, 13.5 m
        P = dict(bark="MT_BarkDead", leaves=None, sides=[12, 6, 4, 3], height=13.5, crown_base=0.35, crown_w=7.0,
                 trunk_h=0.95, trunk_r=0.42, lean=0.04, wiggle=0.025, flare=0.4, flare_h=0.8, trunk_taper=0.4,
                 collar=dict(lobes=5, amp=0.4, sharp=4, h=0.8), lump=0.2, below=0.6, trunk_tip=0.5,
                 rope=dict(amp=0.05, k=6, twist=0.18),
                 levels=[
                     dict(n=1.0, t=(0.3, 0.92), angle=(45, 75), absolute_elev=(-5, 40), len_env=(0.55, 0.95),
                          r_rel=0.35, seg=0.5, tropism=0.1, wander=0.9, tip_r=0.15, max_len=4.2, break_p=0.3,
                          min_r=0.05),
                     dict(n=1.3, t=(0.25, 1.0), angle=(30, 65), to_env=False, len_rel=(0.25, 0.5), r_rel=0.5, seg=0.35,
                          tropism=0.1, wander=1.0, tip_r=0.3, max_len=1.8, min_len=0.3, min_r=0.015, min_tip_r=0.008),
                     dict(n=1.5, t=(0.3, 1.0), angle=(30, 70), to_env=False, len_rel=(0.3, 0.6), r_rel=0.5, seg=0.3,
                          tropism=0.1, wander=1.2, tip_r=0.3, max_len=0.8, min_len=0.2, min_r=0.01, min_tip_r=0.006),
                 ])
    return broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Great Forest giants
# --------------------------------------------------------------------------------------------
def giant(name, variant):
    base = dict(bark="MT_BarkGiant", leaves="MT_LeavesGiant", sides=[40, 12, 7, 4], below=1.5, trunk_seg=1.4,
                flare=0.55, flare_h=3.0, trunk_taper=0.4, trunk_power=0.8, wiggle=0.012, mass_lump=0.18,
                mass_flat=0.5,
                foliage=dict(R=2.9, n=8, size=4.5, along=[1.0, 0.55], along_scale=0.8, up_bias=0.35, tilt=0.55,
                             fill=300, fill_gap=1.05, min_gap=0.7, max_env=1.2, normal_w=(0.62, 0.23, 0.15),
                             fill_min_z=-0.45, tip_levels=[2, 3], wire_r=0.12, wire_max=12.0))
    lv2 = dict(n=0.4, t=(0.2, 1.0), angle=(30, 65), len_env=(0.55, 0.92), r_rel=0.45, seg=1.5, tropism=0.04,
               wander=0.35, tip_r=0.3, max_len=10.0, outward_bias=0.2, up_bias=0.2)
    lv3 = dict(n=0.45, t=(0.35, 1.0), angle=(35, 65), len_env=(0.5, 0.95), r_rel=0.5, seg=1.3, tropism=0.05,
               wander=0.45, tip_r=0.4, max_len=4.2, min_len=1.0, up_bias=0.2)
    if variant == "A":     # 46 m, ~4.2 m trunk, crown of six cloud masses
        P = dict(base, height=46.0, crown_base=0.44, crown_w=42.0, trunk_h=0.74, trunk_r=2.1, lean=0.015,
                 collar=dict(lobes=7, amp=1.7, sharp=10, h=4.5),
                 roots=dict(n=7, len=(7.0, 12.0), r=(0.6, 0.9), rise=1.8, sink=1.5, sides=7),
                 masses=canopy_masses(6, top=(0.34, 0.22), ring_dist=(0.6, 0.7), ring_r=(0.26, 0.32),
                                      ring_rz=(0.15, 0.2), z_range=(0.05, 0.45),
                                      extra_rings=[(4, (0.3, 0.4), (0.26, 0.32), (0.15, 0.2), (0.5, 0.8))]),
                 levels=[dict(n=6, t=(0.6, 1.0), angle=(30, 55), r_rel=0.5, seg=2.0, tropism=0.02, wander=0.25,
                              tip_r=0.3), lv2, lv3])
    else:                  # 55 m, ~5 m trunk, taller canopy of seven masses
        P = dict(base, height=50.5, crown_base=0.4, crown_w=46.0, crown_d=42.0, trunk_h=0.78, trunk_r=2.5,
                 lean=0.02, collar=dict(lobes=8, amp=1.8, sharp=12, h=5.5),
                 roots=dict(n=8, len=(8.0, 14.0), r=(0.65, 1.0), rise=2.0, sink=1.6, sides=7),
                 masses=canopy_masses(7, top=(0.3, 0.2), ring_dist=(0.6, 0.72), ring_r=(0.22, 0.28),
                                      ring_rz=(0.13, 0.17), z_range=(0.0, 0.35),
                                      extra_rings=[(5, (0.32, 0.45), (0.24, 0.3), (0.14, 0.18), (0.45, 0.75))]),
                 levels=[dict(n=7, t=(0.5, 1.0), angle=(30, 55), r_rel=0.46, seg=2.2, tropism=0.02, wander=0.25,
                              tip_r=0.3), lv2, lv3])
    P["foliage"] = dict(P["foliage"], min_z=P["crown_base"] * P["height"] * 0.97)
    return broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Southern jungle
# --------------------------------------------------------------------------------------------
def jungle(name, variant):
    base = dict(bark="MT_BarkGiant", leaves="MT_LeavesGiant", sides=[16, 8, 5, 3], below=0.8, trunk_seg=1.0,
                flare=0.35, flare_h=1.5, trunk_taper=0.5, trunk_power=0.8, wiggle=0.012, mass_lump=0.15,
                mass_flat=0.55, mass_flat_top=0.3,
                foliage=dict(R=1.9, n=7, size=3.0, along=[1.0, 0.55], along_scale=0.8, up_bias=0.5, tilt=0.45,
                             fill=210, fill_gap=1.05, min_gap=0.7, max_env=1.2, normal_w=(0.62, 0.23, 0.15),
                             fill_min_z=-0.35, tip_levels=[2, 3], wire_r=0.06, wire_max=7.0))
    lv2 = dict(n=0.55, t=(0.25, 1.0), angle=(35, 65), len_env=(0.55, 0.92), r_rel=0.45, seg=1.0, tropism=0.05,
               wander=0.4, tip_r=0.3, max_len=5.0, outward_bias=0.4, up_bias=0.1)
    lv3 = dict(n=0.7, t=(0.35, 1.0), angle=(35, 65), len_env=(0.5, 0.95), r_rel=0.5, seg=1.0, tropism=0.05,
               wander=0.5, tip_r=0.4, max_len=2.2, min_len=0.6, up_bias=0.2)
    if variant == "A":     # emergent umbrella, 24 m: flat canopy of seven low masses
        P = dict(base, height=24.0, crown_base=0.64, crown_w=22.0, trunk_h=0.72, trunk_r=0.72, lean=0.03,
                 collar=dict(lobes=5, amp=1.1, sharp=8, h=1.8),
                 masses=canopy_masses(6, top=(0.36, 0.17), ring_dist=(0.5, 0.64), ring_r=(0.28, 0.36),
                                      ring_rz=(0.13, 0.17), z_range=(0.35, 0.9)),
                 levels=[dict(n=6, t=(0.8, 1.0), angle=(40, 70), r_rel=0.5, seg=1.2, tropism=0.03, wander=0.25,
                              tip_r=0.3), lv2, lv3])
    else:                  # 28 m, leaning, two-tier layered canopy
        P = dict(base, height=28.0, crown_base=0.5, crown_w=25.0, trunk_h=0.8, trunk_r=0.8, lean=0.05,
                 s_curve=0.015, collar=dict(lobes=6, amp=1.3, sharp=9, h=2.2),
                 masses=canopy_masses(7, top=(0.34, 0.15), ring_dist=(0.42, 0.66), ring_r=(0.24, 0.32),
                                      ring_rz=(0.1, 0.14), z_range=(0.0, 0.95)),
                 levels=[dict(n=8, t=(0.6, 1.0), angle=(40, 70), r_rel=0.48, seg=1.2, tropism=0.03, wander=0.25,
                              tip_r=0.3), lv2, lv3])
    P["foliage"] = dict(P["foliage"], min_z=P["crown_base"] * P["height"] * 0.95)
    return broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Demon Continent
# --------------------------------------------------------------------------------------------
def demon(name, variant):
    base = dict(bark="MT_BarkDemon", leaves="MT_LeavesDemon", sides=[14, 7, 5, 3], below=0.6, trunk_seg=0.5,
                flare=0.5, flare_h=0.8, trunk_taper=0.4, wiggle=0.05, lump=0.3, lump_scale=1.0,
                collar=dict(lobes=5, amp=0.55, sharp=5, h=0.9),
                foliage=dict(R=0.85, n=5, size=1.5, along=[1.0], up_bias=0.2, tilt=0.7, droop=0.5, fill=0,
                             min_gap=1.2, max_env=1.4, normal_w=(0.45, 0.35, 0.2), tip_levels=[2, 3], keep_p=0.45))
    if variant == "A":     # 8.5 m, twisted rope trunk, wide claw crown
        P = dict(base, height=8.5, crown_base=0.35, crown_w=10.0, trunk_h=0.5, trunk_r=0.42, lean=0.08,
                 s_curve=0.04, rope=dict(amp=0.16, k=4, twist=0.55),
                 levels=[
                     dict(n=5, t=(0.55, 1.0), angle=(35, 60), absolute_elev=(10, 45), len_env=(0.8, 0.98),
                          r_rel=0.55, seg=0.45, tropism=0.1, wander=0.6, curl=0.5, tip_r=0.2, max_len=6.5),
                     dict(n=0.9, t=(0.25, 1.0), angle=(35, 70), len_env=(0.5, 0.9), r_rel=0.45, seg=0.4,
                          tropism=0.05, wander=0.9, curl=0.8, tip_r=0.2, max_len=2.8, up_bias=0.1, gravity=0.1),
                     dict(n=1.2, t=(0.3, 1.0), angle=(35, 70), to_env=False, len_rel=(0.3, 0.6), r_rel=0.5, seg=0.3,
                          gravity=0.25, wander=1.0, curl=1.2, tip_r=0.3, max_len=1.2, min_len=0.25, min_r=0.012,
                          min_tip_r=0.006),
                 ])
    elif variant == "B":   # 12 m corkscrew trunk with drooping claws
        P = dict(base, height=10.0, crown_base=0.5, crown_w=7.5, trunk_h=0.86, trunk_r=0.4, lean=0.02,
                 helix_r=0.55, helix_pitch=5.5, rope=dict(amp=0.14, k=3, twist=0.9), trunk_taper=0.3,
                 levels=[
                     dict(n=0.9, t=(0.45, 0.98), angle=(45, 75), absolute_elev=(0, 38), len_env=(0.75, 0.98),
                          r_rel=0.42, seg=0.4, tropism=-0.02, gravity=0.12, wander=0.7, curl=0.7, tip_r=0.2,
                          max_len=4.2),
                     dict(n=1.1, t=(0.25, 1.0), angle=(35, 70), to_env=False, len_rel=(0.3, 0.6), r_rel=0.5,
                          seg=0.35, gravity=0.3, wander=0.9, curl=1.1, tip_r=0.25, max_len=1.8, min_len=0.3,
                          min_r=0.015, min_tip_r=0.006),
                 ],
                 foliage=dict(base["foliage"], tip_levels=[1, 2], keep_p=0.5))
    else:                  # 6.5 m squat, split trunk, spiky
        P = dict(base, height=6.5, crown_base=0.3, crown_w=8.5, crown_d=7.0, trunk_h=0.55, trunk_r=0.36,
                 rope=dict(amp=0.12, k=5, twist=0.7), trunk_taper=0.35,
                 stems=[dict(off=(0.0, 0.0), lean=0.28, az=0.3, h=1.0, r=1.0, s_curve=0.05),
                        dict(off=(0.08, 0.05), lean=0.34, az=3.3, h=0.85, r=0.85, s_curve=-0.06)],
                 levels=[
                     dict(n=4, t=(0.5, 1.0), angle=(35, 60), absolute_elev=(15, 55), len_env=(0.7, 0.98),
                          r_rel=0.5, seg=0.35, tropism=0.12, wander=0.8, curl=0.9, tip_r=0.15, max_len=4.0,
                          outward_from_stem=True),
                     dict(n=1.3, t=(0.25, 1.0), angle=(35, 70), to_env=False, len_rel=(0.25, 0.55), r_rel=0.5,
                          seg=0.3, tropism=0.15, wander=1.0, curl=1.2, tip_r=0.2, max_len=1.6, min_len=0.25,
                          min_r=0.012, min_tip_r=0.006),
                 ],
                 foliage=dict(base["foliage"], tip_levels=[1, 2], keep_p=0.4))
    return broadleaf(name, P)


# --------------------------------------------------------------------------------------------
# Oasis palms
# --------------------------------------------------------------------------------------------
def palm_trunk(geo, rng, height, r0, lean, lean_az, bow, s_curve, noff, base=(0.0, 0.0)):
    pts = trunk_polyline(rng, height, lean=lean, wiggle=0.004, below=0.5, seg=0.16, bow=bow, lean_az=lean_az,
                         noise_off=noff, s_curve=s_curve, base=base)
    radii = []
    for p in pts:
        h = max(0.0, p.z)
        t = h / height
        r = r0 * (1.0 - 0.28 * t) * (1.0 + 0.55 * math.exp(-h / 0.55))
        ring = (h / 0.32) % 1.0
        r *= 1.0 + 0.07 * (ring ** 3)        # leaf-scar rings: sawtooth bulges
        radii.append(r)
    # crown shaft: swelling just below the fronds
    for i, p in enumerate(pts):
        h = max(0.0, p.z)
        radii[i] *= 1.0 + 0.35 * smoothstep(height - 1.4, height - 0.5, h) * (1.0 - smoothstep(height - 0.35, height, h))
    tube(geo, pts, radii, "MT_BarkDead", 10, tip="point", tip_len=0.5)
    T, _, _ = L.transport_frames(pts)
    return pts[-1], T[-1]


def palm_crown(geo, rng, top, axis, n_fronds, length, width, n_dead, noff):
    golden = 2.39996
    az0 = rng.uniform(0, 2 * math.pi)
    env = Envelope(top, (length, length, length * 0.6))
    for i in range(n_fronds):
        f = i / max(1, n_fronds - 1)
        az = az0 + golden * i
        el = math.radians(62 - 85 * f + rng.uniform(-8, 8))     # young fronds up, old ones drooping
        d = (dir_from_angles(az, el) + axis * 0.15).normalized()
        ln = length * rng.uniform(0.85, 1.1) * (0.75 + 0.25 * min(1.0, f * 2.5))
        start = top + d * 0.15
        pts = grow(start, d, ln, ln / 8.0, rng, gravity_bend=0.34 + 0.2 * f, wander=0.15,
                   noise_off=(noff[0] + i, noff[1], noff[2]), min_segs=8)
        sides = []
        for j in range(len(pts)):
            tan = (pts[min(j + 1, len(pts) - 1)] - pts[max(j - 1, 0)]).normalized()
            s = tan.cross(UP)
            if s.length < 1e-3:
                s = L.perp(tan)
            # slight twist along the frond so it does not read as a flat plank
            s = Matrix.Rotation(math.radians(18) * (j / len(pts)) * (1 if i % 2 else -1), 3, tan) @ s
            sides.append(s.normalized())
        widths = [width * (0.35 + 0.65 * math.sin(math.pi * min(1.0, 0.12 + 0.95 * j / (len(pts) - 1))) ** 0.6)
                  for j in range(len(pts))]

        def nfn(p, card_n, top=top):
            q = p - top
            return (q.normalized() * 0.6 + UP * 0.4 + card_n * 0.25).normalized()
        L.strip_card(geo, pts, sides, widths, "MT_LeavesPalm", normal_fn=nfn, fold=[-w * 0.18 for w in widths])
    for i in range(n_dead):
        az = az0 + 2 * math.pi * i / n_dead + 0.6
        d = dir_from_angles(az, math.radians(-68 + rng.uniform(-8, 8)))
        ln = length * rng.uniform(0.45, 0.6)
        pts = grow(top - axis * 0.3 + d * 0.2, d, ln, ln / 5.0, rng, gravity_bend=0.05, min_segs=5)
        sides = [d.cross(UP).normalized() if d.cross(UP).length > 1e-3 else Vector((1, 0, 0)) for _ in pts]
        L.strip_card(geo, pts, sides, [width * 0.55] * len(pts), "MT_GrassDry",
                     normal_fn=lambda p, n: (n * 0.5 + (p - top).normalized() * 0.5).normalized())


def palm(name, variant):
    rng = random.Random(L.seed_of(name))
    noff = (rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100))
    geo = Geo()
    if variant == "A":
        top, axis = palm_trunk(geo, rng, 9.0, 0.3, 0.12, rng.uniform(0, 6.28), 0.08, 0.0, noff)
        palm_crown(geo, rng, top, axis, 24, 5.0, 1.9, 5, noff)
    else:   # twin-trunk oasis palm
        az = rng.uniform(0, 6.28)
        top, axis = palm_trunk(geo, rng, 11.4, 0.31, 0.08, az, 0.06, 0.02, noff)
        palm_crown(geo, rng, top, axis, 20, 5.2, 1.95, 5, noff)
        top2, axis2 = palm_trunk(geo, rng, 8.5, 0.26, 0.2, az + math.pi * 0.85, 0.1, 0.0,
                                 (noff[0] + 3, noff[1], noff[2]), base=(0.16, -0.22))
        palm_crown(geo, rng, top2, axis2, 16, 4.4, 1.75, 4, (noff[0] + 9, noff[1], noff[2]))
    return geo, {}


# --------------------------------------------------------------------------------------------
# Registry
# --------------------------------------------------------------------------------------------
SPECIES = {}
META = {}


def _add(name, fn, style, regions, budget, hrange):
    SPECIES[name] = fn
    META[name] = dict(style=style, regions=regions, budget=budget, hrange=hrange)


for _v in "ABC":
    _add("SM_Tree_Oak_" + _v, (lambda v: lambda n: oak(n, v))(_v), "Temperate", [1, 2, 5, 11], (2000, 10000),
         (10.0, 16.0))
for _v in "AB":
    _add("SM_Tree_Birch_" + _v, (lambda v: lambda n: birch(n, v))(_v), "Temperate", [1, 2, 3, 4, 11], (2000, 10000),
         (12.0, 15.0))
for _v in "ABC":
    _add("SM_Tree_Pine_" + _v, (lambda v: lambda n: pine(n, v))(_v), "Mountain", [3, 4, 6, 12], (2000, 10000),
         (12.0, 22.0))
for _v in "AB":
    _add("SM_Tree_PineSnow_" + _v, (lambda v: lambda n: pine_snow(n, v))(_v), "Northern", [3, 4, 7, 12],
         (2000, 10000), (12.0, 22.0))
for _v in "AB":
    _add("SM_Tree_Dead_" + _v, (lambda v: lambda n: dead(n, v))(_v), "Barren", [5, 8, 14, 4], (2000, 10000),
         (8.0, 16.0))
for _v in "AB":
    _add("SM_Tree_Giant_" + _v, (lambda v: lambda n: giant(n, v))(_v), "GreatForest", [10], (2000, 20000),
         (35.0, 55.0))
for _v in "ABC":
    _add("SM_Tree_Demon_" + _v, (lambda v: lambda n: demon(n, v))(_v), "Demon", [8, 9], (2000, 10000), (6.0, 12.0))
for _v in "AB":
    _add("SM_Tree_Palm_" + _v, (lambda v: lambda n: palm(n, v))(_v), "Desert", [13, 14, 15, 6], (2000, 10000),
         (8.0, 15.0))
for _v in "AB":
    _add("SM_Tree_Jungle_" + _v, (lambda v: lambda n: jungle(n, v))(_v), "Jungle", [6], (2000, 10000), (18.0, 32.0))
