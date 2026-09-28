# LAPLACE: ability and VFX combat overhaul (design + contracts)

This is the single source of truth for the combat overhaul. It covers what every ability does, how big it is, and how its effects, animation and sound are staged. It also fixes the contracts between the pieces built in parallel:
- gameplay C++
- the VFX kit
- the generated art assets
- the animations

**Status:** everything here is written offline in a cloud container with no Unreal Engine. The in-engine playtest and grading loop (§8) has to run on the Mac.

## 1. Goals (from the brief)

These characters are extremely powerful, and every ability must read that way the moment it is pressed. The target qualities:
- **Big and fast:** large, cinematic, fast, responsive.
- **Heavy impact:** destructive-looking, slightly overpowered.
- **Distinct:** highly animated, visually distinct from each other.
- **Area:** each can hit several enemies.

Hard rules:
- **Full sequence every time.** Every ability plays: anticipation → casting animation → mana/VFX build-up → release → impact → environment reaction → recovery. Stronger attacks get stronger anticipation and impact.
- **Never a static caster.** The character never stands still while magic simply appears. Effects originate from the hands (cast sockets) or from visible formations, never from the stomach, and never before the casting animation starts.
- **Forgiving, but honest, hitboxes.** An ability's `AOERadius` (or width) is the *visual* size of its damage area. The effective hit radius is `visual × HitForgiveness`, with a default of **1.12** (range 1.10–1.15). The effects always scale from the same visual number, so a giant hitbox always has giant effects and there are no invisible hits.
- **No leftovers.** Nothing lingers after an ability ends except environment marks (scorch, cracks, wet ground, craters), which fade out within 12–20 s. Important map geometry is never destroyed.
- **Camera.** Minor abilities get a tiny impulse, heavy ones a short shake, ultimates a larger shake plus a brief FOV kick, always attenuated by distance and never constant. Hit-stop (0.06–0.09 s) is reserved for very powerful close-range impacts (Dragon Crush).
- **Sound.** Every spell has positional sound for build-up, travel (where it travels) and impact.
- **Performance.** Quality levels scale effect density (`mt.VFX.Quality` 0–3). Hero layers stay at full quality, and distant effects shed particles. Effects are pooled, and there are hard caps on live particles and decals. No spell leaves persistent collision actors behind.

**What it must never look like:** default particles, coloured spheres, generic beams, flat PNG cards, directionless random particles, tiny hitboxes, giant invisible hitboxes, attacks clipping through everything, uncontrolled screen shake, opaque effects that cover the screen, or identical casting animations for every ability.

**Not in scope, and why:**
- **Gameplay Ability System:** the project doesn't use GAS. Its own data-driven ability system already has everything the brief lists per ability: mana, cooldown, cast time, charge, damage, radius, knockback, status effects, animation, VFX and sound. It is structured like GAS:
  - abilities are instances with phases;
  - status effects play the role of Gameplay Effects;
  - VFX cues play the role of Gameplay Cues, cosmetic and separate from gameplay.

  Migrating to GAS would be a rewrite, and nothing can be tested here.
- **Multiplayer:** the game has no networking today. The new code keeps damage and hit resolution in gameplay code and all visuals in cues, so replication can be added later without untangling them.

## 2. Shared ability pipeline (C++)

| Stage | What happens | Where |
|---|---|---|
| Anticipation | The ability starts: cast animation (`Montage`), `<CueSet>_Build` attached to the cast socket (loop; `Charge` updated every frame), telegraphs (Earth Spikes aim line) | `UMTAbility::TryActivate` / `Tick` |
| Release | On the clip's **`Release` AnimNotify** (frame-accurate, per character), or at `CastTime` if no notify arrives: `ExecuteAction`, then `<CueSet>_Release` at the hand, aimed | `UMTAbility::HandleAnimEvent` |
| Travel / body | Gameplay actor (projectile, wave, serpent, zone, spikes) plus looping cues | actors |
| Impact | Damage, knockback, launch, status, then `<CueSet>_Impact`. The cue carries its camera tier and environment decals. | actors / behaviours |
| Recovery | `RecoveryTime`; dodge-cancellable (existing) | `UMTAbility` |

Two new rules sit on top of these stages:
- **Full-body commit.** `bFullBodyCommit` (Inferno, Flood, Water Dragon, Barrage finale, Dragon Crush) stops the caster's movement during anticipation and release. Otherwise the existing upper-body layer keeps casting blended with locomotion.
- **Disruption.** Disturb Magic can cancel an ability in Anticipation. `UMTAbilityComponent::LockAbility(Id, Seconds)` then seals it: `CanActivate` fails with "Sealed". This mirrors canon, where Ran Ma seals the magic of one hand while the other still works.

**New `FMTAbilityData` fields:**
- `Tier` (Minor / Heavy / Ultimate)
- `HitForgiveness` (1.12)
- `PierceCount`, `PierceMaxHealth`
- `ChargeSizeScale`, `ChargeRadiusScale`, `ChargeKnockbackScale`
- `Launch` (vertical cm/s), `Pull` (cm/s)
- `ProjectileWidth` (a crescent box-sweep for Wind Blade)
- `bFullBodyCommit`
- `BurnSeconds`, `BurnDamagePerSecond`
- `FX.CueSet`
- `Params`: a `TMap<FName, float>` of behaviour-specific numbers. The names are listed per ability below.

## 3. The abilities

Numbers are visual sizes, where VR is the visual radius. The effective hit size is ×1.12 unless noted. Times are for Rudeus's clips. Orsted's clips are shorter, and his notifies fire earlier.

### 3.1 Rudeus: Signature Stone Cannon (`Rudeus_StoneCannon`, Projectile/Piercing, chargeable)
- **Cast (0.35 s, hold to charge up to 1.6 s).** Rudeus raises one hand.
  - Earth fragments rip out of the ground and converge 20–40 cm above the palm.
  - They visibly form, compress, smooth and densify, then start rotating and accelerate to an insane spin.
  - Circular pressure rings form around the slug, and loose dust is pulled toward it.
  - At maximum spin the slug vibrates at high frequency and distorts the air around it.
  - Charge increases size (×1.7), spin, ring count and the impact radius (×1.6).
- **Release (palm thrust).** The launch runs at 9000 cm/s (tap) up to 14400 cm/s (full), too fast for the eye to follow at launch. At the release the game plays:
  - a huge air-pressure cone;
  - a circular shockwave at the hand;
  - a trailing spiral;
  - a dust blast behind Rudeus;
  - a camera impulse;
  - a directional sonic boom.
- **Impact.** The impact produces:
  - a large radial shockwave (VR 260, up to 416 charged);
  - stone fragments, dirt and debris;
  - a crater decal;
  - a flash;
  - strong knockback (2200, up to 3520 charged).
- **Pierce.** It continues through up to 3 non-boss targets with ≤ 600 max health, then stops on the first heavy target.
- **Damage and cost.** Direct damage 90, up to ×2.4 at full charge; the splash does 60% of that. Mana 45 (×2 at full), cooldown 1.6 s.
- **Hit and anim.** The hit radius for characters is 40 cm, the size of the visible spiral shroud. Anim: `StoneCannon_Charge` → `StoneCannon_Hold` loop → `StoneCannon_Release`. CueSet `StoneCannonRudeus`.
- **Must not be:** a slowly floating brown ball, a pebble, a basic projectile, motionless before launch, visually weak, or missing its impact.

### 3.2 Rudeus: Quagmire (`Rudeus_Quagmire`, Zone/Mire, chargeable)
- **Cast (0.45 s, hold to expand up to 1.8 s).** Rudeus lowers his palm toward the ground, and a faint circular disturbance expands under the target area.
- **Transformation (1.2 s).** Cracks form (0–0.35 s), then water pushes up through them (0.35–0.8 s), then the soil liquefies (0.8–1.2 s). The terrain reads solid → saturated → moving mud → deep quagmire. The slow ramps in with the transformation.
- **Size.** VR 900 (a plaza) at a tap, up to VR 1980 at full charge, which covers a battlefield. Duration 9 s, then 1 s of drying.
- **Depth.** Depth is 1.0 at the centre and falls smoothly to 0.35 at the edge. It scales each penalty:
  - speed ×(1 − 0.72·depth)
  - acceleration ×(1 − 0.5·depth)
  - jump ×(1 − 0.7·depth)
  - dodge distance ×(1 − 0.6·depth)
- **Effects on targets.**
  - **Momentum loss:** fast targets lose 60%·depth of their momentum on entry.
  - **Sinking:** targets visibly sink by up to 30 cm × depth over 1.5 s. Heavy targets (bosses) sink only 35% as far and take 60% of the penalties.
  - **Traces:** moving targets leave ripples and mud displacement, and sinking footprints.
- **Visuals.** Ripples, bubbles, mud splashes, displaced water, moving surface normals, and earth fragments along the boundary.
- **Stone synergy.** `State.InQuagmire` stays, and stone hits on mired targets do more.
- **Cost and anim.** Mana 90 (×1.8 at full), cooldown 14 s. Anim: `Quagmire` → `Quagmire_Hold` → `Quagmire_Release`. CueSet `Quagmire`.
- **Must not be:** a painted brown circle, instant, fully opaque, unreadable, or a 3 m puddle.

### 3.3 Rudeus: Elemental Barrage (`Rudeus_ElementalBarrage`, Barrage)
- **Cast (0.6 s).** Rudeus raises both hands, and four formations appear around him (offsets in his local frame, X forward, Y right, Z up):
  - Fire, behind upper left: (−70, −95, +175)
  - Water, behind upper right: (−70, +95, +175)
  - Earth, lower left: (−30, −120, +70)
  - Wind, lower right: (−30, +120, +70)

  Each formation spins with its own particle behaviour (embers vs water ring vs orbiting rocks vs streak rings), and the set sways ±12° around him.
- **Barrage (3.0 s, one shot every 0.2 s).** Rudeus points, and the formations fire in turn: Stone Cannon (Earth) → Water Lance (Water) → Wind Blade (Wind) → Fireball (Fire) → repeat. Each shot launches from its own formation with ±4° spread. Rudeus can drift at 35% speed.
- **Finale (at 3.6 s).** The four formations collapse together 150 cm in front of him over 0.35 s. The combined orb then detonates as a multi-element explosion (VR 900):
  - earth erupts outward;
  - water spirals up;
  - fire expands through the centre;
  - wind pushes the whole blast outward.

  It does 260 damage, 3000 knockback and 600 launch.
- **Cost.** Mana 140, cooldown 16 s.
- **Sub-rows:** `Barrage_Stone`, `Barrage_WaterLance`, `Barrage_WindBlade`, `Barrage_Fireball`, `Barrage_Finale`.
- **Anim and cues.** Anim `Barrage` has events at Formation 0.35, BarrageStart 0.6 and Finale 3.6. CueSets: `Barrage`, `BarrageStone`, `BarrageWater`, `BarrageWind`, `BarrageFire`, `BarrageFinale`.
- **Must not be:** four coloured balls, one beam, one explosion with four colours, slow, or repetitive. Each element keeps its physical identity.

### 3.4 Orsted: Disturb Magic (`Orsted_DisturbMagic`, Disrupt)
- **Cast (0.12 s).** Orsted calmly raises one hand, with no wind-up. A small concentrated distortion forms at the palm: pale blue-white energy, black distortion ripples, thin dragon-shaped geometric patterns, and warped air.
- **Target.** It picks, in order: the locked target, else a hostile that is casting in front of him (35°, 1800 cm), else the aim point.
- **Pulse.** A fast, nearly invisible pulse (6500 cm/s) travels to the target. It shows as a thin warped-air ripple with a small glyph at its head.
- **Hitting a caster.** If the pulse reaches a target whose disruptable spell is still forming (Anticipation or charge), that spell is cancelled. It collapses by element:
  - Fire: collapses into sparks.
  - Water: loses shape and falls as drops.
  - Earth: the construct crumbles.
  - Wind: disperses.

  The ability is then **sealed for 3 s** (`LockAbility`). Orsted gets 60% of his cooldown back and a Dragon God Knowledge trigger.
- **Along the path.** Hostile projectiles within 220 cm of the pulse line collapse (defensive use).
- **Persistent magic.** Anything within 450 cm of the end point is destabilised rather than deleted:
  - zones: remaining time ×0.5, strength ×0.6;
  - walls: take 40% of their maximum health;
  - tornadoes: shrink ×0.6, remaining time ×0.5;
  - serpents in flight: collapse.
- **No damage, no stun, no permanent silence.** Mana 45, cooldown 6 s. CueSet `DisturbMagic`.
- **Must not be:** a laser, a damage projectile, an explosion, a permanent silence, or a stun.

### 3.5 Orsted: Dragon Step (`Orsted_DragonStep`, Dash)
- **Cast (0.08 s).** The stance lowers slightly, and dragon aura gathers at the feet for a fraction of a second. Then, BOOM:
  - a circular ground-pressure burst (VR 350);
  - a dust ring;
  - 3 streaked afterimages;
  - a warped-air trail.
- **Targeted (a lock within 2600 cm).** Orsted reappears beside or slightly behind the target, 115 cm from its centre, on the flank facing his approach, in **0.12 s**. His arrival makes another pressure ring (VR 220), with a light shock of 10 damage and 25 stagger. It opens a **0.9 s follow-up window**: Dragon Crush then starts ×0.35 faster, auto-targets that enemy and deals ×1.2 damage.
- **Free (no target).** An ultra-long dash of 1700 cm (a dodge covers 380–450 cm) in 0.2 s. A capsule sweep shortens it at walls.
- **Defence.** Invulnerable during the travel. Stamina 20, cooldown 3.5 s. CueSet `DragonStep`.
- **Must not be:** a portal, a slow fade, a sprint, or a float.

### 3.6 Orsted: Dragon Crush (`Orsted_DragonCrush`, Strike, new)
- **Wind-up (0.40 s; 0.14 s from Dragon Step).** Orsted pulls one arm back, and a tight, compressed Dragon God aura wraps it (small, not yet giant). He lunges if the target is within 450 cm.
- **Impact frame.** All the stored energy releases at once:
  - a massive circular shockwave (VR 650);
  - broken ground (a crack decal, VR 520);
  - raised chunks of earth;
  - pressure distortion;
  - an expanding dust ring;
  - a bass impact;
  - a short camera shake and **hit-stop 0.085 s**.
- **Targets.** The primary target is the lock within 350 cm in front of him, else the closest enemy within 300 cm in a 70° cone; with neither, he strikes the ground. The primary takes 320 damage, 5000 knockback and 900 launch (knockdown). Everyone else in the shockwave takes 160 damage and is launched outward (2600 / 500).
- **Cost.** Stamina 35, cooldown 7 s. CueSet `DragonCrush`.
- **Must not be:** a generic punch, a fire punch, a beam, or a sword technique.

### 3.7 Fire (shared)
- **Fireball (`Fire_Fireball`, Projectile, chargeable up to 1.2 s).**
  - A compressed sphere of fire forms above the palm, with the fire rotating **inward** rather than flickering outward.
  - Charge shifts the colour: orange → bright yellow → almost white core.
  - It travels at 5500 cm/s (up to ×1.25).
  - Impact: explosion VR 320 → 560, damage 60 → 150, knockback 900 → 1600, burning for 4 s.
  - Cast 0.30 s. Mana 24, cooldown 1.6 s.
- **Flame Wave (`Fire_FlameWave`, Wave).**
  - An arm sweep sends a huge curved wall of fire (120°) rolling forward from 150 cm out to 1300 cm over 0.9 s (height 260, front band 180).
  - Each enemy is hit once as the front passes: 75 damage, 800 knockback, burning.
  - Scorch marks and embers are left along the path.
  - Cast 0.42 s. Mana 55, cooldown 8 s.
- **Inferno (`Fire_Inferno`, Zone/Inferno, ultimate, full-body commit).**
  - The caster raises one hand. Fire circles appear around the target area (VR 1100) and a flame vortex forms at its centre.
  - From 0.6 s, pillars erupt repeatedly: 3 every 0.35 s for 3.4 s, biased toward enemies (pillar VR 190, height 900).
  - Each pillar deals 70 damage and a small launch. The area burns for 15 per second.
  - Cast 0.90 s. Mana 220, cooldown 30 s.

### 3.8 Water (shared)
- **Water Bullet (`Water_WaterBullet`, Projectile/Piercing).**
  - High-pressure water condenses above the palm and launches as a compressed lance (8000 cm/s), with vapour and pressure rings around it.
  - Pierces 1 target. Damage 48, knockback 1000.
  - The splash burst has VR 150.
  - Cast 0.22 s. Mana 20, cooldown 1.2 s.
- **Water Dragon (`Water_WaterDragon`, Serpent, ultimate, full-body commit).**
  - A gigantic serpent made entirely of moving water: 16 segments, 1700 cm long, with a 220 cm head.
  - It builds from spiralling streams (0.8 s), circles behind and above the caster (0.5 s), then hunts the target for up to 2.4 s (2800 cm/s, turning up to 160° per second).
  - On contact it collapses into an enormous water explosion (VR 780): 220 damage, 2400 knockback, 450 launch, wet ground.
  - Cast 0.75 s. Mana 160, cooldown 22 s.
- **Flood (`Water_Flood`, Wave/front, ultimate, full-body commit).**
  - The caster slams both arms forward while water accumulates behind them for 0.5 s.
  - A **huge wave** is then released: 2400 wide, 380 high, travelling 2600 cm at 1500 cm/s and following the terrain.
  - Enemies struck take 90 damage and are **carried along** (pushed at 1300 cm/s while inside the front).
  - It leaves wet trails and splashes against obstacles.
  - Cast 0.70 s. Mana 170, cooldown 26 s.

### 3.9 Earth (shared)
- **Stone Cannon (`Earth_StoneCannon`, Projectile/Piercing, chargeable).**
  - Still a fast, hardened, rotating slug (6500 cm/s, up to ×1.4), clearly smaller and plainer than Rudeus's signature cannon.
  - Impact VR 170 (up to ×1.4), damage 60 (up to ×2), pierces 1 weak target.
  - Mana 32, cooldown 2.0 s.
- **Earth Wall (`Earth_EarthWall`, Structure).**
  - The caster raises a hand, and 7 connected, naturally fractured segments (3 mesh variants, random tilt and height) erupt one after another from the centre outward, on a 550 cm arc.
  - The wall blocks projectiles, movement and some magic.
  - Each segment has 450 health and **visibly cracks** at 66% and 33% before crumbling. It lasts 15 s.
  - Cast 0.40 s. Mana 70, cooldown 18 s.
- **Earth Spikes (`Earth_EarthSpikes`, EruptionLine).**
  - While casting, an aim line is telegraphed on the ground (1400 cm).
  - Stone lances then erupt one after another, 0.12 s apart: spikes 1–3 at 320, 640 and 960 (VR 150, height 280, 55 damage each), then an **enormous final spike** at 1300 (VR 300, height 700, 140 damage, launches non-boss enemies 1300 upward).
  - The spikes crumble after 1.2 s.
  - Cast 0.40 s. Mana 60, cooldown 9 s.

### 3.10 Wind (shared)
- **Wind Blade (`Wind_WindBlade`, Projectile/Wave).**
  - A giant curved slash of compressed air, **visible** as pale cyan and white distortion, condensation and streaks.
  - Width VR 520, thickness 60. It travels at 9000 cm/s over 2600 cm and pierces everything.
  - The hit test is a box sweep across the crescent.
  - Damage 50, knockback 850. Cast 0.25 s. Mana 22, cooldown 1.4 s.
- **Tornado (`Wind_Tornado`, Zone/Vortex).**
  - A large spinning vortex lasting 6 s. It forms in 0.4 s and drifts toward the target at 220 cm/s.
  - It grows as it lives: radius VR 360 → 560, height 950 → 1400.
  - Dust, leaves and debris are drawn in and orbit it.
  - **Small enemies are lifted** and orbit the funnel for 1.2 s before being dropped. Large enemies are slowed ×0.5 and pulled.
  - The pull reaches radius ×2.2, at 650 cm/s (250 for heavy targets). The core deals 12 damage every 0.25 s and deflects weak projectiles.
  - Cast 0.55 s. Mana 110, cooldown 22 s.
- **Wind Burst (`Wind_WindBurst`, Burst).**
  - Air visibly compresses around the caster for 0.12 s, then a huge sphere of pressure releases (VR 620).
  - Knockback 2400 in every direction, plus a 280 launch and 30 damage.
  - It deflects light incoming projectiles and gives 0.25 s of invulnerability, making it an escape or counter move.
  - Mana 35, cooldown 7 s.

### 3.11 Hotbar and slots (no placeholders)

| Slot | Rudeus | Orsted |
|---|---|---|
| LMB | Stone Bullet | Palm Strike |
| 1 / 2 / 3 | Stone Cannon, Quagmire, Elemental Barrage | Disturb Magic, Dragon Step, **Dragon Crush** |
| F | Demon Eye | **Saint Dragon Battle Aura** (this slot was empty) |
| G | Awakening | Awakening |
| 4–6 / 7–9 | the element's 3 spells, as above | same |

## 4. Casting personality

| | Rudeus | Orsted |
|---|---|---|
| Feel | Overwhelming talent; chantless, instinctive, creative, explosive, high-mana, less restrained | Power is normal for him: precise, controlled, intimidating, efficient, effortless |
| Hands | The hands actively shape the magic: fingers move, wrists rotate, palms rise, energy is pulled together, and gestures direct each attack | Small hand movements produce enormous consequences, and his posture stays calm |
| Timing | Visible build-up, and fast | Clips run about 55–70% as long as Rudeus's, with smaller gestures |
| VFX | Raw energy, mana distortion around the hands before big releases, pressure lifting clothes and hair | Cleaner build-ups (`<Cue>@Orsted` variants); a subtle Dragon God aura (pale gold, white, deep blue) on stronger techniques; impacts just as hard |

Both use the same ability rows. `AMTCharacterBase::ResolveLineageAnim` plays each character's own `A_<C>_<Key>` clip. The VFX kit tries the `<CueId>@<CharacterId>` variant before the default cue.

## 5. Contract: VFX kit (C++) — owned by the VFX workstream

Files: `Source/MushokuRPG/Public|Private/VFX/`.

```cpp
USTRUCT(BlueprintType) struct FMTVFXContext {
  FVector Location; FVector Direction = Forward; FVector Normal = Up;   // world space
  float Radius = 0 (visual radius; 0 = authored size); float Scale = 1; float Charge = 0..1; float Intensity = 1;
  float Length = 0; float Height = 0; float Angle = 0;                  // lines, columns, arcs
  FLinearColor Tint (A = 0: no override); FName Style;                  // Style "Orsted" -> tries CueId@Orsted first
  TWeakObjectPtr<USceneComponent> AttachTo; FName AttachSocket;          // attached cues follow
  float Duration = -1;                                                  // loops: auto-stop after (seconds)
  TWeakObjectPtr<AActor> Instigator;
};
class UMTVFXSubsystem : public UTickableWorldSubsystem {
  static UMTVFXSubsystem* Get(const UObject* WorldContext);
  int32 PlayCue(FName CueId, const FMTVFXContext& Ctx);  // 0 = nothing (unknown or culled cue); never crashes
  void UpdateCue(int32 Handle, const FMTVFXContext& Ctx); // move / regrow running loops
  void StopCue(int32 Handle, bool bImmediate = false);    // loops fade out; one-shots finish
  bool IsCuePlaying(int32 Handle) const; bool HasCue(FName CueId) const;
  int32 GetActiveCueCount() const; int32 GetActiveElementCount() const;   // leak checks
};
```
- **Cue definitions:** `Content/Data/VFXCues.json`, loaded at runtime and hot-reloadable. Each cue lists layers of these types:
  - Flash, Ring, Cone, Sprites (CPU particles on pooled instanced quads), Debris (instanced rock meshes)
  - Mesh (animated static mesh, optionally arrayed along an arc, line or ring), Trail, Decal, Light, Distortion
  - Sound (positional, with runtime attenuation), Camera (tier)

  An optional `"Niagara"` soft path replaces the procedural layers once an authored Niagara system exists, fed the user parameters `User.Radius`, `User.Charge`, `User.Color`, `User.Direction`, `User.Length`, `User.Height`.
- **Camera layers** call `MTCamera::Impulse(WorldContext, Source, Tier, Scale)`. The gameplay workstream provides it in `Public/Combat/MTCameraImpulse.h`.

### Cue names the gameplay code plays
The names are `<CueSet>_<Role>`. A missing cue is a silent no-op, and the validator lists missing ones.

| CueSet | Roles |
|---|---|
| Projectile sets: `StoneCannonRudeus`, `StoneCannon`, `Fireball`, `WaterBullet`, `WindBlade`, `BarrageStone`, `BarrageWater`, `BarrageWind`, `BarrageFire`, `BarrageFinale` | `Build` (loop on the hand, Charge), `Release`, `Travel` (loop on the projectile), `Pierce`, `Impact` (Radius), `Dissipate` |
| `Quagmire` | `Build`, `Cast`, `Crack`, `Seep`, `Liquefy`, `Loop` (Radius), `Ripple`, `Footprint`, `Splash`, `End` |
| `Barrage` | `Build`, `Formation_Fire`, `Formation_Water`, `Formation_Earth`, `Formation_Wind` (loops, moved every frame), `Muzzle_Fire`, `Muzzle_Water`, `Muzzle_Earth`, `Muzzle_Wind`, `Collapse` |
| `DisturbMagic` | `Build`, `Pulse` (moving loop; Direction, Length), `Hit`, `Collapse_Fire`, `Collapse_Water`, `Collapse_Earth`, `Collapse_Wind`, `Collapse_Arcane`, `Destabilize`, `Seal` (loop, Duration) |
| `DragonStep` | `Gather`, `Depart`, `Trail` (loop on Orsted), `Arrive` |
| `DragonCrush` | `Charge` (loop on the hand), `Impact` (Radius), `TargetHit` |
| `FlameWave` | `Build`, `Release`, `Front` (loop: Location = arc origin, Direction, Radius = current front distance, Angle, Height), `Hit`, `Scorch`, `End` |
| `Inferno` | `Build`, `Circles` (Radius), `Vortex` (loop, Height), `Pillar` (Radius, Height), `Loop` (Radius), `End` |
| `WaterDragon` | `Build`, `Form`, `Body` (loop on the head), `Impact` (Radius) |
| `Flood` | `Build`, `Gather`, `Front` (loop: Location = front centre, Direction, Radius = half width, Height), `Splash`, `Wet`, `End` |
| `EarthWall` | `Build`, `Rise` (per segment), `Crack`, `Crumble` |
| `EarthSpikes` | `Build`, `Aim` (loop line: Direction, Length), `Spike` (Radius, Height), `Final` (Radius, Height), `Crumble` |
| `Tornado` | `Build`, `Form`, `Loop` (Radius, Height, moving), `Lift` (on lifted enemies), `End` |
| `WindBurst` | `Compress` (on the caster), `Release` (Radius) |
| Generic | `Hit_Fire`, `Hit_Water`, `Hit_Earth`, `Hit_Wind`, `Hit_Arcane`, `Burning` (loop on characters), `OrstedAura` (loop on Orsted) |

## 6. Contract: generated art assets — owned by the assets workstream

**Location.** Sources live in `SourceArt/VFX/{Textures,Meshes,Sounds}/` and are generated by `Tools/vfx/*.py`. `Content/Python/mt_create_vfx_assets.py` imports them and builds the master materials. It is headless and idempotent, and runs from `Tools/mac/build_and_setup.sh` after `mt_create_materials.py`.

**Unreal paths.** Assets import to `/Game/VFX/Textures/T_MT_*`, `/Game/VFX/Meshes/SM_MT_*`, `/Game/VFX/Materials/M_MT_*` and `/Game/VFX/Sounds/S_MT_*`.

**Master materials** (every parameter listed must exist with exactly these names):

| Material | Blend | Parameters |
|---|---|---|
| `M_MT_VFX_Additive` | additive, unlit, two-sided | `Tex`, `NoiseTex`, `Color`, `Intensity`, `Opacity`, `Erosion`, `PanSpeed`, `Tiling`, `FresnelPower`, `SoftDepth`, `UseInstanceData`, `FlipbookGrid` |
| `M_MT_VFX_Translucent` | translucent, unlit, two-sided | same as Additive |
| `M_MT_VFX_Distortion` | translucent refraction | `NormalTex`, `Strength`, `Opacity`, `PanSpeed`, `Tiling`, `UseInstanceData` |
| `M_MT_VFX_Water` | translucent, lit surface | `Color`, `ShallowColor`, `Opacity`, `NormalTex`, `FoamTex`, `FoamAmount`, `FlowSpeed`, `Tiling`, `Erosion`, `Intensity` |
| `M_MT_VFX_Fire` | additive emissive | `Color`, `CoreColor`, `Intensity`, `Erosion`, `FlowSpeed`, `Tiling`, `NoiseTex`, `Tex` |
| `M_MT_VFX_Rock` | opaque masked, lit | `Tint`, `Crack`, `Wetness`, `Heat`, `Dissolve`, `AlbedoTex`, `NormalTex` |
| `M_MT_VFX_Aura` | additive fresnel | `Color`, `Intensity`, `FresnelPower`, `NoiseTex`, `PanSpeed`, `Opacity` |
| `M_MT_Decal` | deferred decal | `Mask`, `NormalTex`, `Color`, `Opacity`, `Roughness`, `Emissive`, `Wetness`, `Flow`, `Tiling`, `Erosion` |

Notes on the materials:
- `UseInstanceData = 1` makes the material read PerInstanceCustomData: [0..2] RGB tint, [3] opacity, [4] flipbook frame, [5] emissive boost.
- `Erosion` is a noise-threshold dissolve (0 = none, 1 = gone).

**Textures** (`T_MT_`):
- **Sprites:** SoftDot, SmokePuff (4×4 flipbook), Flame (4×4 flipbook), Spark, Ember, Droplet, Bubble, MudBlob, Leaf (2×2), DustPuff, Streak, Ring
- **Noise and normals:** NoiseFBM, NoiseWorley, Caustics, Foam, SwirlNormal, ShockwaveNormal, WaterNormal, RockAlbedo, RockNormal
- **Decal masks:** CrackMask, CraterMask, CraterNormal, ScorchMask, WetMask, MudMask, MudNormal, Footprint, AimLine
- **Circles and glyphs:** MagicCircleFire, MagicCircleEarth, MagicCircleWater, MagicCircleWind, DragonGlyph

All designs are original.

**Meshes** (`SM_MT_`; UE axes X forward, Z up, cm):
- **Shapes:**
  - Quad: YZ plane, faces +X, 100×100
  - Ring: annulus in XY, r 90–100, U = angle, V = radial
  - RingThick: r 50–100
  - Torus
  - Sphere: r 50
  - Cone: apex at the origin, opening along +X, length 100, r 50
  - Disc: r 100
- **Projectile and flame shapes:**
  - Crescent: blade in XY, width 200 along Y, arcing back toward −X
  - Funnel: tornado; bottom r 20, top r 100, height 100, open
  - Pillar: open cylinder, r 50, h 100
  - Spiral: helix ribbon along +X, 3 turns
  - Slug: pointed stone bullet, 60 along +X, r 15
  - FlameTongue
  - FlameWall: segment 100 wide × 100 tall
- **Rocks and earth:**
  - RockA–RockD: irregular chunks, 20–35 cm
  - Shard
  - Spike: tapered stone lance, base r 25, h 100 along +Z, pivot at the base
  - WallSegmentA–WallSegmentC: fractured slabs, 120 (Y) × 60 (X) × 180 (Z), pivot at the base
  - SlabRaised: 100×100×25, pivot at the inner edge
- **Water:**
  - DragonHead: 220 along +X, pivot at the neck
  - DragonSegment: tube 100 along +X, r 45, open, U = angle, V = along
  - WaveSheet: 200 wide (Y) × 100 tall face, curling forward at the top

**Sounds** (`S_MT_`, mono 48 kHz WAV, synthesised):
- **Charge:** Charge_Earth, Charge_Fire, Charge_Water, Charge_Wind, Charge_Arcane
- **Release:** Release_Boom, Release_Whoosh, Release_Fire, Release_Water, Release_Wind
- **Travel:** Travel_Rock, Travel_Fire, Travel_Water
- **Impact:** Impact_Rock, Impact_Fire, Impact_Water, Impact_Wind, Impact_Heavy, Impact_Huge
- **Earth and ground:** Rumble, Crack, Crumble, Erupt, WallRise
- **Loops:** Mud_Loop, Mud_Squelch, Water_Rush_Loop, Fire_Loop, Wind_Loop
- **Orsted:** Disrupt_Pulse, Disrupt_Collapse, Step_Boom, Seal

**Bodies the gameplay actors own** (paths are fixed in C++ with engine-shape fallbacks):
- the Stone Cannon slug (Slug + Rock with `Heat`);
- Fireball (Sphere + Fire);
- the Water Bullet lance (Sphere stretched + Water);
- Wind Blade (Crescent + Distortion and Additive);
- the water dragon (DragonHead / DragonSegment + Water);
- the flood (WaveSheet + Water);
- the flame wave (FlameWall + Fire);
- the tornado (Funnel ×2: Translucent and Additive);
- wall segments (WallSegmentA–C + Rock, `Crack`).

## 7. Contract: animations — owned by the animation workstream

**Clip keys** (both characters; Orsted's versions are minimal and 55–70% as long):

| Key | Events (seconds, Rudeus) | Notes |
|---|---|---|
| `Cast_Fireball`, `Cast_Fireball_Hold` (loop), `Cast_Fireball_Release` | Release 0.30 (anticipation end), Release 0.05 (release clip) | palm up, sphere formed and turned inward by the fingers, then thrown |
| `Cast_FlameWave` | Release 0.42 | horizontal arm sweep |
| `Cast_Inferno` | Release 0.90 | one hand raised overhead, clenched, brought down |
| `Cast_WaterBullet` | Release 0.22 | two fingers aimed, snap forward |
| `Cast_WaterDragon` | Build 0.20, Release 0.75 | both hands spiral up, then point |
| `Cast_Flood` | Release 0.70 | both arms gather, then slam or sweep forward |
| `Cast_StoneCannon`, `Cast_StoneCannon_Hold`, `Cast_StoneCannon_Release` | Release 0.30 / 0.05 | one hand forward, compress, thrust |
| `Cast_EarthWall` | Release 0.40 | hand drives upward from low |
| `Cast_EarthSpikes` | Release 0.40 | hand drives down toward the ground along the aim |
| `Cast_WindBlade` | Release 0.25 | horizontal slash with the hand |
| `Cast_Tornado` | Release 0.55 | circular stirring gesture |
| `Cast_WindBurst` | Release 0.12 | arms pull in (compress), then spread (release) |
| `StoneCannon_Charge` / `_Hold` / `_Release` (Rudeus, upgraded) | Release 0.05 | hand raise, finger and wrist compression, palm thrust with recoil |
| `Quagmire`, `Quagmire_Hold`, `Quagmire_Release` | Release 0.45 / 0.05 | palm lowered, pressing, fingers spread |
| `Barrage` (4.3 s) | Formation 0.35, BarrageStart 0.6, Finale 3.6 | both hands raise, point, chained gestures, both hands converge and push |
| `DisturbMagic` (Orsted) | Release 0.12 | minimal raised hand, finger flick |
| `DragonStep` (Orsted) | Depart 0.08 | low stance, blur lean, arrival settle |
| `DragonCrush` (Orsted) | Impact 0.40 | arm pulled back, driven palm, held, recovery by 0.9 s |

**Event timing.** Events are exported to the sidecar `SourceArt/Characters/<C>/<C>_Animated.anim.json` under `"events": { "<Key>": { "<Event>": seconds } }`. `mt_setup_rudeus.py` adds them to the imported sequences as `UMTAnimNotify_Event` notifies (Python `unreal.MTAnimNotify_Event`, property `event_name`), on a track named `MT`. The data `CastTime` equals Rudeus's `Release` time, and `Tools/validate_data.py` checks this within one frame.

## 8. Testing and grading

- **Offline (this repo).**
  - `Tools/validate_data.py` checks:
    - every hotbar slot is filled for both characters and every element;
    - every ability has an animation;
    - every cue its code plays exists;
    - hit size is ≤ visual × 1.15;
    - `CastTime` matches the clip's Release event.
  - Plus: animation QA per clip, VFX cue previews rendered offline, and asset contact sheets.
- **In-engine (Mac).**
  - `MTAbilityShowcase [Character]` spawns a row of training dummies and casts every slot in turn. Per ability it logs PASS/FAIL: executed, hit ≥ 1 target, no leaked actors or cues after it ended.
  - `Automation RunTests MushokuRPG` runs the data-integrity tests headless.
  - The 10-category grading table from the brief is filled in only after watching every ability in play. It is repeated until the average is ≥ 8.5, with no category below 8.0.

## 9. Research notes

The fandom wiki and Epic docs pages are blocked for this session, so these notes come from search-result summaries.

- **Stone Cannon:** Rudeus's signature improvement on Rock Bullet. He can tweak size, shape (drill-like), speed and temperature, and it can pierce god-level Battle Aura. His "Gatling" variant fires about 10 per second. He casts voicelessly. Sources: [Rudeus/Powers and Abilities](https://mushokutensei.fandom.com/wiki/Rudeus_Greyrat/Powers_and_Abilities), [Game Rant](https://gamerant.com/mushoku-tensei-jobless-reincarnation-rudeus-most-powerful-abilities/).
- **Quagmire:** Earth + Water mixed magic that turns ground into a trapping mud swamp. Rudeus uses it to entrap large numbers of monsters, hence "Quagmire Rudeus". Source: [Quagmire](https://mushokutensei.fandom.com/wiki/Quagmire).
- **Disturb Magic:** infuses counter-magic into a spell while it is forming; if the infused power is greater, the spell dissipates before it manifests. It can target one part of the body; Ran Ma sealed Rudeus's right hand while his left still worked. It was devised by Dragon God Urupen. Sources: [Disturb Magic](https://mushokutensei.fandom.com/wiki/Disturb_Magic), [Orsted/Powers and Abilities](https://mushokutensei.fandom.com/wiki/Orsted/Powers_and_Abilities), [Epicstream](https://epicstream.com/article/mushoku-tensei-disturb-magic-explained).
- **Magic system:** a seven-tier spell ranking; mixed magic combines spells consecutively (for example Frost Nova, Dry Steaming). Source: [Magic Spells](https://mushokutensei.fandom.com/wiki/Magic_Spells).
- **LAPLACE originals:** Dragon Step, Dragon Crush, Elemental Barrage and the elemental move list are this game's own interpretations.
- **Engine:**
  - montages have sections and notifies, and `Montage_JumpToSection` and `PlayMontageNotify` callbacks exist ([Animation Montage](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine), [Animation Notifies](https://dev.epicgames.com/documentation/unreal-engine/animation-notifies-in-unreal-engine?lang=en-US));
  - GAS is made of the ability system component, abilities, effects, attributes and tags, with Gameplay Cues for audio and visuals ([GAS](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system));
  - Niagara systems expose User Parameters set from C++, and Effect Types handle scalability ([Niagara scalability](https://dev.epicgames.com/documentation/unreal-engine/scalability-and-best-practices-for-niagara)).
