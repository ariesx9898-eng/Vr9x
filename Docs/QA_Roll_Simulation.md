# QA: Roll simulation

`Tools/sim_roll.py` mirrors `UMTRollSubsystem::Roll` step by step. It uses the same pool ordering, tier renormalization, soft and hard pity, Mythic hard pity, duplicate protection and pity updates. The Character pool uses the **shipped** config from `Content/Data/RollConfigs.json`, so the result matches `GetEffectiveConfig`. Only the random number generator differs (C++ uses `FRandomStream`), so the statistics transfer.

Reproduce: `python3 Tools/sim_roll.py --spins 200000 --seed 12345 --trials 20000`

## Findings (shipped data)

- **Character pool = Rudeus (Legendary) + Orsted (Mythic) only.** The pool has no other characters, as the scope rule requires.
- **Orsted: 6.0% displayed, 6.12% observed per spin.** The Mythic hard pity **guarantees Orsted within 60 spins**. Over 20,000 fresh accounts the first Orsted takes 16.3 spins on average (median 12, p99 60, max 60). That's rare, but spins come only from gameplay (quests, bosses, level-ups, rank-ups), so it's reachable without grinding for weeks.
- **Hard pity is never exceeded** in any scenario, including the stress test with soft pity disabled.
- Rudeus is owned from the start, so a Legendary result is always a duplicate. Duplicate protection has nothing unowned left in that tier, so it pays compensation (150 gold + 20 Rudeus mastery XP) instead of nothing.
- The displayed odds (`GetPool`) are base rates before pity. In the character pool they match the observed rates within noise.
- **Design note:** rarity does not grant automatic victory. Orsted trades Rudeus's huge mana pool and ranged pressure for timing-based defense, and he has very low mana regeneration (canon). See `Docs/Lore_Research.md`.

## Raw output

# Roll simulation output (Tools/sim_roll.py)

## 1. Character pool (current data: Rudeus Legendary, Orsted Mythic; config from RollConfigs.json)

Config: SoftPityStart=20, SoftPityStep=0.03, HardPity=40, MythicHardPity=60, DuplicateProtection=True, weights={'Common': 50.0, 'Uncommon': 30.0, 'Rare': 14.0, 'Legendary': 0.94, 'Mythic': 0.06}
Pool: Orsted (Mythic), Rudeus (Legendary); initially owned: ['Rudeus']

Displayed odds (GetPool, base rates before pity):

| Entry | Rarity | Displayed odds |
|---|---|---|
| Orsted | Mythic | 6.000% |
| Rudeus | Legendary | 94.000% |

Simulated 200,000 spins on one account (seed 12345):

| Rarity | Count | Observed rate |
|---|---|---|
| Legendary | 187,764 | 93.882% |
| Mythic | 12,236 | 6.118% |

| Entry | Count | Observed rate |
|---|---|---|
| Orsted | 12,236 | 6.118% |
| Rudeus | 187,764 | 93.882% |

- Legendary+ results: 200,000; effective Legendary+ rate 100.000%
- Longest run of consecutive spins WITHOUT a Legendary+: 0 (bound: HardPity - 1 = 39)
- Max spins needed to reach a Legendary+: 1 (hard pity bound: 40); mean 1.00
- Max spins needed to reach a Mythic: 60 (Mythic hard pity bound: 60); mean 16.34
- Hard pity (Legendary+ or Mythic guarantee) decided the tier on 322 spins (0.161%)
- Duplicate-protection re-rolls: 0; new unlocks: 1; duplicate compensation paid: 29,999,850 gold, 3,999,980 mastery XP
- First Mythic on this account at spin 8
- HARD PITY NEVER EXCEEDED: PASS (Legendary+), PASS (Mythic)
- Average spins to FIRST Mythic over 20,000 fresh accounts: 16.28 (median 12, p99 60, max 60)

## 2. Illustrative mixed pool (all tiers, default config)

Config: SoftPityStart=40, SoftPityStep=0.02, HardPity=70, MythicHardPity=160, DuplicateProtection=True, weights={'Common': 50.0, 'Uncommon': 30.0, 'Rare': 14.0, 'Legendary': 5.0, 'Mythic': 1.0}
Pool: MythicA (Mythic), LegendaryA (Legendary), LegendaryB (Legendary), RareA (Rare), RareB (Rare), UncommonA (Uncommon), CommonA (Common), CommonB (Common); initially owned: none

Displayed odds (GetPool, base rates before pity):

| Entry | Rarity | Displayed odds |
|---|---|---|
| MythicA | Mythic | 1.000% |
| LegendaryA | Legendary | 2.500% |
| LegendaryB | Legendary | 2.500% |
| RareA | Rare | 7.000% |
| RareB | Rare | 7.000% |
| UncommonA | Uncommon | 30.000% |
| CommonA | Common | 25.000% |
| CommonB | Common | 25.000% |

Simulated 200,000 spins on one account (seed 12445):

| Rarity | Count | Observed rate |
|---|---|---|
| Common | 99,284 | 49.642% |
| Uncommon | 59,821 | 29.911% |
| Rare | 27,783 | 13.892% |
| Legendary | 10,512 | 5.256% |
| Mythic | 2,600 | 1.300% |

| Entry | Count | Observed rate |
|---|---|---|
| MythicA | 2,600 | 1.300% |
| LegendaryA | 5,251 | 2.626% |
| LegendaryB | 5,261 | 2.631% |
| RareA | 13,952 | 6.976% |
| RareB | 13,831 | 6.915% |
| UncommonA | 59,821 | 29.911% |
| CommonA | 49,369 | 24.684% |
| CommonB | 49,915 | 24.957% |

- Legendary+ results: 13,112; effective Legendary+ rate 6.556%
- Longest run of consecutive spins WITHOUT a Legendary+: 61 (bound: HardPity - 1 = 69)
- Max spins needed to reach a Legendary+: 62 (hard pity bound: 70); mean 15.25
- Max spins needed to reach a Mythic: 160 (Mythic hard pity bound: 160); mean 76.90
- Hard pity (Legendary+ or Mythic guarantee) decided the tier on 466 spins (0.233%)
- Duplicate-protection re-rolls: 1; new unlocks: 8; duplicate compensation paid: 29,998,800 gold, 3,999,840 mastery XP
- First Mythic on this account at spin 160
- HARD PITY NEVER EXCEEDED: PASS (Legendary+), PASS (Mythic)
- Average spins to FIRST Mythic over 20,000 fresh accounts: 77.87 (median 67, p99 160, max 160)

## 3. Hard-pity stress test (soft pity off, Legendary weight 0.2)

Config: SoftPityStart=40, SoftPityStep=0.0, HardPity=70, MythicHardPity=160, DuplicateProtection=True, weights={'Common': 50.0, 'Uncommon': 30.0, 'Rare': 14.0, 'Legendary': 0.2, 'Mythic': 0.0}
Pool: LegendaryA (Legendary), RareA (Rare), UncommonA (Uncommon), CommonA (Common); initially owned: none

Displayed odds (GetPool, base rates before pity):

| Entry | Rarity | Displayed odds |
|---|---|---|
| LegendaryA | Legendary | 0.212% |
| RareA | Rare | 14.862% |
| UncommonA | Uncommon | 31.847% |
| CommonA | Common | 53.079% |

Simulated 200,000 spins on one account (seed 12545):

| Rarity | Count | Observed rate |
|---|---|---|
| Common | 104,604 | 52.302% |
| Uncommon | 62,837 | 31.418% |
| Rare | 29,485 | 14.742% |
| Legendary | 3,074 | 1.537% |

| Entry | Count | Observed rate |
|---|---|---|
| LegendaryA | 3,074 | 1.537% |
| RareA | 29,485 | 14.742% |
| UncommonA | 62,837 | 31.418% |
| CommonA | 104,604 | 52.302% |

- Legendary+ results: 3,074; effective Legendary+ rate 1.537%
- Longest run of consecutive spins WITHOUT a Legendary+: 69 (bound: HardPity - 1 = 69)
- Max spins needed to reach a Legendary+: 70 (hard pity bound: 70); mean 65.04
- Hard pity (Legendary+ or Mythic guarantee) decided the tier on 2,656 spins (1.328%)
- Duplicate-protection re-rolls: 0; new unlocks: 4; duplicate compensation paid: 29,999,400 gold, 3,999,920 mastery XP
- HARD PITY NEVER EXCEEDED: PASS (Legendary+), PASS (Mythic)

OVERALL: PASS
