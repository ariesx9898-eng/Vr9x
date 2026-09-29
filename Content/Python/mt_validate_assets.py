"""Project-wide asset check, run headless after Tools/mac/build_and_setup.sh:

    Tools/mac/run_editor_python.sh mt_validate_assets.py

  1. every asset under /Game loads (maps and World Partition actor packages are checked through 2 and 3),
  2. no package - including every map's external actors - has a hard reference to a /Game or /Engine package that
     does not exist,
  3. every map opens,
  4. every /Game path named in Content/Data/*.json that points into content the Mac pipeline generates (LA PLACE,
     Rudeus, Orsted, materials, maps) exists. Paths elsewhere (/Game/UI/Icons, /Game/Audio, /Game/Characters/Enemies,
     /Game/VFX ...) name art from the original design that nobody has made yet; the game falls back for each (runtime
     VFX presets, element tiles with initials for icons, no sound), so they are counted as "not authored yet" instead
     of failing.

Ends with "[mt_validate_assets] RESULT: OK" or "... RESULT: <n> problem(s)", one PROBLEM line each.
"""
import json
import os

import unreal

TAG = "[mt_validate_assets]"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AR = unreal.AssetRegistryHelpers.get_asset_registry()
GENERATED_ROOTS = ("/Game/LaPlace/", "/Game/Characters/Rudeus/", "/Game/Characters/Orsted/", "/Game/Materials/", "/Game/Maps/")
PROBLEMS = []


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def problem(msg):
    PROBLEMS.append(msg)
    unreal.log_warning("%s PROBLEM %s" % (TAG, msg))


def package_exists(package):
    return len(AR.get_assets_by_package_name(package)) > 0 or unreal.EditorAssetLibrary.does_asset_exist(package)


def hard_dependencies(package):
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                                    include_searchable_names=False, include_soft_management_references=False,
                                                    include_hard_management_references=False)
    return [str(d) for d in (AR.get_dependencies(package, options) or [])]


def check_assets():
    AR.search_all_assets(True)
    assets = AR.get_assets_by_path("/Game", recursive=True)
    packages = sorted({str(a.package_name) for a in assets})
    maps = sorted({str(a.package_name) for a in assets if str(a.asset_class_path.asset_name) == "World"})
    loaded = failed = 0
    for a in assets:
        package = str(a.package_name)
        if package.startswith(("/Game/__ExternalActors__", "/Game/__ExternalObjects__")) or package in maps:
            continue
        path = "%s.%s" % (package, a.asset_name)
        if unreal.load_asset(path) is None:
            failed += 1
            problem("does not load: " + path)
        else:
            loaded += 1
    missing = 0
    for package in packages:
        for dep in hard_dependencies(package):
            if dep.startswith(("/Game/", "/Engine/")) and not package_exists(dep):
                missing += 1
                problem("%s has a hard reference to missing %s" % (package, dep))
    external = sum(1 for p in packages if p.startswith("/Game/__External"))
    log("assets: %d loaded, %d failed; %d packages (%d World Partition actor packages) scanned for hard references, %d missing"
        % (loaded, failed, len(packages), external, missing))
    return maps


def check_maps(maps):
    for package in maps:
        world = unreal.EditorLoadingAndSavingUtils.load_map(package)
        if world is None:
            problem("map does not open: " + package)
            continue
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        log("map %s opens (%d loaded actors)" % (package, len(actors)))


def walk(value, field, out):
    if isinstance(value, dict):
        for k, v in value.items():
            walk(v, k, out)
    elif isinstance(value, list):
        for v in value:
            walk(v, field, out)
    elif isinstance(value, str) and value.startswith("/Game/"):
        out.append((field, value))


def check_data_references():
    folder = os.path.join(PROJECT, "Content", "Data")
    checked = 0
    unauthored = {}
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".json"):
            continue
        with open(os.path.join(folder, name), "r", encoding="utf-8") as f:
            data = json.load(f)
        refs = []
        walk(data, None, refs)
        for field, path in refs:
            checked += 1
            if package_exists(path.split(".")[0]):
                continue
            if path.startswith(GENERATED_ROOTS):
                problem("%s: %s -> missing %s" % (name, field, path))
            else:
                root = "/".join(path.split("/")[:4])
                unauthored.setdefault(root, set()).add(path)
    log("data references: %d checked, %d distinct not authored yet (fallbacks in game): %s" % (
        checked, sum(len(v) for v in unauthored.values()),
        ", ".join("%s %d" % (root, len(paths)) for root, paths in sorted(unauthored.items()))))


def main():
    maps = check_assets()
    check_data_references()
    check_maps(maps)
    if PROBLEMS:
        log("RESULT: %d problem(s)" % len(PROBLEMS))
    else:
        log("RESULT: OK")


main()
