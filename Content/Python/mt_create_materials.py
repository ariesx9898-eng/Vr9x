"""Creates the small set of master materials the C++ code falls back to.
Run once in the editor before playing:  py mt_create_materials.py

  /Game/Materials/M_MT_ZoneDecal       deferred decal; params Color, Opacity, Wetness, RingOnly, Age
                                       (ground spells are projected decals -> no coplanar planes, no z-fighting)
  /Game/Materials/M_MT_SpellBody       emissive spell body; param Color
  /Game/Materials/M_MT_ForesightGhost  translucent fresnel silhouette for Demon Eye; params Color, Opacity
  /Game/Materials/M_MT_ToonCharacter   lit character master; param BaseTexture (keeps the model's painted colours)
"""
import unreal

MEL = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Materials"


def new_material(name):
    path = "%s/%s" % (DIR, name)
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())


def scalar(mat, name, default, x, y):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def vector(mat, name, default, x, y):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def zone_decal():
    mat = new_material("M_MT_ZoneDecal")
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    color = vector(mat, "Color", unreal.LinearColor(0.1, 0.07, 0.04, 1), -900, -200)
    opacity = scalar(mat, "Opacity", 0.0, -900, 0)
    wet = scalar(mat, "Wetness", 0.0, -900, 150)
    ring_only = scalar(mat, "RingOnly", 0.0, -900, 300)
    # Radial mask from decal UVs: soft disc, or a ring when RingOnly = 1.
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1300, 400)
    grad = MEL.create_material_expression(mat, unreal.MaterialExpressionRadialGradientExponential, -1100, 400)
    MEL.connect_material_expressions(uv, "", grad, "UVs")
    ring = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -950, 450)
    MEL.connect_material_expressions(grad, "", ring, "")
    ring_band = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -800, 450)
    MEL.connect_material_expressions(grad, "", ring_band, "A")
    MEL.connect_material_expressions(ring, "", ring_band, "B")
    ring_boost = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 450)
    ring_boost.set_editor_property("const_b", 4.0)
    MEL.connect_material_expressions(ring_band, "", ring_boost, "A")
    mask = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -500, 400)
    MEL.connect_material_expressions(grad, "", mask, "A")
    MEL.connect_material_expressions(ring_boost, "", mask, "B")
    MEL.connect_material_expressions(ring_only, "", mask, "Alpha")
    final_opacity = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 200)
    MEL.connect_material_expressions(mask, "", final_opacity, "A")
    MEL.connect_material_expressions(opacity, "", final_opacity, "B")
    # Wet mud: low roughness when Wetness = 1, dry ground otherwise.
    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -300, 100)
    rough.set_editor_property("const_a", 0.85)
    rough.set_editor_property("const_b", 0.12)
    MEL.connect_material_expressions(wet, "", rough, "Alpha")
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(final_opacity, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    eal.save_loaded_asset(mat)


def spell_body():
    mat = new_material("M_MT_SpellBody")
    color = vector(mat, "Color", unreal.LinearColor(1, 1, 1, 1), -600, 0)
    boost = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 100)
    boost.set_editor_property("const_b", 3.0)
    MEL.connect_material_expressions(color, "", boost, "A")
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(boost, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)
    eal.save_loaded_asset(mat)


def foresight_ghost():
    mat = new_material("M_MT_ForesightGhost")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = vector(mat, "Color", unreal.LinearColor(0.55, 0.85, 1.0, 1), -700, 0)
    opacity = scalar(mat, "Opacity", 0.45, -700, 200)
    fresnel = MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel, -700, 350)
    edge = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 250)
    MEL.connect_material_expressions(fresnel, "", edge, "A")
    MEL.connect_material_expressions(opacity, "", edge, "B")
    emissive = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 0)
    emissive.set_editor_property("const_b", 2.0)
    MEL.connect_material_expressions(color, "", emissive, "A")
    MEL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(edge, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(mat)
    eal.save_loaded_asset(mat)


def toon_character():
    mat = new_material("M_MT_ToonCharacter")
    mat.set_editor_property("two_sided", True)  # source material is double-sided (thin cloth)
    tex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "BaseTexture")
    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", 0.75)
    MEL.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    eal.save_loaded_asset(mat)


for build in (zone_decal, spell_body, foresight_ghost, toon_character):
    try:
        build()
        unreal.log("PASS created " + build.__name__)
    except Exception as exc:
        unreal.log_error("FAIL %s: %s" % (build.__name__, exc))
