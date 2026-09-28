"""LA PLACE world build: /Game/Maps/L_LaPlace (World Partition) from the world generator's outputs (Docs/LaPlace/Spec.md).

Run headless with the GPU available (the landscape's edit layers are merged on the GPU):
    RENDER=1 Tools/mac/run_editor_python.sh mt_build_world.py
Environment:
    MT_WORLD_STAGES  comma list, default "textures,materials,map,landscape,water,instances,save"
    MT_WORLD_FRESH=1 delete and recreate the map (needed to re-import the landscape)
    MT_WORLD_JSON / MT_WORLD_SRC / MT_WORLD_MAP  test overrides (World.json, the folder holding Height.r16 + Layers/,
                     the map path)

  /Game/LaPlace/Textures/{Terrain,Water}  terrain layer and water textures (SourceArt/Textures, Tools/textures)
  /Game/LaPlace/World/Materials           M_LaPlace_Landscape, M_LaPlace_Ocean / Lake / River / Ice
  /Game/LaPlace/World/Layers              LI_<Layer> landscape layer infos
  /Game/LaPlace/World/Water               generated lake / river / ocean meshes
  /Game/Maps/L_LaPlace                    the map: sky, sun, fog, clouds, post, day-night, landscape (always loaded
                                          streaming proxies), water, instance sets (foliage, cities; streamed)
"""
import json
import math
import os
import struct
import zlib

import unreal

from mt_laplace_common import EAL, MEL, PROJECT, ROOT, TOOLS, Graph, configure_texture, import_file, log, set_tag, warn

set_tag("mt_build_world")

WB = unreal.MTWorldBuildLibrary
WORLD_JSON = os.environ.get("MT_WORLD_JSON") or os.path.join(PROJECT, "Content", "Data", "World.json")
SRC = os.environ.get("MT_WORLD_SRC") or os.path.join(PROJECT, "SourceArt", "World")
MAP = os.environ.get("MT_WORLD_MAP") or "/Game/Maps/L_LaPlace"
STAGES = [s.strip() for s in (os.environ.get("MT_WORLD_STAGES") or "textures,materials,map,landscape,water,instances,save").split(",")]
FRESH = os.environ.get("MT_WORLD_FRESH") == "1"
MAT_DIR = ROOT + "/World/Materials"
TEX_DIR = ROOT + "/Textures"
TEX_SRC = os.path.join(PROJECT, "SourceArt", "Textures")
WATER_DIR = ROOT + "/World/Water"
LAYER_DIR = ROOT + "/World/Layers"

# Fallback look per paint layer when its textures are missing: linear albedo, rough min / max, and the cliff-rock tint
# used where that layer dominates (steep slopes are drawn with the Rock textures, tinted to the local ground).
LAYER_LOOK = {
    "Grass": ((0.12, 0.20, 0.05), 0.75, 0.95, (1.00, 1.00, 1.00)),
    "Farmland": ((0.22, 0.15, 0.08), 0.80, 0.98, (1.00, 0.97, 0.92)),
    "ForestFloor": ((0.11, 0.09, 0.05), 0.80, 0.98, (0.92, 0.95, 0.88)),
    "Moss": ((0.07, 0.14, 0.04), 0.70, 0.95, (0.88, 0.95, 0.85)),
    "Snow": ((0.80, 0.84, 0.90), 0.45, 0.75, (0.95, 0.98, 1.05)),
    "Sand": ((0.52, 0.45, 0.32), 0.75, 0.95, (1.15, 1.05, 0.90)),
    "Desert": ((0.60, 0.42, 0.22), 0.80, 0.98, (1.30, 1.00, 0.72)),
    "DemonSoil": ((0.18, 0.07, 0.05), 0.80, 0.98, (0.85, 0.55, 0.48)),
    "Rock": ((0.27, 0.26, 0.25), 0.75, 0.95, (1.00, 1.00, 1.00)),
    "Road": ((0.28, 0.22, 0.15), 0.85, 1.00, (1.00, 0.95, 0.88)),
    "Mud": ((0.11, 0.08, 0.05), 0.35, 0.80, (0.85, 0.82, 0.78)),
    "Cobble": ((0.30, 0.29, 0.27), 0.65, 0.95, (1.00, 0.98, 0.95)),
}
LAYER_DEBUG = {
    "Grass": (0.2, 0.7, 0.2), "Farmland": (0.8, 0.6, 0.2), "ForestFloor": (0.3, 0.4, 0.1), "Moss": (0.1, 0.5, 0.2),
    "Snow": (0.95, 0.95, 1.0), "Sand": (0.95, 0.85, 0.55), "Desert": (0.95, 0.65, 0.3), "DemonSoil": (0.6, 0.15, 0.1),
    "Rock": (0.5, 0.5, 0.5), "Road": (0.55, 0.4, 0.25), "Mud": (0.3, 0.2, 0.1), "Cobble": (0.4, 0.4, 0.45),
}
DEFAULT_TILE_M = {"Snow": 6.0, "Desert": 8.0, "DemonSoil": 6.0, "Rock": 8.0, "Cobble": 3.0}
# How strongly the per-region tint (SourceArt/World/Macro/RegionTint.png) colours each layer.
VEGETATION_TINT = {"Grass": 1.0, "Moss": 0.8, "ForestFloor": 0.6, "Farmland": 0.35, "Mud": 0.2}
GRASS_DIR = ROOT + "/World/Grass"
# Landscape grass (spawned by the GPU around the camera from the landscape material's grass outputs):
# name -> (varieties [(kit mesh, instances per 10 m2)], (cull start, cull end) cm, scale range)
GRASS_TYPES = {
    "Lush": ([("Plants/SM_Grass_A", 40), ("Plants/SM_Grass_B", 30), ("Plants/SM_Grass_C", 22)], (4500, 8000), (0.8, 1.25)),
    "Dry": ([("Plants/SM_Grass_Dry_A", 34), ("Plants/SM_Grass_Dry_B", 26)], (4500, 8000), (0.8, 1.2)),
    "Snow": ([("Plants/SM_Grass_Snow_A", 22)], (4500, 8000), (0.8, 1.2)),
    "Flowers": ([("Plants/SM_Flowers_A", 5), ("Plants/SM_Flowers_B", 5), ("Plants/SM_Flowers_C", 5)], (4000, 7000), (0.8, 1.2)),
    "Wheat": ([("Plants/SM_Wheat_A", 90), ("Plants/SM_Wheat_B", 70)], (9000, 14000), (0.9, 1.15)),
    "Forest": ([("Plants/SM_Fern_A", 10), ("Plants/SM_Fern_B", 8)], (5000, 9000), (0.8, 1.3)),
    "Reeds": ([("Plants/SM_Reeds_A", 12), ("Plants/SM_Reeds_B", 10)], (5000, 9000), (0.8, 1.2)),
    "Pebbles": ([("Rocks/SM_Rock_Small_A", 1.2), ("Rocks/SM_Rock_Small_B", 1.2), ("Rocks/SM_Rock_Small_C", 1.2)], (3000, 6000), (0.3, 0.8)),
}


def load_world():
    with open(WORLD_JSON, "r", encoding="utf-8") as f:
        return json.load(f)


def src_path(rel):
    """World.json paths are repo-relative (SourceArt/World/...); MT_WORLD_SRC relocates them for tests."""
    if rel.startswith("SourceArt/World/"):
        return os.path.join(SRC, rel[len("SourceArt/World/"):])
    return os.path.join(PROJECT, rel)


def texture_manifest():
    path = os.path.join(TEX_SRC, "manifest.json")
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    rows = data.get("materials", data.get("entries", data)) if isinstance(data, dict) else data
    out = {}
    for row in rows if isinstance(rows, list) else []:
        if isinstance(row, dict) and "name" in row:
            out[row["name"]] = row
    return out


# --------------------------------------------------------------------------------------------- tiny PNG writer

def write_png(path, width, height, pixel):
    """Solid-colour RGBA PNG (fallback textures) without PIL (the editor's Python has no imaging library)."""
    row = b"\x00" + bytes(pixel) * width
    raw = row * height

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


# --------------------------------------------------------------------------------------------- textures

def stage_textures():
    """Imports SourceArt/Textures/{Terrain,Water} (Tools/textures/build_textures.py) and the fallback textures."""
    fallback_dir = os.path.join(PROJECT, "Saved", "LaPlaceFallback")
    os.makedirs(fallback_dir, exist_ok=True)
    for name, pixel, kind in (("T_Fallback_Grey", (128, 128, 128, 255), "mask"),
                              ("T_Fallback_White", (255, 255, 255, 255), "color"),
                              ("T_Fallback_Normal", (128, 128, 255, 255), "normal"),
                              ("T_Fallback_Mask", (255, 220, 128, 255), "mask")):
        path = os.path.join(fallback_dir, name + ".png")
        write_png(path, 8, 8, pixel)
        tex = import_file(path, TEX_DIR + "/Fallback", name)
        if kind != "color":
            configure_texture(tex, kind)
        elif tex:
            EAL.save_loaded_asset(tex)
    count = 0
    for sub in ("Terrain", "Water"):
        folder = os.path.join(TEX_SRC, sub)
        if not os.path.isdir(folder):
            warn("no %s textures yet (%s): the materials use flat fallbacks" % (sub, folder))
            continue
        for fname in sorted(os.listdir(folder)):
            if not fname.lower().endswith(".png"):
                continue
            name = os.path.splitext(fname)[0]
            tex = import_file(os.path.join(folder, fname), "%s/%s" % (TEX_DIR, sub), name)
            if tex is None:
                continue
            if name.endswith("_N"):
                configure_texture(tex, "normal")
            elif name.endswith("_M") or name.startswith("T_Macro"):
                configure_texture(tex, "mask")
            else:
                tex.set_editor_property("srgb", True)
                EAL.save_loaded_asset(tex)
            # Terrain detail is seen at grazing angles all the time: keep it streaming, but never below 1024.
            count += 1
    macro = os.path.join(SRC, "Macro")
    for name in ("RegionTint", "GrassMix"):
        path = os.path.join(macro, name + ".png")
        if os.path.exists(path):
            tex = import_file(path, ROOT + "/World/Macro", "T_" + name)
            if tex:
                tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
                tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
                configure_texture(tex, "mask")
                count += 1
    log("imported %d world textures" % count)


def texture_or(path, fallback):
    return path if EAL.does_asset_exist(path) else fallback


# --------------------------------------------------------------------------------------------- materials

class WorldGraph(Graph):
    """Graph with Custom-node extra outputs and typed texture-object parameters; reports compile errors."""

    def finish(self):
        MEL.recompile_material(self.mat)
        errors = WB.get_material_compile_errors(self.mat)
        for e in errors:
            warn("%s: %s" % (self.path, e))
        EAL.save_asset(self.path)
        log("material %s%s" % (self.path, " FAILED (%d errors)" % len(errors) if errors else " ok"))

    def texture_param(self, name, asset_path, sampler):
        n = self.node(unreal.MaterialExpressionTextureObjectParameter, parameter_name=name)
        tex = unreal.load_asset(asset_path)
        if tex:
            n.set_editor_property("texture", tex)
        n.set_editor_property("sampler_type", sampler)
        n.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        return n

    def custom_multi(self, code, inputs, out="float3", extra=None):
        n = self.custom(code, inputs, out)
        outs = []
        types = {"float": unreal.CustomMaterialOutputType.CMOT_FLOAT1, "float2": unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                 "float3": unreal.CustomMaterialOutputType.CMOT_FLOAT3, "float4": unreal.CustomMaterialOutputType.CMOT_FLOAT4}
        for name, kind in (extra or {}).items():
            o = unreal.CustomOutput()
            o.set_editor_property("output_name", name)
            o.set_editor_property("output_type", types[kind])
            outs.append(o)
        n.set_editor_property("additional_outputs", outs)
        MEL.recompile_material(self.mat)  # refresh pins before connecting the extra outputs
        return n


def landscape_layers(world):
    """Final paint layers: SourceArt/World/LayersFinal (generator layers + city streets, Tools/world/finalize_world.py)
    when present, else the generator's own layers."""
    final = os.path.join(SRC, "LayersFinal", "layers.json")
    if os.path.exists(final):
        with open(final, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data["Layers"], [src_path(p) for p in data["Files"]]
    ls = world["Landscape"]
    return list(ls["LayerNames"]), [src_path(p) for p in ls["Layers"]]


def layer_texture_base(name):
    """Terrain layers use T_Ground_<Layer>; Cobble streets reuse the cobblestone palette texture."""
    if name == "Cobble":
        return "%s/Palette/T_Cobble" % TEX_DIR
    return "%s/Terrain/T_Ground_%s" % (TEX_DIR, name)


def layer_tile(manifest, name):
    for key in ("Ground_" + name, "T_Ground_" + name, name if name == "Cobble" else None):
        if key and key in manifest and "tile_m" in manifest[key]:
            return float(manifest[key]["tile_m"])
    for row in manifest.values():
        files = row.get("files", {})
        if row.get("group") == "Terrain" and str(files.get("D", "")).endswith("T_Ground_%s_D.png" % name) and "tile_m" in row:
            return float(row["tile_m"])
    return DEFAULT_TILE_M.get(name, 4.0)


def layer_code(i, name, tile_m, has_d):
    albedo, rmin, rmax, rock_tint = LAYER_LOOK.get(name, LAYER_LOOK["Grass"])
    k = 1.0 / tile_m
    tint = "float3(1,1,1)" if has_d else "float3(%.4f,%.4f,%.4f)" % albedo
    return dict(i=i, k="%.6f" % k, tint=tint, rmin="%.3f" % rmin, rmax="%.3f" % rmax,
                rt="float3(%.3f,%.3f,%.3f)" % rock_tint, veg="%.2f" % VEGETATION_TINT.get(name, 0.0), name=name)


def landscape_hlsl(layers, rock_index, names, rect):
    """HLSL for the landscape: height-blended paint layers (only layers with weight are sampled), region tint,
    anti-tiling, macro variation, triplanar cliff rock tinted to the local ground, wet shoreline, and the densities
    for the landscape grass types. World-space normal out."""
    def w(name):
        return "W%d" % names.index(name) if name in names else "0.0"

    L = []
    L.append("float3 Nv = normalize(VN);")
    L.append("float3 Tn = normalize(float3(1,0,0) - Nv * Nv.x); float3 Bn = cross(Nv, Tn);")
    L.append("float2 P = WP.xy * 0.01; float2 Pdx = ddx(P); float2 Pdy = ddy(P);")
    L.append("SamplerState S = View.MaterialTextureBilinearWrapedSampler;")
    L.append("float farT = saturate((Dist - 3000.0) / 9000.0);")
    L.append("float2 wuv = (WP.xy - float2(%.1f, %.1f)) * float2(%.9f, %.9f);" % (rect[0], rect[1], 1.0 / rect[2], 1.0 / rect[3]))
    L.append("float3 rtint = RTint.SampleLevel(S, wuv, 0).rgb * 2.0;")
    # Pass 1: heights -> blend weights.
    for l in layers:
        L.append("float a{i} = -1.0; if (W{i} > 0.002) {{ float h = M{i}.SampleGrad(S, P * {k}, Pdx * {k}, Pdy * {k}).b; "
                 "a{i} = W{i} + h * 0.45; }}".format(**l))
    L.append("float amax = " + "max(" * (len(layers) - 1) + "a0" + "".join(", a%d)" % l["i"] for l in layers[1:]) + ";")
    for l in layers:
        L.append("float b{i} = max(a{i} - amax + 0.30, 0.0);".format(**l))
    L.append("float bsum = " + " + ".join("b%d" % l["i"] for l in layers) + " + 1e-5;")
    L.append("float3 col = 0; float2 nxy = 0; float rough = 0; float ao = 0; float3 rockTint = 0;")
    for l in layers:
        L.append(("if (b{i} > 0.002) {{ float w = b{i} / bsum; float2 uv = P * {k}; float2 dx = Pdx * {k}; float2 dy = Pdy * {k};"
                  " float3 c = D{i}.SampleGrad(S, uv, dx, dy).rgb;"
                  " if (farT > 0.01) {{ float3 cf = D{i}.SampleGrad(S, uv * 0.27 + float2(0.37, 0.71), dx * 0.27, dy * 0.27).rgb; c = lerp(c, cf, farT * 0.5); }}"
                  " c *= {tint} * lerp(float3(1,1,1), rtint, {veg});"
                  " float4 m = M{i}.SampleGrad(S, uv, dx, dy);"
                  " col += c * w; nxy += (N{i}.SampleGrad(S, uv, dx, dy).rg * 2.0 - 1.0) * w;"
                  " rough += lerp({rmin}, {rmax}, m.g) * w; ao += m.r * w; rockTint += {rt} * w; }}").format(**l))
    # Macro variation (brightness / warmth) at 150 m and 37 m.
    L.append("float3 mac1 = Macro.Sample(S, P / 150.0).rgb; float3 mac2 = Macro.Sample(S, P / 37.0 + 0.5).rgb;")
    L.append("col *= saturate(1.0 + (mac1.r - 0.5) * MacroStrength + (mac2.g - 0.5) * MacroStrength * 0.6);")
    L.append("col = lerp(col, col * float3(1.07, 1.0, 0.88), saturate((mac1.b - 0.5) * 1.2 + 0.5) * 0.35);")
    L.append("nxy *= NormalStrength * (1.0 - farT * 0.6);")
    L.append("float3 nts = float3(nxy, sqrt(saturate(1.0 - dot(nxy, nxy))));")
    L.append("float3 N = normalize(nts.x * Tn + nts.y * Bn + nts.z * Nv);")
    r = rock_index
    L.append("float slope = 1.0 - saturate(Nv.z); float cliff = saturate((slope - CliffStart) / CliffRange);")
    L.append("float3 Pr = WP * (0.01 * RockK); float2 uX = Pr.yz, uY = Pr.xz, uZ = Pr.xy;")
    L.append("float2 dXx = ddx(uX), dXy = ddy(uX), dYx = ddx(uY), dYy = ddy(uY), dZx = ddx(uZ), dZy = ddy(uZ);")
    L.append(("if (cliff > 0.002) {{ float3 bw = pow(abs(Nv), 4.0); bw /= (bw.x + bw.y + bw.z);"
              " float3 rc = D{r}.SampleGrad(S, uX, dXx, dXy).rgb * bw.x + D{r}.SampleGrad(S, uY, dYx, dYy).rgb * bw.y + D{r}.SampleGrad(S, uZ, dZx, dZy).rgb * bw.z;"
              " rc *= RockTintBase.rgb * (rockTint + (bsum < 0.01 ? 1.0 : 0.0));"
              " float2 nX = N{r}.SampleGrad(S, uX, dXx, dXy).rg * 2.0 - 1.0; float2 nY = N{r}.SampleGrad(S, uY, dYx, dYy).rg * 2.0 - 1.0; float2 nZ = N{r}.SampleGrad(S, uZ, dZx, dZy).rg * 2.0 - 1.0;"
              " float3 tX = float3(nX + Nv.zy, Nv.x); float3 tY = float3(nY + Nv.xz, Nv.y); float3 tZ = float3(nZ + Nv.xy, Nv.z);"
              " float3 wn = normalize(tX.zyx * bw.x + tY.xzy * bw.y + tZ.xyz * bw.z);"
              " float ro = M{r}.SampleGrad(S, uZ, dZx, dZy).g;"
              " col = lerp(col, rc, cliff); N = normalize(lerp(N, wn, cliff)); rough = lerp(rough, lerp(0.7, 0.95, ro), cliff); ao = lerp(ao, 1.0, cliff * 0.5); }}").format(r=r))
    L.append("float wet = saturate(1.0 - (WP.z - 40.0) / 160.0) * saturate((WP.z + 400.0) / 400.0);")
    L.append("col *= lerp(1.0, 0.6, wet); rough = lerp(rough, 0.2, wet);")
    # Landscape grass densities (0..1).
    L.append("float4 gm = GMix.SampleLevel(S, wuv, 0); float land = saturate((WP.z - 20.0) / 40.0); float ok = (1.0 - cliff) * land;")
    L.append("G_Lush = saturate((%s + 0.4 * %s) * gm.r * 1.6) * ok;" % (w("Grass"), w("Moss")))
    L.append("G_Dry = saturate((%s + 0.25 * %s + 0.3 * %s + 0.2 * %s) * gm.g * 1.6) * ok;" % (w("Grass"), w("Desert"), w("DemonSoil"), w("Sand")))
    L.append("G_Snow = saturate((%s + 0.6 * %s + 0.15 * %s) * gm.b * 1.6) * ok;" % (w("Grass"), w("Moss"), w("Snow")))
    L.append("G_Flowers = saturate(%s * gm.a * 3.0) * ok;" % w("Grass"))
    L.append("G_Wheat = saturate(%s * 1.2 - 0.1) * ok;" % w("Farmland"))
    L.append("G_Forest = saturate((%s + 0.5 * %s) * 0.9) * ok;" % (w("ForestFloor"), w("Moss")))
    L.append("G_Reeds = saturate(%s * 0.8) * (1.0 - cliff);" % w("Mud"))
    L.append("G_Pebbles = saturate(%s * 0.3 + %s * 0.08 + %s * 0.15 + %s * 0.05) * (1.0 - cliff);" % (w("Rock"), w("Desert"), w("DemonSoil"), w("Road")))
    L.append("OutNormal = N; OutRough = saturate(rough); OutAO = saturate(ao);")
    L.append("return saturate(col);")
    return "\n".join(L)


def build_grass_types():
    """LGT_<Name> landscape grass types from the kit meshes (skipped until the kit is imported)."""
    made = 0
    for name, (varieties, cull, scale) in GRASS_TYPES.items():
        path = "%s/LGT_%s" % (GRASS_DIR, name)
        if EAL.does_asset_exist(path):
            lgt = unreal.load_asset(path)
        else:
            factory = getattr(unreal, "LandscapeGrassTypeFactory", None)
            lgt = TOOLS.create_asset("LGT_" + name, GRASS_DIR, unreal.LandscapeGrassType, factory() if factory else None)
        if lgt is None:
            warn("could not create " + path)
            continue
        out = []
        for mesh_rel, density in varieties:
            mesh = unreal.load_asset("%s/Kit/%s" % (ROOT, mesh_rel))
            if mesh is None:
                continue
            v = unreal.GrassVariety()
            v.set_editor_property("grass_mesh", mesh)
            d = unreal.PerPlatformFloat()
            d.set_editor_property("default", float(density))
            v.set_editor_property("grass_density", d)
            c0 = unreal.PerPlatformInt()
            c0.set_editor_property("default", int(cull[0]))
            c1 = unreal.PerPlatformInt()
            c1.set_editor_property("default", int(cull[1]))
            v.set_editor_property("start_cull_distance", c0)
            v.set_editor_property("end_cull_distance", c1)
            v.set_editor_property("scaling", unreal.GrassScaling.UNIFORM)
            v.set_editor_property("scale_x", unreal.FloatInterval(scale[0], scale[1]))
            v.set_editor_property("random_rotation", True)
            v.set_editor_property("align_to_surface", True)
            v.set_editor_property("cast_dynamic_shadow", False)
            out.append(v)
        lgt.set_editor_property("grass_varieties", out)
        EAL.save_loaded_asset(lgt)
        made += 1 if out else 0
    log("%d landscape grass types with meshes" % made)


def build_landscape_material(world):
    names, _ = landscape_layers(world)
    manifest = texture_manifest()
    ls = world["Landscape"]
    rect = (ls["LocationCm"]["X"], ls["LocationCm"]["Y"], (ls["VerticesX"] - 1) * ls["QuadSizeCm"], (ls["VerticesY"] - 1) * ls["QuadSizeCm"])
    g = WorldGraph("M_LaPlace_Landscape", MAT_DIR)
    g.mat.set_editor_property("tangent_space_normal", False)
    fallback_w = TEX_DIR + "/Fallback/T_Fallback_White"
    fallback_n = TEX_DIR + "/Fallback/T_Fallback_Normal"
    fallback_m = TEX_DIR + "/Fallback/T_Fallback_Mask"
    inputs = {
        "WP": g.node(unreal.MaterialExpressionWorldPosition),
        "VN": g.node(unreal.MaterialExpressionVertexNormalWS),
        "Dist": g.node(unreal.MaterialExpressionPixelDepth),
        "MacroStrength": g.scalar("MacroStrength", 0.35),
        "NormalStrength": g.scalar("NormalStrength", 1.0),
        "CliffStart": g.scalar("CliffStart", 0.30),
        "CliffRange": g.scalar("CliffRange", 0.14),
        "RockK": g.scalar("RockTilesPerMetre", 1.0 / 10.0),
        "RockTintBase": g.vector("RockTint", (1.0, 1.0, 1.0)),
    }
    layers = []
    for i, name in enumerate(names):
        base = layer_texture_base(name)
        has_d, has_n, has_m = (EAL.does_asset_exist(base + suffix) for suffix in ("_D", "_N", "_M"))
        inputs["W%d" % i] = g.node(unreal.MaterialExpressionLandscapeLayerSample, parameter_name=name, preview_weight=0.0)
        inputs["D%d" % i] = g.texture_param(name + "_D", base + "_D" if has_d else fallback_w, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        inputs["N%d" % i] = g.texture_param(name + "_N", base + "_N" if has_n else fallback_n, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        inputs["M%d" % i] = g.texture_param(name + "_M", base + "_M" if has_m else fallback_m, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        layers.append(layer_code(i, name, layer_tile(manifest, name), has_d))
    inputs["Macro"] = g.texture_param("MacroNoise", texture_or(TEX_DIR + "/Terrain/T_Macro_Noise", fallback_m), unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    inputs["RTint"] = g.texture_param("RegionTint", texture_or(ROOT + "/World/Macro/T_RegionTint", TEX_DIR + "/Fallback/T_Fallback_Grey"),
                                      unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    inputs["GMix"] = g.texture_param("GrassMix", texture_or(ROOT + "/World/Macro/T_GrassMix", fallback_m),
                                     unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    rock_index = names.index("Rock") if "Rock" in names else 0
    code = landscape_hlsl(layers, rock_index, names, rect)
    extra = {"OutNormal": "float3", "OutRough": "float", "OutAO": "float"}
    for gname in GRASS_TYPES:
        extra["G_" + gname] = "float"
    node = g.custom_multi(code, inputs, "float3", extra)
    g.out(node, unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "OutNormal", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(node, "OutRough", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(node, "OutAO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    g.out(g.const(0.35), unreal.MaterialProperty.MP_SPECULAR)
    grass = g.node(unreal.MaterialExpressionLandscapeGrassOutput)
    entries = []
    for gname in GRASS_TYPES:
        gi = unreal.GrassInput()
        gi.set_editor_property("name", "G_" + gname)
        lgt = GRASS_DIR + "/LGT_" + gname
        if EAL.does_asset_exist(lgt):
            gi.set_editor_property("grass_type", unreal.load_asset(lgt))
        entries.append(gi)
    grass.set_editor_property("grass_types", entries)
    for gname in GRASS_TYPES:
        if not MEL.connect_material_expressions(node, "G_" + gname, grass, "G_" + gname):
            warn("grass output G_%s not connected" % gname)
    g.finish()
    return g.mat


def water_graph(name, scatter, absorb, tile_m, speed, foam_depth, river=False):
    """Single Layer Water: two panning normal layers, depth-absorbed colour, shoreline foam."""
    g = WorldGraph(name, MAT_DIR)
    # The water output node and shading model come first: the scene-depth-below-water node (and every intermediate
    # compile while the graph is built) is only valid in a Single Layer Water material that already has its output.
    out = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
    for pin, src in (("Scattering Coefficients", g.vector("Scattering", scatter)), ("Absorption Coefficients", g.vector("Absorption", absorb)),
                     ("Phase G", g.const(0.2)), ("Color Scale Behind Water", g.const(1.0))):
        if not MEL.connect_material_expressions(src, "", out, pin):
            MEL.connect_material_expressions(src, "", out, pin.replace(" ", ""))
    g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    normal = texture_or(TEX_DIR + "/Water/T_Ocean_N", texture_or(ROOT + "/VFX/Textures/T_VFX_WaterN", TEX_DIR + "/Fallback/T_Fallback_Normal"))
    foam = texture_or(TEX_DIR + "/Water/T_Foam_D", TEX_DIR + "/Fallback/T_Fallback_White")
    inputs = {
        "WP": g.node(unreal.MaterialExpressionWorldPosition),
        "UV": g.node(unreal.MaterialExpressionTextureCoordinate),
        "Time": g.node(unreal.MaterialExpressionTime),
        "Dist": g.node(unreal.MaterialExpressionPixelDepth),
        "Behind": g.node(unreal.MaterialExpressionSceneDepthWithoutWater),
        "NTex": g.texture_param("WaveNormal", normal, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        "FTex": g.texture_param("Foam", foam, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        "Tile": g.scalar("TileMetres", tile_m),
        "Speed": g.scalar("Speed", speed),
        "FoamDepth": g.scalar("FoamDepthCm", foam_depth),
        "WaveStrength": g.scalar("WaveStrength", 0.55),
    }
    flow = "float2(Time * Speed * 0.35, 0.0)" if river else "float2(Time * Speed * 0.020, Time * Speed * 0.013)"
    uv = "UV * float2(0.5, 1.0) / Tile" if river else "WP.xy * 0.01 / Tile"
    code = "\n".join([
        "SamplerState S = View.MaterialTextureBilinearWrapedSampler;",
        "float2 base = %s;" % uv,
        "float2 f = %s;" % flow,
        "float2 n1 = NTex.Sample(S, base + f).rg * 2.0 - 1.0;",
        "float2 n2 = NTex.Sample(S, base * 0.37 + float2(-f.y, f.x) * 0.7 + 0.21).rg * 2.0 - 1.0;",
        "float2 nxy = (n1 + n2 * 0.8) * WaveStrength * lerp(1.0, 0.25, saturate(Dist / 40000.0));",
        "float depth = max(Behind - Dist, 0.0);",
        "float foamEdge = 1.0 - saturate(depth / FoamDepth);",
        "float foamTex = FTex.Sample(S, base * 3.0 + f * 2.0).a;",
        "OutFoam = saturate(foamEdge * foamEdge * (0.35 + foamTex * 1.3));",
        "return normalize(float3(nxy, 1.0));",
    ])
    node = g.custom_multi(code, inputs, "float3", {"OutFoam": "float"})
    g.out(node, unreal.MaterialProperty.MP_NORMAL)
    g.out(g.vector("FoamColor", (0.85, 0.9, 0.92)), unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(node, "OutFoam", unreal.MaterialProperty.MP_OPACITY)
    g.out(g.scalar("Roughness", 0.05), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.5), unreal.MaterialProperty.MP_SPECULAR)
    g.finish()
    return g.mat


def build_ice_material():
    g = WorldGraph("M_LaPlace_Ice", MAT_DIR)
    g.out(g.vector("Color", (0.62, 0.75, 0.85)), unreal.MaterialProperty.MP_BASE_COLOR)
    g.out(g.scalar("Roughness", 0.18), unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.const(0.6), unreal.MaterialProperty.MP_SPECULAR)
    g.finish()
    return g.mat


def stage_materials(world):
    build_grass_types()
    build_landscape_material(world)
    water_graph("M_LaPlace_Ocean", (0.010, 0.045, 0.060), (0.30, 0.07, 0.045), 14.0, 1.0, 140.0)
    water_graph("M_LaPlace_Lake", (0.012, 0.040, 0.035), (0.35, 0.10, 0.09), 9.0, 0.6, 90.0)
    water_graph("M_LaPlace_River", (0.014, 0.040, 0.035), (0.40, 0.12, 0.10), 6.0, 1.0, 60.0, river=True)
    build_ice_material()


# --------------------------------------------------------------------------------------------- map + lighting

def level_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_actor(label):
    for a in actor_subsystem().get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def spawn(cls, label, location=unreal.Vector(0, 0, 0), rotation=unreal.Rotator(0, 0, 0), spatially_loaded=False):
    actor = find_actor(label)
    if actor is None:
        actor = actor_subsystem().spawn_actor_from_class(cls, location, rotation)
        actor.set_actor_label(label)
    else:
        actor.set_actor_location_and_rotation(location, rotation, False, False)
    try:
        actor.set_editor_property("is_spatially_loaded", spatially_loaded)
    except Exception:  # noqa: BLE001 - non-partitioned test maps
        pass
    return actor


def open_map():
    exists = EAL.does_asset_exist(MAP)
    if exists and FRESH:
        folder = MAP.rsplit("/", 1)
        external = "/Game/__ExternalActors__%s" % MAP[len("/Game"):]
        external_objects = "/Game/__ExternalObjects__%s" % MAP[len("/Game"):]
        log("deleting %s and its external actors" % MAP)
        EAL.delete_asset(MAP)
        for d in (external, external_objects):
            if EAL.does_directory_exist(d):
                EAL.delete_directory(d)
        exists = False
    if exists:
        level_subsystem().load_level(MAP)
        log("loaded " + MAP)
        return False
    if not level_subsystem().new_level(MAP, True):
        raise RuntimeError("could not create " + MAP)
    level_subsystem().load_level(MAP)
    log("created %s (World Partition)" % MAP)
    return True


def stage_map(world):
    open_map()
    # Streamed content (foliage chunks, city districts) loads within ~1.6 km; terrain, water and sky are always loaded.
    WB.configure_world_partition_grid(51200, 160000)

    sun = spawn(unreal.DirectionalLight, "LaPlace_Sun", unreal.Vector(0, 0, 100000), unreal.Rotator(0, -38, 35))
    comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("atmosphere_sun_light", True)
    comp.set_editor_property("intensity", 10.0)
    comp.set_editor_property("light_source_angle", 1.0)
    comp.set_editor_property("dynamic_shadow_distance_movable_light", 30000.0)
    comp.set_editor_property("far_shadow_cascade_count", 0)

    spawn(unreal.SkyAtmosphere, "LaPlace_SkyAtmosphere")
    sky = spawn(unreal.SkyLight, "LaPlace_SkyLight", unreal.Vector(0, 0, 100000))
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_comp.set_editor_property("real_time_capture", True)

    fog = spawn(unreal.ExponentialHeightFog, "LaPlace_HeightFog", unreal.Vector(0, 0, 0))
    fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_comp.set_editor_property("fog_density", 0.012)
    fog_comp.set_editor_property("fog_height_falloff", 0.06)
    fog_comp.set_editor_property("start_distance", 8000.0)
    fog_comp.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.35, 0.45, 0.6, 1.0))
    fog_comp.set_editor_property("directional_inscattering_luminance", unreal.LinearColor(0.9, 0.75, 0.5, 1.0))

    clouds = spawn(unreal.VolumetricCloud, "LaPlace_Clouds")
    cloud_mat = unreal.load_asset("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst")
    if cloud_mat:
        clouds.get_component_by_class(unreal.VolumetricCloudComponent).set_editor_property("material", cloud_mat)

    post = spawn(unreal.PostProcessVolume, "LaPlace_Post")
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    for prop, value in (("override_bloom_intensity", True), ("bloom_intensity", 0.6),
                        ("override_vignette_intensity", True), ("vignette_intensity", 0.25),
                        ("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.3)):
        try:
            settings.set_editor_property(prop, value)
        except Exception as exc:  # noqa: BLE001
            warn("post setting %s: %s" % (prop, exc))
    post.set_editor_property("settings", settings)

    cls = unreal.load_class(None, "/Script/MushokuRPG.MTDayNightController")
    if cls:
        ctrl = spawn(cls, "LaPlace_DayNight", unreal.Vector(0, 0, 50000))
        ctrl.set_editor_property("sun_light", sun)
        ctrl.set_editor_property("sky_light", sky)
        ctrl.set_editor_property("height_fog", fog)
        ctrl.set_editor_property("preview_hour", 9.0)
        ctrl.set_editor_property("fog_density_day", 0.012)
        ctrl.set_editor_property("fog_density_night", 0.02)

    ws = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
    ws.set_editor_property("kill_z", -60000.0)
    log("sky, sun, fog, clouds, post and day-night placed")


# --------------------------------------------------------------------------------------------- landscape

def landscape_query_setup(world):
    ls = world["Landscape"]
    loc = ls["LocationCm"]
    scale = ls["ScaleCm"]
    return WB.load_heightmap_for_queries(src_path(ls["Heightmap"]), ls["VerticesX"], ls["VerticesY"],
                                         unreal.Vector(loc["X"], loc["Y"], loc["Z"]), unreal.Vector(scale["X"], scale["Y"], scale["Z"]))


def stage_landscape(world):
    for a in actor_subsystem().get_all_level_actors():
        if isinstance(a, unreal.LandscapeProxy):
            log("landscape already present (%s): run with MT_WORLD_FRESH=1 to re-import" % a.get_actor_label())
            return
    ls = world["Landscape"]
    names, files = landscape_layers(world)
    infos = []
    for name in names:
        dbg = LAYER_DEBUG.get(name, (0.5, 0.5, 0.5))
        infos.append(WB.create_landscape_layer_info(LAYER_DIR, name, unreal.LinearColor(dbg[0], dbg[1], dbg[2], 1.0)))
    material = unreal.load_asset(MAT_DIR + "/M_LaPlace_Landscape")
    loc, scale = ls["LocationCm"], ls["ScaleCm"]
    landscape = WB.import_landscape(src_path(ls["Heightmap"]), ls["VerticesX"], ls["VerticesY"], ls["SectionsPerComponent"],
                                    ls["QuadsPerSection"], unreal.Vector(loc["X"], loc["Y"], loc["Z"]),
                                    unreal.Vector(scale["X"], scale["Y"], scale["Z"]), material, infos, files, 2, False,
                                    "LaPlace_Landscape")
    if landscape is None:
        raise RuntimeError("landscape import failed (see LogMTWorldBuild)")
    log("landscape imported: %d x %d" % (ls["VerticesX"], ls["VerticesY"]))


# --------------------------------------------------------------------------------------------- water

def triangulate(poly):
    """Ear clipping for a simple polygon [(x, y), ...] (lake outlines, ~100 points). Returns index triples."""
    pts = list(poly)
    if len(pts) > 3 and pts[0] == pts[-1]:
        pts = pts[:-1]
    n = len(pts)
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    idx = list(range(n)) if area > 0 else list(reversed(range(n)))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def inside(p, a, b, c):
        return cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0

    tris = []
    guard = 0
    while len(idx) > 3 and guard < 20000:
        guard += 1
        clipped = False
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = pts[i0], pts[i1], pts[i2]
            if cross(a, b, c) <= 0:
                continue
            if any(inside(pts[j], a, b, c) for j in idx if j not in (i0, i1, i2)):
                continue
            tris += [i0, i1, i2]
            idx.pop(k)
            clipped = True
            break
        if not clipped:  # degenerate outline: fan the rest
            break
    for k in range(1, len(idx) - 1):
        tris += [idx[0], idx[k], idx[k + 1]]
    return pts, tris


def place_mesh_actor(label, mesh, location, tags):
    actor = spawn(unreal.StaticMeshActor, label, location, unreal.Rotator(0, 0, 0))
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    comp.set_static_mesh(mesh)
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("cast_shadow", False)
    actor.set_editor_property("tags", tags)
    return actor


def stage_water(world):
    WB.destroy_actors_with_tag("MTWater")
    ocean_mat = unreal.load_asset(MAT_DIR + "/M_LaPlace_Ocean")
    lake_mat = unreal.load_asset(MAT_DIR + "/M_LaPlace_Lake")
    river_mat = unreal.load_asset(MAT_DIR + "/M_LaPlace_River")
    ice_mat = unreal.load_asset(MAT_DIR + "/M_LaPlace_Ice")
    # Ocean: a 16 x 16 quad grid 120 km across at sea level (Z = 0), centred on the world.
    half, n = 6000000.0, 16
    verts, tris = [], []
    for y in range(n + 1):
        for x in range(n + 1):
            verts.append(unreal.Vector(-half + 2 * half * x / n, -half + 2 * half * y / n, 0.0))
    for y in range(n):
        for x in range(n):
            a = y * (n + 1) + x
            tris += [a, a + n + 1, a + 1, a + 1, a + n + 1, a + n + 2]
    mesh = WB.create_static_mesh_asset(WATER_DIR + "/SM_Ocean", verts, tris, [], ocean_mat, False, False)
    place_mesh_actor("LaPlace_Ocean", mesh, unreal.Vector(0, 0, 0), ["MTWater"])
    lakes = 0
    for lake in world.get("Lakes", []):
        poly = [(p["X"], p["Y"]) for p in lake.get("Polygon", [])]
        if len(poly) < 3:
            continue
        pts, idx = triangulate(poly)
        cx = sum(p[0] for p in pts) / len(pts)
        cy = sum(p[1] for p in pts) / len(pts)
        z = float(lake.get("WaterZ", 0.0))
        verts = [unreal.Vector(p[0] - cx, p[1] - cy, 0.0) for p in pts]
        mat = ice_mat if lake.get("Frozen") else lake_mat
        mesh = WB.create_static_mesh_asset("%s/SM_Lake_%s" % (WATER_DIR, lake["Id"]), verts, idx, [], mat, False, False)
        if mesh:
            place_mesh_actor("LaPlace_Lake_" + lake["Id"], mesh, unreal.Vector(cx, cy, z), ["MTWater"])
            lakes += 1
    rivers = 0
    for river in world.get("Rivers", []):
        pts = river.get("Points", [])
        if len(pts) < 2:
            continue
        cx = sum(p["X"] for p in pts) / len(pts)
        cy = sum(p["Y"] for p in pts) / len(pts)
        cz = min(p["Z"] for p in pts)
        verts, uvs, tris = [], [], []
        dist = 0.0
        for i, p in enumerate(pts):
            q0 = pts[max(i - 1, 0)]
            q1 = pts[min(i + 1, len(pts) - 1)]
            dx, dy = q1["X"] - q0["X"], q1["Y"] - q0["Y"]
            length = math.hypot(dx, dy) or 1.0
            nx, ny = -dy / length, dx / length
            w = 0.5 * float(p.get("WidthCm", 600.0)) * 1.15
            if i > 0:
                dist += math.hypot(p["X"] - pts[i - 1]["X"], p["Y"] - pts[i - 1]["Y"])
            z = p["Z"] - cz
            verts.append(unreal.Vector(p["X"] - cx + nx * w, p["Y"] - cy + ny * w, z))
            verts.append(unreal.Vector(p["X"] - cx - nx * w, p["Y"] - cy - ny * w, z))
            uvs.append(unreal.Vector2D(dist / 100.0, 0.0))
            uvs.append(unreal.Vector2D(dist / 100.0, w * 2 / 100.0))
            if i > 0:
                a = 2 * (i - 1)
                tris += [a, a + 1, a + 2, a + 2, a + 1, a + 3]
        mesh = WB.create_static_mesh_asset("%s/SM_River_%s" % (WATER_DIR, river["Id"]), verts, tris, uvs, river_mat, False, False)
        if mesh:
            place_mesh_actor("LaPlace_River_" + river["Id"], mesh, unreal.Vector(cx, cy, cz), ["MTWater"])
            rivers += 1
    log("water: ocean, %d lakes, %d rivers" % (lakes, rivers))


# --------------------------------------------------------------------------------------------- instances

def stage_instances(world):
    folder = os.path.join(SRC, "Scatter")
    if not os.path.isdir(folder):
        warn("no instance sets yet (%s)" % folder)
        return
    for fname in sorted(os.listdir(folder)):
        if not fname.endswith(".json"):
            continue
        name = os.path.splitext(fname)[0]
        binary = os.path.join(folder, name + ".bin")
        if not os.path.exists(binary):
            continue
        tag = "MTSet_" + name
        removed = WB.destroy_actors_with_tag(tag)
        count = WB.spawn_instance_set_from_file(os.path.join(folder, fname), binary, tag)
        log("instance set %s: %d instances (replaced %d actors)" % (name, count, removed))


def stage_save():
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("saved")


def main():
    world = load_world()
    log("stages: %s  map: %s  source: %s" % (",".join(STAGES), MAP, SRC))
    if "textures" in STAGES:
        stage_textures()
    if "materials" in STAGES:
        stage_materials(world)
    if any(s in STAGES for s in ("map", "landscape", "water", "instances")):
        if "map" in STAGES:
            stage_map(world)
        else:
            open_map()
    if "landscape" in STAGES:
        stage_landscape(world)
    if "water" in STAGES:
        stage_water(world)
    if "instances" in STAGES:
        stage_instances(world)
    if "save" in STAGES:
        stage_save()
    log("done")


main()
