"""LA PLACE editor setup: imports generated art / audio / kit meshes and builds the runtime effect materials.

Run headless (Tools/mac/build_and_setup.sh does this) or in the editor:  py mt_setup_laplace.py
Idempotent: re-running replaces the assets it owns under /Game/LaPlace.

  /Game/LaPlace/VFX/Textures   T_VFX_*          from SourceArt/VFX/Textures (Tools/vfx/make_vfx_textures.py)
  /Game/LaPlace/VFX/Materials  M_VFX_*, M_Decal_*  runtime spell effects (Source/.../VFX/MTVFXLibrary.cpp)
  /Game/LaPlace/UI/...         icons, title / location art, frame ornaments (Tools/ui/prepare_ui_art.py)
  /Game/LaPlace/Audio/...      sounds and music (Tools/audio/synth_sfx.py), when present
  /Game/LaPlace/Kit/...        Blender kit meshes (Tools/kit/*.py), when present
"""
import json
import os

import unreal

from mt_laplace_common import (EAL, MEL, PROJECT, ROOT, TOOLS, Graph, configure_texture, import_file, import_folder,
                               import_glb_mesh, log, set_tag, warn)

set_tag("mt_setup_laplace")


def common_inputs(g):
    """Time, texture coordinates, the effect parameters and the per-instance colour (particles)."""
    return {
        "Time": g.node(unreal.MaterialExpressionTime),
        "UV": g.node(unreal.MaterialExpressionTextureCoordinate),
        "Color": g.vector("Color", (1.0, 1.0, 1.0)),
        "Intensity": g.scalar("Intensity", 4.0),
        "Opacity": g.scalar("Opacity", 1.0),
        "CDR": g.custom_data(0),
        "CDG": g.custom_data(1),
        "CDB": g.custom_data(2),
        "CDA": g.custom_data(3),
    }


FX_DIR = ROOT + "/VFX/Materials"
TEX = ROOT + "/VFX/Textures/"


def build_glow():
    """Additive energy (fire cores, rings, pillars): noise-broken, optional fresnel rim."""
    g = Graph("M_VFX_Glow", FX_DIR)
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    i = common_inputs(g)
    i["Noise"] = g.texture_object("NoiseTex", TEX + "T_VFX_Noise.T_VFX_Noise")
    i["FresnelMix"] = g.scalar("FresnelMix", 0.0)
    i["NoiseAmount"] = g.scalar("NoiseAmount", 0.3)
    i["Fres"] = g.node(unreal.MaterialExpressionFresnel, exponent=2.5)
    i["Fade"] = g.node(unreal.MaterialExpressionDepthFade, fade_distance_default=40.0)
    i["UVScaleX"] = g.scalar("UVScaleX", 1.7)
    i["UVScaleY"] = g.scalar("UVScaleY", 1.7)
    i["PanX"] = g.scalar("PanX", 0.13)
    i["PanY"] = g.scalar("PanY", 0.41)
    i["NoisePower"] = g.scalar("NoisePower", 1.0)
    code = """
float2 s = float2(UVScaleX, UVScaleY);
float2 a = UV * s + Time * float2(PanX, PanY);
float2 b = UV * s * 0.53 - Time * float2(PanX * 1.7, PanY * 0.22);
float n = saturate(Texture2DSample(Noise, NoiseSampler, a).r * 0.8 + Texture2DSample(Noise, NoiseSampler, b).b * 0.7);
n = pow(n, NoisePower) * lerp(1.0, 1.9, saturate(NoisePower - 1.0));
float rim = lerp(1.0, saturate(1.0 - Fres) * 0.35 + saturate(Fres) * 1.4, FresnelMix);
float m = rim * lerp(1.0, n * 1.7, NoiseAmount);
return Color * float3(CDR, CDG, CDB) * Intensity * Opacity * CDA * m * Fade;
"""
    e = g.custom(code, i)
    g.out(e, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def build_sprite():
    """Additive camera-facing sprites (sparks, embers, motes, flames)."""
    g = Graph("M_VFX_Sprite", FX_DIR)
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    i = common_inputs(g)
    i["Tex"] = g.texture_object("Texture", TEX + "T_VFX_Dot.T_VFX_Dot")
    i["Fade"] = g.node(unreal.MaterialExpressionDepthFade, fade_distance_default=30.0)
    code = """
float4 t = Texture2DSample(Tex, TexSampler, UV);
return Color * float3(CDR, CDG, CDB) * t.rgb * t.a * Intensity * Opacity * CDA * Fade;
"""
    g.out(g.custom(code, i), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def build_smoke():
    """Lit soft sprites (smoke, dust, mist, steam, mud bubbles)."""
    g = Graph("M_VFX_Smoke", FX_DIR)
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    i = common_inputs(g)
    i["Tex"] = g.texture_object("Texture", TEX + "T_VFX_Puff.T_VFX_Puff")
    base = g.custom("return Color * float3(CDR, CDG, CDB) * Texture2DSample(Tex, TexSampler, UV).rgb;",
                    {k: i[k] for k in ("Color", "CDR", "CDG", "CDB", "Tex", "UV")})
    fade = g.node(unreal.MaterialExpressionDepthFade, fade_distance_default=60.0)
    op = g.custom("return saturate(Texture2DSample(Tex, TexSampler, UV).a * Opacity * CDA) * Fade;",
                  {"Tex": i["Tex"], "UV": i["UV"], "Opacity": i["Opacity"], "CDA": i["CDA"], "Fade": fade}, "float")
    g.out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(op, unreal.MaterialProperty.MP_OPACITY)
    g.out(g.const(1.0), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.0), unreal.MaterialProperty.MP_SPECULAR)
    g.finish()


def build_water():
    """Clear water bodies (water bullet, dragon, flood): refraction, fresnel opacity, foam rim."""
    g = Graph("M_VFX_Water", FX_DIR)
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("refraction_method", unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    i = common_inputs(g)
    wn = g.texture_object("WaterNormal", TEX + "T_VFX_WaterN.T_VFX_WaterN")
    normal = g.custom("""
float3 a = Texture2DSample(WN, WNSampler, UV * 2.0 + Time * float2(0.35, 0.12)).xyz * 2.0 - 1.0;
float3 b = Texture2DSample(WN, WNSampler, UV * 1.3 - Time * float2(0.18, 0.31)).xyz * 2.0 - 1.0;
return normalize(float3(a.xy + b.xy, a.z * b.z));
""", {"WN": wn, "UV": i["UV"], "Time": i["Time"]})
    fres = g.node(unreal.MaterialExpressionFresnel, exponent=3.0)
    base = g.custom("return Color * float3(CDR, CDG, CDB);", {k: i[k] for k in ("Color", "CDR", "CDG", "CDB")})
    op = g.custom("return saturate(lerp(0.18, 0.7, Fres) * Opacity * CDA);",
                  {"Fres": fres, "Opacity": i["Opacity"], "CDA": i["CDA"]}, "float")
    foam = g.custom("return (pow(saturate(Fres), 3.0) * 0.22 + 0.015) * Opacity * CDA * float3(0.8, 0.92, 1.0);",
                    {"Fres": fres, "Opacity": i["Opacity"], "CDA": i["CDA"]})
    g.out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(op, unreal.MaterialProperty.MP_OPACITY)
    g.out(normal, unreal.MaterialProperty.MP_NORMAL)
    g.out(foam, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.const(0.04), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.9), unreal.MaterialProperty.MP_SPECULAR)
    g.out(g.const(1.25), unreal.MaterialProperty.MP_REFRACTION)
    g.finish()


def build_air():
    """Compressed air / heat haze: screen distortion with a faint bright edge (wind blades, shockwaves)."""
    g = Graph("M_VFX_Air", FX_DIR)
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("refraction_method", unreal.RefractionMode.RM_PIXEL_NORMAL_OFFSET)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    i = common_inputs(g)
    wn = g.texture_object("DistortNormal", TEX + "T_VFX_WaterN.T_VFX_WaterN")
    strength = g.scalar("Distortion", 1.0)
    fres = g.node(unreal.MaterialExpressionFresnel, exponent=2.0)
    normal = g.custom("""
float3 a = Texture2DSample(WN, WNSampler, UV * 1.5 + Time * float2(0.9, 0.2)).xyz * 2.0 - 1.0;
return normalize(float3(a.xy * 2.0, 1.0));
""", {"WN": wn, "UV": i["UV"], "Time": i["Time"]})
    refr = g.custom("return 1.0 + Distortion * 0.6 * Opacity * CDA * (0.35 + 0.65 * (1.0 - saturate(Fres)));",
                    {"Distortion": strength, "Opacity": i["Opacity"], "CDA": i["CDA"], "Fres": fres}, "float")
    edge = g.custom("return Color * pow(saturate(Fres), 3.0) * 0.35 * Opacity * CDA;",
                    {"Color": i["Color"], "Fres": fres, "Opacity": i["Opacity"], "CDA": i["CDA"]})
    op = g.custom("return 0.02 * Opacity * CDA;", {"Opacity": i["Opacity"], "CDA": i["CDA"]}, "float")
    g.out(normal, unreal.MaterialProperty.MP_NORMAL)
    g.out(refr, unreal.MaterialProperty.MP_REFRACTION)
    g.out(edge, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(op, unreal.MaterialProperty.MP_OPACITY)
    g.finish()


def build_rock():
    """Conjured stone (bullets, cannon shells, spikes, debris), with optional glowing mana seams."""
    g = Graph("M_VFX_Rock", FX_DIR)
    m = g.mat
    m.set_editor_property("used_with_instanced_static_meshes", True)
    i = common_inputs(g)
    i["Noise"] = g.texture_object("NoiseTex", TEX + "T_VFX_Noise.T_VFX_Noise")
    i["RockColor"] = g.vector("RockColor", (0.3, 0.25, 0.2))
    i["Glow"] = g.scalar("Glow", 0.0)
    i["WP"] = g.node(unreal.MaterialExpressionWorldPosition)
    base = g.custom("""
float n = Texture2DSample(Noise, NoiseSampler, UV * 2.0).r;
float s = Texture2DSample(Noise, NoiseSampler, UV * 6.0).a;
return RockColor * float3(CDR, CDG, CDB) * lerp(0.55, 1.25, n) * lerp(0.85, 1.1, s);
""", {k: i[k] for k in ("Noise", "UV", "RockColor", "CDR", "CDG", "CDB")})
    glow = g.custom("""
float w = Texture2DSample(Noise, NoiseSampler, UV * 1.5).g;
float seam = pow(saturate(w), 10.0);
return Color * Glow * seam * (0.75 + 0.25 * sin(Time * 9.0));
""", {k: i[k] for k in ("Noise", "UV", "Color", "Glow", "Time")})
    g.out(base, unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.const(0.85), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()


def build_ghost():
    """Dragon Step afterimages: additive rim-lit silhouette."""
    g = Graph("M_VFX_Ghost", FX_DIR)
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("used_with_skeletal_mesh", True)
    color = g.vector("Color", (0.8, 0.85, 1.0))
    intensity = g.scalar("Intensity", 2.0)
    opacity = g.scalar("Opacity", 1.0)
    fres = g.node(unreal.MaterialExpressionFresnel, exponent=2.0)
    e = g.custom("return Color * Intensity * Opacity * (pow(saturate(Fres), 1.5) * 0.9 + 0.08);",
                 {"Color": color, "Intensity": intensity, "Opacity": opacity, "Fres": fres})
    g.out(e, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def decal_graph(name):
    g = Graph(name, FX_DIR)
    g.mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    return g


def build_decals():
    # Scorch: charred ground with embers that cool faster than the burn fades.
    g = decal_graph("M_Decal_Scorch")
    tex = g.texture_object("Texture", TEX + "T_VFX_Scorch.T_VFX_Scorch")
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    life = g.node(unreal.MaterialExpressionDecalLifetimeOpacity)
    color = g.vector("Color", (1.0, 0.4, 0.08))
    intensity = g.scalar("Intensity", 3.0)
    char = g.scalar("Char", 1.0)
    op = g.custom("return saturate(Texture2DSample(T, TSampler, UV).r * Char * 0.92 + Texture2DSample(T, TSampler, UV).g * Intensity * 0.2) * Life;",
                  {"T": tex, "UV": uv, "Life": life, "Char": char, "Intensity": intensity}, "float")
    em = g.custom("float e = Texture2DSample(T, TSampler, UV).g; return Color * Intensity * e * Life;",
                  {"T": tex, "UV": uv, "Life": life, "Color": color, "Intensity": intensity})
    g.out(g.custom("return float3(0.02, 0.018, 0.016);", {}), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(op, unreal.MaterialProperty.MP_OPACITY)
    g.out(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.out(g.const(0.95), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()

    # Cracks: broken ground from slams, spikes and big stone impacts.
    g = decal_graph("M_Decal_Cracks")
    tex = g.texture_object("Texture", TEX + "T_VFX_Cracks.T_VFX_Cracks")
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    life = g.node(unreal.MaterialExpressionDecalLifetimeOpacity)
    color = g.vector("Color", (0.06, 0.05, 0.04))
    g.out(g.custom("return Color;", {"Color": color}), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.custom("return Texture2DSample(T, TSampler, UV).a * Life;", {"T": tex, "UV": uv, "Life": life}, "float"), unreal.MaterialProperty.MP_OPACITY)
    g.out(g.const(0.95), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()

    # Wet: darker, glossy ground where water landed.
    g = decal_graph("M_Decal_Wet")
    noise = g.texture_object("NoiseTex", TEX + "T_VFX_Noise.T_VFX_Noise")
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    life = g.node(unreal.MaterialExpressionDecalLifetimeOpacity)
    op = g.custom("""
float d = distance(UV, float2(0.5, 0.5)) * 2.0;
float n = Texture2DSample(N, NSampler, UV * 1.3).r;
return saturate((1.0 - d) * 1.6 + (n - 0.5) * 0.8) * 0.6 * Life;
""", {"N": noise, "UV": uv, "Life": life}, "float")
    g.out(g.custom("return float3(0.035, 0.04, 0.045);", {}), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(op, unreal.MaterialProperty.MP_OPACITY)
    g.out(g.const(0.06), unreal.MaterialProperty.MP_ROUGHNESS)
    g.finish()

    # Magic circle: glowing runes turning on the ground (Inferno's warning).
    g = decal_graph("M_Decal_Circle")
    tex = g.texture_object("Texture", TEX + "T_VFX_MagicCircle.T_VFX_MagicCircle")
    uv = g.node(unreal.MaterialExpressionTextureCoordinate)
    life = g.node(unreal.MaterialExpressionDecalLifetimeOpacity)
    time = g.node(unreal.MaterialExpressionTime)
    color = g.vector("Color", (1.0, 0.45, 0.1))
    intensity = g.scalar("Intensity", 8.0)
    spin = g.scalar("SpinSpeed", 30.0)
    rot = g.custom("""
float a = radians(Time * Spin);
float2 p = UV - 0.5;
float2 r = float2(p.x * cos(a) - p.y * sin(a), p.x * sin(a) + p.y * cos(a)) + 0.5;
return Texture2DSample(T, TSampler, r).a;
""", {"T": tex, "UV": uv, "Time": time, "Spin": spin}, "float")
    em = g.custom("return Color * Intensity * Mask * Life * (0.85 + 0.15 * sin(Time * 6.0));",
                  {"Color": color, "Intensity": intensity, "Mask": rot, "Life": life, "Time": time})
    g.out(g.custom("return Color * 0.25;", {"Color": color}), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.custom("return Mask * 0.7 * Life;", {"Mask": rot, "Life": life}, "float"), unreal.MaterialProperty.MP_OPACITY)
    g.out(em, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def setup_vfx():
    src = os.path.join(PROJECT, "SourceArt", "VFX", "Textures")
    kinds = {"T_VFX_Noise": "mask", "T_VFX_WaterN": "normal"}
    import_folder(src, ROOT + "/VFX/Textures", lambda n: kinds.get(n, "vfx"))
    for build in (build_glow, build_sprite, build_smoke, build_water, build_air, build_rock, build_ghost, build_decals):
        try:
            build()
        except Exception as exc:  # keep going: one broken material must not block the rest
            unreal.log_error("[mt_setup_laplace] %s failed: %s" % (build.__name__, exc))


# --------------------------------------------------------------------------------------------- UI art

def setup_ui():
    base = os.path.join(PROJECT, "SourceArt", "UI")
    import_folder(os.path.join(base, "Icons"), ROOT + "/UI/Icons", lambda n: "ui", lambda n: 256)
    import_folder(os.path.join(base, "Art"), ROOT + "/UI/Art", lambda n: "ui", lambda n: 2048)
    import_folder(os.path.join(base, "Frame"), ROOT + "/UI/Frame", lambda n: "ui", lambda n: 2048)
    world_map = os.path.join(PROJECT, "SourceArt", "World", "WorldMap.png")
    if os.path.exists(world_map):
        configure_texture(import_file(world_map, ROOT + "/UI/Art", "T_WorldMap"), "ui", 4096)


# --------------------------------------------------------------------------------------------- audio

def spell_attenuation():
    """3D falloff for spell / combat sounds: full within 5 m, fading out by 45 m."""
    path = ROOT + "/Audio/ATT_Spell"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    att = TOOLS.create_asset("ATT_Spell", ROOT + "/Audio", unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = att.get_editor_property("attenuation")
    settings.set_editor_property("attenuate", True)
    settings.set_editor_property("spatialize", True)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(500.0, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", 4000.0)
    att.set_editor_property("attenuation", settings)
    EAL.save_loaded_asset(att)
    return att


def setup_audio():
    base = os.path.join(PROJECT, "SourceArt", "Audio")
    manifest_path = os.path.join(base, "manifest.json")
    if not os.path.exists(manifest_path):
        warn("no SourceArt/Audio/manifest.json yet: audio skipped")
        return
    manifest = json.load(open(manifest_path))
    entries = manifest if isinstance(manifest, list) else manifest.get("files", manifest.get("sounds", []))
    try:
        attenuation = spell_attenuation()
    except Exception as exc:
        warn("attenuation asset failed: %s" % exc)
        attenuation = None
    count = 0
    for entry in entries:
        rel = entry.get("file") or entry.get("path")
        if not rel:
            continue
        src = os.path.join(base, rel)
        if not os.path.exists(src):
            continue
        category = entry.get("category") or os.path.dirname(rel) or "Misc"
        name = os.path.splitext(os.path.basename(rel))[0]
        wave = import_file(src, "%s/Audio/%s" % (ROOT, category), name)
        if wave is None:
            continue
        if entry.get("loop"):
            wave.set_editor_property("looping", True)
        if attenuation and entry.get("spatial") == "3D":
            wave.set_editor_property("attenuation_settings", attenuation)
        EAL.save_loaded_asset(wave)
        count += 1
    log("imported %d sounds" % count)


# --------------------------------------------------------------------------------------------- kit meshes

def slot_material(slot_name):
    """Our material for a kit slot name: effect meshes get the effect materials; palette slots get MI_<Slot> once
    the kit materials exist (mt_setup_laplace builds them from SourceArt/Textures), else the rock effect material."""
    if slot_name == "VFX":
        return unreal.load_asset(FX_DIR + "/M_VFX_Glow")
    kit_mi = "%s/Materials/MI_%s" % (ROOT + "/Kit", slot_name)
    if EAL.does_asset_exist(kit_mi):
        return unreal.load_asset(kit_mi)
    return unreal.load_asset(FX_DIR + "/M_VFX_Rock")


def import_mesh(glb, dest_dir, name):
    return import_glb_mesh(glb, dest_dir, name, slot_material)


def setup_kit(categories=None):
    base = os.path.join(PROJECT, "SourceArt", "Kit")
    if not os.path.isdir(base):
        warn("no SourceArt/Kit yet: kit skipped")
        return
    for category in sorted(os.listdir(base)):
        folder = os.path.join(base, category)
        if not os.path.isdir(folder) or (categories and category not in categories):
            continue
        dest = "%s/Kit/%s" % (ROOT, category)
        count = 0
        for fname in sorted(os.listdir(folder)):
            if not fname.lower().endswith(".glb"):
                continue
            name = os.path.splitext(fname)[0]
            mesh = import_mesh(os.path.join(folder, fname), dest, name)
            if mesh is None:
                warn("kit import failed: " + fname)
                continue
            EAL.save_loaded_asset(mesh)
            count += 1
        log("imported %d meshes into %s" % (count, dest))


def main():
    setup_vfx()
    setup_ui()
    setup_audio()
    setup_kit(["VFX"])
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("done")


main()
