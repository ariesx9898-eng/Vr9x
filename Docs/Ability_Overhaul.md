# LA PLACE: ability and VFX combat overhaul (design + contracts)

This file is the single source of truth for the combat overhaul. It covers what every ability does, how big it is, and how its effects, animation and sound are staged. It also fixes the contracts between the pieces built in parallel:
- gameplay C++;
- the effect presets;
- the generated meshes, textures and sounds;
- the animations.

**The overhaul upgrades the game as it exists.** It keeps the ability IDs, the hotbar (LMB, 1–4 loadout, F, G), the ABILITIES menu, the HUD, the map, the runtime effect system (`AMTSpellVFX` presets in `MTVFXLibrary.cpp`), the synthesized audio and the asset pipeline (`Tools/mac/build_and_setup.sh`). Nothing is replaced: rows are retuned, behaviours gain depth, presets get richer, and every ability gets its own casting clip.

**Status:** everything here is written offline in a cloud container with no Unreal Engine. The in-engine playtest and grading loop (§8) runs on the Mac.

## 1. Goals (from the brief)

These characters are extremely powerful, and every ability must read that way the moment it is pressed. The target qualities:
- **Big and fast:** large, cinematic, fast, responsive.
- **Heavy impact:** destructive-looking, slightly overpowered.
- **Distinct:** highly animated, visually distinct from each other.
- **Area:** each can hit several enemies.

Hard rules:
- **Full sequence every time.** Every ability plays: anticipation → casting animation → mana/VFX build-up → release → impact → environment reaction → recovery. Stronger attacks get stronger anticipation and impact.
- **Never a static caster.** The character never stands still while magic simply appears. Effects start at the hands (cast sockets) or from visible formations, never from the stomach and never before the casting animation starts.
- **Forgiving, but honest, hitboxes.** An ability's `AOERadius` (or width, or projectile radius) is the *visual* size of its damage area. The effective hit size is `visual × HitForgiveness`: default **1.12**, allowed range 1.0–1.15, checked by the validator. Effects always scale from the same visual number, so a giant hitbox always comes with giant effects.
- **No leftovers.** Nothing lingers after an ability ends except environment marks (scorch, cracks, wet ground, craters). Those fade within 8–20 s. Important map geometry is never destroyed, and no spell leaves collision behind except Earth Wall, which crumbles on its own.
- **Camera.** Minor abilities give a tiny impulse, heavy ones a short shake, and ultimates a larger shake plus a brief FOV kick. All are attenuated by distance and none is constant (§5.4). Hit-stop (0.085 s) is reserved for Dragon Crush.
- **Sound.** Every spell has positional sound for build-up, travel (where it travels) and impact.
- **Performance.** Effect density scales with `mt.VFX.Quality` (0–3). Hero layers stay at full quality and distant effects shed particles. Live effects and decals have hard caps, frequently spawned effects are pooled, and hit tests are overlap queries rather than spawned collision actors.

**What it must never look like:** default particles, coloured spheres, generic beams, flat PNG cards, directionless random particles, tiny hitboxes, giant invisible hitboxes, attacks clipping through everything, uncontrolled screen shake, opaque effects covering the screen, or identical casting animations for every ability.

**Not in scope, and why:**
- **Gameplay Ability System.** The project doesn't use GAS. Its own data-driven ability system (`UMTAbility`, `Abilities.json`) already has everything the brief lists per ability: mana, cooldown, cast time, charge, damage, radius, knockback, status effects, animation, effects and sound. It is structured the same way: abilities are instances with phases, `FMTStatusEffect` plays the role of Gameplay Effects (Quagmire slow, seal, burning, resistances), and presets play the role of Gameplay Cues (cosmetic, spawned by gameplay, never affecting it). Migrating would be a rewrite that cannot be tested here.
- **Niagara.** Niagara systems can only be authored inside the editor, and this pass was written without one. Every phase is therefore a layered runtime preset. Each preset has flashes, pressure rings and cones, instanced sprite and mesh particles with directional motion, debris that bounces and settles, lights, ground decals and camera shakes, scaled by quality level. A Niagara system can replace any phase later without code changes: fill the row's `FX.Formation`, `FX.Travel`, `FX.Impact` or `FX.Dissipation` and `MTCombat::SpawnSpellFX` uses it instead of the preset.
- **Multiplayer.** The game has no networking today. The new code keeps damage and hit resolution in gameplay code and all visuals in presets, which already skip dedicated servers. Authoritative replication can be added later without untangling them.

## 2. How the overhaul plugs into the existing game

| Stage | What happens | Where |
|---|---|---|
| Anticipation | Cast clip (`Montage`, resolved per character to `A_<C>_<Key>`), `<Preset>.Formation` at the cast socket (`SetCharge` every frame while charging), cast sound, telegraphs (Quagmire target ring, Earth Spikes aim line) | `UMTAbility::TryActivate`, `TickAnticipation` |
| Release | The clip's **`Release` notify** (`UMTAnimNotify_Event`, frame-accurate per character) or `CastTime` if no notify arrives, whichever comes first (never before 40% of `CastTime`). Chargeable abilities release when the input is released. `ExecuteAction` runs, then `<Preset>.Release` plays at the hand, aimed. | `UMTAbility::HandleAnimEvent`, `Tick` |
| Travel / body | Gameplay actors: `AMTProjectile`, `AMTZoneActor`, `AMTEarthWall`, `AMTWaterSerpent` (new), with presets riding on them | actors |
| Impact | Damage, knockback, launch and status effects, then `<Preset>.Impact` (with its camera tier and ground marks) and the impact sound | actors / behaviours |
| Recovery | `RecoveryTime`, dodge-cancellable (existing) | `UMTAbility` |

Two new rules sit on top of these stages:
- **Full-body commit.** `bFullBodyCommit` (Inferno, Flood, Water Dragon, Dragon Crush, and the Barrage finale) stops the caster's movement during anticipation and release. Otherwise casting keeps blending with locomotion, as today.
- **Sealing.** Disturb Magic can cancel an ability in Anticipation. `UMTAbilityComponent::LockAbility(Id, Seconds)` then seals it: `CanActivate` fails with "Sealed", and the hotbar shows it unavailable. This mirrors canon, where Ran Ma seals the magic of one hand while the other still works.

**New `FMTAbilityData` fields** (all optional; defaults keep old rows unchanged):
- **Hit size:** `HitForgiveness` (1.12), `PierceCount`, `PierceMaxHealth`, `ProjectileWidth` (Wind Blade's crescent box sweep; 0 means a sphere).
- **Charge scaling:** `ChargeSizeScale`, `ChargeRadiusScale`, `ChargeKnockbackScale`.
- **Hit effects:** `Launch` (vertical cm/s added to knockback), `BurnSeconds`, `BurnDamagePerSecond`.
- **Commitment:** `bFullBodyCommit`.
- **`Params`:** a `TMap<FName, float>` of behaviour-specific numbers. The names are listed per ability below.

`FMTDamageSpec` gains `Launch`. A hit with `Launch > 0` always launches a target that is not crowd-control immune; heavy targets get 35% of it.

**New behaviours:**
- `Barrage` (Elemental Barrage)
- `Disrupt` (Disturb Magic)
- `Strike` (Dragon Crush)
- `Serpent` (Water Dragon)

**New zone kind:** `Arc` (Flame Wave).

**Upgraded:**
- Projectile: pierce rules, crescent width, charge scales, release effect, charge sound fade.
- Dash: flank arrival and combo window.
- Structure: arc, sequential rise, 3 variants, crack stages.
- Zones: Mire depth/sink/momentum, Eruptions biased pillars and area burn, LineEruptions fixed distances and final spike, Vortex growth/lift/pull, Wave front box and carrying, Burst deflect and invulnerability.

**Knockback units.** Knockback is horizontal speed in cm/s and Launch is vertical speed in cm/s, so airtime ≈ 2·Launch/980 s and flight ≈ Knockback × airtime. The numbers below are chosen for flights of 5–18 m, so targets fly far but stay in the fight.

## 3. The abilities

Numbers are visual sizes, where VR is the visual radius. The effective hit size is ×1.12 unless noted. Times are Rudeus's; Orsted's clips are shorter and his `Release` notifies fire earlier (§7).

### 3.1 Rudeus: Signature Stone Cannon (`Rudeus_StoneCannon`, Projectile, chargeable)
- **Cast (0.35 s tap, hold up to 1.6 s).** Rudeus raises one hand.
  - Earth fragments rip out of the ground and converge 20–40 cm above the palm.
  - They visibly form, compress, smooth and densify, then start rotating and accelerate to an insane spin.
  - Pressure rings form around the slug, and loose dust is pulled toward it.
  - At maximum spin the slug vibrates at high frequency and distorts the air around it.
  - The formation preset gets `SetCharge(0..1)` every frame, driving size ×1.7, spin, ring count and vibration.
- **Release (palm thrust with recoil).** The launch runs at 9000 cm/s (tap) up to 14 400 cm/s (full), too fast for the eye to follow at launch. `RudeusCannon.Release` plays:
  - a huge air-pressure cone;
  - a circular shockwave at the hand;
  - a trailing spiral;
  - a dust blast behind Rudeus;
  - a camera impulse;
  - a directional sonic boom (sound `sonic_boom`).
- **Travel.** The drill slug spins inside a spiral shroud (visual radius 36, up to 61 charged).
- **Impact.** VR 260, up to 416 charged:
  - a flash;
  - a radial pressure ring;
  - rock fragments and dirt;
  - lingering dust;
  - a crater decal.
- **Pierce.** It passes through up to 3 targets that are not crowd-control immune and have ≤ 600 max health (`RudeusCannon.Pierce` at each), then detonates on the next.
- **Numbers.**
  - Damage 90 (×2.4 charged); splash 60%.
  - Stagger 50 (×2.5).
  - Knockback 1100 (×1.45), launch 300.
  - Mana 45 (×2 charged), cooldown 1.6 s, range 4500.
- **Anim:** `StoneCannon_Charge` → `StoneCannon_Hold` (loop) → `StoneCannon_Release`.
- **Must not be:** a slowly floating brown ball, a pebble, a basic projectile, motionless before launch, visually weak, or missing its impact.

### 3.2 Rudeus: Quagmire (`Rudeus_Quagmire`, Zone/Mire, chargeable)
- **Cast (0.45 s tap, hold up to 1.8 s).** Rudeus lowers his palm toward the ground. `Quagmire.Target`, a faint circular disturbance, expands on the ground target and grows with the charge.
- **Transformation (1.2 s, authored in `Quagmire.Zone`).** Cracks form (0–0.35 s), then water pushes up through them (0.35–0.8 s), then the soil liquefies (0.8–1.2 s). The terrain reads solid → saturated → moving mud → deep quagmire. Penalties ramp in with it.
- **Size.** VR 900 (a plaza) on a tap, up to 1980 at full charge (a battlefield). Duration 9 s, then `Quagmire.Dissipation` dries it over about 1 s.
- **Depth.** Depth is 1.0 at the centre and falls smoothly (smoothstep) to 0.35 at the edge. It scales each penalty:
  - speed ×(1 − 0.72·d)
  - acceleration ×(1 − 0.5·d)
  - jump ×(1 − 0.7·d)
  - dodge distance ×(1 − 0.6·d)
- **Effects on targets.**
  - **Momentum loss:** targets entering faster than 250 cm/s lose 60%·d of their momentum, with a splash (`Quagmire.Splash`).
  - **Sinking:** targets visibly sink by up to 30 cm × d (mesh offset, eased over 1.5 s). They rise back out over 0.6 s after leaving.
  - **Heavy targets** (crowd-control immune) sink 35% as far and take 60% of the penalties.
  - **Traces:** moving targets leave `Quagmire.Ripple` (ripples, mud displacement, a short sinking footprint) every 0.3 s.
- **Visuals.** Ripples, bubbles, mud splashes, displaced water, a moving surface normal, and earth fragments around the boundary.
- **Stone synergy (existing):** stone against a mired target does more damage and stagger.
- **Params:** `TransformTime` 1.2, `EdgeDepth` 0.35, `SpeedLoss` 0.72, `AccelLoss` 0.5, `JumpLoss` 0.7, `DodgeLoss` 0.6, `SinkDepth` 30, `HeavySink` 0.35, `HeavyPenalty` 0.6, `MomentumLoss` 0.6.
- **Cost and anim.** Mana 90 (×1.8 charged), cooldown 14 s, range 2400. Anim: `Quagmire` → `Quagmire_Hold` → `Quagmire_Release`.
- **Must not be:** a painted brown circle, instant, fully opaque, unreadable, or a 3 m puddle.

### 3.3 Rudeus: Elemental Barrage (`Rudeus_ElementalBarrage`, Barrage)
- **Formation (0.6 s).** Rudeus raises both hands, and four formations fade in around him. Offsets are from the capsule centre in his local frame, X forward, Y right:
  - Fire, behind upper left: (−70, −100, +95), embers spiralling into a turning flame core
  - Water, behind upper right: (−70, +100, +95), a spinning water ring with droplets
  - Earth, lower left: (−30, −125, +5), orbiting rock fragments around a dense stone
  - Wind, lower right: (−30, +125, +5), streak rings and a distortion core

  The set sways ±12° around him as the formations turn.
- **Barrage (3.0 s, one shot every 0.2 s: 15 shots).** Rudeus points. The formations fire in turn: Stone Cannon (Earth) → Water Lance (Water) → Wind Blade (Wind) → Fireball (Fire) → repeat. Each shot:
  - launches from its own formation toward the aim point with ±4° spread;
  - gets a muzzle burst (`Barrage.Muzzle<Element>`);
  - and the formation kicks back.

  Rudeus can drift at 35% speed.
- **Finale.** The four formations collapse together 150 cm in front of him over 0.35 s (`Barrage.Collapse`). The combined orb then fires (`Barrage_Finale`, 7000 cm/s) and detonates as a multi-element explosion (`BarrageFinale.Impact`, VR 900):
  - earth erupts outward;
  - water spirals up;
  - fire expands through the centre;
  - wind pushes the whole blast outward.

  It deals 260 damage to everyone inside (no splash falloff), knockback 1100 and launch 550. Rudeus is committed (no movement) during the collapse.
- **Params:** `BarrageTime` 3.0, `ShotInterval` 0.2, `CollapseTime` 0.35, `Spread` 4, `Sway` 12.
- **Cost.** Mana 140, cooldown 16 s.
- **Sub-rows:** `Barrage_Stone`, `Barrage_WaterCannon` (shown as Water Lance), `Barrage_WindBlade`, `Barrage_FireBurst` (Fireball), `Barrage_Finale` (new).
- **Anim:** `Barrage`, 4.6 s, with `Release` 0.6 (the barrage starts) and `Finale` 3.95.
- **Must not be:** four coloured balls, one beam, one explosion with four colours, slow, or repetitive. Each element keeps its physical identity.

### 3.4 Orsted: Disturb Magic (`Orsted_DisturbMagic`, Disrupt)
- **Cast (0.06 s).** Orsted calmly raises one hand, with no wind-up. `DisturbMagic.Formation` shows a small concentrated distortion at the palm: pale blue-white energy, black distortion ripples, thin dragon-shaped geometric lines, and warped air. `DisturbMagic.Cast` is the flick that sends the pulse.
- **Target.** In order:
  1. the locked target;
  2. else a hostile that is casting in front of him (within 35°, 1800 cm);
  3. else the aim direction (1800 cm).
- **Pulse.** A fast, nearly invisible pulse travels at 6500 cm/s: `DisturbMagic.Pulse` is a thin warped-air ripple with a small glyph at its head.
- **Hitting a caster.** If the pulse reaches a target whose disruptable ability is still forming (Anticipation or charging), that ability is cancelled. `DisturbMagic.Collapse<Element>` plays at its cast socket:
  - Fire: collapses into sparks.
  - Water: loses shape and falls as drops.
  - Earth: the construct crumbles.
  - Wind: disperses.
  - Arcane: flickers out.

  The ability is then **sealed for 3 s** (`DisturbMagic.Seal` on that hand). Orsted gets 60% of his cooldown back and a Dragon God Knowledge trigger.
- **Ward.** For 0.35 s, hostile projectiles that come within 450 cm of Orsted, or within 220 cm of the pulse line, collapse (`DisturbMagic.Collapse<Element>`). This also counts as a success (refund + Dragon God Knowledge) and keeps the existing defensive timing.
- **Persistent magic.** Anything within 450 cm of the pulse's end point is destabilised rather than deleted (`DisturbMagic.Destabilize`):
  - zones: remaining time ×0.5, strength ×0.6;
  - Earth Walls: lose 40% of their maximum health;
  - tornadoes: shrink ×0.6 and remaining time ×0.5;
  - water serpents in flight: collapse.
- **No damage, no stun, no permanent silence.**
- **Params:** `PulseSpeed` 6500, `PathRadius` 220, `SearchRange` 1800, `SearchAngle` 35, `SealSeconds` 3, `Refund` 0.6, `EndRadius` 450. `CounterWindow` 0.35 and `CounterRadius` 450 keep their meaning.
- **Cost and anim.** Mana 45, cooldown 6 s. Anim: `DisturbMagic` (Release 0.06).
- **Must not be:** a laser, a damage projectile, an explosion, a permanent silence, or a stun.

### 3.5 Orsted: Dragon Step (`Orsted_DragonStep`, Dash)
- **Cast (0.08 s).** The stance lowers slightly, and `DragonStep.Formation` gathers dragon aura at the feet (pale gold, white, deep blue). Then, BOOM (`DragonStep.Launch`):
  - a circular ground-pressure burst (VR 350);
  - a dust ring.

  `DragonStep.Travel` rides with him: 3 streaked afterimages and a warped-air trail.
- **Targeted (a lock within 2600 cm).** Orsted reappears beside or slightly behind the target: 115 cm from its centre, on the flank facing his approach, in **0.12 s**, facing it. His arrival (`DragonStep.Arrive`) makes a pressure ring (VR 220) and a light shock of 10 damage and 25 stagger. It opens a **0.9 s follow-up window**: Dragon Crush then winds up ×0.35 faster, auto-targets that enemy and deals ×1.2 damage. While the window is open, the Dragon Crush hotbar slot pulses white-gold (`SMTHudOverlay::PaintHotbar`), so the player can see when to press.
- **Free (no target).** An ultra-long dash of 1700 cm in 0.2 s along the movement input or facing (a dodge covers about 400 cm). A capsule sweep shortens it at walls.
- **Defence.** Invulnerable during the travel.
- **Params:** `TargetRange` 2600, `ArriveOffset` 115, `TargetDuration` 0.12, `FreeDistance` 1700, `FreeDuration` 0.2, `ComboWindow` 0.9.
- **Cost and anim.** Stamina 20, cooldown 3.5 s. Anim: `DragonStep`.
- **Must not be:** a portal, a slow fade, a sprint, or a float.

### 3.6 Orsted: Dragon Crush (`Orsted_DragonCrush`, Strike)
- **Wind-up (0.40 s; 0.14 s out of Dragon Step).** Orsted pulls one arm back, and `DragonCrush.Formation` wraps it in a tight, compressed Dragon God aura (small, not yet giant). If the primary target is within 450 cm but out of reach, he lunges to 110 cm from it. Full-body commit.
- **Impact frame (`Release` notify).** All the stored energy releases at once (`DragonCrush.Impact`, on the ground):
  - a massive circular shockwave (VR 650);
  - broken ground (a crack decal, VR 520);
  - raised chunks of earth;
  - pressure distortion;
  - an expanding dust ring;
  - a bass impact;
  - a short heavy camera shake with an FOV kick.

  **Hit-stop 0.085 s** freezes Orsted and the primary target.
- **Targets.** The primary target is:
  1. the combo target;
  2. else the lock within 350 cm in front;
  3. else the closest hostile within 300 cm in a 70° cone.

  The primary takes 320 damage, 1300 knockback and 700 launch (`DragonCrush.TargetHit` on it). Everyone else in the shockwave takes 160 damage, 800 knockback outward and 420 launch.
- **Params:** `PrimaryDamage` 320, `PrimaryKnockback` 1300, `PrimaryLaunch` 700, `PrimaryRange` 350, `ConeRange` 300, `ConeHalfAngle` 35, `LungeRange` 450, `HitStop` 0.085, `ComboCastScale` 0.35, `ComboDamageScale` 1.2.
- **Cost and anim.** Stamina 35, cooldown 7 s. Anim: `DragonCrush` (Release 0.40, recovered by 0.9 s).
- **Must not be:** a generic punch, a fire punch, a beam, or a sword technique.

### 3.7 Fire (shared)
- **Fireball (`Fire_Fireball`, Projectile, chargeable up to 1.2 s).**
  - A compressed sphere of fire forms above the palm, with the fire rotating **inward** rather than flickering outward.
  - `SetCharge` shifts the colour: orange → bright yellow → almost white core.
  - It travels at 5500 cm/s (×1.25 charged), with a visual radius of 30.
  - Impact: explosion VR 320 → 560, damage 60 → 150, knockback 600 → 900 with launch 250, burning for 4 s at 8/s.
  - Cast 0.30 s. Mana 24 (×1.6 charged), cooldown 1.6 s.
  - Anim: `Cast_Fireball` → `Cast_Fireball_Hold` → `Cast_Fireball_Release`.
- **Flame Wave (`Fire_FlameWave`, Zone/Arc).**
  - An arm sweep sends a huge curved wall of fire (120°) rolling forward from 150 cm out to 1300 cm over 0.9 s, following the ground (height 260, front band 180).
  - It is drawn by 11 `FlameWave.Segment` pieces spread along the arc.
  - Each enemy is hit once as the front passes: 75 damage, knockback 450 outward, launch 150, burning 4 s.
  - It leaves scorch marks and embers (`FlameWave.Trail`) along the path.
  - Params: `ArcDegrees` 120, `StartRadius` 150, `EndRadius` 1300, `TravelTime` 0.9, `Band` 180, `Segments` 11.
  - Cast 0.42 s. Mana 55, cooldown 8 s. Anim: `Cast_FlameWave`.
- **Inferno (`Fire_Inferno`, Zone/Eruptions, ultimate, full-body commit).**
  - The caster raises one hand. Fire circles appear around the target area (VR 1100, `Inferno.Zone`) and a flame vortex forms at its centre (`Inferno.Vortex`).
  - From 0.6 s, pillars erupt: 3 every 0.35 s, 10 times (`Inferno.Eruption`, pillar VR 190, height 900). Two of each three are aimed at enemies inside (±80 cm).
  - Each pillar deals 70 damage and a small launch (380). Enemies inside burn (15 per second).
  - Params: `PillarsPerPulse` 3, `EnemyBias` 0.67, `AreaBurnDps` 15.
  - Cast 0.90 s. Mana 220, cooldown 30 s, range 2600. Anim: `Cast_Inferno`.

### 3.8 Water (shared)
- **Water Bullet (`Water_WaterBullet`, Projectile/Piercing).**
  - High-pressure water condenses above the palm and launches as a compressed lance (8000 cm/s), with vapour and pressure rings around it.
  - It pierces 1 target. Damage 48, knockback 750, launch 150; the splash burst is VR 150.
  - Cast 0.22 s. Mana 20, cooldown 1.2 s. Anim: `Cast_WaterBullet`.
- **Water Dragon (`Water_WaterDragon`, Serpent, ultimate, full-body commit).**
  - A gigantic serpent made entirely of moving water: 16 segments, 1700 cm long, with a head of visual radius 110. Its body is drawn by `AMTWaterSerpent` using the water material, with `WaterDragon.Travel` on the head.
  - Spiralling streams build it around the caster (`WaterDragon.Formation`, over the 0.75 s cast). It then circles behind and above the caster (0.5 s), and hunts the target for up to 2.4 s (2800 cm/s, turning up to 160° per second).
  - On contact with a hostile or the ground, it collapses into an enormous water explosion (`WaterDragon.Impact`, VR 780): 220 damage, knockback 1200, launch 500, wet ground.
  - Cast 0.75 s. Mana 160, cooldown 22 s. Anim: `Cast_WaterDragon`.
- **Flood (`Water_Flood`, Zone/Wave, ultimate, full-body commit).**
  - The caster slams both arms forward while water accumulates behind them (`Flood.Formation`).
  - A **huge wave** is then released (`Flood.Zone`): 2400 wide, 380 high, travelling 2600 cm at 1500 cm/s and following the terrain. `Flood.Trail` leaves wet ground across its width.
  - Enemies inside the front band (300 deep, full width) take 90 damage once and are **carried along** at 1300 cm/s while inside. When the wave breaks, they are released with knockback 900 and launch 250.
  - It splashes against walls (`Flood.Splash`) and breaks early if the path is blocked.
  - Params: `Width` 2400, `Distance` 2600, `CarrySpeed` 1300, `Band` 300.
  - Cast 0.70 s. Mana 170, cooldown 26 s. Anim: `Cast_Flood`.

### 3.9 Earth (shared)
- **Stone Cannon (`Earth_StoneCannon`, Projectile, chargeable up to 1.2 s).**
  - A fast, hardened, rotating slug (6500 cm/s, ×1.4 charged), clearly smaller and plainer than Rudeus's signature cannon (no mana glow, fewer rings, no sonic boom).
  - Impact VR 170 (×1.4), damage 60 (×2), knockback 700 (×1.4), launch 200. It pierces 1 target with ≤ 400 max health.
  - Mana 32, cooldown 2.0 s. Anim: `Cast_StoneCannon` → `Cast_StoneCannon_Hold` → `Cast_StoneCannon_Release`.
- **Earth Wall (`Earth_EarthWall`, Structure).**
  - The caster raises a hand, and 7 connected, naturally fractured segments erupt one after another from the centre outward (0.06 s apart), on an arc 550 cm out.
  - Each segment uses 1 of 3 mesh variants, with random tilt ±6° and height ±15%.
  - The wall blocks projectiles, movement and some magic.
  - Each segment has 450 health and **visibly cracks** at 66% and 33% (`EarthWall.Crack`, material `Crack` 0.5 / 1). It crumbles at 0 (`EarthWall.Crumble`) and lasts 15 s.
  - Cast 0.40 s. Mana 70, cooldown 18 s. Anim: `Cast_EarthWall`.
- **Earth Spikes (`Earth_EarthSpikes`, Zone/LineEruptions).**
  - While casting, `EarthSpikes.Aim` shows an aim line on the ground (1300 cm) that follows the aim.
  - Stone lances then erupt one after another, 0.12 s apart:
    - spikes 1–3 at 320, 640 and 960 cm (`EarthSpikes.Eruption`, VR 150, height 280): 55 damage, knockback 300, launch 450;
    - then an **enormous final spike** at 1300 cm (`EarthSpikes.Final`, VR 300, height 700): 140 damage, and non-boss enemies are launched 950 up.
  - The spikes crumble after 1.2 s.
  - Params: `Spike1` 320, `Spike2` 640, `Spike3` 960, `Final` 1300, `FinalRadius` 300, `FinalDamage` 140, `FinalLaunch` 950.
  - Cast 0.40 s. Mana 60, cooldown 9 s. Anim: `Cast_EarthSpikes`.

### 3.10 Wind (shared)
- **Wind Blade (`Wind_WindBlade`, Projectile/Wave).**
  - A giant curved slash of compressed air, **visible** as pale cyan and white distortion, condensation and streaks.
  - Width 520 (the hit box is 291 each side), thickness 60. It travels at 9000 cm/s over 2600 cm and pierces every character. The hit test is a box sweep across the crescent every frame.
  - Damage 50, knockback 650, launch 120.
  - Cast 0.25 s. Mana 22, cooldown 1.4 s. Anim: `Cast_WindBlade`.
- **Tornado (`Wind_Tornado`, Zone/Vortex).**
  - A large spinning vortex lasting 6 s. It forms in 0.4 s and drifts toward the target at 220 cm/s.
  - It grows as it lives: VR 360 → 560, with a height of about 950 → 1400 (`SetEffectScale`).
  - Dust, leaves and debris are drawn in and orbit it.
  - **Small enemies** (weak) in the core are lifted, orbit the funnel up to 320 cm high for 1.2 s (`Tornado.Lift`), then are dropped.
  - **Other enemies** within radius ×2.2 are pulled at 650 cm/s (250 for crowd-control immune targets) and slowed ×0.5 inside.
  - The core deals 12 damage every 0.25 s and deflects weak projectiles.
  - Params: `GrowTo` 560, `FormTime` 0.4, `PullScale` 2.2, `PullSpeed` 650, `HeavyPullSpeed` 250, `LiftTime` 1.2, `LiftHeight` 320, `HeavySlow` 0.5.
  - Cast 0.55 s. Mana 110, cooldown 22 s. Anim: `Cast_Tornado`.
- **Wind Burst (`Wind_WindBurst`, Zone/Burst).**
  - Air visibly compresses around the caster for 0.12 s (`WindBurst.Formation`, pulling inward), then a huge sphere of pressure releases (`WindBurst.Impact`, VR 620).
  - Knockback 1500 in every direction, launch 350, damage 30.
  - It reverses light hostile projectiles within 700 cm and gives the caster 0.25 s of invulnerability, making it an escape or counter move.
  - Mana 35, cooldown 7 s. Anim: `Cast_WindBurst`.

### 3.11 Hotbar (the existing one; no placeholders)

| Key | Rudeus | Orsted |
|---|---|---|
| LMB | Stone Bullet (`Rudeus_Basic`) | Palm Strike (`Orsted_Basic`) |
| 1 / 2 / 3 / 4 (default loadout) | Stone Cannon, Quagmire, Elemental Barrage, Fireball | Disturb Magic, Dragon Step, Dragon Crush, Wind Burst |
| F | Demon Eye | Saint Dragon Battle Aura |
| G | Awakening (Quagmire Magician) | Awakening (Dragon God) |

The ABILITIES menu can put any of the 12 element spells or the character's uniques on 1–4, and every one of them works from there. The default loadouts now open with each character's three uniques, so both signature combos are on 1-2-3. A loadout the player already saved is kept.

## 4. Casting personality

| | Rudeus | Orsted |
|---|---|---|
| Feel | Overwhelming talent; chantless, instinctive, creative, explosive, high-mana, less restrained | Power is normal for him: precise, controlled, intimidating, efficient, effortless |
| Hands | The hands actively shape the magic: fingers move, wrists rotate, palms rise, energy is pulled together, and gestures direct each attack | Small hand movements produce enormous consequences, and his posture stays calm |
| Timing | Visible build-up, and fast | Clips run about 55–65% as long as Rudeus's, with smaller gestures |
| VFX | Raw energy, and mana distortion around the hands before big releases | The same presets, tinted and trimmed by `Style` (§5.2), plus a subtle Dragon God aura (pale gold, white, deep blue) on stronger techniques; impacts are just as hard |

Both characters use the same ability rows. `AMTCharacterBase::ResolveLineageAnim` plays each character's own `A_<C>_<Key>` clip.

## 5. Contract: effect presets (C++, `Source/MushokuRPG/*/VFX/`)

### 5.1 Names and conventions
- **Names.** Gameplay spawns presets `<FX.Preset>.<Phase>` through `MTCombat::SpawnPresetPhase` / `SpawnSpellFX` / `UMTAbility::SpawnPhaseFX`. A missing preset is a silent no-op, and `Tools/vfx/check_presets.py` lists missing ones against `Tools/vfx/phase_contract.json`.
- **Transform.** Location as stated per phase. Rotation X is the travel or aim direction and Z is up. Ground phases sit on the ground point.
- **Scale.** Scale = actual visual size ÷ the authored base size. Each preset is **authored at the base visual size listed in §3** (for example Quagmire.Zone at VR 900, Fireball.Impact at VR 320), so charge, awakening and area bonuses scale it.
- **Loops** are stopped by gameplay (`MTCombat::StopSpellFX`) and fade out; one-shots end on their own.

### 5.2 Runtime API added to `AMTSpellVFX`
Gameplay calls exactly these:
```cpp
void SetCharge(float Alpha);             // 0..1 every frame while charging; presets respond through FMTVFXDesc::Charge
void SetEffectScale(float NewScale);     // live resize (tornado growth, Quagmire target ring)
float GetEffectScale() const;
```
New preset fields:
- `FMTVFXDesc::Charge`, holding the values at full charge: `SizeScale`, `IntensityScale`, `SpinScale`, `RateScale`, `TintAtFull` (A = 0 means no tint), `Vibration` (cm of high-frequency jitter).
- `FMTVFXDecal::bUntilStop`: the decal lives until the effect is stopped, then fades. Zone-length marks use it.
- `FMTVFXShake::FOVKick`, in degrees, which calls `AMTPlayerCharacter::AddFOVKick(Degrees, Duration)`.
- `FMTVFXEmitter::bHero`: exempt from quality and distance scaling.

**Styles.** `SpawnPreset` tries `<Name>@<CharacterID>` first when the source actor is an `AMTCharacterBase` (for example `Fireball.Formation@Orsted`), then `<Name>`. Orsted variants are optional: cleaner, fewer particles, and a pale-gold/white/deep-blue accent.

**Quality and caps.**
- `mt.VFX.Quality` (0–3, default 3) scales non-hero emitter bursts and rates by 0.35, 0.55, 0.8 and 1.
- Beyond 4000 cm from the camera, non-hero emitters run at ×0.5, and beyond 8000 cm at ×0.25.
- At most 120 live effects. Past that, pooled one-shots (hits, trails, ripples, muzzles) are skipped.
- At most 80 live spell decals. Past that, the oldest fade early.
- Frequently spawned presets reuse finished actors (pool).

### 5.3 Phases gameplay spawns (and the base sizes to author at)

| Preset | Phases |
|---|---|
| `RudeusCannon` | `Formation` (loop, hand, charge-driven), `Release` (hand, aimed; scale 1 + 0.5·charge), `Travel` (loop on the projectile, radius 36), `Pierce` (at the pierced target), `Impact` (VR 260), `Dissipation` |
| `StoneCannon` | same phases, plainer (Travel radius 22, Impact VR 170) |
| `StoneBullet` | `Formation`, `Travel`, `Impact` (existing basic attack) |
| `Quagmire` | `Formation` (loop, hand, charge), `Target` (loop on the ground target; VR 900, resized live), `Zone` (loop attached to the zone; VR 900; transformation 0–1.2 s then mud; `bUntilStop` decals), `Ripple` (at a moving target's feet), `Splash` (fast entry), `Dissipation` (drying, VR 900) |
| `Barrage` | `Formation` (hand), `OrbFire`, `OrbWater`, `OrbEarth`, `OrbWind` (loops attached to the caster; gameplay moves them), `MuzzleFire`, `MuzzleWater`, `MuzzleEarth`, `MuzzleWind` (at the orb, aimed), `Collapse` (loop at the convergence point) |
| `BarrageFinale` | `Travel` (the combined orb, radius 60), `Impact` (VR 900 multi-element explosion) |
| `DisturbMagic` | `Formation` (hand), `Cast` (hand, aimed flick), `Pulse` (loop; gameplay moves it; X = direction), `Hit` (at the target), `CollapseFire`, `CollapseWater`, `CollapseEarth`, `CollapseWind`, `CollapseArcane` (where the spell was forming or flying), `Seal` (on the sealed hand, 3 s), `Destabilize` (on the zone, wall, tornado or serpent) |
| `DragonStep` | `Formation` (feet), `Launch` (VR 350 ground burst), `Travel` (loop on Orsted, afterimages), `Arrive` (VR 220) |
| `DragonCrush` | `Formation` (loop on the striking hand), `Impact` (ground, VR 650, crack decal 520), `TargetHit` (on the primary target) |
| `Fireball` | `Formation` (loop, charge: orange → yellow → white), `Release`, `Travel` (radius 30), `Impact` (VR 320), `Dissipation` |
| `FlameWave` | `Formation`, `Segment` (loop; one of 11 along the arc; X = outward; authored 300 wide along Y, 260 high), `Trail` (scorch + embers, 250 wide), `Dissipation` |
| `Inferno` | `Formation`, `Zone` (loop; circles, VR 1100), `Vortex` (centre), `Eruption` (pillar VR 190, height 900), `Dissipation` |
| `WaterBullet` | `Formation`, `Release`, `Travel` (lance radius 20), `Pierce`, `Impact` (VR 150), `Dissipation` |
| `WaterDragon` | `Formation` (loop, attached to the caster; spiral streams, 0.75 s), `Travel` (loop on the head: foam, spray, mist, drips; head visual radius 110), `Impact` (VR 780 + wet ground), `Dissipation` |
| `Flood` | `Formation` (behind the caster), `Zone` (loop; the moving wave front, 2400 wide, 380 high, X = travel), `Trail` (wet ground across 2400, spawned every 300 cm), `Splash`, `Dissipation` |
| `EarthWall` | `Formation`, `Rise` (per segment), `Crack` (per crack stage), `Crumble` |
| `EarthSpikes` | `Formation`, `Aim` (loop; a line from the caster's feet along +X, authored 1300 long), `Eruption` (VR 150, height 280), `Final` (VR 300, height 700), `Crumble` |
| `WindBlade` | `Formation`, `Release`, `Travel` (crescent 520 wide), `Impact`, `Dissipation` |
| `Tornado` | `Formation`, `Zone` (loop; VR 360, height 950; resized live to ×1.56), `Trail`, `Lift` (loop on a lifted enemy), `Dissipation` |
| `WindBurst` | `Formation` (compress, 0.12 s), `Impact` (VR 620) |
| Generic (existing) | `Hit.Light`, `Hit.Heavy`, `Cast.Generic`, `Dodge.Dust` |

### 5.4 Camera tiers (preset `Shakes`)

| Tier | Strength | Duration | Radius | FOVKick | Used by |
|---|---|---|---|---|---|
| Minor | 0.08–0.15 | 0.2 s | 2500 | – | Water Bullet, Wind Blade, taps of Fireball and Stone Cannon, Barrage shots, Dragon Step arrive |
| Heavy | 0.3–0.45 | 0.35 s | 4000 | – | charged Stone Cannon (Rudeus release and impact), Flame Wave, Earth Spikes final, Earth Wall, Wind Burst, Tornado forming |
| Ultimate | 0.55–0.75 | 0.5 s | 6000 | 4–6° | Inferno, Water Dragon impact, Flood, Barrage finale, Dragon Crush |

The player's shake never stacks: a new shake only replaces a weaker one (existing).

## 6. Contract: generated assets (git-ignored outputs, rebuilt by `Tools/mac/build_and_setup.sh`)

- **Meshes** (`Tools/kit/arch_vfx.py` → `SourceArt/Kit/VFX/*.glb` → `/Game/LaPlace/Kit/VFX/`). Existing: `SM_VFX_Ring`, `ShockRing`, `Disc`, `Crescent`, `Flame`, `Funnel`, `Beam`, `DragonSegment`, `DragonHead`, `Spike_A/B`, `RockChunk_A–D`, `EarthWall`, `Crystal`. New:
  - `SM_VFX_Slug`: pointed drill slug along +X, fluted spiral grooves, 60 long, radius 15.
  - `SM_VFX_Spiral`: helix ribbon along +X, 3 turns, 100 long.
  - `SM_VFX_WaveSheet`: 200 wide (Y) × 100 tall wave face, curling forward at the top.
  - `SM_VFX_EarthWall_B` and `SM_VFX_EarthWall_C`: fractured slab variants on the `EarthWall` footprint.
  - `SM_VFX_Slab`: raised ground slab, 100×100×25.
  - `SM_VFX_Glyph`: flat dragon-line glyph, 100 across, for Disturb Magic.
- **Textures and materials** (`Tools/vfx/make_vfx_textures.py`, `Content/Python/mt_setup_laplace.py`, `/Game/LaPlace/VFX/`). They keep the existing parameter names (`Color`, `Intensity`, `Opacity`, `Texture`, `FresnelMix`, `NoiseAmount`, `Distortion`, `Glow`, `Char`, `SpinSpeed`, ...). New:
  - a mud decal (`M_Decal_Mud`: flowing normal, wetness, bubbles; `Flow`, `Age`);
  - a crater decal (`M_Decal_Crater`);
  - an aim-line decal (`M_Decal_AimLine`);
  - `Crack` (0–1) on `M_VFX_Rock` for the wall stages.
- **Sounds** (`Tools/audio/*` → `SourceArt/Audio/<Category>/<name>.wav` → `/Game/LaPlace/Audio/<Category>/<name>`). The new ones, referenced by the rows:
  - **Earth:** `stone_compress` (1.8 s), `sonic_boom`, `quagmire_transform`, `mud_squelch`, `earth_wall_crack`, `earth_wall_crumble`, `spike_final`, `ground_crack_run`.
  - **Fire:** `fireball_charge`, `flamewave_roar`, `inferno_circle`, `inferno_pillar`.
  - **Water:** `water_lance`, `water_dragon_roar`, `flood_gather`.
  - **Wind:** `wind_slash`, `wind_compress`, `tornado_form`.
  - **Orsted:** `disturb_pulse`, `disturb_collapse`, `seal`, `dragon_step_arrive`, `dragon_crush_charge`, `dragon_crush_impact`.
  - **Rudeus:** `barrage_orbs`, `barrage_finale`.
- **Charge sounds.** A `CastSound` on a chargeable ability plays as an audio component and fades out on release or cancel.
- **Which sound plays when** (the row's `FX.*Sound`, all placed in 3D through `ATT_Spell`):
  - `CastSound`: the build-up, at the hand, when the ability starts. Zones do not repeat it when they appear.
  - `ReleaseSound`: at the hand on the release frame.
  - `TravelSound`, by what carries it:
    - projectiles: a looping sound rides with the projectile and fades when it lands (`fireball_travel_loop`, `stone_spin_loop`); a one-shot is the launch voice (the Barrage shots);
    - zones: the sustained bed, attached to the zone, fading in over 0.35 s and out over 0.6 s as it expires (`tornado_loop`, `quagmire_loop`, `flame_burn_loop`, `flood_wave_loop`; Saint Dragon Aura's `aura_hum_loop`);
    - Water Dragon: the roar when it starts hunting;
    - Earth Wall: a segment cracking.
  - `ImpactSound`: every hit, pulse or eruption.
  - `AccentSound`: a zone's own voice where it appears (the mud transforming, the fire circles, the vortex touching down). It is also the final spike, the crumbling wall, Dragon Step's arrival and Disturb Magic's seal.

## 7. Contract: animations (`Tools/anim`, both characters, same skeleton)

**Clip keys.** Orsted's versions are minimal and 55–65% as long as Rudeus's, with his `Release` earlier.

| Key | `Release` (s): Rudeus / Orsted | Motion |
|---|---|---|
| `Cast_Fireball`, `Cast_Fireball_Hold` (loop), `Cast_Fireball_Release` | 0.30 / 0.19; release clip 0.04 | palm up, sphere formed and turned inward by the fingers, then thrown forward |
| `Cast_FlameWave` | 0.42 / 0.26 | horizontal arm sweep across the body |
| `Cast_Inferno` | 0.90 / 0.55 | one hand raised overhead, clenched, brought down (full body) |
| `Cast_WaterBullet` | 0.22 / 0.14 | two fingers aimed, snap forward |
| `Cast_WaterDragon` | 0.75 / 0.45 | both hands spiral upward, then point (full body) |
| `Cast_Flood` | 0.70 / 0.42 | both arms gather back, then slam or sweep forward (full body) |
| `Cast_StoneCannon`, `Cast_StoneCannon_Hold`, `Cast_StoneCannon_Release` | 0.35 / 0.22; release clip 0.04 | one hand forward, compress, thrust |
| `Cast_EarthWall` | 0.40 / 0.25 | hand drives upward from low |
| `Cast_EarthSpikes` | 0.40 / 0.25 | hand drives down toward the ground along the aim |
| `Cast_WindBlade` | 0.25 / 0.15 | horizontal slash with the hand |
| `Cast_Tornado` | 0.55 / 0.34 | circular stirring gesture, rising |
| `Cast_WindBurst` | 0.12 / 0.08 | arms pull in (compress), then spread (release) |
| `StoneCannon_Charge` / `_Hold` / `_Release` (upgrade) | 0.35; release clip 0.04 | hand raise, finger and wrist compression, palm thrust with recoil |
| `Quagmire`, `Quagmire_Hold`, `Quagmire_Release` | 0.45; release clip 0.04 | palm lowered, pressing down, fingers spread |
| `Barrage` (4.6 s) | 0.60, plus `Finale` 3.95 | both hands raise, point, chained directing gestures, both hands converge and push |
| `DisturbMagic` (Orsted) | 0.06 | minimal raised hand, finger flick |
| `DragonStep` (Orsted) | 0.08 | low stance, blur lean, arrival settle |
| `DragonCrush` (Orsted, new) | 0.40 | arm pulled back, driven palm, held, recovered by 0.9 s |

**Event export and import.**
- Events are exported to the sidecar `SourceArt/Characters/<C>/<C>_Animated.anim.json` under `"events": { "<Key>": { "<Event>": seconds } }`.
- `Content/Python/mt_setup_rudeus.py` (used for both characters) adds them to the imported sequences as `UMTAnimNotify_Event` notifies (Python `unreal.MTAnimNotify_Event`, property `event_name`) on a notify track named `MT`, replacing any earlier ones on that track.
- Each ability's `CastTime` equals Rudeus's `Release` time (checked within one frame by `Tools/validate_data.py`).

**Chargeable clips.** Chargeable abilities ignore `Release` during the charge: they release on input release, and their release clips carry `Release` at 0.04 s.

## 8. Testing and grading

- **Offline (this repo):**
  - `Tools/validate_data.py` checks:
    - every hotbar slot and loadout entry is valid for both characters;
    - every loadout-able ability has a clip for both characters and a preset;
    - `HitForgiveness` is within 1.0–1.15;
    - `CastTime` matches the Rudeus `Release` event;
    - behaviour-specific `Params` are present.
  - `Tools/vfx/check_presets.py` checks every contract phase exists.
  - Plus: animation QA metrics and contact sheets per clip, and audio loudness and contact sheets.
- **In-engine (Mac):**
  - `Automation RunTests MushokuRPG` runs the integration tests, including new ones for:
    - pierce;
    - Quagmire depth;
    - Disturb Magic seal;
    - the Dragon Step → Dragon Crush combo;
    - Wind Burst invulnerability;
    - the Flood carry;
    - Tornado lift.
  - The showcase mode (`?Showcase=1`) screenshots every ability and both signature combos.
- **Grading.** The 10-category table from the brief is filled in only after watching every ability in play. It is repeated until the average is ≥ 8.5 with no category below 8.0. `Docs/QA_Abilities.md` has the sheet and the honest offline pre-grade.

## 9. Research notes

The fandom wiki and Epic docs pages are blocked for this session, so these notes come from search-result summaries.

- **Stone Cannon:** Rudeus's signature improvement on Rock Bullet. He can tweak size, shape (drill-like), speed and temperature, and it can pierce god-level Battle Aura. His "Gatling" variant fires about 10 per second. He casts voicelessly. Sources: [Rudeus/Powers and Abilities](https://mushokutensei.fandom.com/wiki/Rudeus_Greyrat/Powers_and_Abilities), [Game Rant](https://gamerant.com/mushoku-tensei-jobless-reincarnation-rudeus-most-powerful-abilities/).
- **Quagmire:** Earth + Water mixed magic that turns ground into a trapping mud swamp. Rudeus uses it to entrap large numbers of monsters, hence "Quagmire Rudeus". Source: [Quagmire](https://mushokutensei.fandom.com/wiki/Quagmire).
- **Disturb Magic:** infuses counter-magic into a spell while it is forming; if the infused power is greater, the spell dissipates before it manifests. It can target one part of the body; Ran Ma sealed Rudeus's right hand while his left still worked. It was devised by Dragon God Urupen. Sources: [Disturb Magic](https://mushokutensei.fandom.com/wiki/Disturb_Magic), [Orsted/Powers and Abilities](https://mushokutensei.fandom.com/wiki/Orsted/Powers_and_Abilities), [Epicstream](https://epicstream.com/article/mushoku-tensei-disturb-magic-explained).
- **Magic system:** a seven-tier spell ranking; mixed magic combines spells consecutively (for example Frost Nova, Dry Steaming). Source: [Magic Spells](https://mushokutensei.fandom.com/wiki/Magic_Spells).
- **LA PLACE originals:** Dragon Step, Dragon Crush, Elemental Barrage and the elemental move list are this game's own interpretations.
- **Engine:**
  - montages have sections and notifies, and `Montage_JumpToSection` and `PlayMontageNotify` callbacks exist ([Animation Montage](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-montage-in-unreal-engine), [Animation Notifies](https://dev.epicgames.com/documentation/unreal-engine/animation-notifies-in-unreal-engine?lang=en-US));
  - GAS is made of the ability system component, abilities, effects, attributes and tags, with Gameplay Cues for audio and visuals ([GAS](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system));
  - Niagara systems expose User Parameters set from C++, and Effect Types handle scalability ([Niagara scalability](https://dev.epicgames.com/documentation/unreal-engine/scalability-and-best-practices-for-niagara)).
