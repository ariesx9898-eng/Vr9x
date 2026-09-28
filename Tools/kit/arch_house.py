"""Generic storeyed house builder shared by the Asura, Rural, North, Noble and Millis sets.

A house is a stack of rectangular storeys (each may jetty out over the one below), a gabled or hipped roof with
tile courses, openings recessed into the walls with trims, optional half-timbering, chimneys, dormers and hooks
for set-specific extras. Everything is built centred on the origin with the ground-floor front wall facing -Y.
"""
import math

from arch_parts import (stair, Opening, arch_poly, block, chimney, corner_posts, door_trim, joist_ends, rect,
                        roof_gable, roof_hip, side_frame, timber_facade, window_trim, frame_ring, _brace)

SIDES = ("front", "right", "back", "left")


def gable_heights(zw, span, pitch, th):
    ta = math.tan(math.radians(pitch))
    ca = math.cos(math.radians(pitch))
    ze = zw + th / (2 * ca)
    zr = zw + (span / 2) * ta + th / (2 * ca)
    return ze, zr, ta, ca


def shift(op, dv):
    return Opening([(u, v + dv) for (u, v) in op.poly], op.depth, op.back, op.reveal, op.kind, **op.data)


def window_row(L, n, w, h, sill, avoid=(), margin=0.7, kind="rect", arch_segs=8, depth=0.18, back="MT_Glass",
               reveal=None, positions=None):
    """n windows spread over a facade of length L (bay centres), skipping ones that overlap `avoid` intervals."""
    ops = []
    if n <= 0:
        return ops
    us = positions if positions is not None else [margin + (L - 2 * margin) * (k + 0.5) / n for k in range(n)]
    for uc in us:
        u0 = uc - w / 2
        if any(u0 - 0.35 < b and u0 + w + 0.35 > a for a, b in avoid):
            continue
        if kind == "arch":
            poly = arch_poly(u0, sill, w, h, arch_segs)
        elif kind == "pointed":
            poly = arch_poly(u0, sill, w, h, arch_segs, pointed=True)
        elif kind == "round":
            r = w / 2
            poly = [(uc + r * math.cos(math.pi / 8 + math.tau * i / 12), sill + r + r * math.sin(math.pi / 8 +
                     math.tau * i / 12)) for i in range(12)]
        else:
            poly = rect(u0, sill, w, h)
        ops.append(Opening(poly, depth=depth, back=back, reveal=reveal, kind=("arch" if kind in ("arch", "pointed")
                                                                                 else "window")))
    return ops


class House:
    """Parametric house. Call build(g) after configuring the attributes; hooks see the computed levels."""

    def __init__(self, W, D, heights, **kw):
        self.W = W
        self.D = D
        self.heights = list(heights)
        self.floor0 = kw.pop("floor0", 0.45)
        self.jetty = kw.pop("jetty", [])
        self.ridge = kw.pop("ridge", "x")
        self.pitch = kw.pop("pitch", 50)
        self.roof = kw.pop("roof", "gable")
        self.roof_mat = kw.pop("roof_mat", "MT_RoofRed")
        self.roof_under = kw.pop("roof_under", "MT_WoodPlanks")
        self.fascia = kw.pop("fascia", "MT_Timber")
        self.verge = kw.pop("verge", None)
        self.ov = kw.pop("ov", 0.45)
        self.ovv = kw.pop("ovv", 0.3)
        self.th = kw.pop("th", 0.22)
        self.course = kw.pop("course", 0.34)
        self.step = kw.pop("step", 0.045)
        self.walls = kw.pop("walls", ["MT_Plaster"])
        self.timber = kw.pop("timber", [False])
        self.timber_mat = kw.pop("timber_mat", "MT_Timber")
        self.plinth = kw.pop("plinth", "MT_Stone")
        self.plinth_proud = kw.pop("plinth_proud", 0.08)
        self.foundation = kw.pop("foundation", 0.5)
        self.win_w = kw.pop("win_w", 0.9)
        self.win_h = kw.pop("win_h", 1.3)
        self.win_sill = kw.pop("win_sill", 0.95)
        self.win_spacing = kw.pop("win_spacing", 2.0)
        self.win_kind = kw.pop("win_kind", "rect")
        self.win_styles = kw.pop("win_styles", [dict(trim="MT_Timber", shutters=True, sill_mat="MT_Stone")])
        self.side_windows = kw.pop("side_windows", True)
        self.back_windows = kw.pop("back_windows", True)
        self.door = kw.pop("door", dict(side="front", at=0.5, w=1.1, h=2.15, kind="rect"))
        self.door_style = kw.pop("door_style", dict(door_frame_mat="MT_Timber"))
        self.step_mat = kw.pop("step_mat", "MT_Stone")
        self.chimneys = kw.pop("chimneys", [])
        self.dormers = kw.pop("dormers", [])
        self.dormer_wall = kw.pop("dormer_wall", "MT_Plaster")
        self.snow = kw.pop("snow", None)
        self.snow_from = kw.pop("snow_from", 0.25)
        self.gable_windows = kw.pop("gable_windows", True)
        self.gable_timber = kw.pop("gable_timber", None)
        self.corner_posts = kw.pop("corner_posts", None)
        self.hip_finial = kw.pop("hip_finial", None)
        self.extra_openings = kw.pop("extra_openings", {})   # (storey, side) -> [Opening] in storey coords
        self.skip_windows = kw.pop("skip_windows", set())    # {(storey, side)}
        self.window_counts = kw.pop("window_counts", {})     # (storey, side) -> n
        self.window_positions = kw.pop("window_positions", {})  # (storey, side) -> [u centres]
        self.cornices = kw.pop("cornices", None)            # material for storey bands (stone houses)
        self.quoins = kw.pop("quoins", None)
        self.hooks = kw.pop("hooks", [])
        self.ridge_mat = kw.pop("ridge_mat", None)
        self.under_braces = kw.pop("under_braces", True)
        self.corner_boards = kw.pop("corner_boards", None)   # material for corner boards on non-timber storeys
        self.door_steps = kw.pop("door_steps", True)
        self.pitch2 = kw.pop("pitch2", 28)          # mansard upper pitch
        self.mansard_h = kw.pop("mansard_h", 2.4)   # mansard lower slope rise
        self.crest = kw.pop("crest", None)          # ornamental ridge crest material
        self.crest_spacing = kw.pop("crest_spacing", 0.85)
        self.pilasters = kw.pop("pilasters", None)  # corner pilaster material (masonry houses)
        self.side_win_styles = kw.pop("side_win_styles", None)  # simpler trims for back / side facades
        self.parapet = kw.pop("parapet", dict(h=0.9, mat=None, coping=None, merlons=None))  # flat roofs
        self.max_panel = kw.pop("max_panel", 1.5)
        if kw:
            raise TypeError(f"unknown House options {sorted(kw)}")

    # ----------------------------------------------------------------- helpers
    def storey_mat(self, i):
        return self.walls[min(i, len(self.walls) - 1)]

    def storey_timber(self, i):
        return self.timber[min(i, len(self.timber) - 1)]

    def win_style(self, i):
        return self.win_styles[min(i, len(self.win_styles) - 1)]

    def build(self, g):
        W, D = self.W, self.D
        fp = (-W / 2, -D / 2, W / 2, D / 2)
        z = self.floor0
        self.levels = []
        for i, H in enumerate(self.heights):
            if i > 0:
                j = self.jetty[i - 1] if i - 1 < len(self.jetty) else (0.11, 0.11, 0.11)
                jf, jb, js = j
                fp = (fp[0] - js, fp[1] - jf, fp[2] + js, fp[3] + jb)
            self.levels.append({"i": i, "z0": z, "z1": z + H, "H": H, "fp": fp})
            z += H
        top = self.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        span = (y1 - y0) if self.ridge == "x" else (x1 - x0)
        self.zw = top["z1"]
        ze, zr, ta, ca = gable_heights(self.zw, span, self.pitch, self.th)
        self.ta = ta
        self.ze, self.zr = ze, zr
        # ---- plinth
        bx0, by0, bx1, by1 = self.levels[0]["fp"]
        pp = self.plinth_proud
        if self.plinth:
            g.box(bx0 - pp, by0 - pp, g.buried(-self.foundation), bx1 + pp, by1 + pp, self.floor0 - 0.03, self.plinth)
        # ---- storeys
        door = self.door
        for L in self.levels:
            self._storey(g, L, L is top, door if L["i"] == 0 else None)
        # ---- roof
        self._roof(g)
        for c in self.chimneys:
            self._chimney(g, c)
        for d in self.dormers:
            self._dormer(g, d)
        for h in self.hooks:
            h(self, g)
        return self

    # ----------------------------------------------------------------- storeys
    def _storey(self, g, L, is_top, door):
        i = L["i"]
        x0, y0, x1, y1 = L["fp"]
        z0, z1, H = L["z0"], L["z1"], L["H"]
        zb = g.buried(-self.foundation + 0.05) if i == 0 else z0 - 0.12
        mat = self.storey_mat(i)
        timber = self.storey_timber(i)
        wst = self.win_style(i)
        ops = {}
        att = {}
        for side in SIDES:
            fr, Lf = side_frame(x0, y0, x1, y1, z0, side)
            lst = []
            avoid = []
            if door and door.get("side", "front") == side:
                dw, dh = door["w"], door["h"]
                du = Lf * door.get("at", 0.5) - dw / 2
                if door.get("kind") == "arch":
                    dpoly = arch_poly(du, 0.0, dw, dh, 8)
                else:
                    dpoly = rect(du, 0.0, dw, dh)
                lst.append(Opening(dpoly, depth=0.22, back=door.get("mat", "MT_WoodPlanks"), kind=(
                    "arch" if door.get("kind") == "arch" else "door")))
                avoid.append((du, du + dw))
            if (i, side) not in self.skip_windows:
                if side in ("left", "right") and not self.side_windows:
                    n = 0
                elif side == "back" and not self.back_windows:
                    n = 0
                else:
                    n = max(1, int((Lf - 0.8) / self.win_spacing))
                    if side in ("left", "right"):
                        n = max(1, int((Lf - 0.8) / (self.win_spacing * 1.4)))
                n = self.window_counts.get((i, side), n)
                ws = wst.get("win_w", self.win_w)
                wh = wst.get("win_h", self.win_h)
                sill = wst.get("win_sill", self.win_sill)
                if wh + sill > H - 0.3:
                    wh = H - 0.3 - sill
                if wh < 0.45:
                    n = 0
                pos = self.window_positions.get((i, side))
                extra = self.extra_openings.get((i, side), [])
                avoid = avoid + [(o.box[0] - 0.1, o.box[2] + 0.1) for o in extra
                                 if o.box[1] < sill + wh + 0.3 and o.box[3] > sill - 0.3]
                lst += window_row(Lf, n, ws, wh, sill, avoid=avoid, kind=wst.get("kind", self.win_kind),
                                  positions=pos, depth=wst.get("depth", 0.18))
            for o in self.extra_openings.get((i, side), []):
                if i > 0 and o.kind == "door" and o.box[1] < 0.05:
                    # upper-storey door: 5 cm threshold so its pocket floor never meets the storey below
                    o = Opening([(u, v + 0.05) for (u, v) in o.poly], o.depth, o.back, o.reveal, o.kind, **o.data)
                    o.data["jamb_drop"] = 0.06
                lst.append(o)
            ops[side] = lst
            att[side] = (fr, Lf)
        # body
        dv = z0 - zb
        body_ops = {s: [shift(o, dv) for o in lst] for s, lst in ops.items()}
        if is_top and self.roof == "gable":
            # gable face openings (attic windows) in the triangle
            gs = ("left", "right") if self.ridge == "x" else ("front", "back")
            if self.gable_windows:
                for s in gs:
                    fr, Lf = att[s]
                    tri_h = (Lf / 2) * self.ta
                    if tri_h > 1.6:
                        aw = min(0.8, Lf * 0.12)
                        ah = min(1.1, tri_h * 0.35)
                        a_op = Opening(rect(Lf / 2 - aw / 2, H + 0.35, aw, ah), depth=0.16,
                                       kind="window")
                        ops[s].append(a_op)
                        body_ops[s].append(shift(a_op, dv))
            block(g, x0, y0, x1, y1, zb, z1, mat, ops=body_ops,
                  gable=(self.ridge, self.ze, self.zr))
        else:
            ztop = z1 if (not is_top or self.roof == "flat") else z1 + 0.25
            block(g, x0, y0, x1, y1, zb, ztop, mat, ops=body_ops)
        L["ops"] = ops
        L["frames"] = att
        # trim widths are needed by the timber layout
        for side in SIDES:
            for op in ops[side]:
                if "fw" in op.data:
                    continue
                if op.kind == "loft":
                    op.data["fw"] = 0.09
                elif op.kind == "door" or (op.kind == "arch" and op.back == "MT_WoodPlanks"):
                    op.data["fw"] = self.door_style.get("door_frame_w", 0.1)
                elif op.kind == "shop":
                    op.data["fw"] = 0.1
                else:
                    op.data["fw"] = wst.get("frame_w", 0.08)
        # timber
        if timber:
            for side in SIDES:
                fr, Lf = att[side]
                sill_r = []
                wins = [o for o in ops[side] if o.kind in ("window", "arch") and o.box[3] <= H]
                if wins:
                    f = max(o.data["fw"] for o in wins)
                    vs = min(o.box[1] for o in wins)
                    vh = max(o.box[3] for o in wins)
                    if vs - f + 0.015 - 0.16 > 0.35:
                        sill_r.append(vs - f + 0.015 - 0.16)
                    if vh + f - 0.015 + 0.16 < H - 0.28:
                        sill_r.append(vh + f - 0.015)
                timber_facade(g, fr, Lf, H, [o for o in ops[side] if o.box[3] <= H + 0.01], mat=self.timber_mat,
                              extra_rails=sill_r, flip=(side in ("back", "left")), under_braces=self.under_braces,
                              max_panel=self.max_panel)
            cp = self.corner_posts if self.corner_posts is not None else True
            if cp:
                corner_posts(g, x0, y0, x1, y1, z0 - (0.06 if i == 0 else 0.1), z1 + 0.03, self.timber_mat)
        elif self.corner_boards:
            corner_posts(g, x0, y0, x1, y1, z0 - (0.06 if i == 0 else 0.1), z1 + 0.03, self.corner_boards, s=0.19,
                         p=0.045)
        if self.pilasters:
            corner_posts(g, x0, y0, x1, y1, z0 - (0.08 if i == 0 else 0.06), z1 - 0.05, self.pilasters, s=0.55,
                         p=0.075)
            if is_top and self.roof == "gable":
                gs = ("left", "right") if self.ridge == "x" else ("front", "back")
                for s in gs:
                    fr, Lf = att[s]
                    self._gable_timber(g, fr, Lf, H, [o for o in ops[s] if o.box[1] > H])
            if i > 0:
                # joist ends under jettied fronts
                prev = self.levels[i - 1]
                px0, py0, px1, py1 = prev["fp"]
                jf = py0 - y0
                if jf > 0.25:
                    frp, Lp = side_frame(px0, py0, px1, py1, prev["z0"], "front")
                    joist_ends(g, frp, Lp, prev["H"] - 0.12 + 0.02, n_out=jf - 0.05, mat=self.timber_mat)
                jb = y1 - py1
                if jb > 0.25:
                    frp, Lp = side_frame(px0, py0, px1, py1, prev["z0"], "back")
                    joist_ends(g, frp, Lp, prev["H"] - 0.12 + 0.02, n_out=jb - 0.05, mat=self.timber_mat)
        # shutters only where the neighbouring opening leaves room for them
        if wst.get("shutters"):
            for side in SIDES:
                row = sorted([o for o in ops[side]], key=lambda o: o.box[0])
                for a_, b_ in zip(row, row[1:]):
                    need = 0.0
                    for o in (a_, b_):
                        if o.kind == "window":
                            need += o.data.get("fw", 0.08) + 0.04 + (o.box[2] - o.box[0]) / 2
                        else:
                            need += o.data.get("fw", 0.1) + 0.02
                    gap_ = b_.box[0] - a_.box[2]
                    if gap_ < need + 0.03 and a_.box[1] < b_.box[3] and b_.box[1] < a_.box[3]:
                        a_.data["no_shutter_right"] = True
                        b_.data["no_shutter_left"] = True
                for o in row:
                    if o.box[0] < (o.box[2] - o.box[0]) / 2 + 0.35:
                        o.data["no_shutter_left"] = True
                    if Lf - o.box[2] < (o.box[2] - o.box[0]) / 2 + 0.35:
                        o.data["no_shutter_right"] = True
        # trims
        for side in SIDES:
            fr, Lf = att[side]
            for op in ops[side]:
                if op.kind == "shop":
                    from arch_parts import shop_front
                    frame_ring(g, fr, op, width=op.data.get("fw", 0.1), mat=self.timber_mat)
                    shop_front(g, fr, op, op.data.get("awning", "MT_ClothRed"))
                    continue
                if op.kind == "loft":
                    door_trim(g, fr, op, dict(self.door_style, straps=False, battens=True,
                                              door_frame_mat=self.timber_mat, door_frame_w=0.09))
                    continue
                if op.kind in ("door",) or (op.kind == "arch" and op.back == "MT_WoodPlanks"):
                    door_trim(g, fr, op, self.door_style)
                    # steps up to the threshold
                    u0, v0, u1, v1 = op.box
                    if i == 0 and z0 > 0.1 and self.door_steps:
                        self._door_steps(g, fr, u0, u1, z0)
                else:
                    st = dict(wst)
                    if side != "front" and self.side_win_styles:
                        st = dict(self.side_win_styles[min(i, len(self.side_win_styles) - 1)])
                    if timber and op.box[3] <= H:
                        st.setdefault("sill", False)
                        st["shutters"] = st.get("shutters_timber", False)
                    window_trim(g, fr, op, st)
        # storey band / cornice for masonry houses
        if self.cornices and not is_top:
            from arch_parts import cornice
            cornice(g, x0, y0, x1, y1, z1 - 0.08, 0.22, 0.1, self.cornices)

    def _door_steps(self, g, fr, u0, u1, z0):
        """Stone steps from the ground up to just below the threshold (z0), in front of a door on face fr."""
        stair(g, fr, u0 - 0.25, u1 + 0.25, z0 - 0.015, self.step_mat)

    def _gable_timber(self, g, fr, Lf, H, attic_ops):
        """King post, collar rail, queen posts and raking braces on a gable triangle (face frame fr, storey
        height H). Tops are sunk into the roof slab (which starts at the roof underside)."""
        from arch_parts import N_BRACE, N_BRACE2, N_POST, N_RAIL, _split_span
        tm = self.timber_mat
        ta = self.ta

        def v_under(u):
            return H + min(u, Lf - u) * ta

        c = Lf / 2
        hit = lambda ua_, ub_: any(b.box[0] - 0.1 < ub_ and b.box[2] + 0.1 > ua_ for b in attic_ops)
        if not hit(c - 0.1, c + 0.1):
            top = v_under(c - 0.1) + 0.06
            g.plate(fr, rect(c - 0.1, H - 0.05, 0.2, top - H + 0.05), N_POST[0], N_POST[1], tm)
        vc = H + (Lf / 2) * ta * 0.42
        ua = (vc - H) / ta - 0.02
        ub = Lf - ua
        cuts = [(b.box[0] - 0.1, b.box[2] + 0.1) for b in attic_ops if b.box[1] - 0.06 < vc + 0.18 and
                b.box[3] + 0.06 > vc]
        for s_, e_ in _split_span(ua, ub, cuts):
            g.plate(fr, rect(s_, vc, e_ - s_, 0.18), N_RAIL[0], N_RAIL[1], tm)
        for u in (Lf * 0.27, Lf * 0.73):
            if hit(u - 0.09, u + 0.09):
                continue
            g.plate(fr, rect(u - 0.09, H - 0.05, 0.18, vc - H + 0.15), N_POST[0], N_POST[1], tm)
        if Lf * 0.27 - 0.09 - 0.25 > 0.7 and vc - H > 0.6:
            _brace(g, fr, 0.25, H - 0.02, Lf * 0.27 - 0.09, vc, 0.15, N_BRACE, tm, False)
            _brace(g, fr, Lf * 0.73 + 0.09, H - 0.02, Lf - 0.25, vc, 0.15, N_BRACE2, tm, True)

    # ----------------------------------------------------------------- roof
    def _roof(self, g):
        top = self.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        if self.roof == "gable":
            self.roof_info = roof_gable(g, x0, x1, y0, y1, self.zw, self.pitch, self.roof_mat, ov=self.ov,
                                        ovv=self.ovv, th=self.th, course=self.course, step=self.step,
                                        under=self.roof_under, fascia=self.fascia, verge=self.verge or self.fascia,
                                        axis=self.ridge, snow=self.snow, snow_from=self.snow_from,
                                        ridge_mat=self.ridge_mat)
        elif self.roof == "flat":
            from arch_parts import parapet_ring
            p = self.parapet
            parapet_ring(g, x0, y0, x1, y1, self.zw, p.get("h", 0.9), p.get("mat") or self.walls[-1],
                         proud=p.get("proud", 0.05), thick=p.get("thick", 0.35), coping=p.get("coping"),
                         merlons=p.get("merlons"))
            self.roof_info = None
        elif self.roof == "mansard":
            ta = math.tan(math.radians(self.pitch))
            ca = math.cos(math.radians(self.pitch))
            cut = (self.mansard_h + self.ov * ta - self.th / ca) / ta
            self.roof_info = roof_hip(g, x0, x1, y0, y1, self.zw, self.pitch, self.roof_mat, ov=self.ov, th=self.th,
                                      course=self.course, step=self.step, under=self.roof_under, fascia=self.fascia,
                                      hip_mat=self.ridge_mat, cut=cut)
            cx0, cy0, cx1, cy1 = self.roof_info.cut_rect
            self.upper_info = roof_hip(g, cx0, cx1, cy0, cy1, self.roof_info.z_cut - 0.137, self.pitch2, self.roof_mat,
                                       ov=0.0, th=self.th, course=self.course, step=self.step, under=self.roof_under,
                                       fascia=self.fascia, finial=self.hip_finial, hip_mat=self.ridge_mat,
                                       crest=self.crest, crest_spacing=self.crest_spacing)
        else:
            self.roof_info = roof_hip(g, x0, x1, y0, y1, self.zw, self.pitch, self.roof_mat, ov=self.ov, th=self.th,
                                      course=self.course, step=self.step, under=self.roof_under, fascia=self.fascia,
                                      finial=self.hip_finial, snow=self.snow, hip_mat=self.ridge_mat, crest=self.crest,
                                      crest_spacing=self.crest_spacing)

    def roof_top_z(self, x, y):
        """Height of the roof tile base above world (x, y) for gable/hip roofs of this house."""
        top = self.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        if self.roof == "flat":
            return self.zw
        if self.roof == "mansard":
            cx0, cy0, cx1, cy1 = self.roof_info.cut_rect
            if cx0 <= x <= cx1 and cy0 <= y <= cy1:
                return self.upper_info.z_top(min(x - cx0, cx1 - x, y - cy0, cy1 - y))
            return self.roof_info.z_cut
        if self.roof == "gable":
            d = (min(y - y0, y1 - y) if self.ridge == "x" else min(x - x0, x1 - x))
        else:
            d = min(x - x0, x1 - x, y - y0, y1 - y)
        return self.roof_info.z_top(d)

    def ridge_z(self):
        if self.roof == "flat":
            return self.zw + self.parapet.get("h", 0.9)
        if self.roof == "mansard":
            return self.upper_info.z_ridge
        top = self.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        span = (y1 - y0) if self.ridge == "x" else (x1 - x0)
        if self.roof == "hip":
            span = min(y1 - y0, x1 - x0)
        return self.roof_info.z_top(span / 2)

    def _chimney(self, g, c):
        """c: dict(x, y (fractions -0.5..0.5 of the top footprint), w, d, above, mat)."""
        top = self.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        x = (x0 + x1) / 2 + c.get("x", 0.3) * (x1 - x0)
        y = (y0 + y1) / 2 + c.get("y", 0.0) * (y1 - y0)
        w = c.get("w", 0.7)
        d = c.get("d", 0.6)
        zt = max(self.ridge_z() + c.get("above", 0.7), self.roof_top_z(x, y) + 1.0)
        chimney(g, x, y, w, d, top["z0"] + 0.5, zt, c.get("mat", "MT_Stone"), c.get("cap", "MT_Stone"),
                pots=c.get("pots", 1), pot_mat=c.get("pot_mat", "MT_RoofRed"))

    def _dormer(self, g, dspec):
        """Roof dormer on the front (-Y) or back slope of an x-ridge gable or hip roof. dspec: dict(u, w, side)."""
        top = self.levels[-1]
        x0, y0, x1, y1 = top["fp"]
        side = dspec.get("side", "front")
        dw = dspec.get("w", 1.5)
        setback = dspec.get("setback", 0.45)
        # keep the dormer's front wall off the vertical tile-course riser planes
        rs = getattr(self.roof_info, "risers", [])
        for _ in range(8):
            near = [r for r in rs if abs(r - setback) < 0.025]
            if not near:
                break
            setback += 0.05
        if self.ridge == "x":
            xc = (x0 + x1) / 2 + dspec.get("u", 0.0) * (x1 - x0)
            sgn = -1 if side == "front" else 1
            y_wall = y0 if side == "front" else y1
            yf = y_wall - sgn * setback
            zt = self.roof_info.z_top(setback)
            zde = zt + dspec.get("h", 1.25)
            dpitch = dspec.get("pitch", 48)
            dth = 0.16
            ze_d, zr_d, dta, _ = gable_heights(zde, dw, dpitch, dth)
            dd = (zr_d + 0.35 - self.roof_info.z_top(0)) / self.ta
            yb = y_wall - sgn * min(dd, (y1 - y0) / 2 - 0.05)
            ya, yb2 = (yf, yb) if side == "front" else (yb, yf)
            zbot = zt - 0.45
            wh = min(0.95, zde - zt - 0.35)
            ww = min(0.8, dw - 0.6)
            op = Opening(rect(dw / 2 - ww / 2, zt + 0.18 - zbot, ww, wh), depth=0.14, kind="window")
            face = "front" if side == "front" else "back"
            with g.xf():
                block(g, xc - dw / 2, ya, xc + dw / 2, yb2, zbot, zde, dspec.get("wall", self.dormer_wall),
                      ops={face: [op]}, gable=("y", ze_d, zr_d))
                fr, Lf = side_frame(xc - dw / 2, ya, xc + dw / 2, yb2, zbot, face)
                window_trim(g, fr, op, dict(trim=self.timber_mat, shutters=False, sill=False))
                roof_gable(g, xc - dw / 2, xc + dw / 2, ya, yb2, zde, dpitch, self.roof_mat, ov=0.14, ovv=0.18,
                           th=dth, course=0.3, step=0.035, under=self.roof_under, fascia=self.fascia,
                           verge=self.verge or self.fascia, axis="y", snow=self.snow, snow_from=0.0,
                           ridge_mat=self.ridge_mat)
