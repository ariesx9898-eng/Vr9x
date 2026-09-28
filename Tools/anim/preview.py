"""Headless preview renderer for posed Blender meshes (numpy painter's algorithm, flat shaded,
texture colour sampled per triangle). Used to visually verify every authored animation."""
import numpy as np
from PIL import Image, ImageDraw
import bpy

_TEX = None


def load_texture(path):
    global _TEX
    _TEX = np.asarray(Image.open(path).convert("RGB")).astype(np.float32)


def evaluate(arm_obj, mesh_obj):
    """Deformed vertices in armature space + triangles + per-triangle UV centroid."""
    dg = bpy.context.evaluated_depsgraph_get()
    ev = mesh_obj.evaluated_get(dg)
    me = ev.to_mesh()
    to_arm = arm_obj.matrix_world.inverted() @ mesh_obj.matrix_world
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    co = co.reshape(n, 3)
    m = np.array(to_arm)
    co = co @ m[:3, :3].T + m[:3, 3]
    me.calc_loop_triangles()
    nt = len(me.loop_triangles)
    tri = np.empty(nt * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("vertices", tri)
    loops = np.empty(nt * 3, dtype=np.int64)
    me.loop_triangles.foreach_get("loops", loops)
    uv = None
    if me.uv_layers.active:
        nl = len(me.loops)
        uvs = np.empty(nl * 2, dtype=np.float64)
        me.uv_layers.active.data.foreach_get("uv", uvs)
        uvs = uvs.reshape(nl, 2)
        uv = uvs[loops.reshape(nt, 3)].mean(1)
    ev.to_mesh_clear()
    return co, tri.reshape(nt, 3), uv


def draw(co, tri, uv, view="front", size=420, extra_points=None, ground=True, frame_box=None):
    # Armature space: +Z up, character faces -Y, left = +X.
    if view == "front":      # camera in front (at -Y) looking +Y
        sx, sy, depth = co[:, 0], co[:, 2], co[:, 1]
    elif view == "side":     # camera on the character's right (at -X) looking +X: front of body on the left
        sx, sy, depth = co[:, 1], co[:, 2], -co[:, 0]
    elif view == "back":
        sx, sy, depth = -co[:, 0], co[:, 2], -co[:, 1]
    else:                    # three-quarter
        a = np.radians(35)
        x = co[:, 0] * np.cos(a) - co[:, 1] * np.sin(a)
        dpt = co[:, 0] * np.sin(a) + co[:, 1] * np.cos(a)
        sx, sy, depth = x, co[:, 2], dpt
    P = np.stack([sx, sy, depth], 1)
    t = P[tri]
    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
    light = np.array([0.35, 0.55, -0.75])
    light /= np.linalg.norm(light)
    shade = 0.5 + 0.5 * np.abs(n @ light)
    if uv is not None and _TEX is not None:
        h, w = _TEX.shape[:2]
        tx = np.clip((uv[:, 0] % 1.0) * w, 0, w - 1).astype(int)
        ty = np.clip((1.0 - (uv[:, 1] % 1.0)) * h, 0, h - 1).astype(int)
        col = _TEX[ty, tx]
    else:
        col = np.full((len(tri), 3), 200.0)
    col = np.clip(col * shade[:, None], 0, 255).astype(np.uint8)
    lo, hi = (frame_box if frame_box is not None else (np.array([-1.0, -0.05]), np.array([1.0, 1.9])))
    scale = (size - 20) / (hi[1] - lo[1])
    W = int((hi[0] - lo[0]) * scale) + 20
    img = Image.new("RGB", (W, size), (38, 42, 50))
    d = ImageDraw.Draw(img)

    def to_px(x, y):
        return (10 + (x - lo[0]) * scale, size - 10 - (y - lo[1]) * scale)

    if ground:
        gy = to_px(0, 0)[1]
        d.line([(0, gy), (W, gy)], fill=(90, 140, 90), width=1)
    order = np.argsort(-t[:, :, 2].mean(1))  # far first
    for i in order:
        d.polygon([to_px(p[0], p[1]) for p in t[i]], fill=tuple(col[i]))
    if extra_points is not None:
        for p, c in extra_points:
            q = to_px(*p)
            d.ellipse([q[0] - 3, q[1] - 3, q[0] + 3, q[1] + 3], outline=c)
    return img


def sheet(images, cols, label_rows=None):
    w = max(i.width for i in images)
    h = max(i.height for i in images)
    rows = (len(images) + cols - 1) // cols
    out = Image.new("RGB", (w * cols, h * rows), (25, 27, 32))
    d = ImageDraw.Draw(out)
    for k, im in enumerate(images):
        out.paste(im, ((k % cols) * w, (k // cols) * h))
        if label_rows and k < len(label_rows):
            d.text(((k % cols) * w + 6, (k // cols) * h + 4), label_rows[k], fill=(235, 225, 200))
    return out
