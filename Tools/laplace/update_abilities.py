"""LA PLACE ability pass: simpler, visible, epic abilities.

- Element spells become the user's list: Fire (Fireball, Flame Wave, Inferno), Water (Water Bullet, Water Dragon, Flood),
  Wind (Wind Blade, Tornado, Wind Burst), Earth (Stone Cannon, Earth Wall, Earth Spikes).
- Every player ability gets a runtime effect preset (FX.Preset, Source/.../VFX/MTVFXLibrary.cpp), its Higgsfield icon
  (/Game/LaPlace/UI/Icons) and synthesized sounds (/Game/LaPlace/Audio).
- Orsted gains Dragon Crush (a ground-breaking slam). Characters get a default 4-slot loadout.
- Unlock gates (mastery, rank, unlock requirements) are removed from loadout abilities: everything is equippable.
Idempotent. Run: python3 Tools/laplace/update_abilities.py
"""
import collections
import json
import os

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
DATA = os.path.join(ROOT, "Content", "Data")
ANIM = "/Game/Characters/Rudeus/Animations/A_Rudeus_{0}.A_Rudeus_{0}"
ORSTED_ANIM = "/Game/Characters/Orsted/Animations/A_Orsted_{0}.A_Orsted_{0}"


def sound(category, name):
    return "/Game/LaPlace/Audio/{0}/{1}.{1}".format(category, name)


def icon(ability_id):
    return "/Game/LaPlace/UI/Icons/T_Icon_{0}.T_Icon_{0}".format(ability_id)


def fx(preset, cast=None, travel=None, impact=None, scale=1.0, on_ground=False):
    out = collections.OrderedDict(Preset=preset)
    if scale != 1.0:
        out["PresetScale"] = scale
    if on_ground:
        out["bImpactOnGround"] = True
    if cast:
        out["CastSound"] = sound(*cast)
    if travel:
        out["TravelSound"] = sound(*travel)
    if impact:
        out["ImpactSound"] = sound(*impact)
    return out


CASTING = {"GameplayTags": [{"TagName": "State.Casting"}]}

# Element spells: (old id, new row). Numbers tuned for readable, satisfying combat; descriptions are original.
ELEMENT_ROWS = [
    ("Fire_FireBurst", dict(
        AbilityID="Fire_Fireball", DisplayName="Fireball", Behavior="Projectile", Element="Fire",
        Description="Flames spiral together above the palm into a compressed, white-hot sphere that bursts on impact and scorches the ground.",
        ManaCost=22, Cooldown=1.3, CastTime=0.3, RecoveryTime=0.3, Range=3000, AOERadius=190, Damage=40, Stagger=18, Knockback=180,
        Motion="Straight", ProjectileSpeed=2700, ProjectileRadius=22, ProjectileGravity=0, bDisruptable=True,
        Montage=ANIM.format("CastBasic"),
        FX=fx("Fireball", ("Fire", "fire_gather"), ("Fire", "fireball_travel_loop"), ("Fire", "fire_explosion")))),
    ("Fire_FlameField", dict(
        AbilityID="Fire_FlameWave", DisplayName="Flame Wave", Behavior="Zone", Element="Fire",
        Description="A sweep of the arm sends a wide wall of fire rolling across the ground, burning everything it passes through and leaving the grass scorched.",
        ManaCost=50, Cooldown=7, CastTime=0.35, RecoveryTime=0.35, Range=600, AOERadius=320, Damage=22, Stagger=12, Knockback=0,
        Duration=2.0, bDisruptable=True, ZoneKind="DamageField", ZoneMoveSpeed=950, PulseInterval=0.35,
        Montage=ANIM.format("CastTwoHand"),
        FX=fx("FlameWave", ("Fire", "flamewave_cast"), ("Fire", "flame_burn_loop"), ("Fire", "fire_explosion")))),
    ("Fire_InfernoCompression", dict(
        AbilityID="Fire_Inferno", DisplayName="Inferno", Behavior="Zone", Element="Fire",
        Description="A burning magic circle blooms under the target. After a heartbeat of warning, pillars of fire erupt through it and a flame vortex climbs into the sky.",
        ManaCost=140, Cooldown=18, CastTime=0.6, RecoveryTime=0.5, Range=2200, AOERadius=520, Damage=55, Stagger=40, Knockback=320,
        Duration=3.4, bDisruptable=True, ZoneKind="Eruptions", ActivationDelay=1.0, PulseCount=4, PulseInterval=0.35,
        Montage=ANIM.format("CastGround"),
        FX=fx("Inferno", ("Fire", "inferno_charge"), None, ("Fire", "inferno_eruption")))),
    ("Water_WaterCannon", dict(
        AbilityID="Water_WaterBullet", DisplayName="Water Bullet", Behavior="Projectile", Element="Water",
        Description="Water gathers in front of the caster, compresses into a dense sphere and fires with enough force to knock a grown man flat.",
        ManaCost=20, Cooldown=1.2, CastTime=0.25, RecoveryTime=0.3, Range=3000, AOERadius=120, Damage=32, Stagger=22, Knockback=320,
        Motion="Straight", ProjectileSpeed=3600, ProjectileRadius=20, ProjectileGravity=0, bDisruptable=True,
        Montage=ANIM.format("CastBasic"),
        FX=fx("WaterBullet", ("Water", "water_gather"), ("Water", "water_bullet_launch"), ("Water", "water_splash")))),
    ("Water_FrostPrison", dict(
        AbilityID="Water_WaterDragon", DisplayName="Water Dragon", Behavior="Projectile", Element="Water",
        Description="Water spirals upward into the shape of a dragon that hunts the target down and crashes into it in a huge splash and shockwave.",
        ManaCost=110, Cooldown=12, CastTime=0.7, RecoveryTime=0.45, Range=4000, AOERadius=380, Damage=110, Stagger=90, Knockback=650,
        Motion="Straight", ProjectileSpeed=1500, ProjectileRadius=70, ProjectileGravity=0, bDisruptable=True, bHoming=True, HomingStrength=2600,
        Montage=ANIM.format("CastTwoHand"),
        FX=fx("WaterDragon", ("Water", "water_dragon_rise"), None, ("Water", "water_dragon_impact")))),
    ("Water_Cumulonimbus", dict(
        AbilityID="Water_Flood", DisplayName="Flood", Behavior="Zone", Element="Water",
        Description="Water wells up across a wide front and breaks into a wave that rolls forward, sweeping enemies away and soaking the ground.",
        ManaCost=120, Cooldown=16, CastTime=0.55, RecoveryTime=0.45, Range=600, AOERadius=430, Damage=45, Stagger=60, Knockback=0,
        Duration=2.2, bDisruptable=False, ZoneKind="Wave", ZoneMoveSpeed=820,
        Montage=ANIM.format("CastTwoHand"),
        FX=fx("Flood", ("Water", "flood_crash"), ("Water", "flood_wave_loop"), ("Water", "water_splash")))),
    ("Wind_AirBlade", dict(
        AbilityID="Wind_WindBlade", DisplayName="Wind Blade", Behavior="Projectile", Element="Wind",
        Description="A swing of the hand releases a thin blade of compressed air: almost invisible, betrayed only by the dust it drags and the distortion at its edge.",
        ManaCost=18, Cooldown=1.0, CastTime=0.2, RecoveryTime=0.25, Range=3000, AOERadius=0, Damage=28, Stagger=14, Knockback=120,
        Motion="Wave", ProjectileSpeed=3800, ProjectileRadius=60, ProjectileGravity=0, bDisruptable=True,
        Montage=ANIM.format("CastBasic"),
        FX=fx("WindBlade", ("Wind", "wind_gather"), ("Wind", "wind_blade_swish"), ("Wind", "wind_blade_impact")))),
    ("Wind_TempestDomain", dict(
        AbilityID="Wind_Tornado", DisplayName="Tornado", Behavior="Zone", Element="Wind",
        Description="The air starts turning around a single point, then tears upward into a tornado that drags dust, leaves and enemies in and hurls them around.",
        ManaCost=110, Cooldown=18, CastTime=0.6, RecoveryTime=0.5, Range=1400, AOERadius=340, Damage=14, Stagger=12, Knockback=0,
        Duration=5.0, bDisruptable=False, ZoneKind="Vortex", ZoneMoveSpeed=260, PulseInterval=0.4,
        Montage=ANIM.format("CastGround"),
        FX=fx("Tornado", ("Wind", "tornado_start"), ("Wind", "tornado_loop"), None))),
    ("Wind_GaleStep", None),  # kept as-is: a movement dash used by tests, no longer an element spell
    (None, dict(
        AbilityID="Wind_WindBurst", DisplayName="Wind Burst", Behavior="Zone", Element="Wind",
        Description="Compressed air bursts out of the caster in rings, flattening the grass and throwing nearby enemies back.",
        ManaCost=45, Cooldown=8, CastTime=0.25, RecoveryTime=0.35, Range=0, AOERadius=650, Damage=20, Stagger=60, Knockback=900,
        Duration=0.5, bDisruptable=False, ZoneKind="Burst",
        Montage=ANIM.format("CastTwoHand"),
        FX=fx("WindBurst", ("Wind", "wind_gather"), None, ("Wind", "wind_burst")))),
    ("Earth_StoneCannon", dict(
        AbilityID="Earth_StoneCannon", DisplayName="Stone Cannon", Behavior="Projectile", Element="Earth",
        Description="Rock forms above the hand, compresses and spins up before launching at tremendous speed. Hold to compress it further.",
        ManaCost=35, Cooldown=2.0, CastTime=0.35, RecoveryTime=0.3, Range=3200, AOERadius=150, Damage=60, Stagger=45, Knockback=260,
        bChargeable=True, MaxChargeTime=1.2, ChargeSpeedScale=1.5, ChargeDamageScale=1.8, ChargeStaggerScale=1.6, ChargeManaScale=1.4,
        Motion="Straight", ProjectileSpeed=3800, ProjectileRadius=20, ProjectileGravity=0, bDisruptable=True,
        FX=fx("StoneCannon", ("Earth", "stone_form"), ("Earth", "stone_cannon_launch"), ("Earth", "crater_impact")))),
    ("Earth_EarthFortress", dict(
        AbilityID="Earth_EarthWall", DisplayName="Earth Wall", Behavior="Structure", Element="Earth",
        Description="The ground cracks and thick slabs of stone burst upward, shedding dirt and rubble. The walls physically block attacks.",
        ManaCost=70, Cooldown=16, CastTime=0.45, RecoveryTime=0.4, Range=500, Damage=10, Stagger=25, Duration=14, bDisruptable=True,
        StructureCount=3, StructureHealth=380, StructureExtent={"X": 70, "Y": 170, "Z": 170},
        Montage=ANIM.format("CastGround"),
        FX=dict(fx("EarthWall", ("Earth", "ground_rumble_loop"), None, ("Earth", "earth_wall_rise")),
                BodyMesh="/Game/LaPlace/Kit/VFX/SM_VFX_EarthWall.SM_VFX_EarthWall",
                BodyMaterial="/Game/LaPlace/VFX/Materials/M_VFX_Rock.M_VFX_Rock"))),
    ("Earth_Quagmire", dict(
        AbilityID="Earth_EarthSpikes", DisplayName="Earth Spikes", Behavior="Zone", Element="Earth",
        Description="Cracks race across the ground toward the target and stone spikes erupt one after another in their wake.",
        ManaCost=60, Cooldown=9, CastTime=0.4, RecoveryTime=0.35, Range=1500, AOERadius=200, InnerRadius=150, Damage=45, Stagger=70, Knockback=380,
        Duration=1.4, bDisruptable=False, ZoneKind="LineEruptions", ActivationDelay=0.15, PulseCount=7, PulseInterval=0.07,
        Montage=ANIM.format("CastGround"),
        FX=fx("EarthSpikes", ("Earth", "stone_form"), None, ("Earth", "earth_spike")))),
]

# Existing character rows: preset, sounds, icon.
CHARACTER_FX = {
    "Rudeus_Basic": fx("StoneBullet", ("Rudeus", "cast_small"), None, ("Earth", "rock_impact"), scale=1.0),
    "Rudeus_StoneCannon": fx("RudeusCannon", ("Earth", "stone_form"), ("Earth", "stone_cannon_launch"), ("Earth", "crater_impact")),
    "Rudeus_StoneCannon_Awakened": fx("RudeusCannon", ("Earth", "stone_form"), ("Earth", "stone_cannon_launch"), ("Earth", "crater_impact"), scale=1.25),
    "Rudeus_Quagmire": fx("Quagmire", ("Earth", "stone_form"), ("Earth", "quagmire_loop"), None),
    "Rudeus_Quagmire_Awakened": fx("Quagmire", ("Earth", "stone_form"), ("Earth", "quagmire_loop"), None),
    "Rudeus_ElementalBarrage": fx("Barrage", ("Rudeus", "cast_small")),
    "Barrage_Stone": fx("StoneBullet", None, ("Earth", "stone_cannon_launch"), ("Earth", "rock_impact")),
    "Barrage_WindBlade": fx("WindBlade", None, ("Wind", "wind_blade_swish"), ("Wind", "wind_blade_impact"), scale=0.8),
    "Barrage_WaterCannon": fx("WaterBullet", None, ("Water", "water_bullet_launch"), ("Water", "water_splash"), scale=0.8),
    "Barrage_FireBurst": fx("Fireball", None, ("Fire", "fireball_launch"), ("Fire", "fire_explosion"), scale=0.75),
    "Rudeus_DemonEye": fx("DemonEye", ("Rudeus", "demon_eye")),
    "Rudeus_Awakening_QuagmireMagician": fx("QuagmireMagician", ("Rudeus", "awakening")),
    "Orsted_Basic": fx("PalmStrike", None, None, ("Orsted", "palm_strike")),
    "Orsted_DisturbMagic": fx("DisturbMagic", ("Orsted", "disturb_magic"), None, ("Orsted", "disturb_magic")),
    "Orsted_DragonStep": fx("DragonStep", ("Orsted", "dragon_step"), None, ("Orsted", "shockwave_boom")),
    "Orsted_DragonStep_Awakened": fx("DragonStep", ("Orsted", "dragon_step"), None, ("Orsted", "shockwave_boom"), scale=1.2),
    "Orsted_SaintDragonAura": fx("DragonAura", ("Orsted", "aura_activate"), ("Orsted", "aura_hum_loop")),
    "Orsted_Awakening_DragonGod": fx("DragonGod", ("Orsted", "dragon_god_awaken")),
    # Enemies share the readable effects.
    "Goblin_RockThrow": fx("StoneBullet", None, None, ("Earth", "rock_impact"), scale=0.8),
    "BanditMage_FireBolt": fx("Fireball", ("Fire", "fire_gather"), ("Fire", "fireball_launch"), ("Fire", "fire_explosion"), scale=0.7),
}

DRAGON_CRUSH = dict(
    AbilityID="Orsted_DragonCrush", DisplayName="Dragon God Style: Dragon Crush", Behavior="Melee", Element="Arcane",
    CharacterRequirement="Orsted",
    Description="A single downward strike with the full weight of the Dragon God behind it. The ground splits, a shockwave rolls outward and rubble is thrown into the air. No light, no colour: just force.",
    ManaCost=0, StaminaCost=25, Cooldown=9, CastTime=0.4, RecoveryTime=0.5, Range=260, AOERadius=400, Damage=70, Stagger=85, Knockback=720,
    bDisruptable=False, Montage=ORSTED_ANIM.format("CastGround"),
    FX=fx("DragonCrush", ("Orsted", "shockwave_boom"), None, ("Orsted", "ground_crack"), on_ground=True))

DEFAULT_LOADOUT = {
    "Rudeus": ["Rudeus_StoneCannon", "Rudeus_Quagmire", "Fire_Fireball", "Water_WaterDragon"],
    "Orsted": ["Orsted_DragonStep", "Orsted_DisturbMagic", "Orsted_DragonCrush", "Wind_WindBurst"],
}


def ordered_row(row):
    """Readable key order: identity, numbers, presentation."""
    first = ["AbilityID", "DisplayName", "Description", "Behavior", "Element", "CharacterRequirement"]
    out = collections.OrderedDict((k, row[k]) for k in first if k in row)
    for k, v in row.items():
        if k not in out:
            out[k] = v
    return out


def main():
    path = os.path.join(DATA, "Abilities.json")
    rows = json.load(open(path), object_pairs_hook=collections.OrderedDict)
    by_id = collections.OrderedDict((r["AbilityID"], r) for r in rows)

    for old_id, new in ELEMENT_ROWS:
        if new is None:
            continue
        new = dict(new)
        new_id = new["AbilityID"]
        base = by_id.pop(old_id, None) if old_id else None
        base = base or by_id.pop(new_id, None) or collections.OrderedDict()
        merged = collections.OrderedDict(base)
        for gate in ("UnlockRequirement", "MasteryRequirement", "RankRequirement", "ZoneMovement", "ZoneInnerMovement",
                     "ActivationDelay", "PulseCount", "PulseInterval", "InnerRadius", "CounterWindow", "CounterRadius", "DecalMaterial"):
            merged.pop(gate, None)
        merged.update(new)
        merged["ElementRequirement"] = new["Element"]  # mastery XP track (not a gate any more)
        merged["Icon"] = icon(new_id)
        merged.setdefault("MasteryXP", 5)
        merged["ActivationTags"] = CASTING
        by_id[new_id] = ordered_row(merged)

    for ability_id, effects in CHARACTER_FX.items():
        row = by_id.get(ability_id)
        if row is None:
            continue
        old = row.get("FX", {})
        merged = collections.OrderedDict((k, v) for k, v in old.items() if k in ("BodyMesh", "BodyMaterial", "CameraShakeScale"))
        merged.update(effects)
        row["FX"] = merged
        row["Icon"] = icon(ability_id)
        for gate in ("UnlockRequirement", "MasteryRequirement", "RankRequirement"):
            row.pop(gate, None)

    crush = ordered_row(dict(DRAGON_CRUSH, Icon=icon("Orsted_DragonCrush"), MasteryXP=4))
    by_id["Orsted_DragonCrush"] = crush

    json.dump(list(by_id.values()), open(path, "w"), indent=2, ensure_ascii=False)
    open(path, "a").write("\n")
    print("Abilities.json: %d rows" % len(by_id))

    epath = os.path.join(DATA, "Elements.json")
    elements = json.load(open(epath), object_pairs_hook=collections.OrderedDict)
    lists = {
        "Fire": (["Fire_Fireball", "Fire_FlameWave", "Fire_Inferno"], "Fire magic at its most violent: a compressed fireball, a rolling wall of flame and the Inferno that turns a battlefield into a furnace."),
        "Water": (["Water_WaterBullet", "Water_WaterDragon", "Water_Flood"], "Real water, not blue light: a pressurised bullet, a dragon that hunts its target and a flood that sweeps the field clean."),
        "Wind": (["Wind_WindBlade", "Wind_Tornado", "Wind_WindBurst"], "Wind is seen through what it moves: blades of compressed air, a tornado that drags enemies in and bursts that throw them away."),
        "Earth": (["Earth_StoneCannon", "Earth_EarthWall", "Earth_EarthSpikes"], "Earth magic reshapes the ground itself: a spinning stone cannon, walls that stop attacks and spikes that erupt toward the target."),
    }
    for e in elements:
        if e["Element"] in lists:
            e["Abilities"], e["Description"] = lists[e["Element"]]
    json.dump(elements, open(epath, "w"), indent=2, ensure_ascii=False)
    open(epath, "a").write("\n")
    print("Elements.json updated")

    cpath = os.path.join(DATA, "Characters.json")
    chars = json.load(open(cpath), object_pairs_hook=collections.OrderedDict)
    for c in chars:
        cid = c["CharacterID"]
        if cid == "Orsted" and "Orsted_DragonCrush" not in c["Abilities"]:
            c["Abilities"] = ["Orsted_DisturbMagic", "Orsted_DragonStep", "Orsted_DragonCrush"]
            c["SpecialAbility"] = "Orsted_SaintDragonAura"
        if cid in DEFAULT_LOADOUT:
            c["DefaultLoadout"] = DEFAULT_LOADOUT[cid]
    json.dump(chars, open(cpath, "w"), indent=2, ensure_ascii=False)
    open(cpath, "a").write("\n")
    print("Characters.json updated")


if __name__ == "__main__":
    main()
