# LA PLACE spell effects (runtime presets, no Niagara)

Every spell effect in the game is a **preset**: a data description of mesh layers, CPU particles, lights, decals, camera
shakes and afterimages, played by one `AMTSpellVFX` actor. There are no Niagara or Cascade assets. Presets live in C++
(`Source/MushokuRPG/Private/VFX/MTVFXLibrary.cpp`) and use only generated art: kit meshes, procedural textures and the
materials built by the editor setup. This page covers the runtime API, the quality, cap, pool and style rules, every
preset beat by beat, the assets behind them, and how to add a preset.

| What | Where |
|---|---|
| Effect actor, preset structs, runtime API | `Source/MushokuRPG/Public/VFX/MTSpellVFX.h`, `Private/VFX/MTSpellVFX.cpp` |
| Per-world live count, decal cap, pool | `Source/MushokuRPG/Public/VFX/MTVFXSubsystem.h`, `Private/VFX/MTVFXSubsystem.cpp` |
| The presets, asset paths and loaders | `Source/MushokuRPG/Public/VFX/MTVFXLibrary.h`, `Private/VFX/MTVFXLibrary.cpp` |
| Phase names gameplay spawns | `Tools/vfx/phase_contract.json` (`Docs/Ability_Overhaul.md` section 5.3) |
| Contract check | `Tools/vfx/check_presets.py` |
| Meshes | `Tools/kit/arch_vfx.py` → `SourceArt/Kit/VFX/*.glb` → `/Game/LaPlace/Kit/VFX/` (preview `SourceArt/Kit/VFX/_Preview.png`) |
| Textures | `Tools/vfx/make_vfx_textures.py` → `SourceArt/VFX/Textures/*.png` → `/Game/LaPlace/VFX/Textures/` |
| Materials | `Content/Python/mt_setup_laplace.py` (VFX section) → `/Game/LaPlace/VFX/Materials/` |

## Runtime API (`AMTSpellVFX`)

```cpp
static AMTSpellVFX* SpawnPreset(UObject* WorldContext, FName Name, const FTransform& Transform, float Scale = 1.f,
    USceneComponent* AttachTo = nullptr, FName Socket = NAME_None, AActor* Source = nullptr);
static bool HasPreset(FName Name);
void Stop();                              // loops: stop emitting, fade out, held decals start fading
void SetTint(const FLinearColor& Tint);   // recolours every layer (awakening upgrades, charge tint)
bool IsLooping() const;
void SetCharge(float Alpha);              // 0..1, every frame while charging
float GetCharge() const;
void SetEffectScale(float NewScale);      // live resize
float GetEffectScale() const;
FName GetPresetName() const;              // after style resolution, e.g. "Fireball.Formation@Orsted"
```

- **Transform.** X is the aim or travel direction and Z is up. Scale 1 is the base size in the tables below, so gameplay
  passes *actual size / base size*.
- **Attach.** `AttachTo` makes the effect follow a component. On a **socket** the effect follows the socket's position
  but keeps the rotation it was spawned with, turning only with the socket owner's actor rotation. A slug, lance or
  crescent in the hand keeps pointing along the aim instead of spinning with the hand bones. If the actor it rides on
  goes away (a projectile lands, a zone ends), an attached loop fades out by itself.
- **Source** is the caster. It picks style variants (below) and gives Dragon Step's afterimages their skeleton. Pass it
  whenever there is one.
- **Loops** run until `Stop()` (`MTCombat::StopSpellFX` detaches, then stops). Safety nets for loops nobody stopped:
  a free-standing `.Formation` loop fades after 4 s, other free-standing loops after 12 s, and attached loops after 300 s.
  One-shots end on their own; one still alive about 35 s after its duration is removed.
- **SetCharge.** The preset reacts through `FMTVFXDesc::Charge`: size (meshes, emitter shapes, speeds and sizes, light
  radii), emissive intensity, spin and orbit, emitter rates, a colour blend toward `TintAtFull` for emissive layers and
  lights, and a vibration jitter on mesh layers. Layers with a `ChargeThreshold` join as the charge passes it.
- **SetEffectScale** resizes meshes, emitter shapes, speeds and sizes, lights and shake radii on the next tick. Particles
  already born keep their size. Decals keep the size they were spawned with.
- **Impacts stay upright.** Presets marked `bUpright` (impacts, trails, ground phases and zones) keep Z up whatever
  rotation the spawner passes; only the yaw of X is used. Passing `(-HitNormal).Rotation()` is fine.

**What gameplay must do differently (summary):**
1. Pooled presets (names containing `Hit.`, `.Trail`, `.Ripple`, `.Muzzle`, `.Pierce` or `Dodge.`) are fire-and-forget.
   Their actor is recycled when it finishes, so do not keep the returned pointer, and never `Stop()` one later.
   `SpawnPreset` returns null for them when the world already has 120 live effects.
2. Formations of chargeable spells are loops: every path that spawns one must stop it (release, cancel, interrupt).
   The old `UMTAbility_Sequence` path that spawns a formation unattached at 0.7 is covered by the 4 s formation safety,
   but it should still stop it.
3. Held (`bUntilStop`) decals live as long as the effect: zones must be stopped when they end, and the mark then fades.
4. Zone dissipations are authored at the zone's `AOERadius` (Inferno 1100, Quagmire 900, Flame Wave 1300 arc,
   Tornado 360, Flood 2400 wide), so their scale is `Radius / AOERadius`.

## Preset fields added by the overhaul

| Struct | Field | Meaning |
|---|---|---|
| `FMTVFXDesc` | `Charge` (`FMTVFXCharge`) | The values at full charge: `SizeScale`, `IntensityScale`, `SpinScale`, `RateScale` (each 1 = no change), `TintAtFull` (RGB target, A = how far, 0 = no tint), `Vibration` (cm of mesh jitter). |
| `FMTVFXDesc` | `bUpright` | Keep Z up; only the yaw of the spawn rotation is used (impacts, ground phases). |
| `FMTVFXEmitter` | `bHero` | Exempt from `mt.VFX.Quality` and distance thinning: the one or two layers that carry the read. |
| `FMTVFXEmitter` | `ChargeThreshold` | Bursts and emits only once the charge reaches it. |
| `FMTVFXEmitter` | `ArcDegrees` | Ring and Disc shapes: only the arc of this many degrees centred on +X (Flame Wave's 120° arc). |
| `FMTVFXEmitter` | `ParamColor`, `Scalars` | Material parameters for the emitter's material (a rock seam glow, `Crack`, `Distortion`). |
| `FMTVFXMeshLayer` | `Velocity` | Drift in cm/s from `Offset` (sonic-boom rings racing ahead of the hand). |
| `FMTVFXMeshLayer` | `ChargeThreshold` | Shown once the charge reaches it, fading in over the next 0.15 of charge (compression rings). |
| `FMTVFXDecal` | `bUntilStop` | Lives until the effect stops, attached to it (follows it), then fades over `FadeOut`. Its material gets `Age` (seconds since it appeared) every frame. |
| `FMTVFXDecal` | `Length` | > 0: a strip `Size` wide (half-width) and `Length` long (half-length) along the effect's X instead of a disc. |
| `FMTVFXDecal` | `SortOrder` | Added to the base decal sort order (5); higher draws on top. |
| `FMTVFXShake` | `FOVKick` | Degrees; calls `AMTPlayerCharacter::AddFOVKick(Degrees, Duration)`, attenuated with distance like the shake. |
| `FMTVFXShake` | `ScaleStrength` | Strength × (1 + ScaleStrength × (scale − 1)): a charged (bigger) spawn of the same preset shakes harder. |

## Quality, caps, pool and styles

- **`mt.VFX.Quality`** (console variable, 0–3, default 3) multiplies non-hero emitter bursts, rates and particle capacity by
  0.35 / 0.55 / 0.8 / 1. Hero emitters always run at full density.
- **Distance.** At spawn, an effect more than 40 m (4000 cm) from the player's camera runs its non-hero emitters at ×0.5,
  and beyond 80 m at ×0.25. The density is decided once per play.
- **Live-effect cap: 120** per world. Past it, the cheap one-shots (the pooled names above) are skipped. Everything else
  still spawns, since the spell's own phases must never vanish.
- **Decal cap: 80** spell decals per world. Past it, the oldest timed marks (scorches, cracks, wet ground) fade out over
  0.5 s. Held zone marks are retired only when no timed mark is left.
- **Pool.** Finished actors of the cheap one-shots return to a per-world pool (at most 64 idle actors, 16 per preset) with
  their layers built. A reused actor is fully reset: random seed, particles and their per-instance data, meshes, lights,
  decal and shake flags, afterimages, charge, tint, scale and attachment.
- **Ground.** `bFollowGround` particles are placed on the static ground under their spawn point. Shapes wider than 2 m, and
  effects that move or ride on something, trace under each particle, with a budget of 24 traces per effect per frame.
  Decals trace down onto static geometry, align to its normal (anything steeper than about 57° is treated as flat), and
  never project onto characters.
- **Styles.** When `Source` is an `AMTCharacterBase`, `SpawnPreset` first tries `<Name>@<CharacterId>`. The twelve element
  spells have `@Orsted` formations built by `OrstedStyle()`: the same element, cleaner and calmer. They use half the
  particles, 70% light, 30% of the charge vibration, and add a faint Dragon God accent: a thin pale-gold ring turning
  across the aim, a deep-blue rim, and a few pale-gold motes. The accent radius follows the formation size. Only names
  that have at least one styled variant pay for the lookup.

## Camera tiers as authored (`Docs/Ability_Overhaul.md` section 5.4)

| Tier | Helper | Strength / duration / radius | Presets |
|---|---|---|---|
| Minor | `MinorShake` | 0.05–0.15 / 0.2 s / 2500 | Fireball Release (0.08) and Impact (0.14), Inferno Eruption (0.12), Water Bullet Release (0.08) and Impact (0.1), Flood Splash (0.12), Wind Blade Release and Impact (0.1), Stone Bullet Impact (0.05), Earth Wall Crack (0.08) and Crumble (0.15), Earth Spikes Eruption (0.12), Quagmire Zone (0.1 at 0.35 s) and Splash (0.06), Barrage muzzles (0.05–0.06), Disturb Destabilize (0.08), Dragon Step Launch and Arrive (0.12), cannon Pierce (0.08 / 0.12), Hit.Heavy (0.12), Palm Strike (0.12) |
| Minor → Heavy with charge | `ScaleStrength` | see presets | Fireball Impact (0.14 × (1 + 2.2 (s − 1)): a full charge reaches ~0.37), Rudeus Cannon Release (0.14, ScaleStrength 3.5: ~0.39 at scale 1.5) and Impact (0.16, 2.6, radius 4000: ~0.41 charged), Stone Cannon Release (0.1, 1) and Impact (0.12, 1.2) |
| Heavy | `HeavyShake` | 0.3–0.45 / 0.35 s / 4000 | Flame Wave Segment (0.32), Tornado Zone (0.3), Wind Burst Impact (0.35), Earth Wall Rise (0.3), Earth Spikes Final (0.42), Saint Dragon Aura cast (0.2), Quagmire Magician cast (0.45) |
| Ultimate | `UltimateShake` | 0.6–0.72 / 0.5 s / 6000 + FOV kick | Inferno Zone (0.6, 4.5° at 0.6 s), Water Dragon Impact (0.65, 5°), Flood Zone (0.6, 4°), Barrage Finale Impact (0.72, 5.5°), Dragon Crush Impact (0.72, 5.5°), Dragon God cast (0.65, 4° at 1.15 s) |

The player's shake never stacks: a new shake only replaces a weaker one. The 11 Flame Wave segments therefore read as a
single heavy shake.

## The presets

Every impact is layered in the same order: flash → pressure (a ring and warped air) → directional fragments → smoke,
dust or mist → light → ground mark → camera by tier. Formations show the magic being gathered and shaped, never a
finished ball appearing. Element identities:
- **Fire:** turns inward and compresses, then billows out. It has flame tongues (stretched `T_VFX_Flame`), rolling
  bodies (`T_VFX_FirePuff`), embers, smoke, soot and scorch.
- **Water:** real water, not blue light. Refractive bodies (`M_VFX_Water`), droplets, glossy blobs (mesh particles),
  foam, mist and wet ground.
- **Wind:** visible as warped air (`M_VFX_Air`), white condensation streaks, pale-cyan glints, and moving dust and
  leaves.
- **Earth:** chunks, slabs, soil clods, dust clouds and skirts, cracks and craters, and stone that pops up and sinks.
- **Orsted:** restrained; pale gold, white and deep blue with black counter-magic ripples.

**Columns:**
- *Kind*: `loop` runs until stopped; otherwise the one-shot's duration.
- *Base*: the visual size at scale 1. VR is the visual radius.
- *Camera*: the tier.

### Rudeus's signature: Stone Cannon (`RudeusCannon`, Rudeus's `StoneCannon` row) and the shared `StoneCannon`

One builder makes both, and Rudeus's version is dramatically stronger. Rudeus's charge response is size ×1.7, spin ×5,
rate ×2.2, intensity ×2.4 and 1.8 cm of vibration. The shared version is ×1.35, ×3, ×1.6, ×1.4 and 0.6.

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| Formation | loop (1.2 s build), charge | slug 52 cm (Rudeus) / 40 cm, 25 cm above the palm | Fragments rip out of the ground under the hand and converge on the palm (Rudeus: they glow with mana), a rough stone forms, is compressed and disappears into a smooth rifled drill slug that spins up. Dust and grit are pulled in. Rudeus: three pressure rings with mana edges snap on at 25 / 55 / 85% charge, a mana sheath, mana motes drawn in, crackling arcs from 60%, warped air from 70%, a mana glow, violent vibration at full charge. Shared: one plain ring at 50%. | – |
| Release | 0.9 s / 0.6 s; scale 1 + 0.5 × charge | cone 340 long (Rudeus) | Rudeus: a white flash, a huge cone of warped air opening from the hand, a circular shockwave and a ring of warped air at the hand, two sonic-boom rings racing ahead at 24 m/s, a spinning dust-and-mana spiral along the line of fire, dust blasted out behind him, a dust ring at his feet, mana sparks and grit shot forward, a mana light. Shared: a small flash, one ring, grit and a dust puff. | minor → heavy with charge |
| Travel | loop | radius 36 (Rudeus) / 22 | The drill slug spinning very fast inside a spinning spiral shroud (Rudeus: mana; shared: dust), warped air and pressure rings trailing it, grit and dust streaming off; Rudeus adds mana sparks and a light. | – |
| Pierce | 0.7 s, pooled | ring 130 / 80 | A flash, a ring facing the shot, soil and chips blasted out of the far side, a dust puff. | minor |
| Impact | 2.8 s / 2.2 s | VR 260 / 170 | A flash, a pressure sphere and a ground ring of warped air, rock chunks, slabs and soil flung out and bouncing, a dust cloud and a racing dust skirt, a dust ring, a crater under radial cracks. Rudeus adds a mana shockwave, seven ground slabs heaved up around the crater (cracked), mana sparks, lingering dust and a strong mana light. | minor → heavy with charge |
| Dissipation | 0.8 s | ~50 cm | The slug crumbles into chips, soil and dust (Rudeus: mana motes scatter). | – |

`StoneBullet` (basic attack, unchanged role): **Formation** (0.35 s) a small slug compressed from dust and chips circling
in; **Travel** (loop) a spinning slug trailing grit and dust; **Impact** (0.9 s) a flash, chips, soil, a dust puff and a
small ring (minor 0.05).

### Quagmire (Rudeus)

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| Formation | loop, charge (size ×1.45, rate ×2) | ball r12 | A swirling ball of muddy water at the lowered palm with a teal mana ring (a second ring from 50% charge), mud flecks drawn in, drips falling toward the ground, teal motes, a teal glow. | – |
| Target | loop, resized live (`SetEffectScale(radius / 900)`) | VR 900 | A faint teal boundary turning slowly, rings contracting toward the centre, dust stirring along the edge, pebbles trembling inside, a low shimmer. | – |
| Zone | loop | VR 900 | The ground transforms in 1.2 s. From 0 to 0.35 s it cracks: dust spurts and chips, and cracks spread from the centre. From 0.35 to 0.8 s water is pushed up through the cracks: muddy spurts and blobs, and the ground darkens and gets wet. From 0.8 s on it is liquid mud: `M_Decal_Mud` churns slowly, bubbles swell and pop, ripples spread, mud pops, low vapour hangs, a few teal glints show. Earth fragments heave up around the boundary and lie there for 7–9 s. A saturated wet ring (1050) surrounds it. Both decals last until the zone stops. | minor (at 0.35 s) |
| Ripple | 1 s, pooled | r90 | At a moving target's feet: two sheen ripples, a dark glossy sinking footprint, mud flecks, a bubble. | – |
| Splash | 1.2 s | crown r70, ring 140 | A fast entry: a crown of mud thrown up, mud flecks and blobs, brown spray, a ripple ring. | minor |
| Dissipation | 1.2 s | VR 900 | The mud dries into pale cracked earth; steam and dust rise off it, crumbs. | – |

### Elemental Barrage (Rudeus)

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| Formation | loop | gather r90, core r9 | The four elements drawn into the raised hands (embers, water streaks, dust, wind streaks) around a mana core with a spinning ring. | – |
| OrbFire | loop (moved every frame) | core r13–26 | Embers spiralling into a turning flame core with a crown of three flame tongues, licking flames, heat haze, a warm light. Grows in over 0.35 s. | – |
| OrbWater | loop | r30 | A spinning ring of water beads around a water core, droplets flung off, a foam ring, mist. | – |
| OrbEarth | loop | r14 + orbit 32 | A dense stone with faint mana seams, rock fragments orbiting, dust. | – |
| OrbWind | loop | r18–30 | Two streak rings spinning on crossed axes around a core of warped air, condensation and glints orbiting. | – |
| MuzzleFire / Water / Earth / Wind | 0.35 s, pooled | rings 40–70 | At the orb, aimed. Fire: a flash, a ring, flame tongues and sparks. Water: a ring, warped air, spray, vapour. Earth: a dusty ring, dust, grit. Wind: warped air, a pale ring, condensation, glints. | minor 0.05 (not wind) |
| Collapse | loop (0.35 s), at (150, 0, 40) | core r18, shells r30–36 | The four elements stream in from all sides into one unstable white core wrapped in fire and water shells, two rings contracting, warped air, a rising light. | – |
| BarrageFinale.Travel | loop | radius 60 | A white-hot mana core in a turning fire shell, a spinning ring of water, water beads and stones orbiting, warped air, a trail of flame, spray, dust, condensation and embers, a big light. | – |
| BarrageFinale.Impact | 3 s | VR 900 | One blast in which each element keeps its identity. A flash. Fire expands through the centre: a fireball, billowing bodies and tongues. Eight stone spikes erupt outward at 5–6 m with chunks and soil. Water spirals up in a column with a fountain and blobs, then mist. Wind drives the whole blast out as a pressure front: warped air, rings, condensation and a racing dust ring. Scorch, wet ground, a crater and cracks are left behind. | ultimate (0.72, 5.5°) |

### Fire: Fireball, Flame Wave, Inferno

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| Fireball.Formation | loop, charge | core r10–23 (×1.4 at full charge) | A compressed sphere of fire turning inward in the palm: a white-hot core, an inner shell spinning one way and an outer shell spinning the other, closing onto it. Flame tongues are sucked inward and sparks are drawn in, with heat haze and a flickering light. Charge: bigger, faster (spin ×2.3), brighter (×1.9), more flame (×1.8), and orange → yellow → almost white (`TintAtFull` 85%). Compression bands snap on at 50% and 85%, with slight vibration. | – |
| Fireball.Release | 0.45 s | ring 95 | A white flash at the palm, a pressure ring and a ring of warped air, flame tongues and sparks thrown forward, a little smoke, a flash of light. | minor 0.08 (scales with charge) |
| Fireball.Travel | loop | radius 30 | A white-hot core inside two counter-turning shells of fire, heat haze, flame tongues licking back, billowing fire, sparks and smoke trailing, a flickering light. | – |
| Fireball.Impact | 2.2 s | VR 320 | A flash; a fireball of billowing flame expanding to a 2 m radius with a yellow heart; a pressure sphere, a flame ring and a ground ring of warped air; fire bodies and tongues flung out; bouncing sparks; embers rising off the ground; a smoke plume and a soot ring racing along the ground; a big warm flash of light; a scorch with glowing embers. | minor 0.14 → heavy charged |
| Fireball.Dissipation | 0.6 s | ~50 cm | The fire gutters out: a small flash, a smoke puff, sparks, a last lick of flame. | – |
| FlameWave.Formation | loop | arc 140 | A burning crescent sweeping around the arm, flames licking along it, sparks drawn in, heat haze, light. | – |
| FlameWave.Segment | loop (11, moved along the arc) | 300 wide × 260 high × 180 deep | A curling sheet of flame (`SM_VFX_WaveSheet`) with a brighter heart behind it, heat haze, big flame tongues along its foot, fire rolling forward off its crest, embers and smoke rising behind it, a warm light. | heavy 0.32 |
| FlameWave.Trail | 0.5 s, pooled | 250 | A charred scorch with embers settling on it, a few licks of flame, embers rising, a little smoke. | – |
| FlameWave.Dissipation | 1 s | arc radius 1300, 120° | Along the whole arc the wall collapses into billowing smoke, shrinking flames and falling embers. | – |
| Inferno.Formation | loop | core r12–22, heat disc 260 | Fire spirals up the raised arm into an intense core, sparks drawn in, the ground around the caster heating (embers rising from it), heat haze, light. | – |
| Inferno.Zone | loop | VR 1100 | Two counter-turning rune circles (1100 and 640, held decals). A ring of fire at the boundary and an inner ring, embers rising over the whole area, a wall of smoke at the edge, heat shimmer, a big warm light. The ultimate shake lands with the first pillars at 0.6 s. | ultimate (0.6, 4.5°) |
| Inferno.Vortex | 4.2 s | funnel r260 × 10 m | A turning funnel of fire with a faster, brighter core funnel, flames and embers spiralling up, a glowing base ring, a smoke plume at the top, a big light, a charred centre. | – |
| Inferno.Eruption | 1.4 s | pillar VR 190, 900 high | A flash, the ground bursts (a ring, chips), a spinning column of fire with a white-hot heart shoots up in under 0.2 s and mushrooms at the top, a jet of flame, sparks, a smoke column, a flash of light, a scorch. | minor 0.12 |
| Inferno.Dissipation | 1.2 s | VR 1100 | The circle smoulders out: smoke over the area, the boundary flames shrinking, the last embers. | – |

### Water: Water Bullet, Water Dragon, Flood

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| WaterBullet.Formation | 0.32 s | ball r13, ring 24 | Water drawn in as streaks and squeezed into a dense sphere above the palm, a wider body of water contracting around it, a foam band spinning, a ring of warped air, mist. | – |
| WaterBullet.Release | 0.5 s | rings 80–130 | Pressure rings snapping forward (one racing ahead at 9 m/s), a jet of spray and water beads, a puff of vapour. | minor 0.08 |
| WaterBullet.Travel | loop | lance radius 20 (104 long) | A spinning lance of water with a foam sheen, a vapour cone opening behind its nose, two pressure rings trailing, spray and mist behind. | – |
| WaterBullet.Pierce | 0.6 s, pooled | ring 80 | Water blasting through and out of the target, a little back-spray, beads, mist, a ring. | – |
| WaterBullet.Impact | 1.3 s | VR 150 | A splash crown and a dome of water, a foam ring and a pressure sphere, droplets and blobs flung and bouncing, foam skidding along the ground, mist, wet ground. | minor 0.1 |
| WaterBullet.Dissipation | 0.6 s | ~30 cm | The lance loses cohesion and falls as drops, beads and mist. | – |
| WaterDragon.Formation | loop (0.75 s build) | spirals 400 high, r130 | Twin spiral streams of water wind up around the caster while the dragon's mass gathers above and behind him, droplets and beads spiralling up, mist at the feet, a cool light. | – |
| WaterDragon.Travel | loop on the serpent's head | radius 110 | The serpent's head and body are drawn by `AMTWaterSerpent`. This preset adds spray thrown off the head, foam and mist streaming back, drips falling, a ribbon of heavy water blobs in its wake, two glowing eyes (95 ahead, ±32, +38), a haze and a cool light. | – |
| WaterDragon.Impact | 2.4 s | VR 780 | An enormous water explosion: two foam rings and a pressure sphere, a dome and a crown of water, a surge wave sweeping out 7.8 m along the ground, droplets and heavy blobs raining back, foam racing out, a mist cloud, wet ground, a flash of light. | ultimate (0.65, 5°) |
| WaterDragon.Dissipation | 1.4 s | r250 | The serpent slumps and collapses into rain, blobs and mist. | – |
| Flood.Formation | loop | wall 520 wide, 220 high | Behind the caster, a wall of water swells up out of the ground, water rising from it, foam and vapour along its foot, streams drawn into the arms. | – |
| Flood.Zone | loop on the wave front | 2400 wide × 380 high | Six overlapping curling sheets of water, a foaming crest, spray thrown ahead, heavy blobs, churning foam at the foot, a veil of mist behind. | ultimate (0.6, 4°) |
| Flood.Trail | 0.6 s, pooled, every 300 cm | 2500 × 380 strip | A strip of wet ground across the full width, puddle splashes, low mist. | – |
| Flood.Splash | 1.4 s | sheet 440 × 360 | A sheet of water thrown up and along X, droplets, blobs, foam and mist (scale 1 at an enemy's feet, 2 against a wall). | minor 0.12 |
| Flood.Dissipation | 1.6 s | 2400 wide | The wave breaks and collapses: foam bursting forward, water falling, mist, a band of wet ground. | – |

### Wind: Wind Blade, Tornado, Wind Burst

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| WindBlade.Formation | 0.35 s | crescent 110 | Air compressed into a crescent in the palm: a bright edge and a crescent of warped air turning, a sphere of warped air contracting, condensation streaks rushing in and spiralling, glints, a leaf or two pulled in. | – |
| WindBlade.Release | 0.5 s | slash 300 wide | The slash: a bright crescent sweeps out of the hand, a pressure ring of warped air and a pale ring, condensation and glints blasted forward, dust kicked up at the feet. | minor 0.1 |
| WindBlade.Travel | loop | crescent 520 wide | A flat crescent of warped air with a bright streaming edge and a cyan core, condensation and glints peeling off behind, dust torn up from the ground below, leaves. | – |
| WindBlade.Impact | 0.9 s | ring 240 | The crescent splits against what it hit: a flash of its edge, a ring and a pulse of air, condensation, dust and leaves thrown out. | minor 0.1 |
| WindBlade.Dissipation | 0.6 s | 520 | The crescent unravels into condensation wisps and settling dust. | – |
| Tornado.Formation | loop | whirl r22 | A small whirl of warped air spinning in the hand, condensation, dust and a leaf or two circling. | – |
| Tornado.Zone | loop, resized live (to ×1.56) | VR 360, 950 high | A spinning funnel of warped air streaked with pale wind and veiled in dust. Dust, vapour, rocks and leaves are drawn in at the base and flung up and out along it, dust is scoured from the ground, a dust skirt turns at its foot. Heavy shake as it forms. | heavy 0.3 |
| Tornado.Trail | 0.6 s, pooled | r180 | The ground it crosses: dust scoured into a swirl, leaves and pebbles tossed up. | – |
| Tornado.Lift | loop on a lifted enemy | r80 | A tight whirl of warped air around them, condensation and dust spinning. | – |
| Tornado.Dissipation | 1.4 s | VR 360–450 | The funnel unravels: a pulse of air, dust spreading and settling, leaves raining down, condensation dispersing. | – |
| WindBurst.Formation | 0.16 s | sphere 320 → 50 | The air visibly pulled in: a sphere of warped air and two rings contracting onto the caster, condensation rushing inward, dust and leaves dragged along the ground. | – |
| WindBurst.Impact | 1.2 s | VR 620 | A huge sphere of pressure: its bright edge racing out, warped air, three ground rings, a front of condensation, dust and pebbles blown out along the ground, leaves. | heavy 0.35 |

### Earth: Earth Wall, Earth Spikes

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| EarthWall.Formation | loop | r50–120 | Dust drawn to the rising hand, stones circling up around it, a small stone forming, the ground trembling. | – |
| EarthWall.Rise | 1.6 s, per segment | 170 wide foot | The ground bursts along the segment's foot: a dust wall, chunks and soil thrown out, a dust column, pebbles raining off the rising top, a dust ring, cracks. | heavy 0.3 |
| EarthWall.Crack | 0.8 s, at the segment centre | box 90 × 170 × 260 | Chips and grit blown off both faces, a dust puff (scale 1.25 for the second crack). | minor 0.08 |
| EarthWall.Crumble | 2 s | segment | Big cracked chunks and rubble falling off the collapsing slab, a billowing dust cloud at its foot, dust sifting down, cracks. | minor 0.15 |
| EarthSpikes.Formation | loop | r50–100 | Dust drawn to the hand, a stone point forming beneath it and spinning, shards circling in, the ground trembling. | – |
| EarthSpikes.Aim | loop (moved every frame) | line 0–1300, 90 wide | A glowing fault line on the ground from the caster's feet to 13 m (`M_Decal_AimLine`, held): chevrons flowing outward, diamond marks where the spikes will rise (320, 640, 960, 1300). The ground trembles along it, and dust stirs in rings at each mark, widest at the last. | – |
| EarthSpikes.Eruption | 1.3 s | VR 150, 280 high | A stone lance punches up with two shards, the ground bursts (chunks, slabs, soil, a dust cloud, skirt and ring), cracks. The lance withdraws at 1.2 s, when Crumble plays. | minor 0.12 |
| EarthSpikes.Final | 1.6 s | VR 300, 700 high | An enormous spike with a crown of five shards leaning out, a pressure pulse, a big ground burst, a dust column, a crater and wide cracks. | heavy 0.42 |
| EarthSpikes.Crumble | 1.4 s | spike (scale 2 for the final) | Cracked chunks falling off the withdrawing spike, a dust puff, soil. | – |

### Orsted: Disturb Magic, Dragon Step, Dragon Crush

| Phase | Kind | Base | What the player sees | Camera |
|---|---|---|---|---|
| DisturbMagic.Formation | loop | core r5, glyph r16 | Before the calmly raised palm: a tiny pale core and blue halo, a thin dragon-line glyph (`SM_VFX_Glyph`) turning, black ripples and rings of warped air spreading, a few motes, a faint light. | – |
| DisturbMagic.Cast | 0.35 s | glyph r30, rings 60–90 | The finger flick: a glyph flash, a pale ring and a ring of warped air, a black ripple, thin streaks sent forward. | – |
| DisturbMagic.Pulse | loop (moved at 65 m/s) | ring r40 | Nearly invisible: a thin ripple of warped air with an echo, a small turning glyph at its head, a faint trail of pale streaks and black wisps. | – |
| DisturbMagic.Hit | 0.6 s | glyph r55, ring 120–150 | The glyph snaps open on the target with a pale ring, warped air and a black ripple; a few pale sparks disperse. Counter-magic, not an explosion. | – |
| DisturbMagic.CollapseFire / Water / Earth / Wind / Arcane | 0.8 s | glyph r26, ring 60 | The glyph snaps shut on the cancelled spell with a contracting ring and a black ripple, and the element fails in its own way. Fire gutters into sparks, falling embers and smoke. Water slumps and falls as drops. Earth: a cracked husk crumbles into chips, soil and dust. Wind disperses in a pulse of streaks. Arcane flickers out in scattering motes. `DisturbMagic.Impact` (older gameplay) is the arcane collapse. | – |
| DisturbMagic.Seal | 3 s (one-shot, on the sealed hand) | glyph r14 | A binding glyph turning before the hand, two crossed rings (blue and pale gold), black wisps circling, a faint flickering light. | – |
| DisturbMagic.Destabilize | 1 s | base radius 450 (scale = radius / 450, 0.5–4) | A large glyph flares flat across the zone, wall, tornado or serpent; black ripples and a pulse of warped air spread; pale crackles and rising motes. | minor 0.08 |
| DragonStep.Formation | loop, at the feet | rings 180 → 40 | The dragon aura gathers: pale-gold and deep-blue rings closing in, motes rising, dust stirring toward him. | – |
| DragonStep.Launch | 0.8 s | VR 350 | A flash at the feet, dust and pale-gold rings and warped air racing out, a dust ring, chips, cracks. | minor 0.12 |
| DragonStep.Travel | loop on Orsted | wake 340 long | Three rim-lit afterimages, a wake of warped air, pale-gold and deep-blue streaks, dust kicked off the ground. | – |
| DragonStep.Arrive | 0.7 s | VR 220 | A pressure ring in dust, pale gold and deep blue, warped air, a dust ring, pale-gold sparks, a flash of light. | minor 0.12 |
| DragonCrush.Formation | loop on hand_r | fist r8–28 | The Dragon God aura compressed tight around the fist: a white core, a pale-gold shell and a deep-blue rim closing in, two thin rings spinning, motes drawn in, warped air. | – |
| DragonCrush.Impact | 2.6 s, 120 ahead | VR 650, cracks 520 | The stored energy released at once. A white and pale-gold flash, a massive shockwave in dust, gold and deep blue with rings of warped air, nine slabs of ground heaved up around the blow, chunks and soil, a dust ring racing out and a dust column, broken ground and a crater, a bright light. | ultimate (0.72, 5.5°) |
| DragonCrush.TargetHit | 0.6 s, X back toward Orsted | ring 200–260 | A white flash, a pale-gold ring and warped air around the blow, sparks and dust driven straight through the target (along −X). | – |

### Kept for other abilities and generic reactions

| Preset | What it shows |
|---|---|
| `Hit.Light` / `Hit.Heavy` (pooled) | X points back at the attacker: a flash, a ring facing the blow (Heavy: plus warped air), sparks thrown back (Heavy: plus a dust puff and a minor shake). |
| `Cast.Generic` | A small mana spark and motes drawn in. |
| `Dodge.Dust` (pooled) | Dust kicked back from the feet. |
| `DemonEye.Cast` / `.Aura` | A mana eye flare with a ring and motes; orbiting motes. |
| `QuagmireMagician.Cast` / `.Aura` | A maelstrom of mud, stone and mana around Rudeus, a mana shockwave, cracks, a heavy shake; orbiting stones and motes. |
| `PalmStrike.Impact` | A ring of warped air and a silver ring facing the strike, a dust puff. |
| `DragonAura.Cast` / `.Aura` | Gold pressure ripples and shimmer, a heavy shake; slow gold ripples at the feet, rising motes, warped air. |
| `DragonGod.Cast` / `.Aura` | Gold rings condensing into him for 1.1 s, then a flash, a shockwave, a dust blast and cracks with an ultimate shake; motes and warped air. |

## Assets behind the presets

### Meshes (`Tools/kit/arch_vfx.py`)

Built with `python3.11 Tools/kit/build_architecture_kit.py --category VFX` (Blender 5.0 as a Python module). The build
writes the GLBs (git-ignored), merges their entries into `SourceArt/Kit/manifest_architecture.json` and
`SourceArt/Kit/manifest.json`, and renders `SourceArt/Kit/VFX/_Preview.png`. All 25 VFX meshes pass
`qa_architecture_kit.py --only SM_VFX` (closed shells with outward normals, no coplanar overlaps, budget ≤ 3000 tris).
The runtime scales every mesh to the layer's `Size` (half-extents in cm), so only the proportions matter.

| Mesh | Size (m) | Tris | Use |
|---|---|---|---|
| `SM_VFX_Slug` | 0.60 × 0.30 × 0.30, centre pivot | 2080 | Drill slug along +X: ogive nose, five rifling grooves twisting a third of a turn, bevelled flat tail (slot `MT_Rock`). Stone Cannon and Stone Bullet. |
| `SM_VFX_Spiral` | 1.03 × 0.61 × 0.61, centre | 1156 | Helix ribbon along +X, 3 turns (UV v = 0..3 along it): cannon shroud and wake, Water Dragon streams, the finale water column. |
| `SM_VFX_WaveSheet` | 0.94 × 2.00 × 1.00, base pivot | 1052 | Breaking wave face 2 m wide (Y) curling forward (+X): Flame Wave segments, Flood waves and splashes. |
| `SM_VFX_EarthWall_B` | 6.0 × 1.5 × 4.0, base | 136 | Earth wall variant: four leaning broken columns (slot `MT_Rock`). |
| `SM_VFX_EarthWall_C` | 6.0 × 1.5 × 4.0, base | 192 | Earth wall variant: two slabs split by a jagged fracture, a chunk broken off the top. |
| `SM_VFX_Slab` | 0.91 × 0.87 × 0.25, base | 40 | Raised ground slab heaved up around craters (Dragon Crush, Rudeus's cannon). |
| `SM_VFX_Glyph` | 0.95 × 0.95 × 0.015, centre | 1272 | Disturb Magic's dragon-line sigil: broken outer ring, ticks, inner ring, serpentine dragon line with a chevron head. |
| `SM_VFX_Cone` | 1.0 × 1.0 × 1.0, apex at the origin | 1024 | Open pressure cone along +Z (double wall): the cannon release cone, the water lance's vapour cone. |

Until the kit is imported, `MTVFX::LoadMesh` falls back to engine shapes: rings, discs and the glyph use a cylinder;
spikes, funnels, flames, crystals and the cone use a cone; walls and slabs use a cube; the crescent uses a plane; slugs,
spirals and wave sheets use a sphere.

### Textures (`Tools/vfx/make_vfx_textures.py`)

Preview: `Docs/Images/VFX/Textures_Preview.png`.

| Texture | Content | Import kind |
|---|---|---|
| `T_VFX_Flame` 256×128 | Flame tongue along U: lens-shaped, ragged licking edges, filaments, white-hot core (grey = heat, alpha = shape). Symmetric enough to read either way along the motion. | vfx (sRGB) |
| `T_VFX_FirePuff` 256² | Billowing fire: rolling billows with darker creases, a hot centre, a ragged rim. | vfx |
| `T_VFX_Leaf` 128² | A leaf with midrib, veins and stem (alpha) for wind debris. | vfx |
| `T_VFX_Mud` 512², tileable | R albedo variation, G bubble domes, B per-bubble phase (bubbles pop out of step), A boundary noise. | mask |
| `T_VFX_CrackNet` 512², tileable | R crack strength (about 1 on main fractures, 0.6 on branches, 0.3 on hairlines), G a soft halo. | mask |
| `T_VFX_Crater` 1024² | R albedo shade (dark bowl, fresh rim, thrown-dirt rays and clods), G bowl mask, A opacity. | mask |
| `T_VFX_CraterN` 1024² | Crater normals (bowl, raised rim, clods), DirectX / Unreal convention like `T_VFX_WaterN`. | normal |
| `T_VFX_AimLine` 1024×128 | U along the line: R core line + diamond spike marks at 320 / 640 / 960 / 1300 of 1300, G 16 chevrons (tileable along U), B glow, A strip mask (fades in at the caster). | mask |

### Materials (`Content/Python/mt_setup_laplace.py`)

Preview (CPU approximation of the material math, not an engine render): `Docs/Images/VFX/Materials_Preview.png`.

| Material | Parameters | What it does |
|---|---|---|
| `M_Decal_Mud` | `Color`, `Opacity`, `Age` (s), `Flow` | Quagmire's ground. Cracks (`T_VFX_CrackNet`) spread from the centre over 0–0.35 s. Wet dark ground spreads over 0.35–0.8 s (roughness drops). Liquid mud (`Color` × albedo) spreads from 0.8 s, slowly churning: a flowing normal from `T_VFX_WaterN`, rotated around the centre at `Flow`, plus bubble domes that swell and pop. Needs a held decal, since only held decals get `Age`. |
| `M_Decal_Crater` | `Color`, `Opacity`, `NormalStrength` | A dug bowl with a raised rim of fresh soil and thrown dirt. Set `NormalStrength` to −1 if the relief ever reads as a dome. |
| `M_Decal_AimLine` | `Color`, `Intensity`, `Opacity`, `ScrollSpeed`, `AlongV` | An emissive fault line with chevrons scrolling outward and spike marks. The C++ lays the strip so decal U runs along the effect's X. `AlongV = 1` swaps the texture axes if a platform maps decal UVs the other way round. |
| `M_VFX_Rock` `Crack` | 0–1 (default 0) | Reveals the `T_VFX_CrackNet` fracture network: main fractures at about 0.3, branches by 0.6, hairlines by 1. The cracks are dark, and glow in `Color` when `Glow` > 0. Used on heaved slabs (0.6) and crumbling chunks (1). |

Newer decals fall back to older ones until the editor setup has built them: mud to `M_Decal_Wet`, crater to
`M_Decal_Cracks`, aim line to `M_Decal_Circle`.

### Rebuilding on the Mac

`Tools/mac/build_and_setup.sh` regenerates generated art only when a marker file is missing. The markers are
`SourceArt/VFX/Textures/T_VFX_Noise.png` and `SourceArt/Kit/VFX/SM_VFX_Ring.glb`, so a machine that built the older
effects does not get the new textures and meshes. Either run the generators by hand, or move the markers to the newest
outputs, files only the new generators make: `SourceArt/VFX/Textures/T_VFX_AimLine.png` (the texture generator's
last file) and `SourceArt/Kit/VFX/SM_VFX_Cone.glb`. Then re-run the editor setup
(`mt_setup_laplace.py`), which imports every texture and every GLB in `SourceArt/Kit/VFX` and builds the materials.

## Adding a preset

1. **Name it** `<Preset>.<Phase>`, as gameplay spawns it (`Tools/vfx/phase_contract.json` for the contract phases). For a
   character-specific look, add `<Preset>.<Phase>@<CharacterId>` next to the shared one.
2. **Author it** in the element's `Add...` function in `MTVFXLibrary.cpp`, as a block ending in a *literal*
   `Out.Add(TEXT("<Preset>.<Phase>"), D);`. The check script reads those literals.
   - Start from `Desc(Duration, bLoop, FadeOut)`. Impacts and ground phases use `ImpactDesc(Duration)`, which keeps them
     upright.
   - Author at the base visual size, so that scale 1 = the contract's size.
   - Build the beats in order with the helpers:
     - flash: `Flash`;
     - pressure: `ShockwaveRing`, `FacingRing`, `AirPulse`, `FacingAir`;
     - bodies: `Orb`, `WaterBody`, `RockBody`, `Haze`;
     - particles: `FireBody`, `FlameWisps`, `EmberDrift`, `Sparks`, `SmokePuffs`, `DustCloud`, `Debris`, `DirtClods`,
       `Droplets`, `WaterBlobs`, `Foam`, `Mist`, `Leaves`, `Condensation`, `WindStreaks`, `Motes`, `Gather`, `Converge`;
     - light: `Glow`, `FlashLight`;
     - ground marks: `AddScorch`, `AddCrater`, `AddCracks`, `AddWet`, `Decal`;
     - camera: `MinorShake`, `HeavyShake`, `UltimateShake`.
   - Mark the one or two emitters that carry the read `bHero`.
   - Leave `MaxParticles` at 0; `AutoCapacity` sizes it from the burst, rate and life, up to 256.
   - Opaque rock layers cannot fade, so give them a `RiseHoldSink` scale curve.
   - Chargeable formations set `D.Charge` and gate extra layers with `ChargeThreshold`.
3. **Frequently spawned one-shots** should have a name the pool recognises (`.Trail`, `.Ripple`, `.Muzzle`, `.Pierce`,
   `Hit.`, `Dodge.`), or extend `IsCheapPreset` in `MTSpellVFX.cpp`. Such presets must not be loops.
4. **Check the contract:** `python3 Tools/vfx/check_presets.py` (add `--quiet` for the summary only). It lists every phase,
   warns about duplicates and orphan styles, and exits 1 when a contract phase has no preset.
5. **New meshes:** add an `@asset` builder to `Tools/kit/arch_vfx.py`, build and QA it (above), add its path to
   `MTVFX::Paths`, and teach `FallbackMesh` a stand-in. **New textures:** add a function to `make_vfx_textures.py`,
   and a kind in `setup_vfx()` for data or normal maps. **New material parameters:** keep the existing names
   (`Color`, `Intensity`, `Opacity`, ...) so the runtime drives them.

Latest check (`python3 Tools/vfx/check_presets.py --quiet`):

```
127 presets registered (115 base, 12 styles); 103 contract phases, 103 present, 0 missing.
All contract phases have a preset.
```
