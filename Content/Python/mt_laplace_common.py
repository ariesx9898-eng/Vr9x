"""Shared helpers for the LA PLACE editor scripts (mt_setup_laplace.py, mt_build_world.py): asset import, texture
settings and a small material-graph builder around MaterialEditingLibrary."""
import json
import os

import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ROOT = "/Game/LaPlace"


TAG = ["mt_setup_laplace"]


def set_tag(tag):
    """Prefix for log lines (run_editor_python.sh greps for [<script name>])."""
    TAG[0] = tag


def log(msg):
    unreal.log("[%s] %s" % (TAG[0], msg))


def warn(msg):
    unreal.log_warning("[%s] %s" % (TAG[0], msg))


# --------------------------------------------------------------------------------------------- import helpers

def import_file(path, dest_dir, name=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest_dir)
    if name:
        task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    TOOLS.import_asset_tasks([task])
    paths = task.get_editor_property("imported_object_paths")
    if not paths:
        warn("import failed: " + path)
        return None
    return unreal.load_asset(paths[0])


def configure_texture(tex, kind, max_size=0):
    if tex is None:
        return
    if kind == "mask":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    elif kind == "normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "ui":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("never_stream", True)
    elif kind == "vfx":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    if kind in ("vfx", "mask", "normal"):
        tex.set_editor_property("never_stream", True)
    if max_size:
        tex.set_editor_property("max_texture_size", max_size)
    EAL.save_loaded_asset(tex)


def import_folder(src_dir, dest_dir, kind_for, max_size_for=None):
    if not os.path.isdir(src_dir):
        warn("missing folder, skipped: " + src_dir)
        return 0
    count = 0
    for fname in sorted(os.listdir(src_dir)):
        if not fname.lower().endswith(".png"):
            continue
        name = os.path.splitext(fname)[0]
        tex = import_file(os.path.join(src_dir, fname), dest_dir, name)
        configure_texture(tex, kind_for(name), max_size_for(name) if max_size_for else 0)
        count += tex is not None
    log("imported %d textures into %s" % (count, dest_dir))
    return count


# --------------------------------------------------------------------------------------------- material graph helpers

class Graph:
    """Tiny helper around MaterialEditingLibrary: parameters, constants and Custom HLSL nodes."""

    def __init__(self, name, folder):
        path = "%s/%s" % (folder, name)
        if EAL.does_asset_exist(path):
            EAL.delete_asset(path)
        self.mat = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        self.path = path
        self.y = 0

    def _pos(self):
        self.y += 140
        return -1400, self.y

    def node(self, cls, **props):
        x, y = self._pos()
        n = MEL.create_material_expression(self.mat, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def scalar(self, name, default):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

    def vector(self, name, rgb):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def texture_object(self, name, asset_path):
        tex = unreal.load_asset(asset_path)
        n = self.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name=name)
        if tex:
            n.set_editor_property("texture", tex)
        return n

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=value)

    def custom_data(self, index, default=1.0):
        return self.node(unreal.MaterialExpressionPerInstanceCustomData, data_index=index, const_default_value=default)

    def custom(self, code, inputs, out="float3"):
        types = {
            "float": unreal.CustomMaterialOutputType.CMOT_FLOAT1,
            "float2": unreal.CustomMaterialOutputType.CMOT_FLOAT2,
            "float3": unreal.CustomMaterialOutputType.CMOT_FLOAT3,
            "float4": unreal.CustomMaterialOutputType.CMOT_FLOAT4,
        }
        n = self.node(unreal.MaterialExpressionCustom, code=code, output_type=types[out])
        pins = []
        for pin_name in inputs:
            pin = unreal.CustomInput()
            pin.set_editor_property("input_name", pin_name)
            pins.append(pin)
        n.set_editor_property("inputs", pins)
        for pin_name, source in inputs.items():
            if source is not None:
                MEL.connect_material_expressions(source, "", n, pin_name)
        return n

    def out(self, node, prop, pin=""):
        MEL.connect_material_property(node, pin, prop)

    def finish(self):
        MEL.recompile_material(self.mat)
        EAL.save_asset(self.path)
        log("material " + self.path)


# --------------------------------------------------------------------------------------------- meshes

def import_glb_mesh(glb, dest_dir, name, material_for):
    """Imports one GLB as a static mesh at <dest_dir>/<name> (flattening the importer's per-file folders); each
    material slot gets material_for(slot_name) (None keeps the imported one)."""
    flat = "%s/%s" % (dest_dir, name)
    staging = "%s/_Import_%s" % (dest_dir, name)
    if EAL.does_directory_exist(staging):
        EAL.delete_directory(staging)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", glb)
    task.set_editor_property("destination_path", staging)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    TOOLS.import_asset_tasks([task])
    mesh = None
    for path in task.get_editor_property("imported_object_paths") or []:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            mesh = asset
            break
    if mesh is None:
        return None
    # Our materials by slot name, so the importer's per-file material copies can go.
    materials = mesh.get_editor_property("static_materials")
    for i, entry in enumerate(materials):
        slot = str(entry.get_editor_property("material_slot_name"))
        material = material_for(slot)
        if material:
            mesh.set_material(i, material)
    if EAL.does_asset_exist(flat):
        EAL.delete_asset(flat)
    EAL.rename_asset(mesh.get_path_name().split(".")[0], flat)
    EAL.delete_directory(staging)
    return unreal.load_asset(flat)
