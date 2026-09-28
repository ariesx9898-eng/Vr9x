"""Point a lineage's data at its own exported clips (run after its <C>_Animated.glb is built).

    python3 Tools/anim/use_character_clips.py Orsted [--dry-run]

Reads SourceArt/Characters/<C>/<C>_Animated.anim.json (written by build_rudeus_anims.py) and updates:
  * Content/Data/AnimSets.json, the <C> row: every key the character has authored points at
    /Game/Characters/<C>/Animations/A_<C>_<Key>; reference speeds come from the authored gaits.
    Keys it has not authored keep their current value (and are reported).
  * Content/Data/Abilities.json: the lineage's signature abilities use their dedicated clips (ABILITY_CLIPS).
    Every other ability keeps its canonical A_Rudeus_<Key> path; AMTCharacterBase::ResolveLineageAnim plays
    the caster's own A_<C>_<Key> at runtime.
The files keep their formatting (2-space JSON). Run Tools/validate_data.py afterwards.
"""
import argparse
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "Content", "Data")

# AnimSet reference-speed field -> gait clip it was authored for (same table as Tools/validate_data.py)
SPEED_FIELDS = {"WalkSpeedRef": "Walk", "WalkBackSpeedRef": "WalkBack", "StrafeSpeedRef": "StrafeLeft",
                "RunSpeedRef": "Run", "SprintSpeedRef": "Sprint", "RunStrafeSpeedRef": "RunStrafeLeft",
                "RunBackSpeedRef": "RunBack"}

# Signature abilities whose clip key differs from the shared one they borrow today.
ABILITY_CLIPS = {
    "Orsted": {
        "Orsted_Basic": "CastBasic",
        "Orsted_DisturbMagic": "DisturbMagic",
        "Orsted_DragonStep": "DragonStep",
        "Orsted_DragonStep_Awakened": "DragonStep",
        "Orsted_SaintDragonAura": "Aura",
        "Orsted_Awakening_DragonGod": "Awakening",
    },
}


def clip_path(character, key):
    name = "A_%s_%s" % (character, key)
    return "/Game/Characters/%s/Animations/%s.%s" % (character, name, name)


def load(name):
    with open(os.path.join(DATA, name), encoding="utf-8") as f:
        return json.load(f)


def save(name, rows):
    path = os.path.join(DATA, name)
    with open(path, encoding="utf-8") as f:
        trailing_newline = f.read().endswith("\n")  # keep each file's own convention: minimal diffs
    with open(path, "w", encoding="utf-8") as f:
        json.dump(rows, f, indent=2, ensure_ascii=False)
        if trailing_newline:
            f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("character")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    c = a.character
    sidecar_path = os.path.join(ROOT, "SourceArt", "Characters", c, c + "_Animated.anim.json")
    if not os.path.exists(sidecar_path):
        sys.exit("no %s: build the clips first (Tools/anim/build_rudeus_anims.py --character %s)" % (sidecar_path, c))
    clips = json.load(open(sidecar_path, encoding="utf-8"))["clips"]

    anim_sets = load("AnimSets.json")
    row = next((r for r in anim_sets if r.get("CharacterID") == c), None)
    if row is None:
        row = {"CharacterID": c, "Anims": {}}
        anim_sets.append(row)
    anims = row.setdefault("Anims", {})
    changed, kept = [], []
    for key in list(anims.keys()):
        if key in clips:
            if anims[key] != clip_path(c, key):
                anims[key] = clip_path(c, key)
                changed.append(key)
        else:
            kept.append(key)
    signature = set(ABILITY_CLIPS.get(c, {}).values())
    for key in sorted(signature):  # lineage-only keys (DisturbMagic, DragonStep, Aura ...)
        if key in clips and anims.get(key) != clip_path(c, key):
            anims[key] = clip_path(c, key)
            changed.append(key)
    for field, gait in SPEED_FIELDS.items():
        speed = clips.get(gait, {}).get("ref_speed_cm_s")
        if speed is not None:
            row[field] = int(round(speed)) if abs(speed - round(speed)) < 1e-6 else speed

    abilities = load("Abilities.json")
    remapped = []
    for ab in abilities:
        key = ABILITY_CLIPS.get(c, {}).get(ab.get("AbilityID"))
        if key and key in clips and ab.get("Montage") != clip_path(c, key):
            ab["Montage"] = clip_path(c, key)
            remapped.append("%s -> A_%s_%s" % (ab["AbilityID"], c, key))

    print("%s: %d AnimSet keys now use A_%s_* clips" % (c, len(changed), c))
    if kept:
        print("  kept (not authored for %s): %s" % (c, ", ".join(kept)))
    print("  reference speeds: " + ", ".join("%s=%s" % (f, row.get(f)) for f in SPEED_FIELDS))
    for line in remapped:
        print("  ability " + line)
    if a.dry_run:
        print("dry run: nothing written")
        return
    save("AnimSets.json", anim_sets)
    save("Abilities.json", abilities)
    print("wrote Content/Data/AnimSets.json and Content/Data/Abilities.json; now run python3 Tools/validate_data.py")


if __name__ == "__main__":
    main()
