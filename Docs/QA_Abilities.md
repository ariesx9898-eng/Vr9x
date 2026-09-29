# LA PLACE ability overhaul: QA, grading loop and Mac handoff

This is the quality record for the overhaul in `Docs/Ability_Overhaul.md`. It contains four things:
- the checks that already ran offline in the cloud container (no Unreal Engine there);
- the grading sheet from the brief, **which can only be filled in by playing the game on the Mac**;
- the steps that run the loop;
- a prompt for a Claude session on the Mac.

**No score in this file is an in-engine grade until the "In-engine grade" table below has numbers.** The offline column records only what could be verified without the engine. It is not a playtest.

## 1. What is verified offline

| Check | Tool | Result |
|---|---|---|
| Ability data: rows, behaviours, params, loadouts, honest hit sizes (x1.0–1.15), flight distances, sound paths, Release frame = `CastTime` | `python3 Tools/validate_data.py` | see §5 (run log) |
| Every preset phase the gameplay code spawns exists | `python3 Tools/vfx/check_presets.py` | see §5 |
| Casting clips: IK error, foot slip, angular velocity, ground penetration, loops | `Tools/anim/qa_animation_quality.py`, `Docs/QA_Animation.md` | see §5 |
| New sounds: loudness tier, true peak, clean tails | `Tools/audio/synth_sfx.py` analysis, `Docs/LaPlace/Audio.md` | see §5 |
| C++ shadowing (UE treats shadowing as an error) | heuristic checker over every changed file | no findings |

The following **cannot** be verified offline, and are the Mac's job:
- that the C++ compiles;
- how the effects look;
- that animation and effect line up in motion;
- camera feel;
- sound in 3D;
- performance.

## 2. The grading loop (Mac)

1. **Pull and rebuild everything that changed.**

   ```bash
   git pull
   Tools/mac/build_and_setup.sh          # compile, regenerate new VFX meshes/textures/sounds, re-import both characters
   ```

   The generator markers were moved to files that only the new generators create, so the new meshes, textures and sounds are generated even on a machine that ran the old ones. If a step is skipped anyway, delete `SourceArt/Audio`, `SourceArt/VFX/Textures` and `SourceArt/Kit/VFX` and re-run.

   Two independent reviews found no compile errors in the overhaul's gameplay code, but nothing here has been through a compiler. If the first build fails, these engine APIs are used for the first time in this project, so look at them first:
   - `FRootMotionSource_ConstantForce` (`Force`, `Priority`, `FinishVelocityParams`) and `MovementMode == MOVE_None` in `MTZoneActor.cpp` (Tornado pull and lift);
   - `AActor::CustomTimeDilation` and `FTimerDelegate::CreateLambda` in `MTCombatStatics.cpp` (Dragon Crush hit-stop);
   - `USoundBase::IsLooping()` and `UWorld::SweepMultiByObjectType` in `MTProjectile.cpp`;
   - `SetUsingAbsoluteLocation/Rotation/Scale` in `MTWaterSerpent.cpp`;
   - `UAudioComponent::FadeIn/FadeOut/SetPitchMultiplier` on the zone and projectile sound beds.
   - Editor Python, not C++: `unreal.AnimationLibrary` notify calls in `mt_setup_rudeus.py` (`add_animation_notify_track`, `add_animation_notify_event`, `remove_animation_notify_events_by_track`, reading them back). The log should have 19 `PASS [mt_setup_rudeus] events A_Rudeus_...` lines and 22 for Orsted, and no `MANUAL ... MTAnimNotify_Event` line (that one means the C++ module was not compiled into the editor yet).

2. **Automated checks.**

   ```bash
   Tools/mac/run_automation_tests.sh MushokuRPG      # includes MushokuRPG.Abilities.* and the Orsted combo / seal tests
   ```

   `MushokuRPG.Abilities.EveryLoadoutAbilityFires` casts every ability the ABILITIES menu can put on keys 1–4, for both characters, through the real ability path. It checks that each one executes, hits a dummy 4 m ahead (except Earth Wall, Quagmire and Disturb Magic), and leaves no zones, walls, serpents, projectiles or running effects behind.

3. **Showcase (screenshots of every ability and both combos).**

   ```bash
   "/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
     "$PWD/MushokuRPG.uproject" "/Game/Maps/L_Fittoa?game=/Script/MushokuRPG.MTShowcaseGameMode" -game -windowed -resx=1920 -resy=1080
   ```

   Screenshots are saved to `Saved/Screenshots/Showcase/`. The log has one `[Showcase] RESULT` line per entry: PASS/FAIL, damage on the main dummy, and crowd dummies hit. To run only a few entries, pass `?Abilities=Rudeus_StoneCannon+Rudeus_Quagmire+Rudeus_ElementalBarrage,Orsted:Fire_Inferno`.

4. **Play it.** Run `Tools/mac/play.sh` (Rudeus) and `Tools/mac/play.sh --as Orsted`:
   - Put every spell on keys 1–4 through the ABILITIES menu and use each one against the arena opponent.
   - Play the two signature combos: Rudeus 1 → 2 → 3 (Stone Cannon → Quagmire → Elemental Barrage), and Orsted 1 → 2 → 3 (Disturb Magic while the opponent casts → Dragon Step → Dragon Crush inside the 0.9 s window).

   Timing to judge while playing (all measured offline, none seen yet):
   - A charged Stone Cannon or Fireball fires on the frame the key is let go; the release clip's arm reaches full extension about 0.10 s later. If the slug looks like it leaves before the push, move the release clip's `Release` later in `Tools/anim/cast_design`, or shorten the thrust.
   - Targeted Dragon Step arrives after about 0.20 s, but the clip lands at 0.29 s. If Orsted looks like he lands after arriving, set `TargetDuration` to about 0.2 in `Tools/laplace/overhaul_abilities.py`, or land the clip earlier.
   - The awakened Stone Cannon and Quagmire fire slightly before their clips' release pose (`CastTime` 0.30/0.40 against 0.35/0.45).
   - Dragon Crush's ground-break sound is 85 ms after the blow, on the hit-stop release.

5. **Grade** each category 1–10 in §3. Compute the average.
   - **Below 8.5, or any category below 8.0:** fix the weakest categories (§4 lists where each lives), rebuild, and repeat from step 2.
   - **Do not write a score you did not see.**

## 3. Grading sheet (from the brief)

| Category | What a 9 looks like | In-engine grade | Notes |
|---|---|---|---|
| Visual Effects | Every spell looks expensive, layered and powerful; nothing reads as default particles, coloured spheres or flat cards | – | |
| Animations | Body and spell feel like one action: hands shape the magic and release on the frame; no snapping, sliding or frozen poses | – | |
| Impact | Heavy hits land: shockwaves, debris, knockback and launch, hit-stop only on Dragon Crush, camera by tier | – | |
| Character Identity | Rudeus manipulates huge amounts of mana; Orsted barely moves and hits just as hard | – | |
| Ability Uniqueness | Every ability is recognisable from its silhouette, colour, motion and sound alone | – | |
| Combat Responsiveness | Presses fire immediately; chargeable spells feel good at tap and full charge; combos chain | – | |
| Hitbox Quality | Big and forgiving (x1.12), always matching what is drawn; several enemies per cast | – | |
| Sound Design | Build-up, travel and impact for every spell, placed in 3D | – | |
| Environment Interaction | Scorch, cracks, wet ground, craters, mud, dust and leaves move; marks fade; nothing important is destroyed | – | |
| Technical Polish | No broken or floating effects, duplicates, missing collisions, flicker, leftovers or stuck states | – | |
| **Average** | ≥ 8.5, no category below 8.0 | – | |

Per-combo and per-spell checks from the brief (tick them while playing):

| Test | Pass? |
|---|---|
| Rudeus: Stone Cannon → Quagmire → Elemental Barrage | |
| Orsted: Disturb Magic → Dragon Step → Dragon Crush | |
| Fire: Fireball (tap and full charge), Flame Wave, Inferno | |
| Water: Water Bullet, Water Dragon, Flood | |
| Earth: Stone Cannon (tap and charge), Earth Wall (hit it until it cracks), Earth Spikes | |
| Wind: Wind Blade, Tornado (with small and large enemies), Wind Burst | |
| Every ability from the hotbar, both characters, no empty button | |

## 4. Where to fix what

| Weak category | Where |
|---|---|
| Visual Effects | Presets in `Source/MushokuRPG/Private/VFX/MTVFXLibrary.cpp` (layers, sizes, colours; `Docs/LaPlace/VFX.md`), meshes in `Tools/kit/arch_vfx.py`, materials in `Content/Python/mt_setup_laplace.py` |
| Animations | Clips in `Tools/anim/` (`Docs/Animation_Pipeline.md`); Release timing: the sidecar `events` and the rows' `CastTime` |
| Impact / camera | Preset `Shakes` (tiers in `Docs/Ability_Overhaul.md` §5.4), `Knockback` / `Launch` in `Tools/laplace/overhaul_abilities.py`, `HitStop` param |
| Responsiveness | `CastTime`, `RecoveryTime`, `MaxChargeTime` in `Tools/laplace/overhaul_abilities.py`; input buffer in `UMTAbilityComponent` |
| Hitbox Quality | `AOERadius`, `ProjectileRadius`, `ProjectileWidth` (visual sizes) and `HitForgiveness` in the same script |
| Sound | `Tools/audio/` (`Docs/LaPlace/Audio.md`); which sound plays when: `FX.*Sound` in the script |
| Environment | Preset decals and debris; the Quagmire mud (`Quagmire.Zone`), Flood trail, Flame Wave scorch rows |
| Technical Polish | The automation tests and the showcase RESULT lines point at the ability |

After changing the tuning script: run `python3 Tools/laplace/overhaul_abilities.py`, then `python3 Tools/validate_data.py`.

## 5. Offline run log

Filled in by the cloud session when the overhaul was delivered (see the commit that last touched this file):

| Run | Result |
|---|---|
| `Tools/validate_data.py` | ALL CHECKS PASSED: 65 ability rows, both default loadouts valid, every hit size within x1.0–1.15, every non-chargeable `CastTime` within one frame of its clip's `Release`, every sound in the manifest and no loop where it could never stop. 13 warnings are older shared clips (CastBasic, DemonEye, DodgeForward, CastTwoHand, CastGround) that deliberately have no `Release` event. |
| `Tools/vfx/check_presets.py` | 127 presets (115 base, 12 Orsted formation styles); all 103 phases the gameplay spawns exist |
| Animation QA (`Tools/anim`) | New and upgraded clips: IK error 0.00 cm, planted-foot slip 0, loop seams 0, no ground penetration. The worst peak is 30.3°/frame (Rudeus Cast_EarthWall hand) and 31.2 (Orsted Dragon Step). Rebuilding both GLBs from the sources reproduces the committed ones (worst difference 0.0000°). |
| Audio analysis (`Tools/audio`) | 26 new sounds, each within 0.1 LU of its loudness tier, true peak ≤ −1.03 dBTP, tails ≤ −20 dB. The 67 existing sounds are byte-identical, and builds are deterministic across worker counts. |
| Kit QA (effect meshes) | 25 / 25 pass (8 new) |
| C++ reviews (no compiler) | Two independent reviews of the gameplay C++ found no compile errors. They found two gaps, now fixed: the ABILITIES menu category for the new behaviours, and the AI's use of Disturb Magic and Dragon Crush. The effects C++ had two more reviews and needed no changes. The presets: a script checked 2,007 field assignments and 927 helper calls against the structs and caught every one of 20+ planted faults. The runtime, pool and subsystem compiled with clang 18 and gcc 13 against stand-in engine headers with `-Wall -Wextra -Wshadow -Wunreachable-code-loop-increment -Werror`: 0 diagnostics, file by file and as one unity file. Whether the real engine signatures match the stand-ins is only settled by the Mac's compile (watch list in §2). |

## 6. Prompt for a Claude session on the Mac

> Pull the branch, then run `Tools/mac/build_and_setup.sh`. Fix every compile error, FAIL line and failed test, and re-run until `RESULT: OK`; the ability overhaul in `Docs/Ability_Overhaul.md` was written in a cloud container without Unreal, so the first compile may surface API mistakes. Then run the showcase (`Docs/QA_Abilities.md` §2 step 3), look at every screenshot, play both characters (step 4), and fill in the grading sheet in §3 honestly. While the average is below 8.5 or any category is below 8.0, improve the weakest categories (§4), rebuild and grade again. Commit each round with the grades and what changed.
