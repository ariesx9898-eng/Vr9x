#!/usr/bin/env python3
"""
Pure-Python mirror of UMTRollSubsystem::Roll (Source/MushokuRPG/Private/Progression/MTRollSubsystem.cpp).

Keep this file in sync with the C++ implementation. It mirrors, step for step:
  1. Pool order: rarity (highest first), then id (case-insensitive).
  2. Base tier probabilities: only rarity tiers that have entries participate (renormalised);
     all-zero weights fall back to uniform over present tiers.
  3. Tier selection, in priority order:
       a. Mythic hard pity : pool has Mythic and SpinsSinceMythic + 1 >= MythicHardPity -> Mythic.
       b. Legendary+ hard pity: pool has Legendary/Mythic and SpinsSinceLegendary + 1 >= HardPity
          -> Legendary or Mythic (split by base weights; one random draw only if both tiers exist).
       c. Otherwise soft pity: when SpinsSinceLegendary >= SoftPityStart the Legendary+ share becomes
          min(1, base + (SpinsSinceLegendary - SoftPityStart + 1) * SoftPityStep), lower tiers are
          scaled down proportionally; one random draw picks the tier.
  4. Entry inside the tier weighted by RollWeight (one random draw).
  5. Duplicate protection: if the pick is owned, re-roll once among UNOWNED entries of the same tier
     (one random draw) when any exist; otherwise keep the duplicate and pay compensation
     (150 gold + 20 mastery XP).
  6. Pity update: SpinsSinceLegendary = 0 on Legendary+, else +1; SpinsSinceMythic = 0 on Mythic, else +1.

Only the random number generator differs (C++ uses FRandomStream); the statistics are what matter.

Usage: python3 Tools/sim_roll.py [--spins 200000] [--seed 12345] [--trials 20000] [--markdown out.md]
"""

import argparse
import random
import sys

RARITIES = ["Common", "Uncommon", "Rare", "Legendary", "Mythic"]
COMMON, UNCOMMON, RARE, LEGENDARY, MYTHIC = range(5)
DEFAULT_WEIGHTS = [50.0, 30.0, 14.0, 5.0, 1.0]  # code defaults in MTRollSubsystem.cpp

DUPLICATE_GOLD = 150
DUPLICATE_MASTERY_XP = 20.0
KINDA_SMALL_NUMBER = 1.0e-4


class RollConfig:
    """Mirror of FMTRollConfig defaults (MTDataTypes.h) + code-default weights."""

    def __init__(self, soft_pity_start=40, soft_pity_step=0.02, hard_pity=70, mythic_hard_pity=160,
                 duplicate_protection=True, weights=None):
        self.soft_pity_start = max(0, soft_pity_start)
        self.soft_pity_step = max(0.0, soft_pity_step)
        self.hard_pity = max(1, hard_pity)
        self.mythic_hard_pity = max(1, mythic_hard_pity)
        self.duplicate_protection = duplicate_protection
        self.weights = list(weights) if weights is not None else list(DEFAULT_WEIGHTS)


class Entry:
    def __init__(self, entry_id, rarity, weight=1.0):
        self.id = entry_id
        self.rarity = rarity
        self.weight = weight


class Account:
    def __init__(self, owned=()):
        self.owned = set(owned)
        self.since_legendary = 0
        self.since_mythic = 0
        self.total_spins = 0
        self.gold = 0
        self.mastery_xp = 0.0


class Result:
    __slots__ = ("id", "rarity", "pity", "rerolled", "new")

    def __init__(self, entry_id, rarity, pity, rerolled, new):
        self.id = entry_id
        self.rarity = rarity
        self.pity = pity
        self.rerolled = rerolled
        self.new = new


def sort_pool(pool):
    return sorted(pool, key=lambda e: (-e.rarity, e.id.lower()))


def present_tiers(pool):
    present = [False] * 5
    for e in pool:
        present[e.rarity] = True
    return present


def base_tier_probabilities(pool, cfg):
    present = present_tiers(pool)
    prob = [0.0] * 5
    total = 0.0
    count = 0
    for i in range(5):
        if not present[i]:
            continue
        count += 1
        prob[i] = max(0.0, cfg.weights[i])
        total += prob[i]
    if count == 0:
        return prob
    for i in range(5):
        if present[i]:
            prob[i] = prob[i] / total if total > 0.0 else 1.0 / count
    return prob


def apply_soft_pity(cfg, since_legendary, prob):
    if since_legendary < cfg.soft_pity_start or cfg.soft_pity_step <= 0.0:
        return prob
    high = prob[LEGENDARY] + prob[MYTHIC]
    low = 1.0 - high
    if high <= 0.0 or low <= KINDA_SMALL_NUMBER:
        return prob
    bonus = (since_legendary - cfg.soft_pity_start + 1) * cfg.soft_pity_step
    new_high = min(1.0, high + bonus)
    high_scale = new_high / high
    low_scale = (1.0 - new_high) / low
    return [p * (high_scale if i >= LEGENDARY else low_scale) for i, p in enumerate(prob)]


def pick_index_by_weight(weights, u):
    total = 0.0
    last_positive = -1
    for i, w in enumerate(weights):
        if w > 0.0:
            total += w
            last_positive = i
    if last_positive < 0:
        return -1
    target = u * total
    acc = 0.0
    for i, w in enumerate(weights):
        if w <= 0.0:
            continue
        acc += w
        if target < acc:
            return i
    return last_positive


def roll(pool, cfg, acct, rng):
    """One spin. `pool` must already be sorted with sort_pool()."""
    present = present_tiers(pool)
    has_high = present[LEGENDARY] or present[MYTHIC]
    has_mythic = present[MYTHIC]
    prob = base_tier_probabilities(pool, cfg)

    pity = False
    if has_mythic and acct.since_mythic + 1 >= cfg.mythic_hard_pity:
        tier = MYTHIC
        pity = True
    elif has_high and acct.since_legendary + 1 >= cfg.hard_pity:
        pity = True
        if not present[MYTHIC]:
            tier = LEGENDARY
        elif not present[LEGENDARY]:
            tier = MYTHIC
        else:
            wl, wm = prob[LEGENDARY], prob[MYTHIC]
            u = rng.random()
            if wl + wm > 0.0:
                tier = MYTHIC if u * (wl + wm) < wm else LEGENDARY
            else:
                tier = MYTHIC if u < 0.5 else LEGENDARY
    else:
        prob = apply_soft_pity(cfg, acct.since_legendary, prob)
        tier = pick_index_by_weight(prob, rng.random())
    if tier < 0:
        tier = max(i for i in range(5) if present[i])

    tier_entries = [i for i, e in enumerate(pool) if e.rarity == tier]
    tier_weights = [max(0.0, pool[i].weight) for i in tier_entries]
    pick = pick_index_by_weight(tier_weights, rng.random())
    index = tier_entries[pick if pick >= 0 else 0]

    rerolled = False
    if cfg.duplicate_protection and pool[index].id in acct.owned:
        unowned = [i for i in tier_entries if pool[i].id not in acct.owned]
        if unowned:
            pick = pick_index_by_weight([max(0.0, pool[i].weight) for i in unowned], rng.random())
            index = unowned[pick if pick >= 0 else 0]
            rerolled = True
    entry = pool[index]

    acct.total_spins += 1
    acct.since_legendary = 0 if tier >= LEGENDARY else acct.since_legendary + 1
    acct.since_mythic = 0 if tier == MYTHIC else acct.since_mythic + 1

    new = entry.id not in acct.owned
    if new:
        acct.owned.add(entry.id)
    else:
        acct.gold += DUPLICATE_GOLD
        acct.mastery_xp += DUPLICATE_MASTERY_XP
    return Result(entry.id, tier, pity, rerolled, new)


def base_entry_odds(pool, cfg):
    """Mirror of UMTRollSubsystem::GetPool displayed odds."""
    tier_prob = base_tier_probabilities(pool, cfg)
    sums = [0.0] * 5
    counts = [0] * 5
    for e in pool:
        sums[e.rarity] += max(0.0, e.weight)
        counts[e.rarity] += 1
    out = []
    for e in pool:
        in_tier = max(0.0, e.weight) / sums[e.rarity] if sums[e.rarity] > 0 else 1.0 / max(1, counts[e.rarity])
        out.append((e.id, RARITIES[e.rarity], tier_prob[e.rarity] * in_tier))
    return out


def run_long(pool, cfg, owned, spins, seed):
    """One account spinning `spins` times. Tracks rates, pity gaps and hard-pity bounds."""
    rng = random.Random(seed)
    acct = Account(owned)
    rarity_counts = [0] * 5
    entry_counts = {}
    gaps_high = []      # spins needed to reach each Legendary+ (1..HardPity)
    gaps_mythic = []    # spins needed to reach each Mythic (1..MythicHardPity)
    run_no_high = 0
    max_run_no_high = 0
    since_high = 0
    since_mythic = 0
    pity_count = 0
    reroll_count = 0
    new_count = 0
    first_mythic = None
    for n in range(1, spins + 1):
        r = roll(pool, cfg, acct, rng)
        rarity_counts[r.rarity] += 1
        entry_counts[r.id] = entry_counts.get(r.id, 0) + 1
        pity_count += r.pity
        reroll_count += r.rerolled
        new_count += r.new
        since_high += 1
        since_mythic += 1
        if r.rarity >= LEGENDARY:
            gaps_high.append(since_high)
            since_high = 0
            run_no_high = 0
        else:
            run_no_high += 1
            max_run_no_high = max(max_run_no_high, run_no_high)
        if r.rarity == MYTHIC:
            gaps_mythic.append(since_mythic)
            since_mythic = 0
            if first_mythic is None:
                first_mythic = n
    return {
        "spins": spins,
        "rarity_counts": rarity_counts,
        "entry_counts": entry_counts,
        "gaps_high": gaps_high,
        "gaps_mythic": gaps_mythic,
        "max_run_no_high": max_run_no_high,
        "pity_count": pity_count,
        "reroll_count": reroll_count,
        "new_count": new_count,
        "gold": acct.gold,
        "mastery_xp": acct.mastery_xp,
        "first_mythic": first_mythic,
    }


def spins_to_first_mythic(pool, cfg, owned, trials, seed):
    rng = random.Random(seed)
    results = []
    for _ in range(trials):
        acct = Account(owned)
        n = 0
        while True:
            n += 1
            if roll(pool, cfg, acct, rng).rarity == MYTHIC:
                break
        results.append(n)
    return results


def report(title, pool, cfg, owned, spins, seed, trials, out):
    pool = sort_pool(pool)
    out.append(f"## {title}")
    out.append("")
    out.append(f"Config: SoftPityStart={cfg.soft_pity_start}, SoftPityStep={cfg.soft_pity_step}, HardPity={cfg.hard_pity}, "
               f"MythicHardPity={cfg.mythic_hard_pity}, DuplicateProtection={cfg.duplicate_protection}, "
               f"weights={dict(zip(RARITIES, cfg.weights))}")
    out.append(f"Pool: {', '.join(f'{e.id} ({RARITIES[e.rarity]})' for e in pool)}; initially owned: {sorted(owned) or 'none'}")
    out.append("")
    out.append("Displayed odds (GetPool, base rates before pity):")
    out.append("")
    out.append("| Entry | Rarity | Displayed odds |")
    out.append("|---|---|---|")
    for entry_id, rarity, p in base_entry_odds(pool, cfg):
        out.append(f"| {entry_id} | {rarity} | {p * 100:.3f}% |")
    out.append("")

    s = run_long(pool, cfg, owned, spins, seed)
    out.append(f"Simulated {spins:,} spins on one account (seed {seed}):")
    out.append("")
    out.append("| Rarity | Count | Observed rate |")
    out.append("|---|---|---|")
    for i, name in enumerate(RARITIES):
        if present_tiers(pool)[i]:
            out.append(f"| {name} | {s['rarity_counts'][i]:,} | {s['rarity_counts'][i] / spins * 100:.3f}% |")
    out.append("")
    out.append("| Entry | Count | Observed rate |")
    out.append("|---|---|---|")
    for e in pool:
        c = s["entry_counts"].get(e.id, 0)
        out.append(f"| {e.id} | {c:,} | {c / spins * 100:.3f}% |")
    out.append("")
    high = s["gaps_high"]
    myth = s["gaps_mythic"]
    lines = []
    lines.append(f"- Legendary+ results: {len(high):,}; effective Legendary+ rate {len(high) / spins * 100:.3f}%")
    lines.append(f"- Longest run of consecutive spins WITHOUT a Legendary+: {s['max_run_no_high']} (bound: HardPity - 1 = {cfg.hard_pity - 1})")
    if high:
        lines.append(f"- Max spins needed to reach a Legendary+: {max(high)} (hard pity bound: {cfg.hard_pity}); "
                     f"mean {sum(high) / len(high):.2f}")
    if myth:
        lines.append(f"- Max spins needed to reach a Mythic: {max(myth)} (Mythic hard pity bound: {cfg.mythic_hard_pity}); "
                     f"mean {sum(myth) / len(myth):.2f}")
    lines.append(f"- Hard pity (Legendary+ or Mythic guarantee) decided the tier on {s['pity_count']:,} spins "
                 f"({s['pity_count'] / spins * 100:.3f}%)")
    lines.append(f"- Duplicate-protection re-rolls: {s['reroll_count']:,}; new unlocks: {s['new_count']}; "
                 f"duplicate compensation paid: {s['gold']:,} gold, {s['mastery_xp']:,.0f} mastery XP")
    if s["first_mythic"] is not None:
        lines.append(f"- First Mythic on this account at spin {s['first_mythic']}")
    ok_high = all(g <= cfg.hard_pity for g in high) and s["max_run_no_high"] <= cfg.hard_pity - 1
    ok_myth = all(g <= cfg.mythic_hard_pity for g in myth)
    lines.append(f"- HARD PITY NEVER EXCEEDED: {'PASS' if ok_high else 'FAIL'} (Legendary+), {'PASS' if ok_myth else 'FAIL'} (Mythic)")
    out.extend(lines)

    if trials > 0 and present_tiers(pool)[MYTHIC]:
        firsts = spins_to_first_mythic(pool, cfg, owned, trials, seed + 1)
        firsts_sorted = sorted(firsts)
        p50 = firsts_sorted[len(firsts_sorted) // 2]
        p99 = firsts_sorted[int(len(firsts_sorted) * 0.99) - 1]
        out.append(f"- Average spins to FIRST Mythic over {trials:,} fresh accounts: {sum(firsts) / len(firsts):.2f} "
                   f"(median {p50}, p99 {p99}, max {max(firsts)})")
    out.append("")
    return ok_high and ok_myth


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--spins", type=int, default=200_000)
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--trials", type=int, default=20_000, help="fresh accounts for the first-Mythic estimate")
    parser.add_argument("--markdown", type=str, default="", help="also write the report to this file")
    args = parser.parse_args()

    out = []
    out.append("# Roll simulation output (Tools/sim_roll.py)")
    out.append("")
    all_ok = True

    # 1) The real Character pool today: Rudeus (Legendary, owned at start) + Orsted (Mythic).
    character_pool = [Entry("Rudeus", LEGENDARY), Entry("Orsted", MYTHIC)]
    all_ok &= report("1. Character pool (current data: Rudeus Legendary, Orsted Mythic)", character_pool, RollConfig(),
                     {"Rudeus"}, args.spins, args.seed, args.trials, out)

    # 2) Illustrative mixed pool (all five tiers) - exercises soft pity and duplicate protection.
    mixed_pool = [Entry("CommonA", COMMON), Entry("CommonB", COMMON), Entry("UncommonA", UNCOMMON),
                  Entry("RareA", RARE), Entry("RareB", RARE), Entry("LegendaryA", LEGENDARY),
                  Entry("LegendaryB", LEGENDARY), Entry("MythicA", MYTHIC)]
    all_ok &= report("2. Illustrative mixed pool (all tiers, default config)", mixed_pool, RollConfig(), set(),
                     args.spins, args.seed + 100, args.trials, out)

    # 3) Stress test: no soft pity + tiny Legendary weight so the hard pity must fire. The maximum spins
    #    needed for a Legendary+ must equal HardPity exactly (never more).
    stress_cfg = RollConfig(soft_pity_step=0.0, weights=[50.0, 30.0, 14.0, 0.2, 0.0])
    stress_pool = [Entry("CommonA", COMMON), Entry("UncommonA", UNCOMMON), Entry("RareA", RARE), Entry("LegendaryA", LEGENDARY)]
    all_ok &= report("3. Hard-pity stress test (soft pity off, Legendary weight 0.2)", stress_pool, stress_cfg, set(),
                     args.spins, args.seed + 200, 0, out)

    out.append(f"OVERALL: {'PASS' if all_ok else 'FAIL'}")
    text = "\n".join(out)
    print(text)
    if args.markdown:
        with open(args.markdown, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
