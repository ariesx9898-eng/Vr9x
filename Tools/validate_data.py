#!/usr/bin/env python3
"""Offline validator for Content/Data/*.json (no Unreal needed).

The game loads these files with FJsonObjectConverter::JsonArrayStringToUStruct into the USTRUCTs declared in
Source/MushokuRPG/Public/Core/MTTypes.h and MTDataTypes.h. This script parses those headers with regex (enums,
USTRUCT UPROPERTY names + C++ types) and checks every JSON row against them, then runs design/lore rules:

  * JSON parses; each registry file is a top-level array of objects
  * no unknown keys (recursively, incl. nested structs) and value types match the C++ property types
  * enum strings are valid enumerator names; gameplay tags are registered (MTGameplayTags.h)
  * soft object/class paths are well formed (/Game/Dir/Asset.Asset, class paths end in _C, or a native
    /Script/MushokuRPG.Class that exists as a UCLASS in the module headers)
  * every referenced ability / item / enemy / NPC / location / quest id exists
  * AnimSets.json: one row per character, only known clip keys, A_<Char>_<Key> naming, sane reference speeds;
    ability animation paths must be clips listed in an AnimSet (catches typos before the editor does)
  * every element has EXACTLY 3 abilities; character pool is ONLY Rudeus + Orsted
  * every race has passive + ActiveAbility + TransformationAbility; only Human/Migurd/Beast implemented
  * behaviour-specific sanity (zones have radius+duration, dashes distance, counters windows, ...)

Usage:  python3 Tools/validate_data.py [--data Content/Data] [--source Source/MushokuRPG/Public]
Exit code 0 and "ALL CHECKS PASSED" when clean.
"""
import argparse
import json
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Registry files (see UMTDataRegistry::Initialize) -> row struct
FILE_STRUCTS = {
    "Abilities.json": "FMTAbilityData",
    "Characters.json": "FMTCharacterData",
    "Elements.json": "FMTElementData",
    "Races.json": "FMTRaceData",
    "Quests.json": "FMTQuestData",
    "Enemies.json": "FMTEnemyData",
    "Items.json": "FMTItemData",
    "Locations.json": "FMTLocationData",
    "RollConfigs.json": "FMTRollConfig",
    "AnimSets.json": "FMTAnimSetData",
}
REQUIRED_FILES = [f for f in FILE_STRUCTS if f != "Locations.json"]  # Locations.json is owned by the world team

# AnimSet clip keys (A_<Character>_<Key>). Required = authored in Rudeus_Animated.glb; optional = not authored yet.
REQUIRED_ANIM_KEYS = ["Idle", "CombatIdle", "Walk", "WalkBack", "StrafeLeft", "StrafeRight", "Run", "Sprint", "Rise",
                      "Fall", "JumpStart", "Land", "HardLand", "DodgeForward", "DodgeBack", "DodgeLeft", "DodgeRight",
                      "HitFront", "HitBack", "HitLeft", "HitRight", "Stagger", "Knockdown", "Death", "CastBasic",
                      "StoneCannon_Charge", "StoneCannon_Hold", "StoneCannon_Release", "Quagmire", "Barrage",
                      "DemonEye", "Awakening", "CastTwoHand", "CastGround"]
OPTIONAL_ANIM_KEYS = ["TurnLeft90", "TurnRight90"]
# Characters whose AnimSet must list every required key (others fall back gracefully at runtime).
FULL_ANIMSET_CHARACTERS = {"Rudeus"}
# AnimSet speed field -> clip key whose authored ref_speed_cm_s it must match (exporter sidecar *.anim.json).
ANIM_SPEED_FIELDS = {"WalkSpeedRef": "Walk", "WalkBackSpeedRef": "WalkBack", "StrafeSpeedRef": "StrafeLeft",
                     "RunSpeedRef": "Run", "SprintSpeedRef": "Sprint"}
ANIM_PATH_RE = re.compile(r"^/Game/Characters/([A-Za-z0-9_]+)/Animations/A_([A-Za-z0-9]+)_([A-Za-z0-9_]+)\.")

ALLOWED_CHARACTERS = {"Rudeus", "Orsted"}
IMPLEMENTED_RACES = {"Human", "Migurd", "Beast"}
ALLOWED_NPCS = {"NPC_Paul", "NPC_Zenith", "NPC_Lilia", "NPC_FarmerRolf", "NPC_BakerAnna", "NPC_GuardCaptain",
                "NPC_Merchant", "NPC_Herbalist", "NPC_AdventurerGuild"}
ALLOWED_LOCATIONS = {"Buena_Village", "Buena_Square", "Buena_Greyrat_House", "Buena_Farms", "Buena_Hill_Tree",
                     "Buena_Forest_Edge", "Fittoa_River_Ford", "Fittoa_Deep_Forest", "Fittoa_Watchtower_Ruins",
                     "Fittoa_Road_To_Roa", "Roa_Gate", "Fittoa_Wyrm_Foothills"}
# Roll-able / reserved canon characters that must not appear as quest NPCs or in quest text.
FORBIDDEN_QUEST_NAMES = re.compile(r"(?<![A-Za-z])(Eris|Roxy|Sylphiette|Sylphie|Ruijerd|Ghislaine)(?![A-Za-z])", re.I)
CANON_STATUS = {"CANON", "GAMEPLAY ORIGINAL"}
FALLBACK_TAGS = {"State.Casting", "State.Charging", "State.Dodging", "State.Staggered", "State.Blocking",
                 "State.Countering", "State.Awakened", "State.Transforming", "State.Foresight", "State.Aura",
                 "State.Dead", "State.InQuagmire", "State.InStorm", "State.InWindField", "State.Frozen",
                 "State.Burning", "State.KnowledgeBuff", "Enemy", "Enemy.Beast", "Enemy.Humanoid", "Enemy.Monster",
                 "Enemy.Boss", "Enemy.Elite", "NPC.Villager"}

# Engine structs FJsonObjectConverter imports field-by-field.
BUILTIN_STRUCTS = {
    "FVector": {"X": "double", "Y": "double", "Z": "double"},
    "FRotator": {"Pitch": "double", "Yaw": "double", "Roll": "double"},
    "FLinearColor": {"R": "float", "G": "float", "B": "float", "A": "float"},
    "FGameplayTagContainer": {"GameplayTags": "TArray<FGameplayTag>"},
    "FGameplayTag": {"TagName": "FName"},
}
SOFT_OBJECT_RE = re.compile(r"^/Game(/[A-Za-z0-9_]+)+\.[A-Za-z0-9_]+$")
# Native classes: /Script/<Module>.<ClassNameWithoutPrefix>, e.g. /Script/MushokuRPG.MTNativeAnimInstance
SCRIPT_CLASS_RE = re.compile(r"^/Script/([A-Za-z0-9_]+)\.([A-Za-z0-9_]+)$")
GAME_MODULE = "MushokuRPG"

errors = []
warnings = []


def err(msg):
    errors.append(msg)


def warn(msg):
    warnings.append(msg)


# ------------------------------------------------------------------------------------------------ header parsing
def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def parse_enums(src):
    enums = {}
    for m in re.finditer(r"UENUM\s*\([^)]*\)\s*enum\s+class\s+(\w+)\s*(?::\s*\w+)?\s*\{(.*?)\}\s*;", src, re.S):
        values = []
        for part in m.group(2).split(","):
            part = re.sub(r"UMETA\s*\(.*?\)", "", part).strip()
            ident = part.split("=")[0].strip()
            if ident:
                values.append(ident)
        enums[m.group(1)] = values
    return enums


def parse_structs(src):
    structs = {}
    for m in re.finditer(r"USTRUCT\s*\([^)]*\)\s*struct\s+(?:\w+_API\s+)?(\w+)[^{;]*\{", src):
        start = m.end() - 1
        depth = 0
        body = ""
        for j in range(start, len(src)):
            if src[j] == "{":
                depth += 1
            elif src[j] == "}":
                depth -= 1
                if depth == 0:
                    body = src[start + 1:j]
                    break
        fields = {}
        for pm in re.finditer(r"UPROPERTY\s*\((?:[^()]|\([^()]*\))*\)\s*(.+?)\s+(\w+)\s*(?:=\s*[^;]+)?;", body, re.S):
            fields[pm.group(2)] = " ".join(pm.group(1).split())
        structs[m.group(1)] = fields
    return structs


def parse_native_classes(source_dir):
    """UCLASS names declared in the module's public headers, without the U/A prefix (as /Script paths spell them)."""
    names = set()
    for dirpath, _, files in os.walk(source_dir):
        for fname in files:
            if not fname.endswith(".h"):
                continue
            src = strip_comments(open(os.path.join(dirpath, fname), encoding="utf-8").read())
            for m in re.finditer(r"UCLASS\s*\((?:[^()]|\([^()]*\))*\)\s*class\s+(?:\w+_API\s+)?([UA])(\w+)", src):
                names.add(m.group(2))
    return names


def parse_tags(path):
    """MTTags::State_Casting -> "State.Casting" (UE_DEFINE uses the same dotted spelling)."""
    if not os.path.exists(path):
        return set(FALLBACK_TAGS)
    src = strip_comments(open(path, encoding="utf-8").read())
    names = re.findall(r"UE_DECLARE_GAMEPLAY_TAG_EXTERN\s*\(\s*(\w+)\s*\)", src)
    return {n.replace("_", ".") for n in names} or set(FALLBACK_TAGS)


# ------------------------------------------------------------------------------------------------ type checking
def split_top(s):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    parts.append(cur.strip())
    return parts


def is_number(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


class Schema:
    def __init__(self, enums, structs, tags, native_classes=None):
        self.enums, self.structs, self.tags = enums, structs, tags
        self.native_classes = native_classes or set()

    def check_scalar_key(self, key, ctype, path):
        if ctype in self.enums:
            if key not in self.enums[ctype]:
                err(f"{path}: map key '{key}' is not a valid {ctype} ({', '.join(self.enums[ctype])})")
        elif ctype not in ("FName", "FString"):
            err(f"{path}: unsupported map key type {ctype}")

    def check(self, value, ctype, path):
        ctype = ctype.strip()
        if ctype in ("float", "double"):
            if not is_number(value):
                err(f"{path}: expected number ({ctype}), got {value!r}")
        elif ctype in ("int32", "int64", "uint8", "int", "uint32"):
            if not isinstance(value, int) or isinstance(value, bool):
                err(f"{path}: expected integer ({ctype}), got {value!r}")
        elif ctype == "bool":
            if not isinstance(value, bool):
                err(f"{path}: expected true/false, got {value!r}")
        elif ctype in ("FName", "FString", "FText"):
            if not isinstance(value, str):
                err(f"{path}: expected string ({ctype}), got {value!r}")
        elif re.match(r"TSoftObjectPtr<\w+>$", ctype):
            self.check_soft_path(value, path, is_class=False)
        elif re.match(r"TSoftClassPtr<\w+>$", ctype):
            self.check_soft_path(value, path, is_class=True)
        elif ctype.startswith("TArray<") and ctype.endswith(">"):
            if not isinstance(value, list):
                err(f"{path}: expected array ({ctype}), got {type(value).__name__}")
                return
            inner = ctype[len("TArray<"):-1]
            for i, v in enumerate(value):
                self.check(v, inner, f"{path}[{i}]")
        elif ctype.startswith("TMap<") and ctype.endswith(">"):
            if not isinstance(value, dict):
                err(f"{path}: expected object ({ctype}), got {type(value).__name__}")
                return
            ktype, vtype = split_top(ctype[len("TMap<"):-1])
            for k, v in value.items():
                self.check_scalar_key(k, ktype, path)
                self.check(v, vtype, f"{path}.{k}")
        elif ctype in self.enums:
            if not isinstance(value, str) or value not in self.enums[ctype]:
                err(f"{path}: '{value}' is not a valid {ctype} ({', '.join(self.enums[ctype])})")
        elif ctype == "FGameplayTag":
            self.check_struct(value, ctype, BUILTIN_STRUCTS[ctype], path)
            if isinstance(value, dict) and value.get("TagName") not in self.tags:
                err(f"{path}: gameplay tag '{value.get('TagName')}' is not registered")
        elif ctype in self.structs:
            self.check_struct(value, ctype, self.structs[ctype], path)
        elif ctype in BUILTIN_STRUCTS:
            self.check_struct(value, ctype, BUILTIN_STRUCTS[ctype], path)
            if ctype == "FLinearColor" and isinstance(value, dict):
                for c, v in value.items():
                    if is_number(v) and not 0.0 <= v <= 1.0:
                        err(f"{path}.{c}: linear colour channel {v} outside 0..1")
        elif ctype.startswith(("TWeakObjectPtr", "TObjectPtr")):
            err(f"{path}: runtime-only property ({ctype}) must not be authored in JSON")
        else:
            err(f"{path}: validator does not know how to check C++ type '{ctype}'")

    def check_struct(self, value, sname, fields, path):
        if not isinstance(value, dict):
            err(f"{path}: expected object ({sname}), got {type(value).__name__}")
            return
        for key, v in value.items():
            if key not in fields:
                close = [f for f in fields if f.lower() == key.lower()]
                hint = f" (did you mean '{close[0]}'?)" if close else ""
                err(f"{path}: unknown key '{key}' for {sname}{hint}")
                continue
            self.check(v, fields[key], f"{path}.{key}")

    def check_soft_path(self, value, path, is_class):
        if not isinstance(value, str):
            err(f"{path}: expected soft path string, got {value!r}")
            return
        if value == "":
            return
        script = SCRIPT_CLASS_RE.match(value)
        if script:
            if not is_class:
                err(f"{path}: '{value}' is a native /Script path; soft object paths must point at /Game assets")
            elif script.group(1) == GAME_MODULE and script.group(2) not in self.native_classes:
                err(f"{path}: native class '{value}' is not a UCLASS in Source/{GAME_MODULE}/Public")
            return
        if not SOFT_OBJECT_RE.match(value):
            err(f"{path}: malformed soft path '{value}' (expected /Game/Dir/Asset.Asset)")
            return
        package, obj = value.rsplit(".", 1)
        base = package.rsplit("/", 1)[1]
        expected = base + "_C" if is_class else base
        if obj != expected:
            err(f"{path}: soft {'class' if is_class else 'object'} path '{value}' should end in '.{expected}'")


# ------------------------------------------------------------------------------------------------ semantic checks
def ids(rows, key):
    out = {}
    for i, r in enumerate(rows):
        if not isinstance(r, dict):
            continue
        k = r.get(key)
        if k in (None, ""):
            err(f"row {i}: missing {key}")
            continue
        if k in out:
            err(f"duplicate {key} '{k}'")
        out[k] = r
    return out


def nonempty(row, key, ctx):
    if not isinstance(row.get(key), str) or not row.get(key).strip():
        err(f"{ctx}: {key} must be a non-empty string")


def check_abilities(abilities, schema, char_ids, quest_ids):
    elements = set(schema.enums["EMTElement"])
    races = set(schema.enums["EMTRace"])
    for aid, a in abilities.items():
        ctx = f"Abilities[{aid}]"
        nonempty(a, "DisplayName", ctx)
        nonempty(a, "Description", ctx)
        b = a.get("Behavior", "Projectile")
        g = lambda k, d=0: a.get(k, d)
        for k in ("ManaCost", "StaminaCost", "Cooldown", "CastTime", "RecoveryTime", "Damage", "Stagger",
                  "Knockback", "Duration", "AOERadius"):
            if is_number(g(k)) and g(k) < 0:
                err(f"{ctx}: {k} must be >= 0")
        if b == "Projectile":
            if g("ProjectileSpeed", 3000) <= 0 or g("ProjectileRadius", 20) <= 0:
                err(f"{ctx}: projectile needs ProjectileSpeed and ProjectileRadius > 0")
            if g("Motion", "Straight") == "Arc" and g("ProjectileGravity") <= 0:
                err(f"{ctx}: Arc projectile needs ProjectileGravity (gravity scale) > 0")
        elif b == "Zone":
            if g("AOERadius") <= 0 or g("Duration") <= 0:
                err(f"{ctx}: zone needs AOERadius > 0 and Duration > 0")
            if g("InnerRadius") and g("InnerRadius") >= g("AOERadius"):
                err(f"{ctx}: InnerRadius must be smaller than AOERadius")
            if g("PulseCount", 1) < 1:
                err(f"{ctx}: PulseCount must be >= 1")
        elif b == "Dash":
            if g("DashDistance") <= 0 or g("DashDuration", 0.2) <= 0:
                err(f"{ctx}: dash needs DashDistance and DashDuration > 0")
        elif b == "Counter":
            if g("CounterWindow") <= 0 or g("CounterRadius") <= 0:
                err(f"{ctx}: counter needs CounterWindow and CounterRadius > 0")
            if g("CounterWindow") > 1.0:
                err(f"{ctx}: CounterWindow {g('CounterWindow')} s is not timing-based (keep <= 1 s)")
        elif b == "Structure":
            if g("StructureCount", 1) < 1 or g("StructureHealth", 200) <= 0 or g("Duration") <= 0:
                err(f"{ctx}: structure needs StructureCount >= 1, StructureHealth > 0, Duration > 0")
        elif b == "Sequence":
            seq = g("Sequence", [])
            if not seq:
                err(f"{ctx}: Sequence ability has no steps")
            for i, step in enumerate(seq):
                sid = step.get("AbilityId")
                if sid not in abilities:
                    err(f"{ctx}.Sequence[{i}]: missing ability '{sid}'")
                elif abilities[sid].get("Behavior", "Projectile") != "Projectile":
                    err(f"{ctx}.Sequence[{i}]: step '{sid}' must be a Projectile row")
                if step.get("Delay", 0.25) < 0:
                    err(f"{ctx}.Sequence[{i}]: negative Delay")
        elif b == "Buff":
            if g("Duration") <= 0:
                err(f"{ctx}: buff needs Duration > 0")
        if g("bChargeable", False):
            if g("MaxChargeTime") <= 0:
                err(f"{ctx}: chargeable ability needs MaxChargeTime > 0 (no infinite charge)")
            for k in ("ChargeSpeedScale", "ChargeDamageScale", "ChargeStaggerScale", "ChargeManaScale"):
                if g(k, 1.0) < 1.0:
                    err(f"{ctx}: {k} should be >= 1 (linear from 1 at zero charge)")
        if g("bIsAwakening", False):
            if not 0 < g("AwakeningMeterCost") <= 100:
                err(f"{ctx}: awakening needs AwakeningMeterCost in (0, 100]")
            if g("TransformationTime") <= 0:
                err(f"{ctx}: awakening needs a TransformationTime")
            if g("MasteryRequirement") < 6:
                err(f"{ctx}: awakenings require MasteryRequirement >= 6")
        stats = g("BuffStats", {})
        for k in ("DamageResistance", "MagicResistance", "StaggerResistance"):
            if not 0 <= stats.get(k, 0) <= 0.9:
                err(f"{ctx}: BuffStats.{k} must be within 0..0.9")
        if g("FX", {}).get("CameraShakeScale", 0) > 0.35:
            err(f"{ctx}: FX.CameraShakeScale > 0.35 (clamped at runtime; keep shakes subtle)")
        for src, dst in g("AbilityOverrides", {}).items():
            for x in (src, dst):
                if x not in abilities:
                    err(f"{ctx}.AbilityOverrides: missing ability '{x}'")
            if src in abilities and dst in abilities and \
                    abilities[src].get("Behavior") != abilities[dst].get("Behavior"):
                err(f"{ctx}.AbilityOverrides: '{src}' -> '{dst}' changes Behavior")
        if g("CharacterRequirement", "") and g("CharacterRequirement") not in char_ids:
            err(f"{ctx}: CharacterRequirement '{g('CharacterRequirement')}' is not a character")
        # UnlockRequirement grammar (UMTProgressionSubsystem::EvaluateUnlock)
        req = g("UnlockRequirement", "")
        for clause in [c.strip() for c in re.split(r"[;,]", req) if c.strip()]:
            parts = clause.split(":")
            if parts[0] == "Level" and len(parts) == 2 and parts[1].isdigit():
                continue
            if parts[0] == "Quest" and len(parts) == 2:
                if parts[1] not in quest_ids:
                    err(f"{ctx}: UnlockRequirement references missing quest '{parts[1]}'")
                continue
            if parts[0] == "Mastery" and len(parts) == 4 and parts[3].isdigit():
                track, key = parts[1], parts[2]
                valid = {"Character": char_ids, "Element": elements - {"None", "Arcane"}, "Race": races}.get(track)
                if valid is None:
                    err(f"{ctx}: unknown mastery track '{track}' in '{clause}'")
                elif key not in valid:
                    err(f"{ctx}: mastery key '{key}' is not a valid {track} in '{clause}'")
                elif int(parts[3]) != g("MasteryRequirement"):
                    warn(f"{ctx}: UnlockRequirement level {parts[3]} != MasteryRequirement {g('MasteryRequirement')}")
                continue
            if parts[0] == "Mastery" and len(parts) == 3 and parts[2].isdigit():
                continue
            err(f"{ctx}: malformed UnlockRequirement clause '{clause}'")


def load_anim_sidecar(character):
    """Exporter metadata next to the animated GLB (SourceArt/Characters/<C>/<C>_Animated.anim.json), if any."""
    path = os.path.join(ROOT, "SourceArt", "Characters", character, character + "_Animated.anim.json")
    if not os.path.exists(path):
        return None
    try:
        return json.load(open(path, encoding="utf-8")).get("clips", {})
    except (json.JSONDecodeError, AttributeError) as e:
        warn(f"{os.path.relpath(path, ROOT)}: unreadable ({e}); clip cross-check skipped")
        return None


def check_anim_sets(anim_sets, characters, abilities):
    known = set(REQUIRED_ANIM_KEYS) | set(OPTIONAL_ANIM_KEYS)
    listed_paths = set()
    sidecars = {}
    for cid in sorted(set(characters) - set(anim_sets)):
        err(f"AnimSets.json: character '{cid}' has no anim set (the native anim instance would show the bind pose)")
    for cid, row in anim_sets.items():
        ctx = f"AnimSets[{cid}]"
        if cid not in characters:
            err(f"{ctx}: CharacterID is not a character in Characters.json")
        anims = row.get("Anims", {})
        if not isinstance(anims, dict):
            continue  # type error already reported by the schema check
        # Non-string values were already reported by the schema check; ignore them here.
        anims = {k: v for k, v in anims.items() if isinstance(v, str)}
        for key in sorted(set(anims) - known):
            err(f"{ctx}.Anims: unknown clip key '{key}' (known: {', '.join(REQUIRED_ANIM_KEYS + OPTIONAL_ANIM_KEYS)})")
        missing = [k for k in REQUIRED_ANIM_KEYS if not anims.get(k)]
        if missing:
            (err if cid in FULL_ANIMSET_CHARACTERS else warn)(
                f"{ctx}.Anims: missing {', '.join(missing)} (runtime falls back to other clips)")
        for key, path in anims.items():
            if not path:
                continue
            listed_paths.add(path)
            m = ANIM_PATH_RE.match(path)
            if not m:
                warn(f"{ctx}.Anims.{key}: '{path}' does not follow /Game/Characters/<C>/Animations/A_<C>_<Key>")
                continue
            folder_char, name_char, name_key = m.groups()
            if folder_char != name_char:
                warn(f"{ctx}.Anims.{key}: folder '{folder_char}' and clip prefix 'A_{name_char}_' disagree")
            if name_key != key:
                warn(f"{ctx}.Anims.{key}: points at clip key '{name_key}' (deliberate reuse?)")
            # Cross-check against the exporter's sidecar for the clip's owner (Orsted may reuse Rudeus clips).
            if name_char not in sidecars:
                sidecars[name_char] = load_anim_sidecar(name_char)
            clips = sidecars[name_char]
            if clips is not None and name_key not in clips:
                warn(f"{ctx}.Anims.{key}: A_{name_char}_{name_key} is not in {name_char}_Animated.anim.json "
                     f"(not exported yet?)")
        refs = {f: row.get(f) for f in ANIM_SPEED_FIELDS}
        for field, value in refs.items():
            if value is not None and (not is_number(value) or value <= 0):
                err(f"{ctx}: {field} must be > 0 (cm/s)")
        walk, run, sprint = (row.get("WalkSpeedRef", 130), row.get("RunSpeedRef", 360), row.get("SprintSpeedRef", 580))
        if all(is_number(v) for v in (walk, run, sprint)) and not walk < run < sprint:
            err(f"{ctx}: expected WalkSpeedRef < RunSpeedRef < SprintSpeedRef ({walk}, {run}, {sprint})")
        # Reference speeds must match what the clips were authored at, or feet slide.
        for field, clip_key in ANIM_SPEED_FIELDS.items():
            m = ANIM_PATH_RE.match(anims.get(clip_key, "") or "")
            clips = sidecars.get(m.group(2)) if m else None
            authored = (clips or {}).get(m.group(3) if m else "", {}).get("ref_speed_cm_s")
            value = row.get(field)
            if authored is not None and value is not None and abs(authored - value) > 0.5:
                warn(f"{ctx}: {field} {value} != authored {authored} cm/s of {m.group(0)[:-1].rsplit('/', 1)[1]}")
        bone = row.get("UpperBodyRootBone", "spine_C0_1_jnt_061")
        if not isinstance(bone, str) or not bone.strip():
            warn(f"{ctx}: UpperBodyRootBone is empty (UMTNativeAnimInstance default bone is used)")

    # Ability animations: character clips must be ones an AnimSet lists (they are what the import step produces).
    for aid, a in abilities.items():
        ctx = f"Abilities[{aid}]"
        for field in ("Montage", "ChargeLoopAnim", "ReleaseAnim"):
            path = a.get(field, "")
            if isinstance(path, str) and path and ANIM_PATH_RE.match(path) and path not in listed_paths:
                err(f"{ctx}.{field}: '{path}' is not a clip of any AnimSet (typo, or add it to AnimSets.json)")
        has_loop, has_release = bool(a.get("ChargeLoopAnim")), bool(a.get("ReleaseAnim"))
        if (has_loop or has_release) and not a.get("bChargeable", False):
            warn(f"{ctx}: ChargeLoopAnim/ReleaseAnim are only used by chargeable abilities (ignored)")
        if has_loop and not has_release and not a.get("MontageReleaseSection"):
            warn(f"{ctx}: charge loop without ReleaseAnim or MontageReleaseSection (the hold just blends out on release)")
        if a.get("bChargeable", False) and a.get("Montage") and not has_loop and not a.get("MontageStartSection"):
            warn(f"{ctx}: chargeable without ChargeLoopAnim: the pose drops back to locomotion while charging")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.path.join(ROOT, "Content", "Data"))
    ap.add_argument("--source", default=os.path.join(ROOT, "Source", "MushokuRPG", "Public"))
    args = ap.parse_args()

    src = ""
    for h in ("Core/MTTypes.h", "Core/MTDataTypes.h"):
        p = os.path.join(args.source, h)
        if not os.path.exists(p):
            print(f"FATAL: header not found: {p}")
            return 2
        src += strip_comments(open(p, encoding="utf-8").read()) + "\n"
    schema = Schema(parse_enums(src), parse_structs(src), parse_tags(os.path.join(args.source, "Core/MTGameplayTags.h")),
                    parse_native_classes(args.source))
    for needed in ("EMTRarity", "EMTElement", "EMTRace", "EMTAbilityBehavior", "EMTQuestType", "EMTObjectiveType"):
        if needed not in schema.enums:
            err(f"header parse: enum {needed} not found")
    for needed in set(FILE_STRUCTS.values()):
        if needed not in schema.structs:
            err(f"header parse: struct {needed} not found")

    # ---- load + structural checks
    data = {}
    for fname, sname in FILE_STRUCTS.items():
        path = os.path.join(args.data, fname)
        if not os.path.exists(path):
            (err if fname in REQUIRED_FILES else warn)(f"{fname}: file missing")
            data[fname] = []
            continue
        try:
            rows = json.load(open(path, encoding="utf-8"))
        except json.JSONDecodeError as e:
            err(f"{fname}: invalid JSON: {e}")
            data[fname] = []
            continue
        if not isinstance(rows, list):
            err(f"{fname}: top level must be an array of objects")
            rows = []
        for i, row in enumerate(rows):
            schema.check_struct(row, sname, schema.structs.get(sname, {}), f"{fname}[{i}]")
        data[fname] = [r for r in rows if isinstance(r, dict)]
    skipped = sorted(f for f in os.listdir(args.data) if f.endswith(".json") and f not in FILE_STRUCTS) \
        if os.path.isdir(args.data) else []

    abilities = ids(data["Abilities.json"], "AbilityID")
    characters = ids(data["Characters.json"], "CharacterID")
    elements = ids(data["Elements.json"], "Element")
    races = ids(data["Races.json"], "RaceID")
    quests = ids(data["Quests.json"], "QuestID")
    enemies = ids(data["Enemies.json"], "EnemyID")
    items = ids(data["Items.json"], "ItemID")
    rolls = ids(data["RollConfigs.json"], "Category")
    locations = ids(data["Locations.json"], "LocationID") if data["Locations.json"] else {}
    anim_sets = ids(data["AnimSets.json"], "CharacterID")

    def need_ability(aid, ctx):
        if aid not in abilities:
            err(f"{ctx}: references missing ability '{aid}'")
            return None
        return abilities[aid]

    check_abilities(abilities, schema, set(characters), set(quests))
    check_anim_sets(anim_sets, characters, abilities)

    # ---- characters: pool is exactly Rudeus + Orsted
    if set(characters) != ALLOWED_CHARACTERS:
        err(f"Characters.json: pool must contain ONLY {sorted(ALLOWED_CHARACTERS)}, found {sorted(characters)}")
    for cid, c in characters.items():
        ctx = f"Characters[{cid}]"
        for k in ("DisplayName", "Description", "Passive", "PassiveDescription", "BasicAbility", "AwakeningAbility"):
            nonempty(c, k, ctx)
        if len(c.get("Abilities", [])) != 3:
            err(f"{ctx}: must have exactly 3 signature Abilities")
        refs = [c.get("BasicAbility", "")] + c.get("Abilities", []) + [c.get("SpecialAbility", ""),
                                                                       c.get("AwakeningAbility", "")]
        for aid in [r for r in refs if r]:
            row = need_ability(aid, ctx)
            if row and row.get("CharacterRequirement") != cid:
                err(f"{ctx}: ability '{aid}' has CharacterRequirement '{row.get('CharacterRequirement')}'")
        awk = abilities.get(c.get("AwakeningAbility", ""))
        if awk and not awk.get("bIsAwakening"):
            err(f"{ctx}: AwakeningAbility '{c['AwakeningAbility']}' is not flagged bIsAwakening")
        if c.get("CapsuleHalfHeight") and c.get("MeshOffsetZ") not in (None, -c["CapsuleHalfHeight"]):
            warn(f"{ctx}: MeshOffsetZ {c.get('MeshOffsetZ')} != -CapsuleHalfHeight")

    # ---- elements: exactly 3 abilities each
    playable = [e for e in schema.enums.get("EMTElement", []) if e not in ("None", "Arcane")]
    if sorted(elements) != sorted(playable):
        err(f"Elements.json: must define exactly {playable}, found {sorted(elements)}")
    seen = {}
    for el, e in elements.items():
        ctx = f"Elements[{el}]"
        nonempty(e, "DisplayName", ctx)
        nonempty(e, "Description", ctx)
        abil = e.get("Abilities", [])
        if len(abil) != 3:
            err(f"{ctx}: must have EXACTLY 3 abilities (has {len(abil)})")
        for aid in abil:
            row = need_ability(aid, ctx)
            if row and row.get("ElementRequirement") != el:
                err(f"{ctx}: ability '{aid}' has ElementRequirement '{row.get('ElementRequirement', 'None')}'")
            if aid in seen:
                err(f"{ctx}: ability '{aid}' already belongs to {seen[aid]}")
            seen[aid] = el

    # ---- races: all 10 with passive + active + transformation
    all_races = schema.enums.get("EMTRace", [])
    if sorted(races) != sorted(all_races):
        err(f"Races.json: must define all races {all_races}, found {sorted(races)}")
    for rid, r in races.items():
        ctx = f"Races[{rid}]"
        if r.get("Race") != rid:
            err(f"{ctx}: RaceID must equal Race enum ('{r.get('Race')}')")
        for k in ("DisplayName", "Description", "PassiveName", "PassiveDescription", "ActiveAbility",
                  "TransformationAbility", "LoreNote"):
            nonempty(r, k, ctx)
        for k in ("PassiveCanonStatus", "TransformationCanonStatus"):
            if r.get(k, "GAMEPLAY ORIGINAL") not in CANON_STATUS:
                err(f"{ctx}: {k} must be one of {sorted(CANON_STATUS)}")
        if bool(r.get("bImplemented", False)) != (rid in IMPLEMENTED_RACES):
            err(f"{ctx}: bImplemented must be true only for {sorted(IMPLEMENTED_RACES)}")
        for k in ("ActiveAbility", "TransformationAbility"):
            row = need_ability(r.get(k, ""), f"{ctx}.{k}") if r.get(k) else None
            if row and (not row.get("bRequiresRace") or row.get("RaceRequirement", "Human") != rid):
                err(f"{ctx}.{k}: '{r[k]}' must set bRequiresRace true and RaceRequirement '{rid}'")
        t = abilities.get(r.get("TransformationAbility", ""))
        if t and (t.get("Behavior") != "Buff" or t.get("TransformationTime", 0) <= 0):
            err(f"{ctx}: TransformationAbility must be a Buff row with TransformationTime > 0")
        for k in ("MaxHealthMultiplier", "MaxManaMultiplier", "MaxStaminaMultiplier"):
            if not 0.7 <= r.get(k, 1.0) <= 1.3:
                err(f"{ctx}: {k} {r.get(k)} outside modest range 0.7..1.3")

    # ---- items
    for iid, it in items.items():
        nonempty(it, "DisplayName", f"Items[{iid}]")
        nonempty(it, "Description", f"Items[{iid}]")
        if it.get("bQuestItem") and it.get("MaxStack", 99) != 1:
            warn(f"Items[{iid}]: quest item with MaxStack != 1")

    # ---- enemies
    for eid, e in enemies.items():
        ctx = f"Enemies[{eid}]"
        tagnames = [t.get("TagName") for t in e.get("Tags", {}).get("GameplayTags", [])]
        if "Enemy" not in tagnames:
            err(f"{ctx}: Tags must include 'Enemy'")
        if not e.get("Attacks"):
            err(f"{ctx}: no Attacks")
        nphase = len(e.get("PhaseThresholds", []))
        for i, atk in enumerate(e.get("Attacks", [])):
            actx = f"{ctx}.Attacks[{i}]"
            need_ability(atk.get("AbilityId", ""), actx)
            if atk.get("MinRange", 0) > atk.get("MaxRange", 250):
                err(f"{actx}: MinRange > MaxRange")
            if atk.get("TelegraphTime", 0.6) <= 0:
                err(f"{actx}: TelegraphTime must be > 0")
            if atk.get("MinPhase", 0) > nphase:
                err(f"{actx}: MinPhase {atk.get('MinPhase')} but only {nphase} phase thresholds")
            if e.get("bIsBoss"):
                if not 0.7 <= atk.get("TelegraphTime", 0.6) <= 1.2:
                    err(f"{actx}: boss telegraph must be 0.7..1.2 s")
                if not 1.0 <= atk.get("RecoveryTime", 0.8) <= 2.0:
                    err(f"{actx}: boss recovery window must be 1.0..2.0 s")
        if e.get("bIsBoss"):
            th = e.get("PhaseThresholds", [])
            if not th or any(not 0 < x < 1 for x in th) or th != sorted(th, reverse=True):
                err(f"{ctx}: boss PhaseThresholds must be descending fractions in (0,1)")
            if not e.get("bCrowdControlImmune"):
                err(f"{ctx}: bosses must be bCrowdControlImmune")
            if "Enemy.Boss" not in tagnames:
                err(f"{ctx}: boss should carry the Enemy.Boss tag")
        for iid, p in e.get("DropTable", {}).items():
            if iid not in items:
                err(f"{ctx}.DropTable: missing item '{iid}'")
            if not 0 < p <= 1:
                err(f"{ctx}.DropTable.{iid}: chance {p} must be in (0,1]")
        lin = e.get("CharacterLineage", "")
        if lin and lin not in characters:
            err(f"{ctx}: CharacterLineage '{lin}' is not a character")
        if lin:
            for i, atk in enumerate(e.get("Attacks", [])):
                row = abilities.get(atk.get("AbilityId"))
                if row and row.get("CharacterRequirement") not in ("", None, lin):
                    err(f"{ctx}.Attacks[{i}]: '{atk['AbilityId']}' belongs to another lineage")

    # ---- quests
    if len(quests) < 12:
        err(f"Quests.json: need at least 12 quests, found {len(quests)}")
    types_present = {q.get("Type", "Basic") for q in quests.values()}
    for t in schema.enums.get("EMTQuestType", []):
        if t not in types_present:
            err(f"Quests.json: no quest of type {t}")
    obj_kinds = set()
    for qid, q in quests.items():
        ctx = f"Quests[{qid}]"
        nonempty(q, "Title", ctx)
        nonempty(q, "Summary", ctx)
        text = json.dumps(q, ensure_ascii=False)
        m = FORBIDDEN_QUEST_NAMES.search(text)
        if m:
            err(f"{ctx}: mentions reserved roll-able character '{m.group(0)}'")
        for k in ("GiverNPC", "TurnInNPC"):
            if q.get(k) and q[k] not in ALLOWED_NPCS:
                err(f"{ctx}: {k} '{q[k]}' is not an allowed NPC id")
        if q.get("GiverNPC") and not q.get("OfferDialogue"):
            err(f"{ctx}: quest with a giver needs OfferDialogue")
        if not q.get("CompleteDialogue"):
            err(f"{ctx}: needs CompleteDialogue")
        if q.get("Type") == "Story" and not q.get("bSequentialObjectives"):
            err(f"{ctx}: Story quests must set bSequentialObjectives")
        for p in q.get("PrerequisiteQuests", []):
            if p not in quests or p == qid:
                err(f"{ctx}: bad prerequisite '{p}'")
        if not q.get("Objectives"):
            err(f"{ctx}: no objectives")
        for i, o in enumerate(q.get("Objectives", [])):
            octx = f"{ctx}.Objectives[{i}]"
            t, tgt, sec = o.get("Type", "Kill"), o.get("TargetId", ""), o.get("SecondaryId", "")
            obj_kinds.add(t)
            nonempty(o, "Description", octx)
            if o.get("Count", 1) < 1:
                err(f"{octx}: Count must be >= 1")
            ml = o.get("MarkerLocationId", "")
            if ml and ml not in ALLOWED_LOCATIONS:
                err(f"{octx}: MarkerLocationId '{ml}' is not a known location id")
            if t == "Kill":
                if "." in tgt:
                    if tgt not in schema.tags:
                        err(f"{octx}: Kill tag '{tgt}' is not registered")
                elif tgt not in enemies:
                    err(f"{octx}: Kill target '{tgt}' is not an enemy id")
            elif t == "DefeatBoss":
                if tgt not in enemies or not enemies[tgt].get("bIsBoss"):
                    err(f"{octx}: DefeatBoss target '{tgt}' is not a boss enemy")
            elif t in ("Gather", "Deliver"):
                if tgt not in items:
                    err(f"{octx}: item '{tgt}' not in Items.json")
                if t == "Deliver" and sec not in ALLOWED_NPCS:
                    err(f"{octx}: Deliver SecondaryId '{sec}' must be an allowed NPC")
            elif t == "TalkTo":
                if tgt not in ALLOWED_NPCS:
                    err(f"{octx}: TalkTo target '{tgt}' is not an allowed NPC")
            elif t == "Reach":
                if tgt not in ALLOWED_LOCATIONS:
                    err(f"{octx}: Reach target '{tgt}' is not a known location id")
            elif t == "Escort":
                if tgt not in ALLOWED_NPCS or sec not in ALLOWED_LOCATIONS:
                    err(f"{octx}: Escort needs an NPC TargetId and a location SecondaryId")
            elif t == "Defend":
                if o.get("Duration", 0) <= 0:
                    err(f"{octx}: Defend needs Duration > 0")
                if tgt.startswith("NPC_") and tgt not in ALLOWED_NPCS:
                    err(f"{octx}: Defend target '{tgt}' is not an allowed NPC")
            elif t == "UseAbility":
                need_ability(tgt, octx)
            elif t in ("Interact", "Puzzle"):
                if not tgt:
                    err(f"{octx}: {t} needs a TargetId")
        rw = q.get("Reward", {})
        for k, v in rw.items():
            if k != "Items" and is_number(v) and v < 0:
                err(f"{ctx}.Reward.{k}: negative")
        for iid in rw.get("Items", {}):
            if iid not in items:
                err(f"{ctx}.Reward.Items: missing item '{iid}'")
    if len(obj_kinds) < 8:
        err(f"Quests.json: objective variety too low ({sorted(obj_kinds)})")

    # ---- locations cross-check (Locations.json is owned by the world team; only cross-referenced here)
    if locations:
        for loc in sorted(ALLOWED_LOCATIONS - set(locations)):
            err(f"Locations.json: quest location '{loc}' is missing")

    # ---- roll configs
    cats = schema.enums.get("EMTRollCategory", [])
    if sorted(rolls) != sorted(cats):
        err(f"RollConfigs.json: need exactly one row per category {cats}, found {sorted(rolls)}")
    pools = {
        "Character": {c.get("Rarity", "Rare") for c in characters.values()},
        "Element": {e.get("Rarity", "Common") for e in elements.values()},
        "Race": {r.get("Rarity", "Common") for r in races.values() if r.get("bImplemented")},
    }
    for cat, rc in rolls.items():
        ctx = f"RollConfigs[{cat}]"
        w = rc.get("RarityWeights", {})
        if not w or any(not is_number(v) or v <= 0 for v in w.values()):
            err(f"{ctx}: RarityWeights must be positive")
        pool = pools.get(cat, set())
        for rar in w:
            if rar not in pool:
                err(f"{ctx}: weight for '{rar}' but no rollable {cat} has that rarity")
        for rar in pool:
            if rar not in w:
                err(f"{ctx}: rollable rarity '{rar}' has no weight (unreachable)")
        if not rc.get("SoftPityStart", 40) <= rc.get("HardPity", 70) <= rc.get("MythicHardPity", 160):
            err(f"{ctx}: expected SoftPityStart <= HardPity <= MythicHardPity")
    ch = rolls.get("Character", {})
    if ch and "Mythic" in pools["Character"] and ch.get("MythicHardPity", 160) > 60:
        err("RollConfigs[Character]: Orsted (Mythic) must be guaranteed within 60 spins")

    # ---- report
    counts = ", ".join(f"{f.replace('.json', '')}={len(data[f])}" for f in FILE_STRUCTS)
    print(f"Parsed headers: {len(schema.enums)} enums, {len(schema.structs)} structs, {len(schema.tags)} tags")
    print(f"Rows: {counts}")
    if skipped:
        print(f"Skipped non-registry files: {', '.join(skipped)}")
    for w in warnings:
        print(f"WARNING: {w}")
    for e in errors:
        print(f"ERROR: {e}")
    if errors:
        print(f"{len(errors)} ERROR(S) FOUND")
        return 1
    print("ALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
