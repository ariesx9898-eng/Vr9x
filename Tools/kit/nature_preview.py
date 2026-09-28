"""Contact-sheet renderer for the nature kit (Cycles CPU, headless).

Each exported GLB is imported back into an empty stage (so the preview shows exactly what is in
the file), its slots are swapped for preview shaders (procedural bark / rock shading, alpha-card
textures from nature_textures.py) and it is rendered from a three-quarter view. Tiles are
assembled into SourceArt/Kit/<Category>/_Preview.png with name, size and triangle count.
"""
import math
import os
import re
import tempfile

import bpy
from mathutils import Vector
from PIL import Image, ImageDraw, ImageFont

import nature_lib as L
import nature_textures as TX

TEX_CACHE = os.path.join(tempfile.gettempdir(), "laplace_nature_preview_tex_v3")

STYLE_GROUND = {
    "Temperate": (0.33, 0.42, 0.20), "Farmland": (0.42, 0.40, 0.22), "Northern": (0.86, 0.89, 0.93),
    "Desert": (0.80, 0.66, 0.47), "Demon": (0.34, 0.19, 0.15), "GreatForest": (0.24, 0.31, 0.15),
    "Jungle": (0.24, 0.34, 0.15), "Heaven": (0.76, 0.76, 0.75), "Mountain": (0.46, 0.45, 0.41),
    "Wetland": (0.30, 0.36, 0.20), "Barren": (0.50, 0.42, 0.32),
}


def _font(size):
    for p in ("/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc",
              "/Library/Fonts/Arial.ttf"):
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()


# --------------------------------------------------------------------------------------------
# Preview shaders
# --------------------------------------------------------------------------------------------
def _ramp(nt, fac_socket, stops):
    r = nt.nodes.new("ShaderNodeValToRGB")
    els = r.color_ramp.elements
    while len(els) > 1:
        els.remove(els[-1])
    els[0].position = stops[0][0]
    els[0].color = (*stops[0][1], 1.0)
    for pos, col in stops[1:]:
        e = els.new(pos)
        e.color = (*col, 1.0)
    nt.links.new(fac_socket, r.inputs["Fac"])
    return r


def _math(nt, op, a, b):
    m = nt.nodes.new("ShaderNodeMath")
    m.operation = op
    for i, x in enumerate((a, b)):
        if isinstance(x, (int, float)):
            m.inputs[i].default_value = x
        else:
            nt.links.new(x, m.inputs[i])
    return m.outputs[0]


def preview_material(slot, tex_paths):
    name = "PV_" + slot
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    col, rough, is_card = L.NATURE_MATERIALS.get(slot, ((0.5, 0.5, 0.5), 0.8, False))
    lin = L.srgb_to_linear(col)
    m = bpy.data.materials.new(name)
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Base Color"].default_value = (*lin, 1.0)

    def scale(c, k):
        return tuple(min(1.0, x * k) for x in c)

    if slot in tex_paths:
        img = bpy.data.images.load(tex_paths[slot], check_existing=True)
        tx = nt.nodes.new("ShaderNodeTexImage")
        tx.image = img
        tx.interpolation = "Linear"
        nt.links.new(tx.outputs["Color"], bsdf.inputs["Base Color"])
        trans = nt.nodes.new("ShaderNodeBsdfTranslucent")
        nt.links.new(tx.outputs["Color"], trans.inputs["Color"])
        mix = nt.nodes.new("ShaderNodeMixShader")
        mix.inputs["Fac"].default_value = 0.22
        nt.links.new(bsdf.outputs[0], mix.inputs[1])
        nt.links.new(trans.outputs[0], mix.inputs[2])
        transparent = nt.nodes.new("ShaderNodeBsdfTransparent")
        amix = nt.nodes.new("ShaderNodeMixShader")
        # hard alpha test like the game's masked materials
        clip = _math(nt, "GREATER_THAN", tx.outputs["Alpha"], 0.5)
        nt.links.new(clip, amix.inputs["Fac"])
        nt.links.new(transparent.outputs[0], amix.inputs[1])
        nt.links.new(mix.outputs[0], amix.inputs[2])
        nt.links.new(amix.outputs[0], out.inputs["Surface"])
        return m

    tc = nt.nodes.new("ShaderNodeTexCoord")
    if slot.startswith("MT_Bark"):
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (1.0, 0.35, 1.0)
        nt.links.new(tc.outputs["UV"], mp.inputs["Vector"])
        wave = nt.nodes.new("ShaderNodeTexWave")
        wave.wave_type = "BANDS"
        wave.bands_direction = "X"
        wave.inputs["Scale"].default_value = 5.0 if slot != "MT_BarkBirch" else 0.5
        wave.inputs["Distortion"].default_value = 6.0
        wave.inputs["Detail"].default_value = 3.0
        nt.links.new(mp.outputs[0], wave.inputs["Vector"])
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 3.0
        nz.inputs["Detail"].default_value = 6.0
        nt.links.new(tc.outputs["UV"], nz.inputs["Vector"])
        if slot == "MT_BarkBirch":
            # white bark with dark horizontal lenticels
            mp2 = nt.nodes.new("ShaderNodeMapping")
            mp2.inputs["Scale"].default_value = (2.0, 14.0, 1.0)
            nt.links.new(tc.outputs["UV"], mp2.inputs["Vector"])
            nz2 = nt.nodes.new("ShaderNodeTexNoise")
            nz2.inputs["Scale"].default_value = 2.0
            nz2.inputs["Detail"].default_value = 2.0
            nt.links.new(mp2.outputs[0], nz2.inputs["Vector"])
            r = _ramp(nt, nz2.outputs["Fac"], [(0.0, scale(lin, 1.0)), (0.62, scale(lin, 0.95)),
                                                 (0.66, (0.05, 0.05, 0.05)), (1.0, (0.03, 0.03, 0.03))])
            nt.links.new(r.outputs[0], bsdf.inputs["Base Color"])
        else:
            fac = _math(nt, "ADD", _math(nt, "MULTIPLY", wave.outputs["Fac"], 0.55),
                        _math(nt, "MULTIPLY", nz.outputs["Fac"], 0.45))
            r = _ramp(nt, fac, [(0.15, scale(lin, 0.45)), (0.5, lin), (0.85, scale(lin, 1.35))])
            nt.links.new(r.outputs[0], bsdf.inputs["Base Color"])
            bump = nt.nodes.new("ShaderNodeBump")
            bump.inputs["Strength"].default_value = 0.35
            nt.links.new(fac, bump.inputs["Height"])
            nt.links.new(bump.outputs[0], bsdf.inputs["Normal"])
    else:
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 0.9 if slot != "MT_Snow" else 0.6
        nz.inputs["Detail"].default_value = 8.0
        nt.links.new(tc.outputs["Object"], nz.inputs["Vector"])
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        # painterly edge light / cavity dark from pointiness
        pt = _math(nt, "MULTIPLY", _math(nt, "SUBTRACT", geo.outputs["Pointiness"], 0.5), 2.2)
        fac = _math(nt, "ADD", nz.outputs["Fac"], pt)
        if slot == "MT_Snow":
            stops = [(0.2, scale(lin, 0.86)), (0.6, lin), (0.9, (1.0, 1.0, 1.0))]
        elif slot == "MT_RockDemon":
            stops = [(0.2, scale(lin, 0.6)), (0.55, lin), (0.9, (0.36, 0.14, 0.12))]
        else:
            stops = [(0.2, scale(lin, 0.62)), (0.55, lin), (0.9, scale(lin, 1.28))]
        r = _ramp(nt, fac, stops)
        nt.links.new(r.outputs[0], bsdf.inputs["Base Color"])
        bump = nt.nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.25 if slot != "MT_Snow" else 0.08
        nt.links.new(nz.outputs["Fac"], bump.inputs["Height"])
        nt.links.new(bump.outputs[0], bsdf.inputs["Normal"])
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    return m


# --------------------------------------------------------------------------------------------
# Stage
# --------------------------------------------------------------------------------------------
def setup_stage(samples=24):
    L.reset_scene()
    import addon_utils
    addon_utils.enable("cycles", default_set=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    sc.cycles.use_adaptive_sampling = True
    sc.cycles.use_denoising = True
    try:
        sc.cycles.denoiser = "OPENIMAGEDENOISE"
    except Exception:
        pass
    sc.cycles.max_bounces = 4
    sc.cycles.diffuse_bounces = 2
    sc.cycles.glossy_bounces = 1
    sc.cycles.transmission_bounces = 2
    sc.cycles.transparent_max_bounces = 48
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    try:
        sc.view_settings.look = "AgX - Medium High Contrast"
    except Exception:
        pass
    world = bpy.data.worlds.new("PV_World")
    sc.world = world
    nt = world.node_tree
    bg = next(n for n in nt.nodes if n.bl_idname == "ShaderNodeBackground")
    bg.inputs["Color"].default_value = (0.50, 0.62, 0.80, 1.0)
    bg.inputs["Strength"].default_value = 0.85
    sun = bpy.data.objects.new("PV_Sun", bpy.data.lights.new("PV_Sun", "SUN"))
    sun.data.energy = 3.6
    sun.data.color = (1.0, 0.93, 0.82)
    sun.data.angle = math.radians(3.0)
    sun.rotation_euler = (math.radians(48), 0.0, math.radians(-38))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new("PV_Cam", bpy.data.cameras.new("PV_Cam"))
    cam.data.lens = 50
    cam.data.clip_start = 0.05
    cam.data.clip_end = 5000
    sc.collection.objects.link(cam)
    sc.camera = cam
    me = bpy.data.meshes.new("PV_Ground")
    me.from_pydata([(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)], [], [(0, 1, 2, 3)])
    ground = bpy.data.objects.new("PV_Ground", me)
    sc.collection.objects.link(ground)
    gm = bpy.data.materials.new("PV_GroundMat")
    gnt = gm.node_tree
    gb = next(n for n in gnt.nodes if n.bl_idname == "ShaderNodeBsdfPrincipled")
    gb.inputs["Roughness"].default_value = 1.0
    me.materials.append(gm)
    return {"scene": sc, "cam": cam, "ground": ground, "ground_bsdf": gb, "sun": sun}


def _fit_camera(cam, lo, hi, az_deg, el_deg, margin=1.08):
    ctr = (lo + hi) * 0.5
    az, el = math.radians(az_deg), math.radians(el_deg)
    d = Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
    fwd = -d
    right = fwd.cross(Vector((0, 0, 1))).normalized()
    up = right.cross(fwd).normalized()
    tan_half = math.tan(cam.data.angle / 2)
    need = 0.0
    for x in (lo.x, hi.x):
        for y in (lo.y, hi.y):
            for z in (lo.z, hi.z):
                p = Vector((x, y, z)) - ctr
                zc = p.dot(d)  # toward camera
                need = max(need, zc + abs(p.dot(right)) / tan_half, zc + abs(p.dot(up)) / tan_half)
    dist = need * margin
    cam.location = ctr + d * dist
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_end = dist * 10 + 100
    return dist


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    return new, meshes


def clear_imported(objs):
    mats = set()
    mes = set()
    for o in objs:
        if o.type == "MESH":
            mes.add(o.data)
            for s in o.material_slots:
                if s.material is not None and not s.material.name.startswith("PV_"):
                    mats.add(s.material)
        bpy.data.objects.remove(o, do_unlink=True)
    for me in mes:
        if me.users == 0:
            bpy.data.meshes.remove(me)
    for m in bpy.data.materials:
        if not m.name.startswith("PV_") and m.users == 0:
            bpy.data.materials.remove(m)


def render_asset(stage, glb, out_png, style="Temperate", size=420, views=((-35, 12),), tex_paths=None):
    """Render one or more views of a GLB; returns list of PIL images."""
    tex_paths = tex_paths or TX.ensure_textures(TEX_CACHE)
    sc = stage["scene"]
    objs, meshes = import_glb(glb)
    for ob in meshes:
        for s in ob.material_slots:
            if s.material is None:
                continue
            base = re.sub(r"\.\d+$", "", s.material.name)
            s.material = preview_material(base, tex_paths)
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for ob in meshes:
        for c in ob.bound_box:
            w = ob.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    vis_lo = Vector((lo.x, lo.y, max(lo.z, 0.0)))
    ext = max(hi.x - lo.x, hi.y - lo.y, hi.z - vis_lo.z)
    g = stage["ground"]
    g.scale = (ext * 6 + 4, ext * 6 + 4, 1)
    gcol = STYLE_GROUND.get(style, STYLE_GROUND["Temperate"])
    stage["ground_bsdf"].inputs["Base Color"].default_value = (*L.srgb_to_linear(gcol), 1.0)
    sc.render.resolution_x = size
    sc.render.resolution_y = size
    images = []
    for i, (az, el) in enumerate(views):
        _fit_camera(stage["cam"], vis_lo, hi, az, el)
        path = out_png if len(views) == 1 else out_png.replace(".png", "_v%d.png" % i)
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        images.append(Image.open(path).convert("RGB"))
    clear_imported(objs)
    return images


def contact_sheet(tiles, out_path, title, cols=6):
    """tiles: [(PIL image, [label lines])]."""
    tw = max(t[0].width for t in tiles)
    th = max(t[0].height for t in tiles)
    lab = 46
    rows = (len(tiles) + cols - 1) // cols
    head = 44
    sheet = Image.new("RGB", (tw * cols, head + rows * (th + lab)), (24, 26, 30))
    d = ImageDraw.Draw(sheet)
    d.text((12, 8), title, fill=(240, 230, 205), font=_font(26))
    f1, f2 = _font(15), _font(13)
    for k, (im, lines) in enumerate(tiles):
        x = (k % cols) * tw
        y = head + (k // cols) * (th + lab)
        sheet.paste(im, (x, y))
        d.text((x + 6, y + th + 4), lines[0], fill=(242, 236, 220), font=f1)
        if len(lines) > 1:
            d.text((x + 6, y + th + 24), lines[1], fill=(170, 176, 186), font=f2)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    sheet.save(out_path, optimize=True)
    return sheet


def render_lineup(panels, out_path, width=2000, height=640, samples=24):
    """Side-by-side true-scale lineups (one panel per group), labelled under each asset.
    panels: [(title, [glb paths], camera distance m, camera pitch deg)]."""
    import math as _m
    from bpy_extras.object_utils import world_to_camera_view
    tex_paths = TX.ensure_textures(TEX_CACHE)
    images = []
    for title, paths, dist, pitch in panels:
        st = setup_stage(samples)
        sc = st["scene"]
        x = 0.0
        placed = []
        for path in paths:
            objs, meshes = import_glb(path)
            for ob in meshes:
                for sl in ob.material_slots:
                    if sl.material is not None:
                        sl.material = preview_material(re.sub(r"\.\d+$", "", sl.material.name), tex_paths)
            w = max(max(ob.dimensions.x, ob.dimensions.y) for ob in meshes)
            for ob in meshes:
                ob.location.x = x + w / 2
            placed.append((os.path.splitext(os.path.basename(path))[0], x + w / 2))
            x += w + 2.0
        for ob in bpy.data.objects:
            if ob.type == "MESH" and not ob.name.startswith("PV_"):
                ob.location.x -= x / 2
        placed = [(n, px - x / 2) for n, px in placed]
        dist = max(dist, 0.56 * x)          # 18 mm lens = 90 deg horizontal FOV: fit the whole row
        title = title.format(d=dist)
        st["ground"].scale = (x * 2 + 400, 600, 1)
        st["ground_bsdf"].inputs["Base Color"].default_value = (0.09, 0.16, 0.05, 1.0)
        cam = st["cam"]
        cam.data.lens = 18
        cam.data.sensor_fit = "HORIZONTAL"
        cam.location = (0.0, -dist, 1.7)
        cam.rotation_euler = (_m.radians(90 + pitch), 0.0, 0.0)
        sc.render.resolution_x, sc.render.resolution_y = width, height
        tmp = os.path.join(tempfile.gettempdir(), "laplace_lineup_%d.png" % len(images))
        sc.render.filepath = tmp
        bpy.ops.render.render(write_still=True)
        im = Image.open(tmp).convert("RGB")
        d = ImageDraw.Draw(im)
        f = _font(15)
        d.text((12, 10), title, fill=(30, 34, 40), font=_font(20))
        for k, (n, px) in enumerate(placed):
            co = world_to_camera_view(sc, cam, Vector((px, 0.0, 0.0)))
            u, v = co.x * width, (1 - co.y) * height
            label = n.replace("SM_Tree_", "")
            tw = d.textlength(label, font=f)
            d.text((u - tw / 2, v + 8 + 20 * (k % 2)), label, fill=(245, 242, 230), font=f)   # staggered rows
        images.append(im)
    sheet = Image.new("RGB", (width, sum(i.height for i in images)), (0, 0, 0))
    y = 0
    for im in images:
        sheet.paste(im, (0, y))
        y += im.height
    sheet.save(out_path, optimize=True)
    return out_path
