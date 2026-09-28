"""Headless preview rendering for the architecture / VFX kit (EEVEE, Blender as a Python module).

render_views(obj or glb, out.png, views) renders one asset from several angles (for iteration);
contact_sheet(category_dir, names, out.png) re-imports every exported GLB and renders a labelled grid.
"""
import math
import os
import tempfile

import bpy
from mathutils import Vector
from PIL import Image, ImageDraw, ImageFont

GROUND = (0.34, 0.38, 0.28)


def _font(size):
    for p in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc",
              "/Library/Fonts/Arial.ttf"):
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()


def setup_scene(res=(640, 480), samples=24, ground=True, bg=(0.62, 0.72, 0.85)):
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    try:
        scn.eevee.taa_render_samples = samples
    except Exception:
        pass
    scn.render.resolution_x, scn.render.resolution_y = res
    scn.render.resolution_percentage = 100
    scn.render.film_transparent = False
    scn.view_settings.view_transform = "Standard"
    scn.view_settings.look = "None"
    scn.view_settings.exposure = 0.0
    world = bpy.data.worlds.get("KitWorld") or bpy.data.worlds.new("KitWorld")
    world.use_nodes = True
    bgn = world.node_tree.nodes.get("Background")
    bgn.inputs[0].default_value = (bg[0], bg[1], bg[2], 1.0)
    bgn.inputs[1].default_value = 0.9
    scn.world = world
    # sun
    if "KitSun" not in bpy.data.objects:
        ld = bpy.data.lights.new("KitSun", "SUN")
        ld.energy = 3.2
        ld.angle = math.radians(4)
        sun = bpy.data.objects.new("KitSun", ld)
        scn.collection.objects.link(sun)
        sun.rotation_euler = (math.radians(50), math.radians(0), math.radians(-35))
    if ground and "KitGround" not in bpy.data.objects:
        me = bpy.data.meshes.new("KitGround")
        s = 2000.0
        me.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
        gm = bpy.data.materials.get("KitGroundMat") or bpy.data.materials.new("KitGroundMat")
        gm.diffuse_color = (*GROUND, 1)
        try:
            gm.use_nodes = True
            gm.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (*GROUND, 1)
            gm.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 1.0
        except Exception:
            pass
        me.materials.append(gm)
        gob = bpy.data.objects.new("KitGround", me)
        scn.collection.objects.link(gob)
    if "KitCam" not in bpy.data.objects:
        cam = bpy.data.objects.new("KitCam", bpy.data.cameras.new("KitCam"))
        scn.collection.objects.link(cam)
        scn.camera = cam
    return scn


def set_ground(visible, z=0.0):
    g = bpy.data.objects.get("KitGround")
    if g:
        g.hide_render = not visible
        g.location.z = z


def world_bbox(objs):
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        if o.type != "MESH":
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector((min(lo[i], w[i]) for i in range(3)))
            hi = Vector((max(hi[i], w[i]) for i in range(3)))
    return lo, hi


def frame_camera(lo, hi, az=-35.0, el=24.0, lens=50.0, margin=1.08, ortho=False, above_ground=True):
    cam = bpy.data.objects["KitCam"]
    cam.data.lens = lens
    cam.data.clip_start = 0.05
    cam.data.clip_end = 20000
    lo = Vector(lo)
    hi = Vector(hi)
    if above_ground:
        lo.z = max(lo.z, 0.0)
    c = (lo + hi) / 2
    R = (hi - lo).length / 2
    a = math.radians(az)
    e = math.radians(el)
    # d points from the target to the camera; az = 0 is straight in front (-Y), negative az = front-left
    d = Vector((math.sin(a) * math.cos(e), -math.cos(a) * math.cos(e), math.sin(e)))
    scn = bpy.context.scene
    aspect = scn.render.resolution_x / scn.render.resolution_y
    if ortho:
        cam.data.type = "ORTHO"
        cam.data.ortho_scale = 2 * R * margin * max(1.0, aspect)
        dist = R * 4 + 10
    else:
        cam.data.type = "PERSP"
        fov = 2 * math.atan(18.0 / lens)  # horizontal fov for a 36 mm sensor
        vfov = 2 * math.atan(math.tan(fov / 2) / aspect) if aspect >= 1 else fov
        dist = R * margin / math.sin(min(fov, vfov) / 2)
    cam.location = c + d * dist
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    return cam


def render_to(path):
    scn = bpy.context.scene
    scn.render.filepath = path
    scn.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    return new


def remove_objects(objs):
    for o in objs:
        me = o.data if o.type == "MESH" else None
        bpy.data.objects.remove(o, do_unlink=True)
        if me is not None and me.users == 0:
            bpy.data.meshes.remove(me)


def render_views(objs, out, views=((-35, 24), (145, 24), (-90, 60)), res=(640, 480), ground=True, labels=None,
                 lens=50.0):
    """Render objs from several (azimuth, elevation) views and tile them horizontally into `out`."""
    setup_scene(res)
    set_ground(ground)
    lo, hi = world_bbox(objs)
    tmp = tempfile.mkdtemp(prefix="kitv_")
    ims = []
    for i, (az, el) in enumerate(views):
        frame_camera(lo, hi, az, el, lens=lens, above_ground=ground)
        p = os.path.join(tmp, f"v{i}.png")
        render_to(p)
        ims.append(Image.open(p).convert("RGB"))
    W = sum(im.width for im in ims)
    sheet = Image.new("RGB", (W, ims[0].height), (20, 20, 24))
    x = 0
    d = ImageDraw.Draw(sheet)
    f = _font(16)
    for i, im in enumerate(ims):
        sheet.paste(im, (x, 0))
        if labels and i < len(labels):
            d.text((x + 8, 6), labels[i], fill=(255, 255, 255), font=f)
        x += im.width
    sheet.save(out)
    return out


def contact_sheet(entries, out, cols=4, tile=(520, 420), title=None, view=(-35, 24)):
    """entries: list of (glb_path, label_lines, opts dict). Renders each GLB alone and tiles them."""
    setup_scene(tile, samples=24)
    tmp = tempfile.mkdtemp(prefix="kitc_")
    ims = []
    for i, (path, label, opts) in enumerate(entries):
        objs = import_glb(path)
        lo, hi = world_bbox(objs)
        ground = opts.get("ground", True)
        set_ground(ground)
        az, el = opts.get("view", view)
        frame_camera(lo, hi, az, el, above_ground=ground, margin=opts.get("margin", 1.05))
        p = os.path.join(tmp, f"c{i}.png")
        render_to(p)
        remove_objects(objs)
        im = Image.open(p).convert("RGB")
        d = ImageDraw.Draw(im)
        f = _font(15)
        y = 6
        for li, line in enumerate(label):
            fnt = _font(17) if li == 0 else f
            d.text((9, y + 1), line, fill=(0, 0, 0), font=fnt)
            d.text((8, y), line, fill=(255, 255, 255), font=fnt)
            y += 20 if li == 0 else 17
        ims.append(im)
    rows = (len(ims) + cols - 1) // cols
    top = 40 if title else 0
    sheet = Image.new("RGB", (tile[0] * cols, tile[1] * rows + top), (24, 26, 30))
    if title:
        ImageDraw.Draw(sheet).text((12, 8), title, fill=(240, 230, 210), font=_font(22))
    for k, im in enumerate(ims):
        sheet.paste(im, ((k % cols) * tile[0], top + (k // cols) * tile[1]))
    sheet.save(out)
    return out
