"""Raster helpers for vertex-aligned world grids (numpy + Pillow only).

A grid "level" with step s covers the full landscape with vertex (i, j) at x = 3*s*j m, y = 3*s*i m from the
landscape min corner, i.e. level vertices coincide with every s-th landscape vertex.
"""
import math

import numpy as np
from PIL import Image, ImageDraw

import wg_config as C


class Level:
    def __init__(self, step):
        self.step = step
        self.W = (C.FULL_W - 1) // step + 1
        self.H = (C.FULL_H - 1) // step + 1
        self.cell = C.QUAD_M * step

    @property
    def shape(self):
        return (self.H, self.W)

    def xy(self, dtype=np.float64):
        x = (np.arange(self.W, dtype=dtype) * self.cell)[None, :]
        y = (np.arange(self.H, dtype=dtype) * self.cell)[:, None]
        return np.broadcast_to(x, self.shape), np.broadcast_to(y, self.shape)

    def xy_full(self, dtype=np.float64):
        x, y = self.xy(dtype)
        return np.ascontiguousarray(x), np.ascontiguousarray(y)

    def uv_to_px(self, u, v):
        return u * (self.W - 1), v * (self.H - 1)

    def m_to_px(self, x, y):
        return x / self.cell, y / self.cell


L0 = Level(1)
L1 = Level(C.L1_STEP)
L2 = Level(C.L2_STEP)


# ------------------------------------------------------------------------------------------ resampling
def _cubic_weights(t, kind="catmull"):
    t = float(t)
    if kind == "bspline":
        w0 = (1 - t) ** 3 / 6.0
        w1 = (3 * t ** 3 - 6 * t ** 2 + 4) / 6.0
        w2 = (-3 * t ** 3 + 3 * t ** 2 + 3 * t + 1) / 6.0
        w3 = t ** 3 / 6.0
    else:
        w0 = -0.5 * t ** 3 + t ** 2 - 0.5 * t
        w1 = 1.5 * t ** 3 - 2.5 * t ** 2 + 1.0
        w2 = -1.5 * t ** 3 + 2.0 * t ** 2 + 0.5 * t
        w3 = 0.5 * t ** 3 - 0.5 * t ** 2
    return (w0, w1, w2, w3)


def _upsample_axis(a, f, axis, kind):
    """Vertex-aligned cubic upsampling by integer factor f along one axis: n -> (n-1)*f+1."""
    a = np.moveaxis(a, axis, 0)
    n = a.shape[0]
    pad = np.concatenate([a[:1], a, a[-1:], a[-1:]], axis=0)  # index k -> pad[k+1]
    out = np.empty(((n - 1) * f + 1,) + a.shape[1:], dtype=np.float32)
    clamp = kind == "catmull_clamped"
    base_kind = "catmull" if clamp else kind
    for r in range(f):
        w = _cubic_weights(r / f, base_kind)
        m = n - 1 if r > 0 else n
        # out[k*f + r] = sum_t w[t] * a[k-1+t]  for k in 0..m-1
        acc = w[0] * pad[0:m] + w[1] * pad[1:m + 1] + w[2] * pad[2:m + 2] + w[3] * pad[3:m + 3]
        if clamp and r > 0:
            # no overshoot: stay within the two bracketing samples (kills ringing at cliff feet)
            lo = np.minimum(pad[1:m + 1], pad[2:m + 2])
            hi = np.maximum(pad[1:m + 1], pad[2:m + 2])
            acc = np.minimum(np.maximum(acc, lo), hi)
        out[r::f][:m] = acc
    return np.moveaxis(out, 0, axis)


def upsample(a, f, kind="catmull"):
    a = np.asarray(a, dtype=np.float32)
    if f == 1:
        return a.copy()
    return _upsample_axis(_upsample_axis(a, f, 1, kind), f, 0, kind)


def downsample(a, f):
    """Vertex-aligned downsampling (tent pre-filter) by integer factor f: n -> (n-1)//f+1."""
    a = np.asarray(a, dtype=np.float32)
    if f == 1:
        return a.copy()
    r = max(1, f // 2)
    b = box_blur(a, r)
    return np.ascontiguousarray(b[::f, ::f])


def resize_bilinear(a, out_h, out_w):
    im = Image.fromarray(np.asarray(a, np.float32), mode="F")
    return np.asarray(im.resize((out_w, out_h), Image.BILINEAR), dtype=np.float32)


def sample_bilinear(a, px, py):
    """Sample grid a at float pixel coordinates (px = column, py = row), clamped."""
    H, W = a.shape
    px = np.clip(np.asarray(px, np.float64), 0.0, W - 1.000001)
    py = np.clip(np.asarray(py, np.float64), 0.0, H - 1.000001)
    x0 = np.floor(px).astype(np.int64)
    y0 = np.floor(py).astype(np.int64)
    fx = (px - x0).astype(np.float32)
    fy = (py - y0).astype(np.float32)
    flat = a.ravel()
    i00 = y0 * W + x0
    v00 = flat[i00]
    v01 = flat[i00 + 1]
    v10 = flat[i00 + W]
    v11 = flat[i00 + W + 1]
    top = v00 + (v01 - v00) * fx
    bot = v10 + (v11 - v10) * fx
    return top + (bot - top) * fy


def sample_nearest(a, px, py):
    H, W = a.shape
    xi = np.clip(np.rint(px).astype(np.int64), 0, W - 1)
    yi = np.clip(np.rint(py).astype(np.int64), 0, H - 1)
    return a[yi, xi]


# ------------------------------------------------------------------------------------------ filters
def _box1d(a, r, axis):
    if r <= 0:
        return a
    a = np.moveaxis(a, axis, 0)
    n = a.shape[0]
    pad = np.concatenate([np.repeat(a[:1], r + 1, axis=0), a, np.repeat(a[-1:], r, axis=0)], axis=0)
    cs = np.cumsum(pad, axis=0, dtype=np.float64)
    out = (cs[2 * r + 1:2 * r + 1 + n] - cs[0:n]) / (2 * r + 1)
    return np.moveaxis(out.astype(np.float32), 0, axis)


def box_blur(a, r):
    a = np.asarray(a, np.float32)
    return _box1d(_box1d(a, r, 1), r, 0)


def blur(a, sigma_cells):
    """Gaussian-like blur (three box passes)."""
    if sigma_cells <= 0.3:
        return np.asarray(a, np.float32).copy()
    # three boxes of radius r approximate sigma^2 = 3 * ((2r+1)^2 - 1) / 12
    r = max(1, int(round(math.sqrt(4.0 * sigma_cells * sigma_cells + 1.0) / 2.0 - 0.5)))
    b = np.asarray(a, np.float32)
    for _ in range(3):
        b = box_blur(b, r)
    return b


def blur_m(a, level, sigma_m):
    return blur(a, sigma_m / level.cell)


def shift(a, dy, dx, fill=None):
    """Shift array by (dy, dx) cells (result[y, x] = a[y - dy, x - dx]); edges replicate unless fill given."""
    H, W = a.shape
    out = np.empty_like(a)
    ys_src = slice(max(0, -dy), H - max(0, dy))
    ys_dst = slice(max(0, dy), H - max(0, -dy))
    xs_src = slice(max(0, -dx), W - max(0, dx))
    xs_dst = slice(max(0, dx), W - max(0, -dx))
    out[ys_dst, xs_dst] = a[ys_src, xs_src]
    if fill is None:
        if dy > 0:
            out[:dy, :] = out[dy:dy + 1, :]
        elif dy < 0:
            out[dy:, :] = out[dy - 1:dy, :]
        if dx > 0:
            out[:, :dx] = out[:, dx:dx + 1]
        elif dx < 0:
            out[:, dx:] = out[:, dx - 1:dx]
    else:
        if dy > 0:
            out[:dy, :] = fill
        elif dy < 0:
            out[dy:, :] = fill
        if dx > 0:
            out[:, :dx] = fill
        elif dx < 0:
            out[:, dx:] = fill
    return out


def gradient(h, cell):
    """Central-difference gradient (dh/dx, dh/dy) in m/m."""
    gx = np.empty_like(h, dtype=np.float32)
    gy = np.empty_like(h, dtype=np.float32)
    gx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) / (2 * cell)
    gx[:, 0] = (h[:, 1] - h[:, 0]) / cell
    gx[:, -1] = (h[:, -1] - h[:, -2]) / cell
    gy[1:-1, :] = (h[2:, :] - h[:-2, :]) / (2 * cell)
    gy[0, :] = (h[1, :] - h[0, :]) / cell
    gy[-1, :] = (h[-1, :] - h[-2, :]) / cell
    return gx, gy


def slope(h, cell):
    gx, gy = gradient(h, cell)
    return np.sqrt(gx * gx + gy * gy)


def max_filter(a, r):
    """Square max filter of radius r (cells), separable (running max via doubling)."""
    out = np.asarray(a, np.float32).copy()
    for axis in (0, 1):
        cur = out
        res = cur.copy()
        for d in range(1, r + 1):
            if axis == 0:
                np.maximum(res, shift(cur, d, 0), out=res)
                np.maximum(res, shift(cur, -d, 0), out=res)
            else:
                np.maximum(res, shift(cur, 0, d), out=res)
                np.maximum(res, shift(cur, 0, -d), out=res)
        out = res
    return out


def dilate_mask(m, r):
    """Binary dilation with a square of radius r (cells), via box filter."""
    if r <= 0:
        return m.copy()
    return box_blur(m.astype(np.float32), r) > 1e-6


def erode_mask(m, r):
    if r <= 0:
        return m.copy()
    return box_blur(m.astype(np.float32), r) > 1.0 - 1e-6


# ------------------------------------------------------------------------------------------ rasterisation
def rasterize_polygons(polys_px, shape, value=1, supersample=1):
    """Fill polygons given in pixel coordinates (x = column, y = row). Returns bool mask (or uint8 labels)."""
    H, W = shape
    ss = supersample
    im = Image.new("L", (W * ss, H * ss), 0)
    d = ImageDraw.Draw(im)
    for poly, val in polys_px:
        pts = [((x + 0.5) * ss - 0.5, (y + 0.5) * ss - 0.5) for x, y in poly]
        d.polygon(pts, fill=int(val), outline=int(val))
    a = np.asarray(im)
    if ss > 1:
        a = np.asarray(Image.fromarray(a).resize((W, H), Image.NEAREST))
    return a


def point_in_polygon(px, py, poly):
    """Vectorised even-odd point-in-polygon test for arrays px, py and a list of (x, y) vertices."""
    inside = np.zeros(np.shape(px), dtype=bool)
    n = len(poly)
    xs = [p[0] for p in poly]
    ys = [p[1] for p in poly]
    j = n - 1
    for i in range(n):
        xi, yi, xj, yj = xs[i], ys[i], xs[j], ys[j]
        cond = (yi > py) != (yj > py)
        if yj != yi:
            xint = (xj - xi) * (py - yi) / (yj - yi) + xi
            inside ^= cond & (px < xint)
        j = i
    return inside


# ------------------------------------------------------------------------------------------ distance transforms
def jfa_nearest(seed_mask, max_dist_cells=None):
    """Jump-flooding nearest-seed transform. Returns (dist_cells float32, seed_y int32, seed_x int32).
    Cells with no seed within reach get dist = inf and seed = -1."""
    H, W = seed_mask.shape
    sy = np.full((H, W), -1, np.int32)
    sx = np.full((H, W), -1, np.int32)
    ys, xs = np.nonzero(seed_mask)
    sy[ys, xs] = ys
    sx[ys, xs] = xs
    Y = np.arange(H, dtype=np.int32)[:, None]
    X = np.arange(W, dtype=np.int32)[None, :]
    BIG = np.int64(1) << 60

    def d2(cy, cx):
        dy = (Y - cy).astype(np.int64)
        dx = (X - cx).astype(np.int64)
        d = dy * dy + dx * dx
        return np.where(cy < 0, BIG, d)

    best = d2(sy, sx)
    n = max(H, W)
    k = 1
    while k * 2 < n:
        k *= 2
    if max_dist_cells is not None:
        k = min(k, 1 << int(math.ceil(math.log2(max(2, max_dist_cells)))))
    steps = []
    while k >= 1:
        steps.append(k)
        k //= 2
    steps += [2, 1]
    for k in steps:
        for dy in (-k, 0, k):
            for dx in (-k, 0, k):
                if dy == 0 and dx == 0:
                    continue
                cy = shift(sy, dy, dx, fill=-1)
                cx = shift(sx, dy, dx, fill=-1)
                dd = d2(cy, cx)
                better = dd < best
                if better.any():
                    best = np.where(better, dd, best)
                    sy = np.where(better, cy, sy)
                    sx = np.where(better, cx, sx)
    dist = np.sqrt(best.astype(np.float64)).astype(np.float32)
    dist[best >= BIG] = np.inf
    return dist, sy, sx


def signed_distance(mask, cell):
    """Signed distance (m) to the boundary of a boolean mask: positive inside, negative outside."""
    m = mask.astype(bool)
    # boundary cells: inside cells with an outside 4-neighbour, and vice versa
    inner = m & ~(shift(m, 1, 0) & shift(m, -1, 0) & shift(m, 0, 1) & shift(m, 0, -1))
    outer = ~m & (shift(m, 1, 0) | shift(m, -1, 0) | shift(m, 0, 1) | shift(m, 0, -1))
    d_in, _, _ = jfa_nearest(outer)
    d_out, _, _ = jfa_nearest(inner)
    sd = np.where(m, d_in - 0.5, -(d_out - 0.5)).astype(np.float32) * cell
    return sd


def polyline_field(level, pts_m, max_dist_m, attrs=None):
    """Distance (m) from every cell to a polyline (list of (x, y) in metres), plus the arc-length position of
    the nearest point and optional linearly interpolated per-vertex attributes. Cells beyond max_dist get inf."""
    H, W = level.shape
    cell = level.cell
    dist = np.full((H, W), np.inf, np.float32)
    along = np.zeros((H, W), np.float32)
    out_attrs = None
    if attrs is not None:
        attrs = np.asarray(attrs, np.float32)
        out_attrs = np.zeros((attrs.shape[1], H, W), np.float32)
    seglen = [math.hypot(pts_m[i + 1][0] - pts_m[i][0], pts_m[i + 1][1] - pts_m[i][1]) for i in range(len(pts_m) - 1)]
    cum = np.concatenate([[0.0], np.cumsum(seglen)])
    for i in range(len(pts_m) - 1):
        (x0, y0), (x1, y1) = pts_m[i], pts_m[i + 1]
        c0 = int(max(0, math.floor((min(x0, x1) - max_dist_m) / cell)))
        c1 = int(min(W - 1, math.ceil((max(x0, x1) + max_dist_m) / cell)))
        r0 = int(max(0, math.floor((min(y0, y1) - max_dist_m) / cell)))
        r1 = int(min(H - 1, math.ceil((max(y0, y1) + max_dist_m) / cell)))
        if c1 < c0 or r1 < r0:
            continue
        xs = (np.arange(c0, c1 + 1, dtype=np.float32) * cell)[None, :]
        ys = (np.arange(r0, r1 + 1, dtype=np.float32) * cell)[:, None]
        dx, dy = x1 - x0, y1 - y0
        L2 = dx * dx + dy * dy
        if L2 < 1e-9:
            t = np.zeros((r1 - r0 + 1, c1 - c0 + 1), np.float32)
        else:
            t = np.clip(((xs - x0) * dx + (ys - y0) * dy) / L2, 0.0, 1.0)
        px = x0 + t * dx
        py = y0 + t * dy
        d = np.sqrt((xs - px) ** 2 + (ys - py) ** 2)
        sub = dist[r0:r1 + 1, c0:c1 + 1]
        better = d < sub
        sub[better] = d[better]
        along[r0:r1 + 1, c0:c1 + 1][better] = (cum[i] + t * seglen[i])[better]
        if attrs is not None:
            for a in range(attrs.shape[1]):
                val = attrs[i, a] + t * (attrs[i + 1, a] - attrs[i, a])
                out_attrs[a, r0:r1 + 1, c0:c1 + 1][better] = val[better]
    dist[dist > max_dist_m] = np.inf
    return dist, along, out_attrs, cum[-1]


# ------------------------------------------------------------------------------------------ labelling
def label_components(mask, connectivity=4):
    """Connected-component labels (int64, -1 outside) via min-label propagation with pointer jumping."""
    H, W = mask.shape
    N = H * W
    m = mask.ravel()
    lab = np.where(m, np.arange(N, dtype=np.int64), -1)
    idx = np.arange(N, dtype=np.int64).reshape(H, W)
    pairs = []
    # horizontal / vertical neighbour pairs inside the mask
    a = idx[:, :-1][mask[:, :-1] & mask[:, 1:]]
    pairs.append((a, a + 1))
    a = idx[:-1, :][mask[:-1, :] & mask[1:, :]]
    pairs.append((a, a + W))
    if connectivity == 8:
        a = idx[:-1, :-1][mask[:-1, :-1] & mask[1:, 1:]]
        pairs.append((a, a + W + 1))
        a = idx[:-1, 1:][mask[:-1, 1:] & mask[1:, :-1]]
        pairs.append((a, a + W - 1))
    pa = np.concatenate([p[0] for p in pairs])
    pb = np.concatenate([p[1] for p in pairs])
    while True:
        la, lb = lab[pa], lab[pb]
        mn = np.minimum(la, lb)
        changed = (la != lb)
        if not changed.any():
            break
        # hook the larger root onto the smaller label
        np.minimum.at(lab, la[changed], mn[changed])
        np.minimum.at(lab, lb[changed], mn[changed])
        # pointer jumping to roots
        for _ in range(64):
            nl = np.where(lab >= 0, lab[np.maximum(lab, 0)], -1)
            if np.array_equal(nl, lab):
                break
            lab = nl
    return lab.reshape(H, W)


def component_sizes(lab):
    valid = lab >= 0
    ids, counts = np.unique(lab[valid], return_counts=True)
    return ids, counts


# ------------------------------------------------------------------------------------------ misc
def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def normalize01(a):
    lo, hi = float(np.min(a)), float(np.max(a))
    return (a - lo) / max(1e-9, hi - lo)
