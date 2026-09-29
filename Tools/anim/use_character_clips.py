"""Point a lineage's data at its own exported clips (run after its <C>_Animated.glb is built).

    python3 Tools/anim/use_character_clips.py Orsted [--dry-run] [--write-abilities]

Reads SourceArt/Characters/<C>/<C>_Animated.anim.json (written by build_rudeus_anims.py) and updates:
  * Content/Data/AnimSets.json, the <C> row: every key the character has authored points at
    /Game/Characters/<C>/Animations/A_<C>_<Key> (keys the row does not list yet are added, so a new clip such as
    Cast_Fireball reaches both rows); reference speeds come from the authored gaits. Keys it has not authored keep
    their current value (and are reported): that is how a lineage without a clip falls back to another of its own.
  * Content/Data/Abilities.json, only with --write-abilities: the lineage's signature abilities use their dedicated
    clips (ABILITY_CLIPS). Without the flag the remaps are only printed (the ability rows belong to gameplay data).
    Every other ability keeps its canonical A_Rudeus_<Key> path; AMTCharacterBase::ResolveLineageAnim plays the
    caster's own A_<C>_<Key> at runtime whenever the caster's AnimSet row has that key.
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
        "Orsted_DragonCrush": "DragonCrush",
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
    ap.add_argument("--write-abilities", action="store_true", help="also write the signature remaps into Abilities.json")
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
    changed, kept, added = [], [], []
    for key in list(anims.keys()):
        if key in clips:
            if anims[key] != clip_path(c, key):
                anims[key] = clip_path(c, key)
                changed.append(key)
        else:
            kept.append(key)
    for key in clips:  # authored clips the row does not list yet (new casts, lineage-only keys)
        if key not in anims:
            anims[key] = clip_path(c, key)
            added.append(key)
    if added:  # keep a charge set together: <Key>, <Key>_Hold, <Key>_Release (and after an existing <Key>)
        def base(k):
            for suffix, rank in (("_Hold", 1), ("_Release", 2)):
                if k.endswith(suffix):
                    return k[:-len(suffix)], rank
            return k, 0
        order = [k for k in anims if k not in added] + sorted(added, key=lambda k: (base(k)[0] not in anims, list(clips).index(k)))
        placed, result = set(), {}
        for k in order:
            if k in placed:
                continue
            result[k] = anims[k]
            placed.add(k)
            for extra in sorted((a for a in added if base(a)[0] == k and a not in placed), key=lambda a: base(a)[1]):
                result[extra] = anims[extra]
                placed.add(extra)
        anims.clear()
        anims.update(result)
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

    print("%s: %d AnimSet keys now use A_%s_* clips, %d added" % (c, len(changed), c, len(added)))
    if added:
        print("  added: " + ", ".join(added))
    if kept:
        print("  kept (not authored for %s): %s" % (c, ", ".join(kept)))
    print("  reference speeds: " + ", ".join("%s=%s" % (f, row.get(f)) for f in SPEED_FIELDS))
    for line in remapped:
        print("  ability " + line)
    if a.dry_run:
        print("dry run: nothing written")
        return
    save("AnimSets.json", anim_sets)
    written = ["Content/Data/AnimSets.json"]
    if remapped and a.write_abilities:
        save("Abilities.json", abilities)
        written.append("Content/Data/Abilities.json")
    elif remapped:
        print("  (Abilities.json not written: pass --write-abilities to apply the remaps above)")
    print("wrote %s; now run python3 Tools/validate_data.py" % " and ".join(written))


if __name__ == "__main__":
    main()
