"""
mt_world_setup.py - one-shot editor setup for the Fittoa / Buena Village map (UE 5.3-5.5 editor Python).

Run inside the editor:  Tools > Execute Python Script... > Content/Python/mt_world_setup.py
       or from the Output Log (Python):  exec(open(unreal.Paths.project_content_dir() + "Python/mt_world_setup.py").read())

What it does (idempotent: re-running reuses the existing map/actors it created):
  1. Creates /Game/Maps/L_Fittoa from the Open World template (World Partition enabled) or loads it if it exists.
  2. Spawns AMTDayNightController and wires sun / sky light / height fog from the template (sun set to Movable).
  3. Spawns AMTVillageGenerator at the Buena square (from Content/Data/Fittoa_Roads.json) - press Generate on it
     AFTER the landscape has been imported (it traces the ground).
  4. Spawns a NavMeshBoundsVolume over the playable area and a PlayerStart at Buena_Square.
  5. Tries to create an HLOD layer asset (guarded) and prints World Partition / HLOD / Data Layer guidance.
  6. Prints the exact landscape import settings for SourceArt/Terrain/Fittoa/Fittoa_Height_2017.png.
"""
import json
import os

import unreal

MAP_PATH = "/Game/Maps/L_Fittoa"
TEMPLATE = "/Engine/Maps/Templates/OpenWorld"
LABEL_PREFIX = "MT_"


def log(msg):
    unreal.log("[mt_world_setup] " + msg)


def warn(msg):
    unreal.log_warning("[mt_world_setup] " + msg)


def content_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as exc:  # noqa: BLE001 - editor script, report and continue
        warn("could not read %s: %s" % (path, exc))
        return None


def actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def level_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def open_or_create_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        log("map exists, loading " + MAP_PATH)
        level_subsystem().load_level(MAP_PATH)
        return True
    log("creating %s from template %s (World Partition)" % (MAP_PATH, TEMPLATE))
    ok = False
    try:
        ok = level_subsystem().new_level_from_template(MAP_PATH, TEMPLATE)
    except Exception as exc:  # older 5.x: only the (now deprecated) EditorLevelLibrary has it
        warn("LevelEditorSubsystem.new_level_from_template failed (%s), trying EditorLevelLibrary" % exc)
    if not ok:
        ok = unreal.EditorLevelLibrary.new_level_from_template(MAP_PATH, TEMPLATE)
    if not ok:
        unreal.log_error("[mt_world_setup] could not create " + MAP_PATH)
    return ok


def all_actors():
    return actor_subsystem().get_all_level_actors()


def find_first(cls):
    for a in all_actors():
        if isinstance(a, cls):
            return a
    return None


def find_by_label(label):
    for a in all_actors():
        if a.get_actor_label() == label:
            return a
    return None


def spawn(cls, label, location, rotation=unreal.Rotator(0, 0, 0)):
    existing = find_by_label(label)
    if existing:
        log("reusing " + label)
        existing.set_actor_location(location, False, False)
        return existing
    actor = actor_subsystem().spawn_actor_from_class(cls, location, rotation)
    if actor:
        actor.set_actor_label(label)
        log("spawned %s at %s" % (label, location))
    else:
        warn("failed to spawn " + label)
    return actor


def vec(d, dz=0.0):
    return unreal.Vector(float(d["X"]), float(d["Y"]), float(d["Z"]) + dz)


def setup_day_night():
    cls = unreal.load_class(None, "/Script/MushokuRPG.MTDayNightController")
    if not cls:
        warn("MTDayNightController class not found - build the C++ module first")
        return
    ctrl = spawn(cls, LABEL_PREFIX + "DayNightController", unreal.Vector(0, 0, 500))
    if not ctrl:
        return
    sun = find_first(unreal.DirectionalLight)
    sky = find_first(unreal.SkyLight)
    fog = find_first(unreal.ExponentialHeightFog)
    if sun:
        comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
        if comp:
            comp.set_mobility(unreal.ComponentMobility.MOVABLE)  # the controller rotates it at runtime
        ctrl.set_editor_property("sun_light", sun)
    if sky:
        ctrl.set_editor_property("sky_light", sky)
    if fog:
        ctrl.set_editor_property("height_fog", fog)
    log("DayNight wired: sun=%s sky=%s fog=%s (moon optional: add a 2nd DirectionalLight and set 'moon_light')"
        % (bool(sun), bool(sky), bool(fog)))


def make_circle_zone(reason, loc, radius_cm):
    zone = unreal.MTExclusionZone()
    zone.set_editor_property("reason", reason)
    zone.set_editor_property("shape", unreal.MTZoneShape.CIRCLE)
    zone.set_editor_property("center", unreal.Vector2D(float(loc["X"]), float(loc["Y"])))
    zone.set_editor_property("radius", float(radius_cm))
    return zone


def setup_generator(roads, locations=None):
    cls = unreal.load_class(None, "/Script/MushokuRPG.MTVillageGenerator")
    if not cls:
        warn("MTVillageGenerator class not found - build the C++ module first")
        return
    center = roads["Village"]["Center"] if roads else {"X": -36800.0, "Y": 29200.0, "Z": 2945.0}
    gen = spawn(cls, LABEL_PREFIX + "BuenaVillageGenerator", vec(center, 50.0))
    if gen:
        gen.set_editor_property("seed", 1717)
        gen.set_editor_property("roads_json_path", "Data/Fittoa_Roads.json")
        # Hand-placed hero spots the generator must leave free (Greyrat house = hero asset, lone-tree hill top).
        by_id = {row["LocationID"]: row for row in (locations or [])}
        zones = []
        try:
            if "Buena_Greyrat_House" in by_id:
                zones.append(make_circle_zone("GreyratHouse", by_id["Buena_Greyrat_House"]["WorldLocation"], 1800))
            if "Buena_Hill_Tree" in by_id:
                zones.append(make_circle_zone("HillTree", by_id["Buena_Hill_Tree"]["WorldLocation"], 3000))
            gen.set_editor_property("extra_exclusion_zones", zones)
        except Exception as exc:
            warn("could not set exclusion zones (%s) - add GreyratHouse/HillTree zones by hand" % exc)
        log("VillageGenerator placed. After importing the landscape: select it > Details > Village > Generate.")


def setup_nav_and_start(locations):
    by_id = {row["LocationID"]: row for row in locations} if locations else {}
    # Playable area = inner 1.8 km of the 2.016 km landscape (the outer rim is backdrop terrain).
    half_xy = 90000.0
    nav = spawn(unreal.NavMeshBoundsVolume, LABEL_PREFIX + "NavMeshBounds", unreal.Vector(0, 0, 12000))
    if nav:
        # Default volume brush is 200 uu; scale it to 1800 m x 1800 m x 400 m.
        nav.set_actor_scale3d(unreal.Vector(half_xy / 100.0, half_xy / 100.0, 20000.0 / 100.0))
    square = by_id.get("Buena_Square")
    if square:
        spawn(unreal.PlayerStart, LABEL_PREFIX + "PlayerStart_BuenaSquare", vec(square["WorldLocation"], 120.0))


def try_create_hlod_layer():
    try:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        path = "/Game/Maps/HLOD"
        full = path + "/HLOD_Fittoa_Instanced"
        if unreal.EditorAssetLibrary.does_asset_exist(full):
            asset = unreal.EditorAssetLibrary.load_asset(full)
            log("HLOD layer already exists")
        else:
            asset = tools.create_asset("HLOD_Fittoa_Instanced", path, unreal.HLODLayer, unreal.HLODLayerFactory())
            if asset:
                log("created HLOD layer " + asset.get_path_name())
        if asset:
            # EHLODLayerType is exposed as unreal.HLODLayerType (UE 5.8); older builds used WorldPartitionHLODLayerType.
            layer_type = getattr(unreal, "HLODLayerType", None) or getattr(unreal, "WorldPartitionHLODLayerType", None)
            try:
                asset.set_editor_property("layer_type", layer_type.INSTANCING)
                unreal.EditorAssetLibrary.save_loaded_asset(asset)
                log("HLOD layer type: Instancing")
            except Exception as exc:  # property name/enum differs between 5.x versions
                warn("HLOD layer exists but its type could not be set (%s) - set 'Layer Type = Instancing' by hand" % exc)
    except Exception as exc:
        warn("HLOD layer asset creation not available from Python here (%s) - create it by hand (see below)" % exc)


def print_guidance(layout):
    lines = [
        "",
        "================ Fittoa world setup - manual steps ================",
        "LANDSCAPE (Landscape mode > New > Import from File):",
        "  Heightmap file : SourceArt/Terrain/Fittoa/Fittoa_Height_2017.png  (16-bit, 2017x2017; .r16 also provided)",
        "  Location       : (0, 0, 0)   <- UE centres the new landscape on this, actor ends at (-100800, -100800, 0)",
        "  Rotation       : (0, 0, 0)",
        "  Scale          : X=100 Y=100 Z=100  (1 m/sample; 16-bit value 32768 == Z 0 cm; 1 unit = 0.78125 cm)",
        "  Section size   : 63x63 quads, Sections/Component 2x2, Components 16x16 (resolution 2017x2017)",
        "  World Partition: Landscape grid size 2 components (252 m streaming proxies)",
        "  Layers (weight-blended, import each PNG as its layer): grass, farmland, forest_floor, dirt_road, riverbed, rock",
        "    -> SourceArt/Terrain/Fittoa/Fittoa_W_<layer>.png (they sum to 255 per pixel)",
        "  Roads are ONLY the dirt_road paint layer (no road meshes: nothing coplanar with the ground -> no z-fighting).",
        "WORLD PARTITION (World Settings > World Partition):",
        "  Runtime grid 'MainGrid': Cell Size 12800 cm, Loading Range 25600 cm, Block On Slow Streaming = off,",
        "  Priority 0. Add an HLOD layer 'HLOD_Fittoa_Instanced' (Instancing) for foliage/props and",
        "  'HLOD_Fittoa_Merged' (Merged mesh) for buildings; set them on the actors / as the grid default HLOD layer.",
        "  Build HLODs: Build > Build HLODs (or -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionHLODsBuilder).",
        "  Sun, sky light, fog, sky atmosphere, MT_DayNightController, NavMeshBounds: Is Spatially Loaded = OFF.",
        "DATA LAYERS (Window > World Partition > Data Layers): create Data Layer assets + instances:",
        "  Buena_Day (runtime, initially Activated): day-only props/NPC markers",
        "  Buena_Night (runtime, initially Loaded): lamps/window emissives tagged MTNightLight; activate on OnNightChanged",
        "  Quests (runtime, initially Unloaded): quest actors, streamed in by the quest system",
        "VALIDATION: run Content/Python/mt_validate_world.py in the editor, or",
        "  UnrealEditor-Cmd MushokuRPG -run=MTValidateWorld -map=/Game/Maps/L_Fittoa  (writes Saved/Validation/L_Fittoa.json)",
        "==================================================================",
    ]
    if layout:
        hm = layout.get("Heightmap", {})
        lines.insert(3, "  (terrain height %.1f m .. %.1f m)" % (hm.get("MinHeightCm", 0) / 100.0, hm.get("MaxHeightCm", 0) / 100.0))
    for line in lines:
        unreal.log(line)


def try_create_data_layers():
    names = ["Buena_Day", "Buena_Night", "Quests"]
    try:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.DataLayerFactory()
        for name in names:
            path = "/Game/Maps/DataLayers/" + name
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                continue
            asset = tools.create_asset(name, "/Game/Maps/DataLayers", unreal.DataLayerAsset, factory)
            if asset:
                try:
                    asset.set_editor_property("data_layer_type", unreal.DataLayerType.RUNTIME)
                except Exception:
                    pass
                log("created Data Layer asset " + path + " (add an instance of it to L_Fittoa in the Data Layers outliner)")
    except Exception as exc:
        warn("Data Layer assets could not be created from Python (%s) - create them by hand (see guidance)" % exc)


def main():
    if not open_or_create_map():
        return
    locations = load_json(os.path.join(content_dir(), "Data", "Locations.json"))
    roads = load_json(os.path.join(content_dir(), "Data", "Fittoa_Roads.json"))
    layout = load_json(os.path.join(project_dir(), "SourceArt", "Terrain", "Fittoa", "fittoa_layout.json"))
    setup_day_night()
    setup_generator(roads, locations)
    setup_nav_and_start(locations)
    try_create_hlod_layer()
    try_create_data_layers()
    print_guidance(layout)
    try:
        level_subsystem().save_current_level()
        log("saved " + MAP_PATH)
    except Exception as exc:
        warn("save failed: %s" % exc)


main()
