"""
mt_validate_world.py - run UMTWorldValidationLibrary on the currently open editor level and print the issues.

Run in the editor: Tools > Execute Python Script... > Content/Python/mt_validate_world.py
Also writes Saved/Validation/<map>_editor.json. (Headless / CI: UnrealEditor-Cmd MushokuRPG -run=MTValidateWorld -map=...)
Note: with World Partition only the currently LOADED actors are scanned - load the region first (or use the commandlet).
"""
import os

import unreal


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        unreal.log_error("[mt_validate_world] no editor world")
        return 2
    lib = unreal.MTWorldValidationLibrary
    issues = lib.run_all_checks(world)
    errors = lib.count_issues_of_severity(issues, unreal.MTValidationSeverity.ERROR)
    warnings = lib.count_issues_of_severity(issues, unreal.MTValidationSeverity.WARNING)
    for issue in issues:
        loc = issue.location
        text = "[%s] %s (A=%s B=%s @ %.0f, %.0f, %.0f)" % (
            issue.category, issue.message, issue.actor_a_name, issue.actor_b_name, loc.x, loc.y, loc.z)
        if issue.severity == unreal.MTValidationSeverity.ERROR:
            unreal.log_error(text)
        else:
            unreal.log_warning(text)
    map_name = world.get_name()
    out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Validation")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, map_name + "_editor.json")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(lib.issues_to_json(issues, map_name))
    unreal.log("[mt_validate_world] %s: %d errors, %d warnings -> %s" % (map_name, errors, warnings, out_path))
    return 1 if errors else 0


main()
