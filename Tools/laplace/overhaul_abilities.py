"""LA PLACE ability overhaul: the numbers, clips, presets and sounds of Docs/Ability_Overhaul.md section 3.

Upgrades the abilities that already exist (same ids, same hotbar):
  Rudeus: Stone Cannon, Quagmire, Elemental Barrage (+ their awakened versions and the barrage shots)
  Orsted: Disturb Magic, Dragon Step, Dragon Crush
  Shared: Fireball, Flame Wave, Inferno, Water Bullet, Water Dragon, Flood, Stone Cannon, Earth Wall, Earth Spikes,
          Wind Blade, Tornado, Wind Burst
For each row it keeps the identity and presentation keys (id, name, description, icon, mastery, tags, element
requirement, character requirement, body mesh) and rewrites every mechanics key from the table below, so stale fields
from earlier passes (a chargeable Inferno with Stone Cannon clips, a Water Dragon with a Freeze zone kind...) are gone.
Adds Barrage_Finale, sets the default loadouts (1-3 = the character's uniques) and Elements.json descriptions.

Idempotent. Run: python3 Tools/laplace/overhaul_abilities.py [--dry-run]; then python3 Tools/validate_data.py
"""
import argparse
import collections
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "Content", "Data")


def clip(key, character="Rudeus"):
    """Montage path; shared rows name Rudeus's clip and ResolveLineageAnim plays the caster's own A_<C>_<Key>."""
    name = "A_%s_%s" % (character, key)
    return "/Game/Characters/%s/Animations/%s.%s" % (character, name, name)


def sound(path):
    category, name = path.split("/")
    return "/Game/LaPlace/Audio/{0}/{1}.{1}".format(category, name)


def fx(preset, cast=None, release=None, travel=None, impact=None, accent=None, scale=1.0, on_ground=False, **extra):
    out = collections.OrderedDict(Preset=preset)
    if scale != 1.0:
        out["PresetScale"] = scale
    if on_ground:
        out["bImpactOnGround"] = True
    for key, value in (("CastSound", cast), ("ReleaseSound", release), ("TravelSound", travel), ("ImpactSound", impact),
                       ("AccentSound", accent)):
        if value:
            out[key] = sound(value)
    out.update(extra)
    return out


CASTING = {"GameplayTags": [{"TagName": "State.Casting"}]}
CHARGING = {"GameplayTags": [{"TagName": "State.Casting"}, {"TagName": "State.Charging"}]}
COUNTERING = {"GameplayTags": [{"TagName": "State.Countering"}]}

# Keys a row keeps from its current version; everything else comes from the tables.
KEEP = ("AbilityID", "DisplayName", "Description", "CharacterRequirement", "ElementRequirement", "RaceRequirement",
        "bRequiresRace", "Icon", "MasteryXP", "UnlockRequirement", "MasteryRequirement", "RankRequirement")
# FX keys kept from the current row (authored meshes); presets and sounds come from the tables.
KEEP_FX = ("BodyMesh", "BodyMaterial", "DecalMaterial")

WALL_MESH = dict(BodyMesh="/Game/LaPlace/Kit/VFX/SM_VFX_EarthWall.SM_VFX_EarthWall",
                 BodyMaterial="/Game/LaPlace/VFX/Materials/M_VFX_Rock.M_VFX_Rock")

# ------------------------------------------------------------------------------------------------ signature rows
ROWS = collections.OrderedDict()

ROWS["Rudeus_StoneCannon"] = dict(
    Behavior="Projectile", Element="Earth", ManaCost=45, Cooldown=1.6, CastTime=0.35, RecoveryTime=0.35, Range=4500,
    AOERadius=260, Damage=90, Stagger=50, Knockback=1100, Launch=300, HitForgiveness=1.12,
    bChargeable=True, MaxChargeTime=1.6, ChargeSpeedScale=1.6, ChargeDamageScale=2.4, ChargeStaggerScale=2.5,
    ChargeManaScale=2.0, ChargeSizeScale=1.7, ChargeRadiusScale=1.6, ChargeKnockbackScale=1.45,
    Motion="Straight", ProjectileSpeed=9000, ProjectileRadius=36, ProjectileGravity=0, bDisruptable=True,
    PierceCount=3, PierceMaxHealth=600,
    Montage=clip("StoneCannon_Charge"), ChargeLoopAnim=clip("StoneCannon_Hold"), ReleaseAnim=clip("StoneCannon_Release"),
    CastSocket="hand_r", ActivationTags=CHARGING,
    FX=fx("RudeusCannon", cast="Earth/stone_compress", release="Earth/sonic_boom", travel="Earth/stone_spin_loop",
          impact="Earth/crater_impact"))

ROWS["Rudeus_Quagmire"] = dict(
    Behavior="Zone", Element="Earth", ManaCost=90, Cooldown=14, CastTime=0.45, RecoveryTime=0.4, Range=2400,
    AOERadius=900, Damage=0, Stagger=0, Duration=9, HitForgiveness=1.1,
    bChargeable=True, MaxChargeTime=1.8, ChargeManaScale=1.8, ChargeRadiusScale=2.2, bDisruptable=True,
    ZoneKind="Mire", ZoneMovement={"SpeedMultiplier": 0.4, "AccelerationMultiplier": 0.5, "JumpMultiplier": 0.35,
                                   "DodgeDistanceMultiplier": 0.45, "bRooted": False},
    Params=dict(TransformTime=1.2, EdgeDepth=0.35, SpeedLoss=0.72, AccelLoss=0.5, JumpLoss=0.7, DodgeLoss=0.6,
                SinkDepth=30, HeavySink=0.35, HeavyPenalty=0.6, MomentumLoss=0.6),
    Montage=clip("Quagmire"), ChargeLoopAnim=clip("Quagmire_Hold"), ReleaseAnim=clip("Quagmire_Release"),
    CastSocket="hand_l", ActivationTags=CHARGING,
    FX=fx("Quagmire", cast="Earth/stone_form", impact="Earth/mud_squelch", accent="Earth/quagmire_transform",
          travel="Earth/quagmire_loop"))

ROWS["Rudeus_ElementalBarrage"] = dict(
    Behavior="Barrage", Element="None", ManaCost=140, Cooldown=16, CastTime=0.6, RecoveryTime=0.6, Range=3000,
    Damage=0, Stagger=0, MoveSpeedWhileActive=0.35,
    Sequence=[{"AbilityId": "Barrage_Stone", "Delay": 0.0, "MontageSection": "Stone"},
              {"AbilityId": "Barrage_WaterCannon", "Delay": 0.0, "MontageSection": "Water"},
              {"AbilityId": "Barrage_WindBlade", "Delay": 0.0, "MontageSection": "Wind"},
              {"AbilityId": "Barrage_FireBurst", "Delay": 0.0, "MontageSection": "Fire"},
              {"AbilityId": "Barrage_Finale", "Delay": 0.0, "MontageSection": "Finale"}],
    Params=dict(BarrageTime=3.0, ShotInterval=0.2, CollapseTime=0.35, Spread=4, Sway=12),
    Montage=clip("Barrage"), CastSocket="hand_r", ActivationTags=CASTING,
    FX=fx("Barrage", cast="Rudeus/barrage_orbs"))

ROWS["Orsted_DisturbMagic"] = dict(
    Behavior="Disrupt", Element="Arcane", ManaCost=45, Cooldown=6, CastTime=0.06, RecoveryTime=0.35, Range=1800,
    Damage=0, Stagger=0, bDisruptable=False, CounterWindow=0.35, CounterRadius=450,
    Params=dict(PulseSpeed=6500, PathRadius=220, SearchRange=1800, SearchAngle=35, SealSeconds=3, Refund=0.6,
                EndRadius=450),
    Montage=clip("DisturbMagic", "Orsted"), CastSocket="hand_r", ActivationTags=COUNTERING,
    FX=fx("DisturbMagic", cast="Orsted/disturb_pulse", impact="Orsted/disturb_collapse", accent="Orsted/seal"))

ROWS["Orsted_DragonStep"] = dict(
    Behavior="Dash", Element="Arcane", ManaCost=0, StaminaCost=20, Cooldown=3.5, CastTime=0.08, RecoveryTime=0.3,
    Range=2600, AOERadius=220, Damage=10, Stagger=25, Knockback=0, bDisruptable=False,
    DashDistance=1700, DashDuration=0.2, bDashTowardTarget=True, bDashInvulnerable=True, WarpTargetName="DragonStepTarget",
    ComboFollowUp="Orsted_DragonCrush",
    Params=dict(TargetRange=2600, ArriveOffset=115, TargetDuration=0.12, FreeDistance=1700, FreeDuration=0.2,
                ComboWindow=0.9),
    Montage=clip("DragonStep", "Orsted"), CastSocket="foot_r",
    FX=fx("DragonStep", cast="Orsted/dragon_step", impact="Orsted/shockwave_boom", accent="Orsted/dragon_step_arrive"))

ROWS["Orsted_DragonCrush"] = dict(
    Behavior="Strike", Element="Arcane", ManaCost=0, StaminaCost=35, Cooldown=7, CastTime=0.4, RecoveryTime=0.5,
    Range=450, AOERadius=650, Damage=160, Stagger=90, Knockback=800, Launch=420, bDisruptable=False,
    bFullBodyCommit=True,
    Params=dict(PrimaryDamage=320, PrimaryKnockback=1300, PrimaryLaunch=700, PrimaryRange=350, ConeRange=300,
                ConeHalfAngle=35, LungeRange=450, HitStop=0.085, ComboCastScale=0.35, ComboDamageScale=1.2),
    Montage=clip("DragonCrush", "Orsted"), CastSocket="hand_r",
    FX=fx("DragonCrush", cast="Orsted/dragon_crush_charge", impact="Orsted/dragon_crush_impact", on_ground=True))

# ------------------------------------------------------------------------------------------------ shared element rows
ROWS["Fire_Fireball"] = dict(
    Behavior="Projectile", Element="Fire", ManaCost=24, Cooldown=1.6, CastTime=0.3, RecoveryTime=0.3, Range=3500,
    AOERadius=320, Damage=60, Stagger=25, Knockback=600, Launch=250, BurnSeconds=4, BurnDamagePerSecond=8,
    bChargeable=True, MaxChargeTime=1.2, ChargeSpeedScale=1.25, ChargeDamageScale=2.5, ChargeStaggerScale=2.0,
    ChargeManaScale=1.6, ChargeSizeScale=1.5, ChargeRadiusScale=1.75, ChargeKnockbackScale=1.5,
    Motion="Straight", ProjectileSpeed=5500, ProjectileRadius=30, ProjectileGravity=0, bDisruptable=True,
    Montage=clip("Cast_Fireball"), ChargeLoopAnim=clip("Cast_Fireball_Hold"), ReleaseAnim=clip("Cast_Fireball_Release"),
    ActivationTags=CHARGING,
    FX=fx("Fireball", cast="Fire/fireball_charge", release="Fire/fireball_launch", travel="Fire/fireball_travel_loop",
          impact="Fire/fire_explosion"))

ROWS["Fire_FlameWave"] = dict(
    Behavior="Zone", Element="Fire", ManaCost=55, Cooldown=8, CastTime=0.42, RecoveryTime=0.35, Range=1300,
    AOERadius=1300, Damage=75, Stagger=30, Knockback=450, Launch=150, BurnSeconds=4, BurnDamagePerSecond=8,
    Duration=1.3, bDisruptable=True, ZoneKind="Arc",
    Params=dict(ArcDegrees=120, StartRadius=150, EndRadius=1300, TravelTime=0.9, Band=180, Segments=11),
    Montage=clip("Cast_FlameWave"), ActivationTags=CASTING,
    FX=fx("FlameWave", cast="Fire/flamewave_cast", accent="Fire/flamewave_roar"))

ROWS["Fire_Inferno"] = dict(
    Behavior="Zone", Element="Fire", ManaCost=220, Cooldown=30, CastTime=0.9, RecoveryTime=0.5, Range=2600,
    AOERadius=1100, InnerRadius=190, Damage=70, Stagger=40, Knockback=150, Launch=380,
    BurnSeconds=3, BurnDamagePerSecond=10, Duration=4.3, bDisruptable=True, bFullBodyCommit=True,
    ZoneKind="Eruptions", ActivationDelay=0.6, PulseCount=10, PulseInterval=0.35,
    Params=dict(PillarsPerPulse=3, EnemyBias=0.67, AreaBurnDps=15),
    Montage=clip("Cast_Inferno"), ActivationTags=CASTING,
    FX=fx("Inferno", cast="Fire/inferno_charge", impact="Fire/inferno_pillar", accent="Fire/inferno_circle",
          travel="Fire/flame_burn_loop"))

ROWS["Water_WaterBullet"] = dict(
    Behavior="Projectile", Element="Water", ManaCost=20, Cooldown=1.2, CastTime=0.22, RecoveryTime=0.3, Range=3500,
    AOERadius=150, Damage=48, Stagger=30, Knockback=750, Launch=150, Motion="Piercing", ProjectileSpeed=8000,
    ProjectileRadius=20, ProjectileGravity=0, bDisruptable=True, PierceCount=1,
    Montage=clip("Cast_WaterBullet"), ActivationTags=CASTING,
    FX=fx("WaterBullet", cast="Water/water_gather", release="Water/water_lance", impact="Water/water_splash"))

ROWS["Water_WaterDragon"] = dict(
    Behavior="Serpent", Element="Water", ManaCost=160, Cooldown=22, CastTime=0.75, RecoveryTime=0.45, Range=4000,
    AOERadius=780, Damage=220, Stagger=90, Knockback=1200, Launch=500, bDisruptable=True, bFullBodyCommit=True,
    Params=dict(Segments=16, Length=1700, HeadRadius=110, CircleTime=0.5, HuntTime=2.4, Speed=2800, TurnRate=160),
    Montage=clip("Cast_WaterDragon"), ActivationTags=CASTING,
    FX=fx("WaterDragon", cast="Water/water_dragon_rise", travel="Water/water_dragon_roar", impact="Water/water_dragon_impact"))

ROWS["Water_Flood"] = dict(
    Behavior="Zone", Element="Water", ManaCost=170, Cooldown=26, CastTime=0.7, RecoveryTime=0.5, Range=600,
    AOERadius=1200, Damage=90, Stagger=60, Knockback=900, Launch=250, Duration=2.1, bDisruptable=True,
    bFullBodyCommit=True, ZoneKind="Wave", ZoneMoveSpeed=1500,
    Params=dict(Width=2400, Distance=2600, CarrySpeed=1300, Band=300, TrailSpacing=300),
    Montage=clip("Cast_Flood"), ActivationTags=CASTING,
    FX=fx("Flood", cast="Water/flood_gather", release="Water/flood_crash", impact="Water/water_splash",
          travel="Water/flood_wave_loop"))

ROWS["Earth_StoneCannon"] = dict(
    Behavior="Projectile", Element="Earth", ManaCost=32, Cooldown=2.0, CastTime=0.35, RecoveryTime=0.3, Range=3500,
    AOERadius=170, Damage=60, Stagger=45, Knockback=700, Launch=200,
    bChargeable=True, MaxChargeTime=1.2, ChargeSpeedScale=1.4, ChargeDamageScale=2.0, ChargeStaggerScale=1.6,
    ChargeManaScale=1.4, ChargeSizeScale=1.3, ChargeRadiusScale=1.4, ChargeKnockbackScale=1.4,
    Motion="Straight", ProjectileSpeed=6500, ProjectileRadius=22, ProjectileGravity=0, bDisruptable=True,
    PierceCount=1, PierceMaxHealth=400,
    Montage=clip("Cast_StoneCannon"), ChargeLoopAnim=clip("Cast_StoneCannon_Hold"),
    ReleaseAnim=clip("Cast_StoneCannon_Release"), ActivationTags=CHARGING,
    FX=fx("StoneCannon", cast="Earth/stone_form", release="Earth/stone_cannon_launch", travel="Earth/stone_spin_loop",
          impact="Earth/rock_impact"))

ROWS["Earth_EarthWall"] = dict(
    Behavior="Structure", Element="Earth", ManaCost=70, Cooldown=18, CastTime=0.4, RecoveryTime=0.4, Range=550,
    Damage=10, Stagger=25, Duration=15, bDisruptable=True,
    StructureCount=7, StructureHealth=450, StructureExtent={"X": 45, "Y": 85, "Z": 150},
    Params=dict(ArcRadius=550, RiseStep=0.06, Tilt=6, HeightJitter=0.15),
    Montage=clip("Cast_EarthWall"), ActivationTags=CASTING,
    FX=fx("EarthWall", cast="Earth/stone_form", travel="Earth/earth_wall_crack", impact="Earth/earth_wall_rise",
          accent="Earth/earth_wall_crumble", **WALL_MESH))

ROWS["Earth_EarthSpikes"] = dict(
    Behavior="Zone", Element="Earth", ManaCost=60, Cooldown=9, CastTime=0.4, RecoveryTime=0.35, Range=1300,
    AOERadius=300, InnerRadius=150, Damage=55, Stagger=50, Knockback=300, Launch=450, Duration=2.0, bDisruptable=False,
    ZoneKind="LineEruptions", ActivationDelay=0.05, PulseCount=4, PulseInterval=0.12,
    Params=dict(Spike1=320, Spike2=640, Spike3=960, Final=1300, FinalRadius=300, FinalDamage=140, FinalLaunch=950,
                CrumbleAfter=1.2),
    Montage=clip("Cast_EarthSpikes"), ActivationTags=CASTING,
    FX=fx("EarthSpikes", cast="Earth/ground_crack_run", impact="Earth/earth_spike", accent="Earth/spike_final"))

ROWS["Wind_WindBlade"] = dict(
    Behavior="Projectile", Element="Wind", ManaCost=22, Cooldown=1.4, CastTime=0.25, RecoveryTime=0.25, Range=2600,
    AOERadius=0, Damage=50, Stagger=30, Knockback=650, Launch=120, Motion="Wave", ProjectileSpeed=9000,
    ProjectileRadius=30, ProjectileWidth=520, ProjectileGravity=0, bDisruptable=True, PierceCount=99,
    Montage=clip("Cast_WindBlade"), ActivationTags=CASTING,
    FX=fx("WindBlade", cast="Wind/wind_gather", release="Wind/wind_slash", impact="Wind/wind_blade_impact"))

ROWS["Wind_Tornado"] = dict(
    Behavior="Zone", Element="Wind", ManaCost=110, Cooldown=22, CastTime=0.55, RecoveryTime=0.45, Range=1600,
    AOERadius=360, Damage=12, Stagger=6, Knockback=0, Duration=6, bDisruptable=True, ZoneKind="Vortex",
    ZoneMoveSpeed=220, PulseInterval=0.25,
    Params=dict(GrowTo=560, FormTime=0.4, PullScale=2.2, PullSpeed=650, HeavyPullSpeed=250, LiftTime=1.2,
                LiftHeight=320, OrbitSpeed=300, HeavySlow=0.5),
    Montage=clip("Cast_Tornado"), ActivationTags=CASTING,
    FX=fx("Tornado", cast="Wind/wind_gather", accent="Wind/tornado_form", travel="Wind/tornado_loop"))

ROWS["Wind_WindBurst"] = dict(
    Behavior="Zone", Element="Wind", ManaCost=35, Cooldown=7, CastTime=0.12, RecoveryTime=0.3, Range=0,
    AOERadius=620, Damage=30, Stagger=50, Knockback=1500, Launch=350, Duration=0.5, bDisruptable=False, ZoneKind="Burst",
    Params=dict(Invulnerable=0.25, DeflectRadius=700),
    Montage=clip("Cast_WindBurst"), ActivationTags=CASTING,
    FX=fx("WindBurst", cast="Wind/wind_compress", impact="Wind/wind_burst"))

# ------------------------------------------------------------------------------------------------ barrage shots
ROWS["Barrage_Stone"] = dict(
    Behavior="Projectile", Element="Earth", ManaCost=0, Cooldown=0, CastTime=0, RecoveryTime=0, Range=3000,
    AOERadius=120, Damage=34, Stagger=40, Knockback=400, Launch=120, Motion="Straight", ProjectileSpeed=7000,
    ProjectileRadius=20, ProjectileGravity=0, bDisruptable=True,
    FX=fx("StoneCannon", travel="Earth/stone_cannon_launch", impact="Earth/rock_impact", scale=0.7))
ROWS["Barrage_WaterCannon"] = dict(
    DisplayName="Barrage: Water Lance",
    Behavior="Projectile", Element="Water", ManaCost=0, Cooldown=0, CastTime=0, RecoveryTime=0, Range=3000,
    AOERadius=110, Damage=30, Stagger=22, Knockback=400, Motion="Piercing", ProjectileSpeed=7500, ProjectileRadius=18,
    ProjectileGravity=0, bDisruptable=True, PierceCount=1,
    FX=fx("WaterBullet", travel="Water/water_lance", impact="Water/water_splash", scale=0.8))
ROWS["Barrage_WindBlade"] = dict(
    Behavior="Projectile", Element="Wind", ManaCost=0, Cooldown=0, CastTime=0, RecoveryTime=0, Range=2600,
    AOERadius=0, Damage=22, Stagger=14, Knockback=250, Motion="Wave", ProjectileSpeed=8000, ProjectileRadius=25,
    ProjectileWidth=260, ProjectileGravity=0, bDisruptable=True, PierceCount=99,
    FX=fx("WindBlade", travel="Wind/wind_blade_swish", impact="Wind/wind_blade_impact", scale=0.5))
ROWS["Barrage_FireBurst"] = dict(
    DisplayName="Barrage: Fireball",
    Behavior="Projectile", Element="Fire", ManaCost=0, Cooldown=0, CastTime=0, RecoveryTime=0, Range=3000,
    AOERadius=160, Damage=32, Stagger=18, Knockback=350, Launch=120, BurnSeconds=2, BurnDamagePerSecond=6,
    Motion="Straight", ProjectileSpeed=5000, ProjectileRadius=24, ProjectileGravity=0, bDisruptable=True,
    FX=fx("Fireball", travel="Fire/fireball_launch", impact="Fire/fire_explosion", scale=0.75))
ROWS["Barrage_Finale"] = dict(
    DisplayName="Barrage: Four-Element Finale", CharacterRequirement="Rudeus",
    Description="The four formations collapse into one orb of earth, water, fire and wind that bursts in every element at once.",
    Behavior="Projectile", Element="Arcane", ManaCost=0, Cooldown=0, CastTime=0, RecoveryTime=0, Range=3000,
    AOERadius=900, Damage=260, Stagger=120, Knockback=1100, Launch=550, Motion="Straight", ProjectileSpeed=7000,
    ProjectileRadius=60, ProjectileGravity=0, bDisruptable=True, Params=dict(SplashScale=1.0),
    FX=fx("BarrageFinale", travel="Earth/stone_cannon_launch", impact="Rudeus/barrage_finale"), MasteryXP=2)

# ------------------------------------------------------------------------------------------------ awakened versions
# Awakening overrides keep the upgraded behaviour and hit harder.
AWAKENED = {
    "Rudeus_StoneCannon_Awakened": ("Rudeus_StoneCannon", dict(Damage=110, ManaCost=50, Cooldown=1.4, CastTime=0.3,
                                                               PierceCount=4, PierceMaxHealth=900, ProjectileSpeed=10000),
                                    dict(PresetScale=1.25)),
    "Rudeus_Quagmire_Awakened": ("Rudeus_Quagmire", dict(AOERadius=1100, ManaCost=100, Cooldown=12, CastTime=0.4, Duration=11),
                                 dict()),
    "Orsted_DragonStep_Awakened": ("Orsted_DragonStep", dict(Damage=26, Stagger=40, Cooldown=2.2, CastTime=0.06),
                                   dict(PresetScale=1.2)),
}

DEFAULT_LOADOUT = {
    "Rudeus": ["Rudeus_StoneCannon", "Rudeus_Quagmire", "Rudeus_ElementalBarrage", "Fire_Fireball"],
    "Orsted": ["Orsted_DisturbMagic", "Orsted_DragonStep", "Orsted_DragonCrush", "Wind_WindBurst"],
}

ELEMENT_TEXT = {
    "Fire": "Fire at its most violent: a compressed fireball that burns white when charged, a curved wall of flame "
            "that rolls across the field, and the Inferno, whose circles and pillars turn a battlefield into a furnace.",
    "Water": "Real water under pressure: a lance that punches through its first target, a dragon of moving water that "
             "hunts its prey, and a flood that carries enemies away.",
    "Wind": "Wind seen through what it moves: a giant blade of compressed air, a tornado that lifts small enemies into "
            "its funnel, and a burst that throws everything back.",
    "Earth": "Earth that reshapes the ground: a spinning stone cannon, a fractured wall that cracks before it falls, and "
             "spikes that march to an enormous final lance.",
}

MONTAGE_KEYS = ("Montage", "ChargeLoopAnim", "ReleaseAnim")


def ordered(row):
    first = ["AbilityID", "DisplayName", "Description", "Behavior", "Element", "CharacterRequirement", "ElementRequirement"]
    out = collections.OrderedDict((k, row[k]) for k in first if k in row)
    for k, v in row.items():
        if k not in out:
            out[k] = v
    return out


def rebuilt(current, spec):
    row = collections.OrderedDict((k, current[k]) for k in KEEP if k in current)
    spec = dict(spec)
    kept_fx = {k: v for k, v in current.get("FX", {}).items() if k in KEEP_FX}
    new_fx = spec.pop("FX", None)
    row.update(spec)
    if new_fx is not None:
        merged = collections.OrderedDict(kept_fx)
        merged.update(new_fx)
        row["FX"] = merged
    return ordered(row)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()

    path = os.path.join(DATA, "Abilities.json")
    rows = json.load(open(path, encoding="utf-8"), object_pairs_hook=collections.OrderedDict)
    by_id = collections.OrderedDict((r["AbilityID"], r) for r in rows)

    changed = []
    for ability_id, spec in ROWS.items():
        current = by_id.get(ability_id, collections.OrderedDict(AbilityID=ability_id))
        new = rebuilt(current, spec)
        new.setdefault("MasteryXP", 1 if ability_id.startswith("Barrage_") else 5)
        if new != current:
            changed.append(ability_id)
        by_id[ability_id] = new

    for ability_id, (base_id, overrides, fx_overrides) in AWAKENED.items():
        current = by_id.get(ability_id)
        if current is None:
            continue
        spec = json.loads(json.dumps(ROWS[base_id]))
        spec.update(overrides)
        spec["FX"].update(fx_overrides)
        new = rebuilt(current, spec)
        if new != current:
            changed.append(ability_id)
        by_id[ability_id] = new

    out_rows = list(by_id.values())

    # Characters: 1-3 are the character's uniques, 4 an element spell (players' saved loadouts are kept).
    cpath = os.path.join(DATA, "Characters.json")
    chars = json.load(open(cpath, encoding="utf-8"), object_pairs_hook=collections.OrderedDict)
    for c in chars:
        if c.get("CharacterID") in DEFAULT_LOADOUT:
            c["DefaultLoadout"] = DEFAULT_LOADOUT[c["CharacterID"]]
        if c.get("CharacterID") == "Orsted":
            c["SpecialAbility"] = "Orsted_SaintDragonAura"

    epath = os.path.join(DATA, "Elements.json")
    elements = json.load(open(epath, encoding="utf-8"), object_pairs_hook=collections.OrderedDict)
    for e in elements:
        if e.get("Element") in ELEMENT_TEXT:
            e["Description"] = ELEMENT_TEXT[e["Element"]]

    print("Abilities.json: %d rows, %d upgraded or added: %s" % (len(out_rows), len(changed), ", ".join(changed)))
    if a.dry_run:
        print("dry run: nothing written")
        return
    for name, data in (("Abilities.json", out_rows), ("Characters.json", chars), ("Elements.json", elements)):
        p = os.path.join(DATA, name)
        trailing = open(p, encoding="utf-8").read().endswith("\n")
        with open(p, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
            if trailing:
                f.write("\n")
    print("wrote Abilities.json, Characters.json, Elements.json; now run python3 Tools/validate_data.py")


if __name__ == "__main__":
    sys.exit(main())
