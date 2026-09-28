"""Material slot palette for the LA PLACE kits (Docs/LaPlace/Spec.md section 6).

Slot names are used verbatim as material names in the exported GLBs; the game maps each slot to its own material
instance. The colours below are only preview / placeholder base colours (sRGB, converted to linear for Blender and
glTF).
"""

# name: (sRGB base colour, roughness, metallic, emission strength)
ARCH = {
    "MT_Plaster": ((0.87, 0.83, 0.74), 0.9, 0.0, 0.0),
    "MT_PlasterTan": ((0.80, 0.65, 0.47), 0.9, 0.0, 0.0),
    "MT_Timber": ((0.26, 0.17, 0.10), 0.8, 0.0, 0.0),
    "MT_WoodPlanks": ((0.55, 0.40, 0.25), 0.8, 0.0, 0.0),
    "MT_Stone": ((0.55, 0.54, 0.51), 0.85, 0.0, 0.0),
    "MT_StoneWhite": ((0.90, 0.89, 0.85), 0.7, 0.0, 0.0),
    "MT_Cobble": ((0.47, 0.45, 0.42), 0.9, 0.0, 0.0),
    "MT_Sandstone": ((0.84, 0.70, 0.49), 0.9, 0.0, 0.0),
    "MT_DemonRock": ((0.36, 0.19, 0.15), 0.9, 0.0, 0.0),
    "MT_RoofRed": ((0.70, 0.30, 0.18), 0.75, 0.0, 0.0),
    "MT_RoofBlue": ((0.30, 0.40, 0.56), 0.6, 0.0, 0.0),
    "MT_RoofDark": ((0.20, 0.21, 0.25), 0.7, 0.0, 0.0),
    "MT_RoofThatch": ((0.66, 0.55, 0.32), 1.0, 0.0, 0.0),
    "MT_RoofSilver": ((0.78, 0.81, 0.86), 0.35, 0.8, 0.0),
    "MT_RoofGreen": ((0.36, 0.64, 0.54), 0.6, 0.2, 0.0),
    "MT_Snow": ((0.95, 0.97, 1.00), 0.6, 0.0, 0.0),
    "MT_ClothRed": ((0.70, 0.14, 0.11), 0.9, 0.0, 0.0),
    "MT_ClothBlue": ((0.16, 0.29, 0.62), 0.9, 0.0, 0.0),
    "MT_ClothGreen": ((0.22, 0.48, 0.24), 0.9, 0.0, 0.0),
    "MT_ClothTan": ((0.82, 0.70, 0.49), 0.9, 0.0, 0.0),
    "MT_Hide": ((0.58, 0.44, 0.31), 0.9, 0.0, 0.0),
    "MT_Iron": ((0.24, 0.24, 0.26), 0.5, 0.9, 0.0),
    "MT_Gold": ((0.88, 0.68, 0.22), 0.35, 1.0, 0.0),
    "MT_Glass": ((0.30, 0.40, 0.46), 0.15, 0.0, 0.0),
    "MT_Crystal": ((0.40, 0.72, 1.00), 0.2, 0.0, 2.0),
    "MT_Bone": ((0.86, 0.81, 0.68), 0.7, 0.0, 0.0),
}

NATURE = {
    "MT_BarkOak": ((0.35, 0.27, 0.20), 0.9, 0.0, 0.0),
    "MT_BarkBirch": ((0.85, 0.83, 0.78), 0.9, 0.0, 0.0),
    "MT_BarkPine": ((0.36, 0.24, 0.17), 0.9, 0.0, 0.0),
    "MT_BarkDead": ((0.45, 0.42, 0.38), 0.9, 0.0, 0.0),
    "MT_BarkGiant": ((0.38, 0.30, 0.22), 0.9, 0.0, 0.0),
    "MT_BarkDemon": ((0.30, 0.18, 0.20), 0.9, 0.0, 0.0),
    "MT_LeavesOak": ((0.30, 0.50, 0.20), 0.8, 0.0, 0.0),
    "MT_LeavesBirch": ((0.45, 0.62, 0.25), 0.8, 0.0, 0.0),
    "MT_NeedlesPine": ((0.18, 0.36, 0.22), 0.8, 0.0, 0.0),
    "MT_NeedlesSnow": ((0.70, 0.80, 0.78), 0.8, 0.0, 0.0),
    "MT_LeavesGiant": ((0.25, 0.48, 0.20), 0.8, 0.0, 0.0),
    "MT_LeavesDemon": ((0.48, 0.25, 0.58), 0.8, 0.0, 0.0),
    "MT_LeavesPalm": ((0.35, 0.55, 0.22), 0.8, 0.0, 0.0),
    "MT_Grass": ((0.35, 0.55, 0.22), 0.9, 0.0, 0.0),
    "MT_GrassDry": ((0.66, 0.60, 0.35), 0.9, 0.0, 0.0),
    "MT_Wheat": ((0.85, 0.72, 0.36), 0.9, 0.0, 0.0),
    "MT_Flowers": ((0.85, 0.55, 0.65), 0.9, 0.0, 0.0),
    "MT_Fern": ((0.28, 0.50, 0.22), 0.9, 0.0, 0.0),
    "MT_Reeds": ((0.55, 0.58, 0.32), 0.9, 0.0, 0.0),
    "MT_MushroomCap": ((0.62, 0.30, 0.55), 0.8, 0.0, 0.0),
    "MT_MushroomStem": ((0.85, 0.80, 0.70), 0.8, 0.0, 0.0),
    "MT_Rock": ((0.50, 0.48, 0.45), 0.9, 0.0, 0.0),
    "MT_RockMossy": ((0.42, 0.48, 0.36), 0.9, 0.0, 0.0),
    "MT_RockSnow": ((0.80, 0.82, 0.85), 0.9, 0.0, 0.0),
    "MT_RockDesert": ((0.75, 0.60, 0.42), 0.9, 0.0, 0.0),
    "MT_RockDemon": ((0.40, 0.22, 0.18), 0.9, 0.0, 0.0),
    "MT_RockPale": ((0.78, 0.76, 0.72), 0.9, 0.0, 0.0),
}

VFX = {"VFX": ((0.62, 0.86, 1.00), 0.4, 0.0, 0.25)}

ALL = {}
ALL.update(ARCH)
ALL.update(NATURE)
ALL.update(VFX)

VALID_SLOTS = frozenset(ALL)


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def get_material(name):
    """Return (creating if needed) a Blender material named exactly `name` with a preview base colour."""
    import bpy
    if name not in ALL:
        raise KeyError(f"material slot {name!r} is not in the Spec palette")
    mat = bpy.data.materials.get(name)
    if mat is not None:
        return mat
    (r, g, b), rough, metal, emit = ALL[name]
    lin = (srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), 1.0)
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = lin
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if emit > 0:
        bsdf.inputs["Emission Color"].default_value = lin
        bsdf.inputs["Emission Strength"].default_value = emit
    mat.diffuse_color = lin
    return mat
