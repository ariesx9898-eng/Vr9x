"""LA PLACE kit: palette materials (MI_<Slot>) and the Blender kit meshes (nature + architecture) for the world.

    Tools/mac/run_editor_python.sh mt_setup_kit.py
Environment: MT_KIT_CATEGORIES=Trees,Rocks limits the mesh folders; MT_KIT_SKIP_MESHES=1 rebuilds materials only.

  /Game/LaPlace/Textures/Palette   T_<Name>_{D,N,M,E}   (SourceArt/Textures/Palette, Tools/textures)
  /Game/LaPlace/Kit/Materials      M_Kit_Surface, M_Kit_Foliage, M_Kit_Crystal masters; MI_<Slot> per palette slot
  /Game/LaPlace/Kit/<Category>     SM_* meshes: Nanite, collision (buildings / rocks: the mesh itself; trees: a trunk
                                   capsule; plants: none)
Palette colours come from Tools/kit/arch_palette.py; textures replace them when present.
"""
import json
import os
import runpy

import unreal

from mt_laplace_common import EAL, MEL, PROJECT, ROOT, TOOLS, Graph, configure_texture, import_file, import_glb_mesh, log, set_tag, warn

set_tag("mt_setup_kit")

WB = unreal.MTWorldBuildLibrary
MAT_DIR = ROOT + "/Kit/Materials"
TEX_DIR = ROOT + "/Textures/Palette"
TEX_SRC = os.path.join(PROJECT, "SourceArt", "Textures", "Palette")
FALLBACK = ROOT + "/Textures/Fallback"
PALETTE = runpy.run_path(os.path.join(PROJECT, "Tools", "kit", "arch_palette.py"))
CARD_SLOTS = {"MT_LeavesOak", "MT_LeavesBirch", "MT_NeedlesPine", "MT_NeedlesSnow", "MT_LeavesGiant", "MT_LeavesDemon",
              "MT_LeavesPalm", "MT_Grass", "MT_GrassDry", "MT_Wheat", "MT_Flowers", "MT_Fern", "MT_Reeds"}
NATURE_CATEGORIES = {"Trees", "Plants", "Rocks"}
SKIP_CATEGORIES = {"VFX"}


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def texture_manifest():
    path = os.path.join(PROJECT, "SourceArt", "Textures", "manifest.json")
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    rows = data.get("materials", data.get("entries", [])) if isinstance(data, dict) else data
    return {r["name"]: r for r in rows if isinstance(r, dict) and "name" in r}


# --------------------------------------------------------------------------------------------- textures

def import_palette_textures():
    if not os.path.isdir(TEX_SRC):
        warn("no palette textures yet (%s): materials use the palette colours" % TEX_SRC)
        return 0
    count = 0
    for fname in sorted(os.listdir(TEX_SRC)):
        if not fname.lower().endswith(".png"):
            continue
        name = os.path.splitext(fname)[0]
        tex = import_file(os.path.join(TEX_SRC, fname), TEX_DIR, name)
        if tex is None:
            continue
        if name.endswith("_N"):
            configure_texture(tex, "normal")
        elif name.endswith("_M") or name.endswith("_E"):
            configure_texture(tex, "mask")
        else:
            tex.set_editor_property("srgb", True)
            EAL.save_loaded_asset(tex)
        count += 1
    log("imported %d palette textures" % count)
    return count


# --------------------------------------------------------------------------------------------- materials

class KitGraph(Graph):
    def tex(self, name, path, sampler):
        n = self.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name=name)
        t = unreal.load_asset(path)
        if t:
            n.set_editor_property("texture", t)
        n.set_editor_property("sampler_type", sampler)
        return n


def surface_inputs(g):
    return {
        "UV": g.node(unreal.MaterialExpressionTextureCoordinate),
        "WP": g.node(unreal.MaterialExpressionWorldPosition),
        "D": g.tex("BaseColorTex", FALLBACK + "/T_Fallback_White", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        "N": g.tex("NormalTex", FALLBACK + "/T_Fallback_Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        "M": g.tex("MaskTex", FALLBACK + "/T_Fallback_Mask", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
        "Tint": g.vector("Tint", (1.0, 1.0, 1.0)),
        "Tiles": g.scalar("TilesPerMetre", 0.5),
        "RoughMin": g.scalar("RoughnessMin", 0.6),
        "RoughMax": g.scalar("RoughnessMax", 0.95),
        "NormalStrength": g.scalar("NormalStrength", 1.0),
        "Variation": g.scalar("WorldVariation", 0.12),
    }


def build_surface_master():
    """Opaque kit surfaces: palette textures tiled at TilesPerMetre (kit UVs are 1 unit = 1 m), tinted, with a little
    world-position variation so repeated buildings / rocks do not look stamped."""
    g = KitGraph("M_Kit_Surface", MAT_DIR)
    inputs = surface_inputs(g)
    inputs["Metallic"] = g.scalar("Metallic", 0.0)
    code = "\n".join([
        "float2 uv = UV * Tiles;",
        "float3 c = D.Sample(DSampler, uv).rgb * Tint.rgb;",
        "float4 m = M.Sample(MSampler, uv);",
        "float2 n = (N.Sample(NSampler, uv).rg * 2.0 - 1.0) * NormalStrength;",
        "float3 p = WP * 0.01;",
        "float v = frac(sin(dot(floor(p.xy / 3.0), float2(12.9898, 78.233))) * 43758.5453);",
        "float w = sin(p.x * 0.37 + p.y * 0.23) * sin(p.y * 0.31 - p.z * 0.19);",
        "c *= 1.0 + (v - 0.5) * Variation + w * Variation * 0.5;",
        "OutNormal = float3(n, sqrt(saturate(1.0 - dot(n, n))));",
        "OutRough = lerp(RoughMin, RoughMax, m.g);",
        "OutAO = m.r;",
        "return saturate(c);",
    ])
    node = g.custom(code, inputs, "float3")
    extra = []
    for name, kind in (("OutNormal", unreal.CustomMaterialOutputType.CMOT_FLOAT3), ("OutRough", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                       ("OutAO", unreal.CustomMaterialOutputType.CMOT_FLOAT1)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", kind)
        extra.append(o)
    node.set_editor_property("additional_outputs", extra)
    MEL.recompile_material(g.mat)
    g.out(node, unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "OutNormal", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(node, "OutRough", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(node, "OutAO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    g.out(inputs["Metallic"], unreal.MaterialProperty.MP_METALLIC)
    g.mat.set_editor_property("used_with_instanced_static_meshes", True)
    g.mat.set_editor_property("used_with_nanite", True)
    g.finish()


def build_foliage_master():
    """Alpha-masked, two-sided foliage cards (leaves, needles, grass): foliage shading with light through the leaves
    and a gentle wind sway that grows with height above the mesh pivot."""
    g = KitGraph("M_Kit_Foliage", MAT_DIR)
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    g.mat.set_editor_property("two_sided", True)
    g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    inputs = {
        "UV": g.node(unreal.MaterialExpressionTextureCoordinate),
        "D": g.tex("BaseColorTex", FALLBACK + "/T_Fallback_White", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        "Tint": g.vector("Tint", (1.0, 1.0, 1.0)),
        "Translucency": g.vector("Translucency", (0.35, 0.45, 0.12)),
        "Tiles": g.scalar("TilesPerMetre", 1.0),
    }
    code = "\n".join([
        "float4 d = D.Sample(DSampler, UV * Tiles);",
        "OutMask = d.a;",
        "OutSSS = Translucency.rgb * d.rgb * 2.0;",
        "return saturate(d.rgb * Tint.rgb);",
    ])
    node = g.custom(code, inputs, "float3")
    extra = []
    for name, kind in (("OutMask", unreal.CustomMaterialOutputType.CMOT_FLOAT1), ("OutSSS", unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        o = unreal.CustomOutput()
        o.set_editor_property("output_name", name)
        o.set_editor_property("output_type", kind)
        extra.append(o)
    node.set_editor_property("additional_outputs", extra)
    MEL.recompile_material(g.mat)
    g.out(node, unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "OutMask", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.connect_material_property(node, "OutSSS", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    g.out(g.scalar("Roughness", 0.75), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.3), unreal.MaterialProperty.MP_SPECULAR)
    # Wind: sway in X/Y by height above the pivot, phase from the object position (each tree moves on its own).
    wind_inputs = {
        "WP": g.node(unreal.MaterialExpressionWorldPosition),
        "OP": g.node(unreal.MaterialExpressionObjectPositionWS),
        "Time": g.node(unreal.MaterialExpressionTime),
        "Wind": g.scalar("WindStrength", 1.0),
    }
    wind = "\n".join([
        "float h = max(WP.z - OP.z, 0.0);",
        "float phase = dot(OP.xy, float2(0.0013, 0.0017));",
        "float s = sin(Time * 1.3 + phase) * 0.6 + sin(Time * 2.9 + phase * 1.7 + WP.x * 0.002) * 0.4;",
        "float amt = Wind * h * 0.012;",
        "return float3(s * amt, s * amt * 0.6, 0.0);",
    ])
    wnode = g.custom(wind, wind_inputs, "float3")
    g.out(wnode, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    g.mat.set_editor_property("used_with_instanced_static_meshes", True)
    g.mat.set_editor_property("used_with_nanite", True)
    try:
        g.mat.set_editor_property("max_world_position_offset_displacement", 40.0)
    except Exception:  # noqa: BLE001 - property name differs between versions
        pass
    g.finish()


def build_crystal_master():
    g = KitGraph("M_Kit_Crystal", MAT_DIR)
    inputs = {
        "Tint": g.vector("Tint", (0.4, 0.72, 1.0)),
        "Glow": g.scalar("Glow", 6.0),
        "Fres": g.node(unreal.MaterialExpressionFresnel),
        "Time": g.node(unreal.MaterialExpressionTime),
    }
    code = "OutEmissive = Tint.rgb * Glow * (0.35 + Fres * 0.9) * (0.85 + 0.15 * sin(Time * 1.7)); return Tint.rgb * 0.3;"
    node = g.custom(code, inputs, "float3")
    o = unreal.CustomOutput()
    o.set_editor_property("output_name", "OutEmissive")
    o.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    node.set_editor_property("additional_outputs", [o])
    MEL.recompile_material(g.mat)
    g.out(node, unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "OutEmissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.const(0.15), unreal.MaterialProperty.MP_ROUGHNESS)
    g.mat.set_editor_property("used_with_instanced_static_meshes", True)
    g.mat.set_editor_property("used_with_nanite", True)
    g.finish()


def make_instance(slot, parent, params_scalar, params_vector, params_texture):
    name = "MI_" + slot
    path = "%s/%s" % (MAT_DIR, name)
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = TOOLS.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.clear_all_material_instance_parameters(mi)
    for k, v in params_scalar.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
    for k, v in params_vector.items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in params_texture.items():
        tex = unreal.load_asset(v)
        if tex:
            MEL.set_material_instance_texture_parameter_value(mi, k, tex)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi)
    return mi


def build_instances():
    manifest = texture_manifest()
    surface = unreal.load_asset(MAT_DIR + "/M_Kit_Surface")
    foliage = unreal.load_asset(MAT_DIR + "/M_Kit_Foliage")
    crystal = unreal.load_asset(MAT_DIR + "/M_Kit_Crystal")
    made = 0
    for slot, (srgb, rough, metal, emit) in dict(PALETTE["ARCH"], **PALETTE["NATURE"]).items():
        short = slot[3:]
        row = manifest.get(short, {})
        base = "%s/T_%s" % (TEX_DIR, short)
        has_d = EAL.does_asset_exist(base + "_D")
        linear = tuple(srgb_to_linear(c) for c in srgb)
        textures = {}
        if has_d:
            textures["BaseColorTex"] = base + "_D"
        if EAL.does_asset_exist(base + "_N"):
            textures["NormalTex"] = base + "_N"
        if EAL.does_asset_exist(base + "_M"):
            textures["MaskTex"] = base + "_M"
        tint = (1.0, 1.0, 1.0) if has_d else linear
        tile_m = float(row.get("tile_m", 1.0 if slot in CARD_SLOTS else 2.0))
        if slot == "MT_Crystal":
            make_instance(slot, crystal, {"Glow": 6.0}, {"Tint": linear}, {})
        elif slot in CARD_SLOTS:
            translucency = tuple(min(1.0, c * 0.8) for c in linear)
            make_instance(slot, foliage, {"TilesPerMetre": 1.0 / tile_m if not has_d else 1.0,
                                          "WindStrength": 0.6 if slot in ("MT_LeavesGiant", "MT_LeavesPalm") else 1.0},
                          {"Tint": tint, "Translucency": translucency}, textures)
        else:
            rr = row.get("roughness_range", [max(0.05, rough - 0.2), min(1.0, rough + 0.08)])
            metallic = float(row.get("metallic", 1.0 if metal >= 0.5 else 0.0))
            make_instance(slot, surface, {"TilesPerMetre": 1.0 / tile_m, "RoughnessMin": rr[0], "RoughnessMax": rr[1],
                                          "Metallic": metallic, "NormalStrength": float(row.get("normal_strength", 1.0)),
                                          "WorldVariation": 0.0 if metallic > 0.5 else 0.12},
                          {"Tint": tint}, textures)
        made += 1
    log("%d palette material instances" % made)


# --------------------------------------------------------------------------------------------- meshes

def slot_material(slot):
    path = "%s/MI_%s" % (MAT_DIR, slot)
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else None


def kit_manifest():
    out = {}
    for fname in ("manifest_nature.json", "manifest_architecture.json", "manifest.json"):
        path = os.path.join(PROJECT, "SourceArt", "Kit", fname)
        if not os.path.exists(path):
            continue
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        for row in data["assets"] if isinstance(data, dict) else data:
            out.setdefault(row["name"], row)
    return out


def configure_mesh(mesh, row, category):
    kind = (row or {}).get("kind", "")
    foliage = category in ("Trees", "Plants")
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    if foliage:
        nanite.set_editor_property("preserve_area", True)
    mesh.set_editor_property("nanite_settings", nanite)
    if category == "Trees" and kind != "dead_log":
        fp = (row or {}).get("footprint", [6.0, 6.0])
        height = (row or {}).get("height", 10.0)
        radius = max(20.0, 0.04 * 50.0 * (fp[0] + fp[1]))
        WB.set_trunk_collision(mesh, radius, max(radius * 2.5, min(600.0, 0.4 * height * 100.0)))
    elif category == "Plants":
        WB.set_complex_collision(mesh, False)
    else:
        WB.set_complex_collision(mesh, True)


def import_meshes(categories):
    base = os.path.join(PROJECT, "SourceArt", "Kit")
    rows = kit_manifest()
    total = 0
    for category in sorted(os.listdir(base)):
        folder = os.path.join(base, category)
        if not os.path.isdir(folder) or category in SKIP_CATEGORIES or (categories and category not in categories):
            continue
        dest = "%s/Kit/%s" % (ROOT, category)
        count = 0
        for fname in sorted(os.listdir(folder)):
            if not fname.lower().endswith(".glb"):
                continue
            name = os.path.splitext(fname)[0]
            mesh = import_glb_mesh(os.path.join(folder, fname), dest, name, slot_material)
            if mesh is None:
                warn("import failed: %s/%s" % (category, fname))
                continue
            configure_mesh(mesh, rows.get(name), category)
            EAL.save_loaded_asset(mesh)
            count += 1
        log("imported %d meshes into %s" % (count, dest))
        total += count
    return total


def main():
    categories = [c for c in (os.environ.get("MT_KIT_CATEGORIES") or "").split(",") if c]
    import_palette_textures()
    build_surface_master()
    build_foliage_master()
    build_crystal_master()
    build_instances()
    if os.environ.get("MT_KIT_SKIP_MESHES") != "1":
        import_meshes(categories)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("done")


main()
