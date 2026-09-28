# Lore Research - Mushoku Tensei RPG (combat + data)

Owner: combat design / lore research. Data it backs: `Content/Data/{Characters,Abilities,Elements,Races,Quests,Enemies,Items,RollConfigs}.json`
(validated by `python3 Tools/validate_data.py`).

## 0. Sources and how reliable they are

**Direct page fetches failed.** Every `WebFetch` request was refused by this environment's network egress proxy
(`EGRESS_BLOCKED`):

| URL requested | Result |
|---|---|
| https://mushokutensei.jp/character/ (official site) | blocked - not read |
| https://mushokutensei.fandom.com/wiki/Rudeus_Greyrat/Powers_and_Abilities | blocked - not read directly |
| https://mushokutensei.fandom.com/wiki/Orsted/Powers_and_Abilities | blocked - not read directly |
| https://mushokutensei.fandom.com/wiki/Orsted | blocked - not read directly |
| https://en.wikipedia.org/wiki/Mushoku_Tensei | blocked - not read |

Research was therefore done with **web search** (about 10 minutes). The facts below come from the search engine's
indexed summaries of the pages listed. That is weaker than reading the pages, so each claim is tagged:

- **[S]** confirmed by a search-result summary (source page(s) named)
- **[M]** from the researcher's memory of the light novel/anime, **not verified** this session. Treat as provisional.

Pages whose indexed content was used:

- Mushoku Tensei Wiki (fandom): [Rudeus Greyrat/Powers and Abilities](https://mushokutensei.fandom.com/wiki/Rudeus_Greyrat/Powers_and_Abilities),
  [Quagmire](https://mushokutensei.fandom.com/wiki/Quagmire), [Demon Eyes](https://mushokutensei.fandom.com/wiki/Demon_Eyes),
  [Orsted](https://mushokutensei.fandom.com/wiki/Orsted), [Orsted/Powers and Abilities](https://mushokutensei.fandom.com/wiki/Orsted/Powers_and_Abilities),
  [Disturb Magic](https://mushokutensei.fandom.com/wiki/Disturb_Magic), [Curses](https://mushokutensei.fandom.com/wiki/Curses),
  [Battle Aura](https://mushokutensei.fandom.com/wiki/Battle_Aura), [Orsted/Chronology](https://mushokutensei.fandom.com/wiki/Orsted/Chronology),
  [Races](https://mushokutensei.fandom.com/wiki/Races), [Migurd](https://mushokutensei.fandom.com/wiki/Migurd),
  [Ruijerd Superdia](https://mushokutensei.fandom.com/wiki/Ruijerd_Superdia), [Badigadi](https://mushokutensei.fandom.com/wiki/Badigadi),
  [Ogre race](https://mushokutensei.fandom.com/wiki/Ogre_race), [Dwarf](https://mushokutensei.fandom.com/wiki/Dwarf),
  [Dragon Tribe](https://mushokutensei.fandom.com/wiki/Dragon_Tribe), [Ghislaine Dedoldia](https://mushokutensei.fandom.com/wiki/Ghislaine_Dedoldia),
  [Monsters](https://mushokutensei.fandom.com/wiki/Monsters), [Classes](https://mushokutensei.fandom.com/wiki/Classes)
- Other: [Villains Wiki - Orsted](https://villains.fandom.com/wiki/Orsted), [QTA Orsted wiki](https://quotetheanime.com/mushoku-tensei/orsted-wiki/),
  [Game Rant - Demon Eyes explained](https://gamerant.com/mushoku-tensei-demon-eyes-explained/),
  [Game Rant - Every race](https://gamerant.com/every-race-in-mushoku-tensei-jobless-reincarnation/),
  [Game Rant - most powerful mages](https://gamerant.com/mushoku-tensei-most-powerful-mages-ranked/),
  [Shapes - magic system](https://shapes.inc/fandom/mushoku-tensei-jobless-reincarnation/magic-system),
  [Ranobeki - magic rank system](https://ranobeki.com/en/series/mushoku-tensei/wiki/magic-rank-system-mt)
- Shindo Life: [Shindo Life Wiki - Bloodline](https://shindo-life-rell.fandom.com/wiki/Bloodline),
  [Shindo Life Wiki - Gamepasses](https://shindo-life-rell.fandom.com/wiki/Gamepasses),
  [Pro Game Guides - unlocking elements](https://progameguides.com/roblox/how-to-unlock-new-elements-in-roblox-shindo-life/),
  [BloxMeta beginner guide](https://bloxmeta.gg/games/shindo-life/guides/beginners-guide)

**Action item:** someone with normal web access should read the fandom pages above once and correct anything tagged [M].

---

## 1. Rudeus Greyrat - kit research

| Topic | Finding | Tag | How the data uses it |
|---|---|---|---|
| Origin | Born into the Greyrat family, Buena Village, Fittoa Region, Asura Kingdom, keeping the memories of his previous life. Trained his mana from infancy, which is why his capacity is so large. | [M] (setting is common knowledge) | `Characters.json` description; `MaxMana 1400`, `ManaRegen 30` (largest pool in the game). |
| Chantless casting | Rudeus casts without incantations. Silent casting is rare and lets a mage perform above their nominal rank (for comparison, some skilled mages can only shorten a chant, not skip it). | [S] Shapes magic-system page, fandom | Passive `ChantlessCasting`: `CastTimeMultiplier 0.55`, `ManaCostMultiplier 0.95`. Big spells keep a visible charge pose, so opponents can still read them. |
| Stone Cannon | His main attack spell. The Immortal Demon King Badigadi called its power Emperor-class. He shapes it like a bullet, hardens it and spins it for penetration. | Power level [S] fandom summary; bullet shape, hardening and spin [M] | Chargeable projectile. Charging to 1.6 s gives up to 1.8x speed, 2.2x damage and 2.5x stagger for 2x mana. |
| Quagmire | An **Earth + Water combination** that turns an area into mud and bogs down anything caught in it; "even a Red Dragon is unable to fly away once caught"; with enough mana it can swallow a town. It became his epithet ("Quagmire"). | [S] fandom Quagmire / Powers pages | Zone `Mire`. Our version slows instead of rooting (speed x0.55, acceleration x0.4, jump x0.5, dodge x0.5) so it stays fair in PvP and still works on CC-immune bosses. The canon trait we intentionally soften is full immobilisation. |
| Demon Eye of Foresight | Given by **Kishirika Kishirisu**, the former Demon Empress. It shows the future; more mana means further ahead. Summaries disagree on how far he reliably tunes it (one says about 10 s). | Gift and mechanism [S] fandom Demon Eyes, Game Rant; exact seconds unclear | `Rudeus_DemonEye`: 10 s buff that shows incoming attacks as predictive silhouettes. It gives no stat boost. |
| Magic ranks | Seven ranks: Beginner, Intermediate, Advanced, Saint, King, Emperor, God (matches `EMTMagicRank`). | [S] Classes page, Game Rant, Ranobeki | `RankRequirement` on element abilities. |
| Rudeus's ranks | He was certified **Water Saint at about age 5** by summoning a thunderstorm (the Saint-tier Water spell Cumulonimbus). Sources disagree on his later ranks: one claims Water Emperor and Saint in the other elements. | Water Saint [S]; later ranks unclear | Cumulonimbus is marked canon Saint-tier but gated at **Advanced** so the vertical slice can reach it. |
| Elemental Barrage | No canon source for this spell. It shows off how fast he can switch between elements. | GAMEPLAY ORIGINAL | Sequence of 4 projectiles, each with different handling. |

## 2. Orsted - research

| Topic | Finding | Tag |
|---|---|---|
| Title / lineage | The current **Dragon God**, son of the First Dragon God. Summaries also name his mother as Lunaria, daughter of the Human God. | [S] fandom Orsted / Gods |
| Curse | An innate curse makes nearly every living being instinctively fear and hate him. People who sense his mana become hostile. Even dragons tremble. | [S] fandom Curses / Orsted |
| Time loop | Bound to a loop that restarts his life roughly every 200 years until he kills Hitogami (the Human God). | [S] Orsted pages |
| Disturb Magic | **Devised by the Dragon God Urupen.** It sends out magic designed to counteract the target spell, so the spell scatters harmlessly before it forms. Orsted first uses it in his first fight with Rudeus. | [S] fandom Disturb Magic |
| Dragon God Style / Saint Dragon Battle Aura | 龍聖闘気 ("Dragon Saint Fighting Spirit") is the ultimate secret technique of the Dragon God Style created by Urupen. Orsted learned it from Urupen's writings. It gives extraordinary defence against most spells and sword techniques. The style is "the strongest unarmed fighting style that makes the least use of mana". | [S] fandom Battle Aura / Orsted Powers |
| **Mana recovery (checked as requested)** | **Confirmed: his mana regenerates extremely slowly.** Summaries say he may not refill his capacity even in 100 years, because of the magic circles on him and his loop, so he avoids spending magic and saves it for Hitogami. | [S] fandom Orsted Powers (search summary) |
| Feats | Killed two Red Dragons in the Red Dragon Mountains with one strike each. Beat a veteran Superd warrior in about 10 s using only his fists. | [S] Orsted Chronology / Villains wiki |
| Helmet | Later wears a **black helmet made by Cliff** that suppresses the curse. He keeps notes so he can rebuild it in future loops. | [S] Orsted Powers (spoiler; good late-game cosmetic) |
| Appearance | Silver hair; golden **sanpaku** eyes (large whites, overpowering stare) typical of the Dragon race; much taller than Rudeus; no visible weapon; white coat with fur. One summary says the coat is made from Ancient White Dragon scales. | Hair, eyes, height and coat [S]; coat material unverified |
| Height | **No source gave a number.** 195 cm is our production choice to make "towers over Rudeus" readable. | GAMEPLAY ORIGINAL proportion |

**Mana regen decision.** Orsted: `MaxMana 600`, `ManaRegen 10`. Rudeus: 1400 and 30. The canon value is "almost
none", which would make the kit unplayable. So his kit spends mostly **stamina** and no mana on its core moves (Palm
Strike and Dragon Step cost 0 mana). Only Disturb Magic (45) and the Saint Dragon Battle Aura (60) spend mana. Using
both on cooldown costs about 9.5 mana/s against 10/s regen, so every whiffed counter hurts. The character description
and passive text both state this handicap.

## 3. Races

| Race | Canon facts used | Tag | Passive canon status | Transformation canon status |
|---|---|---|---|---|
| Human | Most numerous race, no innate racial power. Strength comes from training (battle aura, swordsmanship, magic). | [M] | GAMEPLAY ORIGINAL | GAMEPLAY ORIGINAL |
| Migurd | Demon race closest to humans. Blue hair. Teenage appearance until about 150 of their ~200 years. **Telepathy** between kin at close range. | [S] fandom Migurd | CANON (telepathy) | GAMEPLAY ORIGINAL |
| Superd | Pale skin, green hair, **red forehead jewel (third eye)** that detects living beings and the flow of mana. Laplace's **Curse of Fear**, tied to their spears, made the world fear them. Fearsome warrior reputation. | [S] fandom Ruijerd / Curses | CANON (third eye) | GAMEPLAY ORIGINAL |
| Beast | Great Forest beastfolk. The Doldia tribe descends from the Beast God Giger and has feline (Dedoldia) and canine (Adoldia) branches. Heightened hearing, sight and smell. **Howling** voice magic (echolocation or a stun). | [S] fandom Races / Ghislaine | CANON (senses) | GAMEPLAY ORIGINAL |
| Elf | Long-eared tribe of the southern Great Forest (Millis). Skilled archers and magicians, especially in water, earth and wind. Said to be the first users of magic. Very long lived. | [S] fandom Races (search summary) | CANON | GAMEPLAY ORIGINAL |
| Dwarf | Smiths and craftsmen at the foot of the Blue Dragon Mountains (Millis). Dexterous, with an affinity for earth and fire. Sometimes classed as a demon race because they arose in the Age of Chaos. | [S] fandom Dwarf | GAMEPLAY ORIGINAL (resistance numbers) | GAMEPLAY ORIGINAL |
| Dragon Tribe | Scaled humanoids of the Dragon World, ruled by the First Dragon God, who taught them **Dragon Battle Aura**. The Five Dragon Generals were their strongest. | [S] fandom Dragon Tribe | CANON (scales) | **CANON** (Dragon Battle Aura) |
| Immortal Demon | Descendants of the First Demon God. Males are jet-black and six-armed. Demon Eyes. **Regenerate after destruction**, temporarily shrinking; Badigadi the Immortal is the example. | [S] fandom Badigadi / Races | CANON (regeneration flavour; the regen itself needs a code hook, see open items) | GAMEPLAY ORIGINAL |
| Ogre | Large, immensely powerful humanoids of Ogre Island (Biheiril Kingdom). A civilised culture with a history of war with the sea folk. | [S] fandom Ogre race | CANON (strength) | GAMEPLAY ORIGINAL |
| Sea Race | Ocean race, original inhabitants of the Ocean World. They rule the seas and speak the Sea God tongue. | [S] fandom Races | GAMEPLAY ORIGINAL | GAMEPLAY ORIGINAL |

Only Human, Migurd and Beast are `bImplemented` and rollable (the framework proof). The other 7 races ship with
complete data, including rows for their active and transformation abilities.

## 4. Monsters and the Fittoa setting

- **Goblins are canon**: E-ranked magic beasts, "almost a rat-like existence in the Human World", found in forests and plains. There is also the OVA *Eris the Goblin Slayer*. [S] fandom Monsters. Our goblins are weak and `bIsWeak`.
- **Wolves / alpha wolf**: generic wildlife. No specific canon monster is claimed. GAMEPLAY ORIGINAL framing.
- **Bandits and hedge-mages**: generic humans. Their long chanted cast (0.8 s) reflects canon: ordinary mages chant.
- **Red Dragons / "Red Wyrms"**: canon. They live in the Red Dragon Mountains; the "Red Dragon Upper Jaw" and "Red Dragon Lower Jaw" are named places there. Orsted killed two with one strike each. Quagmire can trap one. [S]
  **A juvenile red wyrm straying south into the Fittoa foothills is a GAMEPLAY ORIGINAL encounter**, and so is the location `Fittoa_Wyrm_Foothills`. A rank-D quest for a red dragon is also generous compared with canon, where they are far above D-rank adventurers. Fire breath is plausible but unverified for canon red dragons.
- Quest NPCs use only the approved ids (Paul, Zenith, Lilia, villagers, guild). No roll-able heroine or hero appears in quests; the validator enforces this.

## 5. CANON vs GAMEPLAY ORIGINAL - every ability

Criterion: **CANON** means the technique or trait exists in the source material. All numbers, timings and hit shapes
are still our adaptation. **GAMEPLAY ORIGINAL** means it was invented for this game, even when it is inspired by canon.

| AbilityID | Name | Owner | Behavior | Status | Notes |
|---|---|---|---|---|---|
| `Rudeus_Basic` | Stone Bullet | Rudeus | Projectile | CANON | Light, fast variant of his canon Stone Cannon |
| `Rudeus_StoneCannon` | Stone Cannon | Rudeus | Projectile | CANON | Signature spell; the charge mechanic is ours |
| `Rudeus_Quagmire` | Quagmire | Rudeus | Zone | CANON | Earth + Water mud field; ours slows and never roots |
| `Rudeus_ElementalBarrage` | Elemental Barrage | Rudeus | Sequence | GAMEPLAY ORIGINAL | Shows off his multi-element chantless casting |
| `Barrage_Stone` | Barrage: Stone Slug | Rudeus | Projectile | GAMEPLAY ORIGINAL | Barrage step |
| `Barrage_WindBlade` | Barrage: Wind Blade | Rudeus | Projectile | GAMEPLAY ORIGINAL | Barrage step |
| `Barrage_WaterCannon` | Barrage: Water Cannon | Rudeus | Projectile | GAMEPLAY ORIGINAL | Barrage step |
| `Barrage_FireBurst` | Barrage: Fire Burst | Rudeus | Projectile | GAMEPLAY ORIGINAL | Barrage step |
| `Rudeus_DemonEye` | Demon Eye of Foresight | Rudeus | Buff | CANON | Demon Eye of Foresight, a gift from Kishirika |
| `Rudeus_Awakening_QuagmireMagician` | Awakening: The Quagmire Magician | Rudeus | Buff | GAMEPLAY ORIGINAL | Named after his canon epithet 'Quagmire' |
| `Rudeus_StoneCannon_Awakened` | Stone Cannon (Awakened) | Rudeus | Projectile | GAMEPLAY ORIGINAL | Empowered variant of a canon spell |
| `Rudeus_Quagmire_Awakened` | Quagmire (Awakened) | Rudeus | Zone | GAMEPLAY ORIGINAL | Empowered variant of a canon spell |
| `Orsted_Basic` | Dragon God Style: Palm Strike | Orsted | Melee | CANON | Dragon God Style unarmed fighting (the specific strike is ours) |
| `Orsted_DisturbMagic` | Disturb Magic | Orsted | Counter | CANON | Devised by Urupen; scatters spells before they form |
| `Orsted_DragonStep` | Dragon Step | Orsted | Dash | GAMEPLAY ORIGINAL | Represents his footwork; not a canon named move |
| `Orsted_DragonStep_Awakened` | Dragon Step (Awakened) | Orsted | Dash | GAMEPLAY ORIGINAL | Awakening override |
| `Orsted_SaintDragonAura` | Saint Dragon Battle Aura | Orsted | Buff | CANON | Saint Dragon Battle Aura, the ultimate Dragon God Style technique |
| `Orsted_Awakening_DragonGod` | Awakening: Dragon God | Orsted | Buff | GAMEPLAY ORIGINAL | Fighting at full power as the Dragon God |
| `Fire_FireBurst` | Fire Burst | Element: Fire | Projectile | GAMEPLAY ORIGINAL | Adapted from canon beginner fire projectiles |
| `Fire_FlameField` | Flame Field | Element: Fire | Zone | GAMEPLAY ORIGINAL |  |
| `Fire_InfernoCompression` | Inferno Compression | Element: Fire | Projectile | GAMEPLAY ORIGINAL |  |
| `Water_WaterCannon` | Water Cannon | Element: Water | Projectile | GAMEPLAY ORIGINAL | Canon has water projectiles; this exact name is unverified |
| `Water_FrostPrison` | Frost Prison | Element: Water | Zone | GAMEPLAY ORIGINAL |  |
| `Water_Cumulonimbus` | Cumulonimbus | Element: Water | Zone | CANON | Saint-tier Water spell (Rudeus's Water Saint certification); gated at Advanced here |
| `Earth_StoneCannon` | Stone Cannon | Element: Earth | Projectile | CANON | Element-school version, weaker than Rudeus's |
| `Earth_Quagmire` | Quagmire | Element: Earth | Zone | CANON | Canon needs Earth + Water; our slot check requires only Earth |
| `Earth_EarthFortress` | Earth Fortress | Element: Earth | Structure | GAMEPLAY ORIGINAL | Consistent with canon earth walls |
| `Wind_AirBlade` | Air Blade | Element: Wind | Projectile | GAMEPLAY ORIGINAL |  |
| `Wind_GaleStep` | Gale Step | Element: Wind | Dash | GAMEPLAY ORIGINAL |  |
| `Wind_TempestDomain` | Tempest Domain | Element: Wind | Zone | GAMEPLAY ORIGINAL |  |
| `Race_Human_SecondWind` | Second Wind | Race: Human | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Human_LimitBreak` | Limit Break | Race: Human | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Migurd_ResonancePulse` | Resonance Pulse | Race: Migurd | Buff | GAMEPLAY ORIGINAL | Inspired by canon Migurd telepathy |
| `Race_Migurd_ManaResonance` | Mana Resonance | Race: Migurd | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Beast_HuntersDash` | Hunter's Dash | Race: Beast | Dash | GAMEPLAY ORIGINAL | Inspired by canon beastfolk agility |
| `Race_Beast_BeastInstinct` | Beast Instinct | Race: Beast | Buff | GAMEPLAY ORIGINAL | Inspired by canon heightened senses |
| `Race_Superd_ThirdEyeSense` | Third Eye Sense | Race: Superd | Buff | CANON | The forehead jewel perceives living beings and mana (active reveal is our adaptation) |
| `Race_Superd_WarriorState` | Warrior State | Race: Superd | Buff | GAMEPLAY ORIGINAL | Inspired by the Superd warrior reputation |
| `Race_Elf_SpiritArrow` | Spirit Arrow | Race: Elf | Projectile | GAMEPLAY ORIGINAL | Inspired by canon elven archery and magic |
| `Race_Elf_AncientMana` | Ancient Mana | Race: Elf | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Dwarf_Stonehide` | Stonehide | Race: Dwarf | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Dwarf_ForgeHeart` | Forge Heart | Race: Dwarf | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Dragon_ScaleGuard` | Scale Guard | Race: DragonTribe | Buff | GAMEPLAY ORIGINAL | Inspired by canon scales |
| `Race_Dragon_Aura` | Dragon Battle Aura | Race: DragonTribe | Buff | CANON | Dragon Battle Aura, taught by the First Dragon God |
| `Race_Immortal_Reassemble` | Reassemble | Race: ImmortalDemon | Buff | GAMEPLAY ORIGINAL | Inspired by canon regeneration (Badigadi) |
| `Race_Immortal_State` | Immortal State | Race: ImmortalDemon | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Ogre_GroundBreaker` | Ground Breaker | Race: Ogre | Melee | GAMEPLAY ORIGINAL |  |
| `Race_Ogre_Champion` | Ogre Champion | Race: Ogre | Buff | GAMEPLAY ORIGINAL |  |
| `Race_Sea_TideRush` | Tide Rush | Race: SeaRace | Dash | GAMEPLAY ORIGINAL |  |
| `Race_Sea_OceanBlessing` | Ocean Blessing | Race: SeaRace | Buff | GAMEPLAY ORIGINAL |  |
| `Wolf_Bite` | Bite | Enemy | Melee | GAMEPLAY ORIGINAL | Generic creature attack |
| `Wolf_Lunge` | Lunge | Enemy | Dash | GAMEPLAY ORIGINAL | Generic creature attack |
| `Goblin_Club` | Club Swing | Enemy | Melee | GAMEPLAY ORIGINAL | Generic creature attack |
| `Goblin_RockThrow` | Rock Throw | Enemy | Projectile | GAMEPLAY ORIGINAL | Generic creature attack |
| `Bandit_Slash` | Sword Slash | Enemy | Melee | GAMEPLAY ORIGINAL | Generic creature attack |
| `BanditMage_FireBolt` | Fire Bolt | Enemy | Projectile | GAMEPLAY ORIGINAL | Generic creature attack |
| `WolfAlpha_Howl` | Alpha Howl | Enemy | Buff | GAMEPLAY ORIGINAL | Generic creature attack |
| `WolfAlpha_Pounce` | Pounce | Enemy | Dash | GAMEPLAY ORIGINAL | Generic creature attack |
| `Wyrm_Claw` | Claw Rake | Enemy | Melee | GAMEPLAY ORIGINAL | Generic creature attack |
| `Wyrm_TailSweep` | Tail Sweep | Enemy | Melee | GAMEPLAY ORIGINAL | Generic creature attack |
| `Wyrm_FireBreath` | Fire Breath | Enemy | Zone | GAMEPLAY ORIGINAL | Generic creature attack |
| `Wyrm_Dive` | Wing Dive | Enemy | Dash | GAMEPLAY ORIGINAL | Generic creature attack |

## 6. Shindo Life - system inspiration only

What we took from the *structure* of RELL's Roblox game Shindo Life:

- **Bloodline vs element separation.** Shindo Life has Bloodlines (eye, clan and elemental) that are separate from elements. [S] Shindo wiki Bloodline. Our equivalent is Character lineage (Rudeus/Orsted), Element and Race as three independent roll categories (`EMTRollCategory`).
- **Multiple element slots.** Shindo players start with element and bloodline slots and unlock more. [S] Pro Game Guides / wiki. We use **two element slots**: slot B unlocks by level (`UMTProgressionSubsystem`).
- **Spins earned from play.** Shindo spins come from missions, game modes and codes. [S] wiki. **Ours come only from gameplay**, as quest and world-event rewards (`CharacterSpins`, `ElementSpins`, `RaceSpins`). They are never sold.
- **Missions as the progression loop.** This maps to our quest board (Basic, Combat, Elite, Boss, Story, WorldEvent).

**We do not copy Shindo Life's UI, names, art, abilities, content, codes or monetisation.** The lore, kits, balance,
pity model (soft/hard pity, a guaranteed Mythic within 60 character spins, duplicate protection) and all text are our
own and built around Mushoku Tensei.

## 7. Roll tuning notes (`RollConfigs.json`)

- **Character**: `Legendary 0.94 / Mythic 0.06`. The pool is only Rudeus (Legendary) and Orsted (Mythic), so every spin is Legendary or better. **`HardPity 40` and the soft pity (start 20, +0.03 per spin) can never trigger for Legendary** in the current pool. They only matter once the pool grows. `MythicHardPity 60` guarantees Orsted within 60 spins. Without pity, the chance of no Orsted in 59 spins is 0.94^59, about 2.6%.
- **Element**: `Common 0.6` (Fire, Water) and `Uncommon 0.4` (Earth, Wind). Pity is effectively disabled (1000).
- **Race**: weights exist only for tiers that contain an **implemented** race: `Common 0.5` (Human) and `Uncommon 0.5` (Migurd, Beast). This keeps the roller from landing on an empty tier. The validator rejects weights on empty tiers. Planned weights once all 10 races ship: Common 0.45 / Uncommon 0.33 / Rare 0.15 / Legendary 0.06 / Mythic 0.01, with pity 50/0.02/90/200.

## 8. Balance notes

- **Rarity does not guarantee a win.** Orsted has the most HP (1500) and poise (180) plus the aura, but only 600 mana at 10/s, no ranged option and no Special slot. Rudeus has an enormous mana pool and ranged pressure, but 900 HP and 70 poise: close the distance and he is staggered easily.
- Rudeus's uncharged Stone Cannon costs about 26.7 mana/s if spammed on cooldown, below his 30/s regen. Adding Quagmire and Barrage on cooldown drains about 12 mana/s net, which empties the pool in roughly 2 minutes.
- **Watch item:** Saint Dragon Battle Aura (stagger resistance 0.5) stacked with the Dragon God awakening (0.4) gives **0.9 stagger resistance** for up to 12 s. Damage resistance is clamped at 0.9 by design, but stagger resistance has no stated clamp. If playtests show Orsted cannot be staggered, clamp stagger resistance at 0.75 in code or lower the awakening's value to 0.25.
- Dragon Step Awakened is 1100 cm. With the awakening's `DashMultiplier 1.2` it effectively travels **1320 cm**. This is intended but worth knowing.

## 9. Open items and canon uncertainties

1. All fandom pages were read only through search summaries. The items tagged [M] need a human check: Stone Cannon spin/hardening, Human racial details, and Rudeus's later ranks.
2. Orsted's exact height is not stated in any source we reached. 195 cm is our choice.
3. What the coat is made of (Ancient White Dragon?) is reported by one summary only.
4. Demon Eye look-ahead: summaries disagree on how far Rudeus reliably sees (one says about 10 s). Our mechanic is abstract.
5. Cumulonimbus is canon Saint-tier but gated at Advanced for playability.
6. The Immortal Demon passive regeneration and the Human Second Wind stamina/poise refill need code hooks. `FMTStatModifier` has no regen fields; the data states this.
7. The UseAbility objectives in `Q_Story_ChantlessLessons` and `Q_Buena_RunawayCart` name `Rudeus_StoneCannon` and `Rudeus_Quagmire` (as specified). A player on Orsted cannot complete them unless the quest code also accepts `Earth_StoneCannon` / `Earth_Quagmire`, or the quest is gated to the Rudeus lineage.
