#!/usr/bin/env python3
"""Checks the runtime effect presets against the phase contract (no Unreal needed).

Reads every preset name registered in Source/MushokuRPG/Private/VFX/MTVFXLibrary.cpp and compares them with
Tools/vfx/phase_contract.json (the "<Preset>.<Phase>" names gameplay spawns, Docs/Ability_Overhaul.md section 5.3).
Prints a table and exits 1 when any contract phase has no preset.

Registration patterns understood:
  Out.Add(TEXT("Preset.Phase"), ...)                           literal names (the library uses only these)
  Out.Add(FName(TEXT("Preset.Phase")), ...)                    literal FName
  for (const TCHAR* Family : { TEXT("A"), TEXT("B") }) { ... Out.Add(FName(FString(Family) + TEXT(".Phase")), ...) }
                                                               family loops, expanded to A.Phase and B.Phase
Style variants ("Preset.Phase@CharacterId", chosen by AMTSpellVFX::SpawnPreset for that character) are listed
separately; the Orsted formations of the twelve element spells are expected (a warning, not a failure).

Usage: python3 Tools/vfx/check_presets.py [--library PATH] [--contract PATH] [--quiet]
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LIBRARY = os.path.join(ROOT, "Source", "MushokuRPG", "Private", "VFX", "MTVFXLibrary.cpp")
CONTRACT = os.path.join(ROOT, "Tools", "vfx", "phase_contract.json")

ELEMENT_SPELLS = ["Fireball", "FlameWave", "Inferno", "WaterBullet", "WaterDragon", "Flood", "StoneCannon",
                  "EarthWall", "EarthSpikes", "WindBlade", "Tornado", "WindBurst"]

LITERAL = re.compile(r'Out\s*\.\s*Add\s*\(\s*(?:FName\s*\(\s*)?TEXT\s*\(\s*"([^"]+)"\s*\)')
FAMILY_LOOP = re.compile(r'for\s*\(\s*const\s+TCHAR\s*\*\s*(\w+)\s*:\s*\{([^}]*)\}\s*\)')
FAMILY_ADD = r'Out\s*\.\s*Add\s*\(\s*FName\s*\(\s*FString\s*\(\s*{var}\s*\)\s*\+\s*TEXT\s*\(\s*"([^"]+)"\s*\)'


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def block_after(src, start):
    """The text of the {...} block that begins at or after index start (brace matched)."""
    i = src.find("{", start)
    if i < 0:
        return ""
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    return src[i:]


def registered_names(path):
    src = strip_comments(open(path, encoding="utf-8").read())
    names = []
    names += LITERAL.findall(src)
    for m in FAMILY_LOOP.finditer(src):
        var, members = m.group(1), m.group(2)
        families = re.findall(r'TEXT\s*\(\s*"([^"]+)"\s*\)', members)
        body = block_after(src, m.end())
        for phase in re.findall(FAMILY_ADD.format(var=re.escape(var)), body):
            names += [f + phase for f in families]
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--library", default=LIBRARY)
    ap.add_argument("--contract", default=CONTRACT)
    ap.add_argument("--quiet", action="store_true", help="only the summary and the problems")
    a = ap.parse_args()

    names = registered_names(a.library)
    seen = {}
    duplicates = []
    for n in names:
        if n in seen:
            duplicates.append(n)
        seen[n] = True
    registered = set(names)
    styled = sorted(n for n in registered if "@" in n)
    base_names = {n for n in registered if "@" not in n}

    contract = json.load(open(a.contract, encoding="utf-8"))
    rows = []
    missing = []
    for preset, phases in contract.items():
        if preset.startswith("_") or not isinstance(phases, dict):
            continue
        for phase, note in phases.items():
            name = f"{preset}.{phase}"
            ok = name in base_names
            styles = [s.split("@", 1)[1] for s in styled if s.split("@", 1)[0] == name]
            rows.append((preset, phase, ok, note, styles))
            if not ok:
                missing.append(name)

    contract_names = {f"{r[0]}.{r[1]}" for r in rows}
    extra = sorted(base_names - contract_names)
    orphan_styles = [s for s in styled if s.split("@", 1)[0] not in base_names]
    missing_orsted = [p for p in ELEMENT_SPELLS if f"{p}.Formation@Orsted" not in registered]

    if not a.quiet:
        wp = max(len(r[0]) for r in rows) if rows else 10
        wf = max(len(r[1]) for r in rows) if rows else 10
        print(f"{'Preset':<{wp}}  {'Phase':<{wf}}  {'Status':<7}  {'Styles':<8}  Contract note")
        print(f"{'-' * wp}  {'-' * wf}  {'-' * 7}  {'-' * 8}  {'-' * 40}")
        for preset, phase, ok, note, styles in rows:
            print(f"{preset:<{wp}}  {phase:<{wf}}  {'ok' if ok else 'MISSING':<7}  {','.join(styles) or '-':<8}  {note}")
        print()
        if extra:
            print(f"Presets outside the contract ({len(extra)}): " + ", ".join(extra))
        if styled:
            print(f"Style variants ({len(styled)}): " + ", ".join(styled))
        print()

    status = 0
    print(f"{len(registered)} presets registered ({len(base_names)} base, {len(styled)} styles); "
          f"{len(rows)} contract phases, {len(rows) - len(missing)} present, {len(missing)} missing.")
    if duplicates:
        print("WARNING: registered more than once (the later one wins): " + ", ".join(sorted(set(duplicates))))
    if orphan_styles:
        print("WARNING: style variants without a base preset: " + ", ".join(orphan_styles))
    if missing_orsted:
        print("WARNING: element formations without an @Orsted style: " + ", ".join(missing_orsted))
    if missing:
        print("MISSING contract phases: " + ", ".join(missing))
        status = 1
    else:
        print("All contract phases have a preset.")
    return status


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:  # piped into head / less that closed early
        sys.exit(0)
